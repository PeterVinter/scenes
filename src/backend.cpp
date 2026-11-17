#include "backend.h"

#include <QtConcurrent>

Backend::Backend(const SceneStore &store, const Hypr &hypr, const Catalog &catalog, HyprEvents *events,
                 const QString &hyprctl, QObject *parent)
    : QObject(parent), m_store(store), m_hypr(hypr), m_catalog(catalog), m_events(events), m_hyprctl(hyprctl)
{
    connect(&m_captureWatcher, &QFutureWatcher<CaptureResult>::finished, this, [this] {
        const CaptureResult result = m_captureWatcher.result();
        m_haveCurrent = result.error.isEmpty();
        m_currentError = result.error;
        if (m_haveCurrent) {
            m_current = result.scene;
            m_currentReport = result.report;
        }
        rebuildView();
        if (m_captureQueued) {
            m_captureQueued = false;
            refreshCurrent();
        }
    });
    if (m_events) {
        connect(m_events, &HyprEvents::desktopChanged, this, &Backend::refreshCurrent);
        connect(m_events, &HyprEvents::connectionChanged, this, &Backend::eventsConnectedChanged);
    }
    reloadScenes();
    refreshCurrent();
}

Backend::~Backend()
{
    m_captureWatcher.waitForFinished();
    m_planWatcher.waitForFinished();
}

void Backend::refreshNow() { refreshCurrent(); }

void Backend::refreshCurrent()
{
    // Never two captures at once: a change that arrives mid-capture queues one more.
    if (m_captureWatcher.isRunning()) {
        m_captureQueued = true;
        return;
    }
    const Hypr hypr = m_hypr;
    const Catalog &catalog = m_catalog;
    m_captureWatcher.setFuture(QtConcurrent::run([hypr, &catalog] {
        CaptureResult result;
        result.scene = hypr.capture({}, catalog, &result.report, &result.error);
        return result;
    }));
}

const Scene *Backend::findScene(const QString &slug) const
{
    for (const auto &scene : m_savedScenes)
        if (scene.slug() == slug)
            return &scene;
    return nullptr;
}

void Backend::reloadScenes()
{
    m_sceneProblems.clear();
    m_savedScenes = m_store.list(&m_sceneProblems);
    m_sceneList.clear();
    for (const auto &scene : m_savedScenes)
        m_sceneList.append(QVariantMap{{"slug", scene.slug()},
                                       {"name", scene.name},
                                       {"apps", scene.appCount()},
                                       {"workspaces", scene.workspaceCount()},
                                       {"monitors", scene.monitors.size()}});
    if (!m_selected.isEmpty() && !findScene(m_selected))
        m_selected.clear();
    emit scenesChanged();
    emit selectedChanged();
    rebuildView();
}

QVariantMap Backend::viewFor(const Scene &scene, bool current) const
{
    QSet<QString> connected;
    if (m_haveCurrent)
        for (const auto &monitor : m_current.monitors)
            connected.insert(monitor.identity);

    QVariantList monitors;
    for (const auto &monitor : scene.monitors) {
        QVariantList workspaces;
        for (const auto &workspace : monitor.workspaces) {
            QVariantList apps;
            for (const auto &id : workspace.apps)
                apps.append(QVariantMap{{"id", id},
                                        {"name", m_catalog.contains(id) ? m_catalog.entry(id).name : id},
                                        {"installed", m_catalog.contains(id)}});
            workspaces.append(QVariantMap{{"slot", workspace.slot}, {"layout", workspace.layout}, {"apps", apps}});
        }
        monitors.append(QVariantMap{{"identity", monitor.identity},
                                    {"label", monitor.label()},
                                    {"x", monitor.x},
                                    {"y", monitor.y},
                                    {"width", monitor.width},
                                    {"height", monitor.height},
                                    {"connected", current || connected.contains(monitor.identity)},
                                    {"workspaces", workspaces}});
    }
    QVariantMap view{{"name", scene.name},
                     {"slug", scene.slug()},
                     {"current", current},
                     {"monitors", monitors},
                     {"apps", scene.appCount()},
                     {"workspaces", scene.workspaceCount()}};
    if (current) {
        view["skippedNamed"] = m_currentReport.skippedNamed;
        view["unmappedWindows"] = m_currentReport.unmappedWindows;
        view["unmappedClasses"] = m_currentReport.unmappedClasses;
    }
    return view;
}

