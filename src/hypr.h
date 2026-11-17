#pragma once

#include <QHash>
#include <QJsonArray>
#include <QJsonObject>
#include <QString>
#include <QStringList>

#include "catalog.h"
#include "scene.h"

// What a capture left out, so the window and the CLI can say so.
struct CaptureReport {
    QStringList skippedNamed;     // Named workspaces: Hyprland reports negative ids.
    QStringList unmappedClasses;  // Windows with no installed desktop entry.
    int unmappedWindows = 0;
};

// Reads the desktop through hyprctl and Omarchy's own commands. It only reads.
class Hypr {
public:
    struct Tools {
        QString hyprctl = "hyprctl";
        // Prints a workspace's layout; floating is not a Hyprland layout, so
        // only this command knows it.
        QString layoutCommand = "omarchy-hyprland-workspace-layout-current";
        // Omarchy's monitor -> workspace block assignment (read only).
        QString blocksFile;
    };

    Hypr() : Hypr(Tools()) {}
    explicit Hypr(const Tools &tools);

    QJsonArray monitors(QString *error) const;
    QJsonArray workspaces(QString *error) const;
    QJsonArray clients(QString *error) const;
    QString layout(int workspace) const;
    // identity -> block, from the file Omarchy's monitor-workspaces.lua keeps.
    QHash<QString, int> blocks() const;

    Scene capture(const QString &name, const Catalog &catalog, CaptureReport *report, QString *error) const;

    // "make model serial", the form Omarchy's own state file names a monitor by.
    static QString identity(const QJsonObject &monitor);
    // Each monitor owns a block of ten workspace ids: block*10 + slot.
    static int blockOf(int workspaceId) { return (workspaceId - 1) / 10; }
    static int slotOf(int workspaceId) { return (workspaceId - 1) % 10 + 1; }
    static int blockFor(const QJsonObject &monitor, const QHash<QString, int> &blocks);

    // The windows of the scenes app itself are never part of a scene.
    static bool isOwnWindow(const QString &windowClass) { return windowClass == "scenes"; }

private:
    QJsonArray readArray(const QStringList &args, QString *error) const;

    Tools m_tools;
};

// Runs a program to completion with a time limit; false (and a message) when
// it can't start, times out or exits non-zero.
bool runTool(const QString &program, const QStringList &args, QByteArray *output, QString *error, int timeoutMs = 5000);
