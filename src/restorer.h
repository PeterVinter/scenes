#pragma once

#include <QEventLoopLocker>
#include <QObject>
#include <QProcess>
#include <QTimer>
#include <memory>

#include "catalog.h"
#include "hyprevents.h"
#include "plan.h"

// Runs a plan's launches one at a time without blocking the GUI thread. It
// holds an event-loop locker while it works, so closing the window mid-restore
// doesn't cut a half-opened scene short.
//
// Starting an app is not enough to put it on a workspace: apps that hand over
// to an instance that is already running (Files, Chromium, Firefox) ignore the
// workspace rule on the process, and open on whatever workspace is active. So
// after each launch the restorer waits for the new window of that app on
// Hyprland's event socket, moves it to its workspace without following it, and
// gives focus back. Only a window that opened after the launch, and whose class
// belongs to the app, is ever moved.
class Restorer : public QObject {
    Q_OBJECT

public:
    Restorer(const Plan &plan, const Catalog &catalog, const QString &hyprctl = "hyprctl",
             const QString &eventSocket = {}, QObject *parent = nullptr);

    void start();
    // How long to wait for an app's window to appear before calling it failed.
    void setWindowTimeout(int milliseconds) { m_windowTimeout.setInterval(milliseconds); }
    const QStringList &failures() const { return m_failures; }
    int launched() const { return m_launched; }
    // False when Hyprland's event socket was not reachable, so launches could
    // not be confirmed and the windows could not be moved.
    bool verified() const { return m_verified; }

    // `hl.exec_cmd` takes a Lua string; the workspace rule opens the window
    // there without taking focus, for the apps that honor it.
    static QString luaQuote(const QString &text);
    static QString shellQuote(const QString &text);
    static QString launchExpression(const LaunchAction &action, const Catalog &catalog);
    static QString moveExpression(const QString &address, int workspace);
    static QString focusExpression(const QString &address);

signals:
    void progress(int done, int total, const QString &app);
    // Each failure reads "App: reason".
    void finished();

private:
    enum class Phase { Idle, Reading, Launching, Waiting, Moving, Checking, Refocusing };

    void next();
    void launched(const QString &address);
    void fail(const QString &message);
    void advance();
    void finish();
    void windowOpened(const QString &address, const QString &workspace, const QString &windowClass);
    void processFinished(int code, QProcess::ExitStatus status);
    void processFailedToStart();
    void handleMatch();
    void finishWithFocus();
    static QString activeAddress(const QByteArray &json);

    Plan m_plan;
    const Catalog &m_catalog;
    QString m_hyprctl;
    HyprEvents m_events;
    std::unique_ptr<QEventLoopLocker> m_locker;
    QProcess m_process;
    QTimer m_windowTimeout;
    Phase m_phase = Phase::Idle;
    int m_index = 0;
    int m_launched = 0;
    bool m_verified = false;
    QStringList m_failures;
    QString m_matchedAddress, m_matchedWorkspace;
    QString m_previousActive;
    QStringList m_opened;
};
