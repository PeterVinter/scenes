#include "scene.h"

#include <QJsonArray>
#include <QRegularExpression>

QString SceneMonitor::label() const
{
    return !model.isEmpty() ? model : identity;
}

int Scene::appCount() const
{
    int count = 0;
    for (const auto &monitor : monitors)
        for (const auto &workspace : monitor.workspaces)
            count += workspace.apps.size();
    return count;
}

int Scene::workspaceCount() const
{
    int count = 0;
    for (const auto &monitor : monitors)
        count += monitor.workspaces.size();
    return count;
}

QString Scene::slugify(const QString &name)
{
    static const QRegularExpression separators(R"([^\p{L}\p{N}]+)");
    QString slug = name.trimmed().toLower();
    slug.replace(separators, "-");
    while (slug.startsWith('-'))
        slug.remove(0, 1);
    while (slug.endsWith('-'))
        slug.chop(1);
    return slug;
}

QJsonObject Scene::toJson() const
{
    QJsonArray monitorArray;
    for (const auto &monitor : monitors) {
        QJsonArray workspaceArray;
        for (const auto &workspace : monitor.workspaces)
            workspaceArray.append(QJsonObject{{"slot", workspace.slot},
                                              {"layout", workspace.layout},
                                              {"apps", QJsonArray::fromStringList(workspace.apps)}});
        monitorArray.append(QJsonObject{{"identity", monitor.identity},
                                        {"make", monitor.make},
                                        {"model", monitor.model},
                                        {"serial", monitor.serial},
                                        {"x", monitor.x},
                                        {"y", monitor.y},
                                        {"width", monitor.width},
                                        {"height", monitor.height},
                                        {"workspaces", workspaceArray}});
    }
    return QJsonObject{{"version", Version}, {"name", name}, {"monitors", monitorArray}};
}

bool Scene::fromJson(const QJsonObject &object, Scene *scene, QString *error)
{
    auto fail = [&](const QString &message) {
        if (error)
            *error = message;
        return false;
    };

    if (object.value("version").toInt(-1) != Version)
        return fail(QString("unsupported format version %1 (this build reads version %2)")
                        .arg(object.value("version").toInt(-1)).arg(Version));
    Scene result;
    result.name = object.value("name").toString().trimmed();
    if (result.name.isEmpty() || result.slug().isEmpty())
        return fail("the scene has no name");
    if (!object.value("monitors").isArray())
        return fail("\"monitors\" must be a list");

    static const QRegularExpression layoutName("^[a-z]+$");
    for (const auto &monitorValue : object.value("monitors").toArray()) {
        const QJsonObject monitorObject = monitorValue.toObject();
        SceneMonitor monitor;
        monitor.identity = monitorObject.value("identity").toString();
        if (monitor.identity.isEmpty())
            return fail("a monitor has no identity");
        monitor.make = monitorObject.value("make").toString();
        monitor.model = monitorObject.value("model").toString();
        monitor.serial = monitorObject.value("serial").toString();
        monitor.x = monitorObject.value("x").toInt();
        monitor.y = monitorObject.value("y").toInt();
        monitor.width = monitorObject.value("width").toInt();
        monitor.height = monitorObject.value("height").toInt();
        if (!monitorObject.value("workspaces").isArray())
            return fail(QString("monitor \"%1\": \"workspaces\" must be a list").arg(monitor.identity));
        for (const auto &workspaceValue : monitorObject.value("workspaces").toArray()) {
            const QJsonObject workspaceObject = workspaceValue.toObject();
            SceneWorkspace workspace;
            workspace.slot = workspaceObject.value("slot").toInt(0);
            if (workspace.slot < 1 || workspace.slot > 10)
                return fail(QString("monitor \"%1\": workspace slot must be 1 to 10").arg(monitor.identity));
            workspace.layout = workspaceObject.value("layout").toString();
            if (!layoutName.match(workspace.layout).hasMatch())
                return fail(QString("monitor \"%1\": workspace %2 has no valid layout")
                                .arg(monitor.identity).arg(workspace.slot));
            for (const auto &app : workspaceObject.value("apps").toArray()) {
                if (!app.isString() || app.toString().isEmpty())
                    return fail(QString("monitor \"%1\": workspace %2 lists an invalid app")
                                    .arg(monitor.identity).arg(workspace.slot));
                workspace.apps.append(app.toString());
            }
            monitor.workspaces.append(workspace);
        }
        result.monitors.append(monitor);
    }
    if (scene)
        *scene = result;
    return true;
}
