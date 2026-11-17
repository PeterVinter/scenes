#include "hyprevents.h"

#include <QSet>

QString HyprEvents::defaultSocketPath()
{
    const QString signature = qEnvironmentVariable("HYPRLAND_INSTANCE_SIGNATURE");
    const QString runtime = qEnvironmentVariable("XDG_RUNTIME_DIR");
    if (signature.isEmpty() || runtime.isEmpty())
        return {};
    return runtime + "/hypr/" + signature + "/.socket2.sock";
}

bool HyprEvents::relevant(const QString &event)
{
    static const QSet<QString> names{"openwindow", "closewindow", "movewindow", "movewindowv2", "workspace",
                                     "workspacev2", "monitoradded", "monitorremoved", "monitoraddedv2",
                                     "monitorremovedv2", "configreloaded", "createworkspace", "destroyworkspace",
                                     "moveworkspace", "moveworkspacev2", "renameworkspace"};
    return names.contains(event);
}

HyprEvents::HyprEvents(const QString &socketPath, QObject *parent)
    : QObject(parent), m_path(socketPath.isEmpty() ? defaultSocketPath() : socketPath)
{
    m_debounce.setSingleShot(true);
    m_debounce.setInterval(150);
    connect(&m_debounce, &QTimer::timeout, this, &HyprEvents::desktopChanged);

    m_reconnect.setSingleShot(true);
    m_reconnect.setInterval(1000);
    connect(&m_reconnect, &QTimer::timeout, this, &HyprEvents::connectNow);

    connect(&m_socket, &QLocalSocket::readyRead, this, &HyprEvents::readLines);
    connect(&m_socket, &QLocalSocket::connected, this, [this] {
        // Whatever happened while the socket was down is unknown, so look again.
        emit connectionChanged();
        m_debounce.start();
    });
    connect(&m_socket, &QLocalSocket::disconnected, this, [this] {
        emit connectionChanged();
        m_reconnect.start();
    });
    connect(&m_socket, &QLocalSocket::errorOccurred, this, [this] {
        if (m_socket.state() != QLocalSocket::ConnectedState)
            m_reconnect.start();
    });
    if (!m_path.isEmpty())
        connectNow();
}

HyprEvents::~HyprEvents()
{
    // The socket outlives the timers its signals restart, so stop listening
    // before it closes.
    m_socket.disconnect(this);
    m_reconnect.stop();
    m_socket.abort();
}

void HyprEvents::connectNow()
{
    if (m_socket.state() != QLocalSocket::UnconnectedState)
        return;
    m_buffer.clear();
    m_socket.connectToServer(m_path);
}

void HyprEvents::readLines()
{
    m_buffer += m_socket.readAll();
    int newline;
    while ((newline = m_buffer.indexOf('\n')) >= 0) {
        const QString line = QString::fromUtf8(m_buffer.left(newline));
        m_buffer.remove(0, newline + 1);
        // "event>>data"
        if (relevant(line.section(">>", 0, 0)))
            m_debounce.start();
    }
}
