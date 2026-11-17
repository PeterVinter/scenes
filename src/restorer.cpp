#include "restorer.h"

#include <QTimer>

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

Restorer::Restorer(const Plan &plan, const Catalog &catalog, const QString &hyprctl, QObject *parent)
    : QObject(parent), m_plan(plan), m_catalog(catalog), m_hyprctl(hyprctl)
{
    connect(&m_process, &QProcess::finished, this, [this](int code, QProcess::ExitStatus status) {
        const LaunchAction &action = m_plan.launches.at(m_index);
        const QString output = QString::fromUtf8(m_process.readAllStandardOutput()).trimmed();
        if (status != QProcess::NormalExit || code != 0 || output != "ok") {
            const QString error = QString::fromUtf8(m_process.readAllStandardError()).trimmed();
            fail(QString("%1: %2").arg(action.name, !error.isEmpty() ? error : output.isEmpty() ? "hyprctl failed" : output));
        } else {
            ++m_launched;
        }
        ++m_index;
        next();
    });
    connect(&m_process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        // finished() never comes when the program can't start.
        if (error != QProcess::FailedToStart)
            return;
        fail(QString("%1: can't run hyprctl").arg(m_plan.launches.at(m_index).name));
        ++m_index;
        QTimer::singleShot(0, this, &Restorer::next);
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
    // Deferred so a caller can connect to the signals after start().
    QTimer::singleShot(0, this, &Restorer::next);
}

void Restorer::next()
{
    if (m_index >= m_plan.launches.size()) {
        m_locker.reset();
        emit finished();
        return;
    }
    const LaunchAction &action = m_plan.launches.at(m_index);
    emit progress(m_index, m_plan.launches.size(), action.name);
    m_process.start(m_hyprctl, {"eval", launchExpression(action, m_catalog)});
}