void Backend::rebuildView()
{
    if (m_selected.isEmpty()) {
        m_view = m_haveCurrent ? viewFor(m_current, true) : QVariantMap{{"current", true}, {"monitors", QVariantList()}};
    } else if (const Scene *scene = findScene(m_selected)) {
        m_view = viewFor(*scene, false);
    } else {
        m_view = {};
    }
    emit viewChanged();
}

void Backend::select(const QString &slug)
{
    if (slug == m_selected || (!slug.isEmpty() && !findScene(slug)))
        return;
    m_selected = slug;
    // A result describes one restore; it stays with the scene it was for.
    if (!m_result.isEmpty() && m_result.value("scene").toString() != slug) {
        m_result.clear();
        emit resultChanged();
    }
    emit selectedChanged();
    rebuildView();
}

void Backend::selectOffset(int delta)
{
    // Index 0 is the current desktop, then the scenes in order.
    int index = 0;
    for (int i = 0; i < m_savedScenes.size(); ++i)
        if (m_savedScenes[i].slug() == m_selected)
            index = i + 1;
    index = qBound(0, index + delta, m_savedScenes.size());
    select(index == 0 ? QString() : m_savedScenes[index - 1].slug());
}

QString Backend::selectedName() const
{
    const Scene *scene = findScene(m_selected);
    return scene ? scene->name : QString();
}

bool Backend::nameTaken(const QString &name, bool ignoreSelected) const
{
    const QString slug = Scene::slugify(name);
    return !slug.isEmpty() && m_store.exists(slug) && !(ignoreSelected && slug == m_selected);
}

void Backend::push(const History &entry)
{
    m_undo.append(entry);
    m_redo.clear();
    emit historyChanged();
}

QString Backend::saveCurrent(const QString &name)
{
    const QString trimmed = name.trimmed();
    if (Scene::slugify(trimmed).isEmpty())
        return tr("Give the scene a name.");
    if (nameTaken(trimmed))
        return tr("A scene called “%1” already exists.").arg(trimmed);
    if (!m_haveCurrent)
        return tr("The current desktop can't be read: %1").arg(m_currentError);

    Scene scene = m_current;
    scene.name = trimmed;
    QString error;
    if (!m_store.save(scene, &error))
        return error;
    reloadScenes();
    select(scene.slug());
    return {};
}

QString Backend::rename(const QString &name)
{
    const Scene *existing = findScene(m_selected);
    if (!existing)
        return tr("Select a scene to rename.");
    const QString trimmed = name.trimmed();
    if (Scene::slugify(trimmed).isEmpty())
        return tr("Give the scene a name.");
    if (nameTaken(trimmed, true))
        return tr("A scene called “%1” already exists.").arg(trimmed);

    const Scene before = *existing;
    Scene after = before;
    after.name = trimmed;
    if (after.name == before.name)
        return {};

    // Written under the new name before the old file goes, so a failure never loses the scene.
    auto apply = [this](const Scene &from, const Scene &to, QString *error) {
        if (!m_store.save(to, error))
            return false;
        if (from.slug() != to.slug())
            m_store.remove(from.slug(), nullptr);
        return true;
    };
    QString error;
    if (!apply(before, after, &error))
        return error;
    push({[=](QString *e) { return apply(after, before, e); }, [=](QString *e) { return apply(before, after, e); },
          before.slug(), after.slug()});
    reloadScenes();
    select(after.slug());
    return {};
}

QString Backend::remove()
{
    const Scene *existing = findScene(m_selected);
    if (!existing)
        return tr("Select a scene to delete.");
    const Scene scene = *existing;
    int index = 0;
    for (int i = 0; i < m_savedScenes.size(); ++i)
        if (m_savedScenes[i].slug() == scene.slug())
            index = i;
    QString error;
    if (!m_store.remove(scene.slug(), &error))
        return error;
    push({[=](QString *e) { return m_store.save(scene, e); }, [=](QString *e) { return m_store.remove(scene.slug(), e); },
          scene.slug(), QString()});
    m_selected.clear();
    reloadScenes();
    // Land on the neighbor, so deleting down a list is a run of the same key.
    const int next = qMin(index, m_savedScenes.size() - 1);
    select(next >= 0 ? m_savedScenes[next].slug() : QString());
    return {};
}

