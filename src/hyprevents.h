#pragma once

#include <QLocalSocket>
#include <QObject>
#include <QTimer>

// Follows Hyprland's event socket instead of polling, and reconnects when the
// socket drops (a Hyprland restart, a config reload that rebuilds it).
class HyprEvents : public QObject {
    Q_OBJECT

public:
    explicit HyprEvents(const QString &socketPath = {}, QObject *parent = nullptr);
    ~HyprEvents() override;

    bool connected() const { return m_socket.state() == QLocalSocket::ConnectedState; }
    static QString defaultSocketPath();
    // Whether an event changes what the "current desktop" view shows.
    static bool relevant(const QString &event);

signals:
    // A window opened: its address ("0x..."), the name of its workspace and its
    // class. Not debounced, so a restore can claim the window it just launched.
    void windowOpened(const QString &address, const QString &workspace, const QString &windowClass);
    // Debounced: a burst of events (a workspace switch) is one change.
    void desktopChanged();
    void connectionChanged();

private:
    void connectNow();
    void readLines();

    QString m_path;
    QLocalSocket m_socket;
    QTimer m_reconnect, m_debounce;
    QByteArray m_buffer;
};
