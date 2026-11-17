#include "hypr.h"

#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QProcess>
#include <QRegularExpression>
#include <algorithm>

bool runTool(const QString &program, const QStringList &args, QByteArray *output, QString *error, int timeoutMs)
{
    QProcess process;
    process.start(program, args);
    if (!process.waitForStarted(timeoutMs)) {
        if (error)
            *error = QString("can't run %1: %2").arg(program, process.errorString());
        return false;
    }
    if (!process.waitForFinished(timeoutMs)) {
        process.kill();
        process.waitForFinished(500);
        if (error)
            *error = QString("%1 did not answer in time").arg(program);
        return false;
    }
    if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
        if (error)
            *error = QString("%1 failed: %2").arg(program, QString::fromUtf8(process.readAllStandardError()).trimmed());
        return false;
    }
    if (output)
        *output = process.readAllStandardOutput();
    return true;
}

Hypr::Hypr(const Tools &tools) : m_tools(tools)
{
    if (m_tools.blocksFile.isEmpty()) {
        const QString state = qEnvironmentVariable("XDG_STATE_HOME", QDir::homePath() + "/.local/state");
        m_tools.blocksFile = state + "/omarchy/monitor-workspaces";
    }
}

QJsonArray Hypr::readArray(const QStringList &args, QString *error) const
{
    QByteArray output;
    if (!runTool(m_tools.hyprctl, args, &output, error))
        return {};
    const QJsonDocument document = QJsonDocument::fromJson(output);
    if (!document.isArray()) {
        if (error)
            *error = QString("hyprctl %1 did not return a list").arg(args.join(' '));
        return {};
    }
    return document.array();
}

QJsonArray Hypr::monitors(QString *error) const { return readArray({"-j", "monitors"}, error); }
QJsonArray Hypr::workspaces(QString *error) const { return readArray({"-j", "workspaces"}, error); }
QJsonArray Hypr::clients(QString *error) const { return readArray({"-j", "clients"}, error); }

QString Hypr::layout(int workspace) const
{
    QByteArray output;
    if (!runTool(m_tools.layoutCommand, {QString::number(workspace)}, &output, nullptr))
        return {};
    const QString layout = QString::fromUtf8(output).trimmed();
    static const QRegularExpression valid("^[a-z]+$");
    return valid.match(layout).hasMatch() ? layout : QString();
}

QString Hypr::identity(const QJsonObject &monitor)
{
    return QStringList{monitor.value("make").toString(), monitor.value("model").toString(),
                       monitor.value("serial").toString()}.join(' ').trimmed();
}

QHash<QString, int> Hypr::blocks() const
{
    QHash<QString, int> result;
    QFile file(m_tools.blocksFile);
    if (!file.open(QIODevice::ReadOnly))
        return result;
    // "<block>\t<connector>\t<identity>" per line.
    for (const auto &line : QString::fromUtf8(file.readAll()).split('\n')) {
        const QStringList fields = line.split('\t');
        bool ok = false;
        const int block = fields.value(0).toInt(&ok);
        if (ok && fields.size() >= 3 && !fields[2].isEmpty())
            result.insert(fields[2], block);
    }
    return result;
}

int Hypr::blockFor(const QJsonObject &monitor, const QHash<QString, int> &blocks)
{
    const auto found = blocks.constFind(identity(monitor));
    if (found != blocks.constEnd())
        return *found;
    // Until Omarchy has written its file, a monitor shows a workspace of its own block.
    const int active = monitor.value("activeWorkspace").toObject().value("id").toInt(-1);
    return active > 0 ? blockOf(active) : 0;
}

