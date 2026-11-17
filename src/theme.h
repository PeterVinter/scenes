#pragma once

#include <QFileSystemWatcher>
#include <QObject>
#include <QProcess>
#include <QTimer>

// Follows the Omarchy theme live: its accent, whether it is light or dark, the
// surface colors, and Hyprland's window rounding. Tests construct it with a
// temp directory and a fake `hyprctl`.
class Theme : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString accent READ accent NOTIFY changed)
    Q_PROPERTY(QString accentForeground READ accentForeground NOTIFY changed)
    Q_PROPERTY(bool light READ light NOTIFY changed)
    Q_PROPERTY(QString background READ background NOTIFY changed)
    Q_PROPERTY(QString surface READ surface NOTIFY changed)
    Q_PROPERTY(QString border READ border NOTIFY changed)
    Q_PROPERTY(QString foreground READ foreground NOTIFY changed)
    Q_PROPERTY(QString secondary READ secondary NOTIFY changed)
    Q_PROPERTY(QString selection READ selection NOTIFY changed)
    Q_PROPERTY(int radius READ radius NOTIFY changed)

public:
    explicit Theme(const QString &currentDirectory = {}, QObject *parent = nullptr, const QString &hyprctl = {});
    ~Theme() override;

    QString accent() const { return m_accent; }
    QString accentForeground() const;
    bool light() const { return m_light; }
    QString background() const { return m_background; }
    QString surface() const { return m_surface; }
    QString border() const;
    QString foreground() const { return m_foreground; }
    // Secondary text: the foreground pulled toward the background, which keeps
    // its contrast where the theme's own "muted" color would not on a light theme.
    QString secondary() const;
    QString selection() const { return m_selection; }
    int radius() const { return m_radius; }

    static QString readAccent(const QString &path);
    static QString readValue(const QString &path, const QString &key, const QString &fallback = {});

signals:
    void changed();

private:
    void reload();
    void refreshRounding();

    QString m_directory;
    QString m_accent = "#FFD60A";
    bool m_light = false;
    QString m_background = "#0e0e10", m_surface = "#1a1a1e", m_foreground = "#e8e8ec", m_selection = "#2b2b31";
    QFileSystemWatcher m_watcher;
    QTimer m_debounce;
    QString m_hyprctl;
    int m_radius = 0;
    QProcess m_roundingProcess;
    QTimer m_roundingPoll, m_roundingTimeout;
};
