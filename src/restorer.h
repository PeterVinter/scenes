#pragma once

#include <QEventLoopLocker>
#include <QObject>
#include <QProcess>
#include <memory>

#include "catalog.h"
#include "plan.h"

// Runs a plan's launches one at a time without blocking the GUI thread. It
// holds an event-loop locker while it works, so closing the window mid-restore
// doesn't cut a half-opened scene short.
class Restorer : public QObject {
    Q_OBJECT

public:
    Restorer(const Plan &plan, const Catalog &catalog, const QString &hyprctl = "hyprctl", QObject *parent = nullptr);

    void start();
    const QStringList &failures() const { return m_failures; }
    int launched() const { return m_launched; }

    // `hl.exec_cmd` takes a Lua string; the workspace rule opens the window
    // there without taking focus.
    static QString luaQuote(const QString &text);
    static QString shellQuote(const QString &text);
    static QString launchExpression(const LaunchAction &action, const Catalog &catalog);

signals:
    void progress(int done, int total, const QString &app);
    // Each failure reads "App: reason".
    void finished();

private:
    void next();
    void fail(const QString &message);

    Plan m_plan;
    const Catalog &m_catalog;
    QString m_hyprctl;
    std::unique_ptr<QEventLoopLocker> m_locker;
    QProcess m_process;
    int m_index = 0;
    int m_launched = 0;
    QStringList m_failures;
};