QString Backend::undo()
{
    if (m_undo.isEmpty())
        return {};
    const History entry = m_undo.takeLast();
    QString error;
    if (!entry.undo(&error)) {
        emit historyChanged();
        return error;
    }
    m_redo.append(entry);
    m_selected = entry.selectAfterUndo;
    reloadScenes();
    emit historyChanged();
    return {};
}

QString Backend::redo()
{
    if (m_redo.isEmpty())
        return {};
    const History entry = m_redo.takeLast();
    QString error;
    if (!entry.redo(&error)) {
        emit historyChanged();
        return error;
    }
    m_undo.append(entry);
    m_selected = entry.selectAfterRedo;
    reloadScenes();
    emit historyChanged();
    return {};
}

void Backend::dismissResult()
{
    if (m_result.isEmpty())
        return;
    m_result.clear();
    emit resultChanged();
}

void Backend::restore()
{
    const Scene *scene = findScene(m_selected);
    if (m_restoring || !scene)
        return;
    m_restoring = true;
    m_result.clear();
    m_progressDone = 0;
    m_progressTotal = 0;
    m_progressApp.clear();
    emit restoringChanged();
    emit resultChanged();
    emit progressChanged();

    // Reading the desktop spawns hyprctl and Omarchy scripts, so it is done off
    // the GUI thread, as the launches that follow are.
    const Scene copy = *scene;
    const Hypr hypr = m_hypr;
    const Catalog &catalog = m_catalog;
    m_planWatcher.disconnect(this);
    connect(&m_planWatcher, &QFutureWatcher<Plan>::finished, this, [this, copy] {
        const Plan plan = m_planWatcher.result();
        if (plan.launches.isEmpty()) {
            finishRestore(plan, copy.slug());
            return;
        }
        m_progressTotal = plan.launches.size();
        emit progressChanged();
        m_restorer = std::make_unique<Restorer>(plan, m_catalog, m_hyprctl);
        connect(m_restorer.get(), &Restorer::progress, this, [this](int done, int total, const QString &app) {
            m_progressDone = done;
            m_progressTotal = total;
            m_progressApp = app;
            emit progressChanged();
        });
        connect(m_restorer.get(), &Restorer::finished, this, [this, plan, copy] { finishRestore(plan, copy.slug()); });
        m_restorer->start();
    });
    m_planWatcher.setFuture(QtConcurrent::run([hypr, copy, &catalog] {
        QString error;
        const QJsonArray monitors = hypr.monitors(&error);
        const QJsonArray clients = hypr.clients(&error);
        QHash<int, QString> layouts;
        return buildPlan(copy, monitors, clients, hypr.blocks(), catalog, [&](int id) {
            if (!layouts.contains(id))
                layouts.insert(id, hypr.layout(id));
            return layouts.value(id);
        });
    }));
}

void Backend::finishRestore(const Plan &plan, const QString &slug)
{
    QVariantList unplaced, layouts;
    for (const auto &monitor : plan.unplaced)
        unplaced.append(QVariantMap{{"label", monitor.label}, {"workspaces", monitor.workspaceSlots.size()}, {"apps", monitor.appCount}});
    for (const auto &layout : plan.layouts)
        layouts.append(QVariantMap{{"workspace", layout.workspace}, {"slot", layout.slot}, {"monitor", layout.monitor},
                                   {"wanted", layout.wanted}, {"current", layout.current}});
    QStringList missing;
    for (const auto &id : plan.missingApps)
        missing.append(id);

    m_result = {{"scene", slug},
                {"launched", m_restorer ? m_restorer->launched() : 0},
                {"alreadyRunning", plan.alreadyRunning},
                {"failed", m_restorer ? m_restorer->failures() : QStringList()},
                {"unplaced", unplaced},
                {"layouts", layouts},
                {"missing", missing}};
    // Released later: this runs inside the restorer's own finished signal.
    if (m_restorer)
        m_restorer.release()->deleteLater();
    m_restoring = false;
    emit restoringChanged();
    emit resultChanged();
}
