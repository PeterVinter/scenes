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
                unplaced.workspaceSlots.append(workspace.slot);
                unplaced.appCount += workspace.apps.size();
            }
            plan.unplaced.append(unplaced);
            continue;
        }

        const QJsonObject current = connected.value(sceneMonitor.identity);
        const int block = Hypr::blockFor(current, blocks);
        // The monitor's own place in the layout, and how much bigger or smaller it
        // is than when the scene was saved, so a floating window keeps its place
        // on a monitor that has a different resolution now.
        const double scale = qMax(0.1, current.value("scale").toDouble(1.0));
        const double scaleX = sceneMonitor.width > 0 ? current.value("width").toDouble() / scale / sceneMonitor.width : 1.0;
        const double scaleY = sceneMonitor.height > 0 ? current.value("height").toDouble() / scale / sceneMonitor.height : 1.0;
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
                for (int n = have; n < wanted.value(app); ++n) {
                    LaunchAction action{app, catalog.entry(app).name, id, sceneMonitor.label()};
                    for (const auto &window : workspace.floating) {
                        if (window.app != app || window.index != n)
                            continue;
                        action.floating = true;
                        action.x = current.value("x").toInt() + qRound(window.x * scaleX);
                        action.y = current.value("y").toInt() + qRound(window.y * scaleY);
                        action.width = qMax(1, qRound(window.width * scaleX));
                        action.height = qMax(1, qRound(window.height * scaleY));
                    }
                    plan.launches.append(action);
                }
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
    {
        QJsonObject entry{{"app", launch.desktopId}, {"name", launch.name},
                          {"workspace", launch.workspace}, {"monitor", launch.monitor}};
        if (launch.floating)
            entry["floating"] = QJsonObject{{"x", launch.x}, {"y", launch.y}, {"width", launch.width}, {"height", launch.height}};
        launchArray.append(entry);
    }
    for (const auto &layout : layouts)
        layoutArray.append(QJsonObject{{"workspace", layout.workspace}, {"slot", layout.slot},
                                       {"monitor", layout.monitor}, {"wanted", layout.wanted},
                                       {"current", layout.current}, {"applied", false}});
    for (const auto &monitor : unplaced) {
        QJsonArray slotArray;
        for (int slot : monitor.workspaceSlots)
            slotArray.append(slot);
        unplacedArray.append(QJsonObject{{"identity", monitor.identity}, {"monitor", monitor.label},
                                         {"workspaces", slotArray}, {"apps", monitor.appCount}});
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
        lines << QString("launch %1 on workspace %2 (%3)%4").arg(launch.desktopId).arg(launch.workspace).arg(launch.monitor)
                     .arg(launch.floating ? QString(", floating at %1,%2 %3x%4").arg(launch.x).arg(launch.y).arg(launch.width).arg(launch.height)
                                          : QString());
    for (const auto &monitor : unplaced)
        lines << QString("skip monitor %1: not connected (workspaces %2, apps: %3)")
                     .arg(monitor.label, [&] { QStringList list; for (int n : monitor.workspaceSlots) list << QString::number(n); return list.join(','); }())
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
