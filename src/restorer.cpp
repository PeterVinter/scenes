#include "restorer.h"

#include <QJsonDocument>
#include <QJsonObject>

QString Restorer::luaQuote(const QString &text)
{
    QString escaped = text;
    escaped.replace('\\', "\\\\").replace('"', "\\\"").replace('\n', "\\n");
    return '"' + escaped + '"';
}

QString Restorer::shellQuote(const QString &text)
{
    QString escaped = text;
    escaped.replace('\'', "'\\''");
    return '\'' + escaped + '\'';
}

QString Restorer::launchExpression(const LaunchAction &action, const Catalog &catalog)
{
    // The same two routes Omarchy's own launcher takes: a terminal app through
    // omarchy-launch-tui, everything else through gtk-launch in a UWSM scope.
    QString command;
    const QStringList terminal = catalog.terminalCommand(action.desktopId);
    if (!terminal.isEmpty()) {
        QStringList words;
        for (const auto &word : terminal)
            words << shellQuote(word);
        command = "omarchy-launch-tui " + words.join(' ');
    } else {
        command = "uwsm-app -- gtk-launch " + shellQuote(action.desktopId + ".desktop");
    }
    return QString("hl.exec_cmd(%1, {workspace=%2})")
        .arg(luaQuote(command), luaQuote(QString::number(action.workspace) + " silent"));
}

QString Restorer::moveExpression(const QString &address, int workspace)
{
    // follow=false: the window goes there and the view stays where it is.
    return QString("hl.dsp.window.move({workspace=%1, follow=false, window=%2})")
        .arg(luaQuote(QString::number(workspace)), luaQuote("address:" + address));
}

QString Restorer::focusExpression(const QString &address)
{
    return QString("hl.dsp.focus({window=%1})").arg(luaQuote("address:" + address));
}

QString Restorer::activeAddress(const QByteArray &json)
{
    return QJsonDocument::fromJson(json).object().value("address").toString();
}

Restorer::Restorer(const Plan &plan, const Catalog &catalog, const QString &hyprctl, const QString &eventSocket,
                   QObject *parent)
    : QObject(parent), m_plan(plan), m_catalog(catalog), m_hyprctl(hyprctl), m_events(eventSocket)
{
    m_windowTimeout.setSingleShot(true);
    m_windowTimeout.setInterval(10000);
    connect(&m_windowTimeout, &QTimer::timeout, this, [this] {
        if (m_phase != Phase::Waiting)
            return;
        fail(QString("%1: no window appeared").arg(m_plan.launches.at(m_index).name));
        advance();
    });
    connect(&m_events, &HyprEvents::windowOpened, this, &Restorer::windowOpened);
    connect(&m_process, &QProcess::finished, this, &Restorer::processFinished);
    connect(&m_process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        // finished() never comes when the program can't start.
        if (error == QProcess::FailedToStart)
            QTimer::singleShot(0, this, &Restorer::processFailedToStart);
    });
}

void Restorer::fail(const QString &message)
{
    m_failures.append(message);
}

void Restorer::start()
{
    m_locker = std::make_unique<QEventLoopLocker>();
    m_index = 0;
    m_launched = 0;
    m_failures.clear();
    m_opened.clear();
    m_previousActive.clear();
    m_verified = m_events.connected();
    // Deferred so a caller can connect to the signals after start().
    QTimer::singleShot(0, this, [this] {
        if (m_verified && !m_plan.launches.isEmpty()) {
            // Remember what has focus, to hand it back after the moves.
            m_phase = Phase::Reading;
            m_process.start(m_hyprctl, {"-j", "activewindow"});
        } else {
            next();
        }
    });
}

void Restorer::next()
{
    if (m_index >= m_plan.launches.size()) {
        if (m_verified && !m_opened.isEmpty() && !m_previousActive.isEmpty()) {
            m_phase = Phase::Checking;
            m_process.start(m_hyprctl, {"-j", "activewindow"});
        } else {
            finish();
        }
        return;
    }
    const LaunchAction &action = m_plan.launches.at(m_index);
    emit progress(m_index, m_plan.launches.size(), action.name);
    m_matchedAddress.clear();
    m_matchedWorkspace.clear();
    m_phase = Phase::Launching;
    m_process.start(m_hyprctl, {"eval", launchExpression(action, m_catalog)});
}

