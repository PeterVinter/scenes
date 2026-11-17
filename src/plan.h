#pragma once

#include <QHash>
#include <QJsonArray>
#include <QJsonObject>
#include <functional>

#include "catalog.h"
#include "scene.h"

struct LaunchAction {
    QString desktopId;
    QString name;
    int workspace = 0;
    QString monitor; // Label, for messages.
    // Set for a window that was floating: where it goes, in layout coordinates
    // (the target monitor's corner added, and scaled if its resolution differs).
    bool floating = false;
    int x = 0, y = 0, width = 0, height = 0;
};

// A workspace whose layout differs from the scene's. Omarchy has no command to
// set a given workspace's layout, so this is reported and never applied.
struct LayoutGap {
    int workspace = 0;
    int slot = 0;
    QString monitor;
    QString wanted, current;
};

struct UnplacedMonitor {
    QString identity, label;
    QList<int> workspaceSlots;
    int appCount = 0;
};

// Everything a restore would do, worked out before anything is touched. A
// restore only adds: it never closes or moves a window that is already open.
struct Plan {
    QList<LaunchAction> launches;
    QList<LayoutGap> layouts;
    QList<UnplacedMonitor> unplaced;
    QStringList missingApps; // Desktop ids that are not installed here.
    int alreadyRunning = 0;

    QJsonObject toJson() const;
    // One line per step, exactly what `restore --dry-run` prints.
    QStringList describe() const;
    bool hasProblems() const { return !unplaced.isEmpty() || !missingApps.isEmpty(); }
};

Plan buildPlan(const Scene &scene, const QJsonArray &monitors, const QJsonArray &clients,
               const QHash<QString, int> &blocks, const Catalog &catalog,
               const std::function<QString(int)> &currentLayout);
