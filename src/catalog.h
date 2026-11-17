#pragma once

#include <QHash>
#include <QString>
#include <QStringList>

struct AppEntry {
    QString id; // The desktop id without ".desktop", as Omarchy's dock stores it.
    QString name;
    QString icon;
    QString exec;
    QString wmClass;
    bool terminal = false;
};

// The installed desktop entries, so a window's class can be turned into the
// desktop id a scene stores, and an id back into something that can launch.
class Catalog {
public:
    explicit Catalog(const QStringList &directories = {});

    static QStringList defaultDirectories();

    bool contains(const QString &id) const { return m_entries.contains(id); }
    AppEntry entry(const QString &id) const { return m_entries.value(id); }
    // Empty when no installed entry claims the class.
    QString idForClass(const QString &windowClass) const;
    // The command words of a terminal app, without the field codes (%f, %U...).
    QStringList terminalCommand(const QString &id) const;

    static QStringList splitExec(const QString &exec);

private:
    QHash<QString, AppEntry> m_entries;
    QHash<QString, QString> m_classes; // lowercase window class -> id
};
