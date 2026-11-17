// Theme watching follows Omacut (MIT, David Heinemeier Hansson).
#include "theme.h"

#include <QColor>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QStandardPaths>
#include <cmath>

Theme::Theme(const QString &directory, QObject *parent, const QString &hyprctl)
    : QObject(parent),
      m_directory(directory.isEmpty() ? QDir::homePath() + "/.local/state/omarchy/current" : directory) {
    m_debounce.setSingleShot(true);
    m_debounce.setInterval(80);
    connect(&m_watcher, &QFileSystemWatcher::directoryChanged, this, [this] { m_debounce.start(); });
    connect(&m_watcher, &QFileSystemWatcher::fileChanged, this, [this] { m_debounce.start(); });
    connect(&m_debounce, &QTimer::timeout, this, &Theme::reload);

    // Corners follow Hyprland's effective rounding, which themes and personal
    // overrides can both set, so ask hyprctl rather than reading its config.
    // Without Hyprland the corners stay sharp, like Omarchy's default.
    m_hyprctl = !hyprctl.isEmpty() ? hyprctl
              : directory.isEmpty() && !qEnvironmentVariableIsEmpty("HYPRLAND_INSTANCE_SIGNATURE")
                  ? QStandardPaths::findExecutable("hyprctl") : QString();
    m_roundingTimeout.setSingleShot(true);
    m_roundingTimeout.setInterval(500);
    connect(&m_roundingTimeout, &QTimer::timeout, &m_roundingProcess, &QProcess::kill);
    connect(&m_roundingProcess, &QProcess::errorOccurred, this, [this] { m_roundingTimeout.stop(); });
    connect(&m_roundingProcess, &QProcess::finished, this, [this](int code, QProcess::ExitStatus status) {
        m_roundingTimeout.stop();
        const auto value = QJsonDocument::fromJson(m_roundingProcess.readAllStandardOutput()).object().value("int");
        if (code != 0 || status != QProcess::NormalExit || !value.isDouble() || value.toInt(-1) < 0)
            return;
        if (value.toInt() != m_radius) {
            m_radius = value.toInt();
            emit changed();
        }
    });
    m_roundingPoll.setInterval(1000);
    connect(&m_roundingPoll, &QTimer::timeout, this, &Theme::refreshRounding);
    if (!m_hyprctl.isEmpty())
        m_roundingPoll.start();
    reload();
}

Theme::~Theme() {
    if (m_roundingProcess.state() != QProcess::NotRunning) {
        m_roundingProcess.kill();
        m_roundingProcess.waitForFinished(1000);
    }
}

void Theme::refreshRounding() {
    if (m_hyprctl.isEmpty() || m_roundingProcess.state() != QProcess::NotRunning)
        return;
    m_roundingProcess.start(m_hyprctl, {"-j", "getoption", "decoration:rounding"});
    m_roundingTimeout.start();
}

QString Theme::readValue(const QString &path, const QString &key, const QString &fallback) {
    QFile file(path);
    if (file.open(QIODevice::ReadOnly)) {
        const QRegularExpression expression("^\\s*" + QRegularExpression::escape(key) + R"re(\s*=\s*["']([^"']*)["'])re",
                                            QRegularExpression::MultilineOption);
        const auto match = expression.match(QString::fromUtf8(file.readAll()));
        if (match.hasMatch())
            return match.captured(1);
    }
    return fallback;
}

QString Theme::readAccent(const QString &path) {
    const QString value = readValue(path, "accent");
    return QRegularExpression("^#[0-9a-fA-F]{6}$").match(value).hasMatch() ? value : "#FFD60A";
}

// Black or white, whichever reads on the accent (WCAG relative luminance).
QString Theme::accentForeground() const {
    const QColor c(m_accent);
    auto linear = [](double v) { return v <= .04045 ? v / 12.92 : std::pow((v + .055) / 1.055, 2.4); };
    const double luminance = .2126 * linear(c.redF()) + .7152 * linear(c.greenF()) + .0722 * linear(c.blueF());
    return luminance > .179 ? "black" : "white";
}

static QString mix(const QString &from, const QString &to, double amount) {
    const QColor a(from), b(to);
    return QColor::fromRgbF(a.redF() + (b.redF() - a.redF()) * amount, a.greenF() + (b.greenF() - a.greenF()) * amount,
                            a.blueF() + (b.blueF() - a.blueF()) * amount).name();
}

QString Theme::secondary() const { return mix(m_foreground, m_background, 0.38); }
QString Theme::border() const { return mix(m_background, m_foreground, 0.14); }

void Theme::reload() {
    refreshRounding();
    const auto paths = m_watcher.files() + m_watcher.directories();
    if (!paths.isEmpty())
        m_watcher.removePaths(paths);

    // A theme switch replaces symlinks and files, so watch the parents too and
    // re-arm after every change. Watching the nearest existing ancestor means
    // installing Omarchy later works without restarting the app.
    QString ancestor = m_directory;
    while (!QFileInfo::exists(ancestor) && ancestor != "/")
        ancestor = QFileInfo(ancestor).absolutePath();
    QStringList candidates{ancestor, QFileInfo(ancestor).absolutePath(), m_directory,
                           m_directory + "/theme", m_directory + "/theme/colors.toml"};
    candidates.removeDuplicates();
    for (const auto &path : candidates)
        if (QFileInfo::exists(path))
            m_watcher.addPath(path);

    const QString colors = m_directory + "/theme/colors.toml";
    const QRegularExpression hex("^#[0-9a-fA-F]{6}$");
    auto color = [&](const QString &key, const QString &fallback) {
        const QString value = readValue(colors, key);
        return hex.match(value).hasMatch() ? value : fallback;
    };

    const bool light = readValue(colors, "mode") == "light";
    // Without a colors file the app keeps its dark look, so a missing Omarchy
    // theme never leaves light text on a light window.
    const bool found = QFileInfo::exists(colors);
    const QString accent = readAccent(colors);
    const QString background = found ? color("background", m_background) : "#0e0e10";
    const QString surface = found ? color("lighter_background", mix(background, light ? "#000000" : "#ffffff", .06)) : "#1a1a1e";
    const QString foreground = found ? color("foreground", light ? "#202020" : "#e8e8ec") : "#e8e8ec";
    const QString selection = found ? color("selection", surface) : "#2b2b31";

    if (accent != m_accent || light != m_light || background != m_background || surface != m_surface
        || foreground != m_foreground || selection != m_selection) {
        m_accent = accent;
        m_light = light;
        m_background = background;
        m_surface = surface;
        m_foreground = foreground;
        m_selection = selection;
        emit changed();
    }
}
