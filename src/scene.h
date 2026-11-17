#pragma once

#include <QJsonObject>
#include <QList>
#include <QString>
#include <QStringList>
#include <QVariantMap>

// One workspace of a monitor: its slot (1-10, the number the bar shows), its
// layout, and the desktop ids of the apps open on it, in order.
struct SceneWorkspace {
    int slot = 1;
    QString layout = "dwindle";
    QStringList apps;
};

// A scene binds to a physical monitor by identity (make, model, serial), never
// to a connector name like DP-1, which a dock changes between plugs.
struct SceneMonitor {
    QString identity;
    QString make, model, serial;
    int x = 0, y = 0, width = 0, height = 0;
    QList<SceneWorkspace> workspaces;

    QString label() const;
};

struct Scene {
    static constexpr int Version = 1;

    QString name;
    QList<SceneMonitor> monitors;

    QString slug() const { return slugify(name); }
    int appCount() const;
    int workspaceCount() const;

    QJsonObject toJson() const;
    static bool fromJson(const QJsonObject &object, Scene *scene, QString *error);
    // Lowercase letters and digits joined by single dashes: a safe file name.
    static QString slugify(const QString &name);
};