Scene Hypr::capture(const QString &name, const Catalog &catalog, CaptureReport *report, QString *error) const
{
    Scene scene;
    scene.name = name;
    CaptureReport local;
    CaptureReport &skipped = report ? *report : local;
    skipped = CaptureReport();

    QString problem;
    const QJsonArray monitorList = monitors(&problem);
    if (monitorList.isEmpty()) {
        if (error)
            *error = problem.isEmpty() ? "Hyprland reports no monitors" : problem;
        return scene;
    }
    const QJsonArray workspaceList = workspaces(&problem);
    if (!problem.isEmpty()) {
        if (error)
            *error = problem;
        return scene;
    }
    const QJsonArray clientList = clients(&problem);
    if (!problem.isEmpty()) {
        if (error)
            *error = problem;
        return scene;
    }

    const QHash<QString, int> blockMap = blocks();
    struct Info { SceneMonitor monitor; QString connector; int block; int activeWorkspace; };
    QList<Info> infos;
    for (const auto &value : monitorList) {
        const QJsonObject object = value.toObject();
        Info info;
        info.connector = object.value("name").toString();
        info.block = blockFor(object, blockMap);
        info.activeWorkspace = object.value("activeWorkspace").toObject().value("id").toInt(-1);
        SceneMonitor &monitor = info.monitor;
        monitor.identity = identity(object);
        monitor.make = object.value("make").toString();
        monitor.model = object.value("model").toString();
        monitor.serial = object.value("serial").toString();
        const double scale = std::max(0.1, object.value("scale").toDouble(1.0));
        monitor.x = object.value("x").toInt();
        monitor.y = object.value("y").toInt();
        monitor.width = qRound(object.value("width").toDouble() / scale);
        monitor.height = qRound(object.value("height").toDouble() / scale);
        infos.append(info);
    }

    // A window that can't be matched to an installed desktop entry has no id to
    // store, so it is counted and left out rather than guessed.
    struct Window { QString id; int x, y; };
    QHash<int, QList<Window>> windowsByWorkspace;
    for (const auto &value : clientList) {
        const QJsonObject client = value.toObject();
        const int workspace = client.value("workspace").toObject().value("id").toInt(-1);
        const QString windowClass = client.value("class").toString();
        if (workspace <= 0 || !client.value("mapped").toBool(true) || isOwnWindow(windowClass))
            continue;
        const QString id = catalog.idForClass(windowClass);
        if (id.isEmpty() || id == "scenes") {
            ++skipped.unmappedWindows;
            if (!windowClass.isEmpty() && !skipped.unmappedClasses.contains(windowClass))
                skipped.unmappedClasses.append(windowClass);
            continue;
        }
        const QJsonArray at = client.value("at").toArray();
        windowsByWorkspace[workspace].append({id, at.at(0).toInt(), at.at(1).toInt()});
    }

    for (const auto &value : workspaceList) {
        const QJsonObject workspace = value.toObject();
        const int id = workspace.value("id").toInt();
        const QString workspaceName = workspace.value("name").toString();
        if (id <= 0) {
            // Special workspaces (the minimized stash) belong to Omarchy. A named
            // workspace reports a negative id, which Hyprland would read as a
            // relative selector if it were ever targeted, so it is skipped.
            if (!workspaceName.startsWith("special:"))
                skipped.skippedNamed.append(workspaceName);
            continue;
        }
        const int owner = [&] {
            for (int i = 0; i < infos.size(); ++i)
                if (infos[i].block == blockOf(id))
                    return i;
            for (int i = 0; i < infos.size(); ++i)
                if (infos[i].connector == workspace.value("monitor").toString())
                    return i;
            return -1;
        }();
        if (owner < 0)
            continue;
        const bool active = infos[owner].activeWorkspace == id;
        if (workspace.value("windows").toInt() == 0 && !active)
            continue;

        SceneWorkspace saved;
        saved.slot = slotOf(id);
        saved.layout = layout(id);
        if (saved.layout.isEmpty())
            saved.layout = workspace.value("tiledLayout").toString("dwindle");
        auto windows = windowsByWorkspace.value(id);
        // Reading order across the screen approximates the order apps were arranged in.
        std::stable_sort(windows.begin(), windows.end(), [](const Window &a, const Window &b) {
            return a.x != b.x ? a.x < b.x : a.y < b.y;
        });
        for (const auto &window : windows)
            saved.apps.append(window.id);
        infos[owner].monitor.workspaces.append(saved);
    }

    for (auto &info : infos) {
        std::sort(info.monitor.workspaces.begin(), info.monitor.workspaces.end(),
                  [](const SceneWorkspace &a, const SceneWorkspace &b) { return a.slot < b.slot; });
        scene.monitors.append(info.monitor);
    }
    std::sort(scene.monitors.begin(), scene.monitors.end(), [](const SceneMonitor &a, const SceneMonitor &b) {
        return a.x != b.x ? a.x < b.x : a.y < b.y;
    });
    return scene;
}
