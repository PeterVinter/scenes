#include "plan.h"

#include "hypr.h"

Plan buildPlan(const Scene &scene, const QJsonArray &monitors, const QJsonArray &clients,
               const QHash<QString, int> &blocks, const Catalog &catalog,
               const std::function<QString(int)> &currentLayout)
{
    Plan plan;

    QHash<QString, QJsonObject> connected;
    for (const auto &value : monitors)
        connected.insert(Hypr::identity(value.toObject()), value.toObject());

    // What is already open on each workspace, by desktop id.
    QHash<int, QHash<QString, int>> running;
    for (const auto &value : clients) {
        const QJsonObject client = value.toObject();
        const int workspace = client.value("workspace").toObject().value("id").toInt(-1);
        if (workspace <= 0 || !client.value("mapped").toBool(true))
            continue;
        const QString id = catalog.idForClass(client.value("class").toString());
        if (!id.isEmpty())
            ++running[workspace][id];
    }

    for (const auto &sceneMonitor : scene.monitors) {
        if (!connected.contains(sceneMonitor.identity)) {
            // Never moved onto another monitor silently: the person decides.
            UnplacedMonitor unplaced;
            unplaced.identity = sceneMonitor.identity;
            unplaced.label = sceneMonitor.label();
            for (const auto &workspace : sceneMonitor.workspaces) {
                unplaced.slots.append(workspace.slot);
                unplaced.appCount += workspace.apps.size();
            }
            plan.unplaced.append(unplaced);
            continue;
        }

        const int block = Hypr::blockFor(connected.value(sceneMonitor.identity), blocks);
        for (const auto &workspace : sceneMonitor.workspaces) {
            const int id = block * 10 + workspace.slot;

            QHash<QString, int> wanted;
            QStringList order;
            for (const auto &app : workspace.apps) {
                if (!wanted.contains(app))
                    order.append(app);
                ++wanted[app];
            }
            for (const auto &app : order) {
                if (!catalog.contains(app)) {
                    if (!plan.missingApps.contains(app))
                        plan.missingApps.append(app);
                    continue;
                }
                const int have = running.value(id).value(app);
                plan.alreadyRunning += qMin(have, wanted.value(app));
                for (int n = have; n < wanted.value(app); ++n)
                    plan.launches.append({app, catalog.entry(app).name, id, sceneMonitor.label()});
            }

            const QString current = currentLayout ? currentLayout(id) : QString();
            if (!current.isEmpty() && current != workspace.layout)
                plan.layouts.append({id, workspace.slot, sceneMonitor.label(), workspace.layout, current});
        }
    }
    return plan;
}

QJsonObject Plan::toJson() const
{
    QJsonArray launchArray, layoutArray, unplacedArray;
    for (const auto &launch : launches)
        launchArray.append(QJsonObject{{"app", launch.desktopId}, {"name", launch.name},
                                       {"workspace", launch.workspace}, {"monitor", launch.monitor}});
    for (const auto &layout : layouts)
        layoutArray.append(QJsonObject{{"workspace", layout.workspace}, {"slot", layout.slot},
                                       {"monitor", layout.monitor}, {"wanted", layout.wanted},
                                       {"current", layout.current}, {"applied", false}});
    for (const auto &monitor : unplaced) {
        QJsonArray slots;
        for (int slot : monitor.slots)
            slots.append(slot);
        unplacedArray.append(QJsonObject{{"identity", monitor.identity}, {"monitor", monitor.label},
                                         {"workspaces", slots}, {"apps", monitor.appCount}});
    }
    return QJsonObject{{"launch", launchArray},
                       {"layouts_not_applied", layoutArray},
                       {"unplaced_monitors", unplacedArray},
                       {"apps_not_installed", QJsonArray::fromStringList(missingApps)},
                       {"already_running", alreadyRunning}};
}

QStringList Plan::describe() const
{
    QStringList lines;
    for (const auto &launch : launches)
        lines << QString("launch %1 on workspace %2 (%3)").arg(launch.desktopId).arg(launch.workspace).arg(launch.monitor);
    for (const auto &monitor : unplaced)
        lines << QString("skip monitor %1: not connected (workspaces %2, %3 apps)")
                     .arg(monitor.label, [&] { QStringList s; for (int n : monitor.slots) s << QString::number(n); return s.join(','); }())
                     .arg(monitor.appCount);
    for (const auto &app : missingApps)
        lines << QString("skip %1: not installed").arg(app);
    for (const auto &layout : layouts)
        lines << QString("layout of workspace %1 stays %2 (scene wants %3): Omarchy has no command to set the layout of a given workspace")
                     .arg(layout.workspace).arg(layout.current, layout.wanted);
    if (lines.isEmpty())
        lines << "nothing to do";
    return lines;
}