void Restorer::advance()
{
    m_windowTimeout.stop();
    m_phase = Phase::Idle;
    ++m_index;
    next();
}

void Restorer::finish()
{
    m_phase = Phase::Idle;
    m_windowTimeout.stop();
    m_locker.reset();
    emit finished();
}

void Restorer::windowOpened(const QString &address, const QString &workspace, const QString &windowClass)
{
    // Only a window that opened after the launch started, and that belongs to
    // the app being launched, is claimed. The first such window wins.
    if ((m_phase != Phase::Launching && m_phase != Phase::Waiting) || !m_matchedAddress.isEmpty())
        return;
    if (m_catalog.idForClass(windowClass) != m_plan.launches.at(m_index).desktopId)
        return;
    m_matchedAddress = address;
    m_matchedWorkspace = workspace;
    if (m_phase == Phase::Waiting)
        handleMatch();
}

void Restorer::handleMatch()
{
    m_windowTimeout.stop();
    const LaunchAction &action = m_plan.launches.at(m_index);
    m_opened.append(m_matchedAddress);
    if (m_matchedWorkspace == QString::number(action.workspace)) {
        ++m_launched; // The rule on the launch worked: already where it belongs.
        advance();
        return;
    }
    m_phase = Phase::Moving;
    // A dispatcher only runs through `dispatch`. `eval` would answer "ok" and do
    // nothing, and `dispatch` also reports when the window is gone.
    m_process.start(m_hyprctl, {"dispatch", moveExpression(m_matchedAddress, action.workspace)});
}

void Restorer::processFinished(int code, QProcess::ExitStatus status)
{
    const QString output = QString::fromUtf8(m_process.readAllStandardOutput()).trimmed();
    const QString error = QString::fromUtf8(m_process.readAllStandardError()).trimmed();
    const bool ok = status == QProcess::NormalExit && code == 0 && output == "ok";
    const QString reason = !error.isEmpty() ? error : output.isEmpty() ? "hyprctl failed" : output;

    switch (m_phase) {
    case Phase::Reading:
        m_previousActive = activeAddress(m_process.exitCode() == 0 ? output.toUtf8() : QByteArray());
        next();
        break;
    case Phase::Launching: {
        const LaunchAction &action = m_plan.launches.at(m_index);
        if (!ok) {
            fail(QString("%1: %2").arg(action.name, reason));
            advance();
        } else if (!m_verified) {
            ++m_launched; // No event socket: the launch is all that can be confirmed.
            advance();
        } else if (!m_matchedAddress.isEmpty()) {
            m_phase = Phase::Waiting;
            handleMatch();
        } else {
            m_phase = Phase::Waiting;
            m_windowTimeout.start();
        }
        break;
    }
    case Phase::Moving: {
        const LaunchAction &action = m_plan.launches.at(m_index);
        if (ok)
            ++m_launched;
        else
            fail(QString("%1: opened, but could not be moved to workspace %2: %3").arg(action.name).arg(action.workspace).arg(reason));
        advance();
        break;
    }
    case Phase::Checking:
        // Moving a window can leave focus on it; hand focus back to what had it,
        // but only if focus is on a window this restore opened.
        if (m_process.exitCode() == 0 && m_opened.contains(activeAddress(output.toUtf8()))) {
            m_phase = Phase::Refocusing;
            m_process.start(m_hyprctl, {"dispatch", focusExpression(m_previousActive)});
        } else {
            finish();
        }
        break;
    case Phase::Refocusing:
        finish();
        break;
    case Phase::Waiting:
    case Phase::Idle:
        break;
    }
}

void Restorer::processFailedToStart()
{
    switch (m_phase) {
    case Phase::Launching:
    case Phase::Moving:
        fail(QString("%1: can't run hyprctl").arg(m_plan.launches.at(m_index).name));
        advance();
        break;
    case Phase::Reading:
        next();
        break;
    default:
        finish();
        break;
    }
}
