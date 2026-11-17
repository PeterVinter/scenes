// Renders the real window offscreen, against made-up content, and saves a PNG.
// It is how the README screenshot is made, and how each mode of the window is
// checked in a dark and a light theme and in other languages:
//
//   scenes_screenshot --state list --theme dark --lang sv --out /path/shot.png
//
// States: empty, list, result, missing, dialog, delete, overlay, help, about.
// Languages: en, sv, or "long" (pseudo-Danish, 1.5x longer) and "rtl" (Arabic script).
#include <QDir>
#include <QFile>
#include <QGuiApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QRegularExpression>
#include <QTemporaryDir>
#include <QTimer>
#include <QEventLoop>

#include "appicons.h"
#include "backend.h"
#include "theme.h"
#include "translator.h"

static void write(const QString &path, const QByteArray &data, bool exec = false)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly))
        return;
    file.write(data);
    file.close();
    if (exec)
        QFile::setPermissions(path, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner);
}

static void wait(int ms)
{
    QEventLoop loop;
    QTimer::singleShot(ms, &loop, &QEventLoop::quit);
    loop.exec();
}

int main(int argc, char *argv[])
{
    qputenv("QT_QPA_PLATFORM", "offscreen");
    // Material draws button backgrounds with effects the software renderer
    // can't, so the window is rendered with OpenGL, which offscreen supports.
    if (qEnvironmentVariableIsEmpty("QSG_RHI_BACKEND"))
        qputenv("QSG_RHI_BACKEND", "opengl");
    if (qEnvironmentVariableIsEmpty("QT_QUICK_BACKEND"))
        qputenv("QT_QUICK_BACKEND", "rhi");
    QGuiApplication app(argc, argv);
    app.setApplicationVersion("0.1.0");
    QString thenTheme, state = "list", themeName = "dark", lang = "en", out = "shot.png";
    int width = 1040, height = 800;
    const QStringList args = app.arguments();
    for (int i = 1; i + 1 < args.size(); i += 2) {
        if (args[i] == "--state") state = args[i + 1];
        else if (args[i] == "--theme") themeName = args[i + 1];
        else if (args[i] == "--lang") lang = args[i + 1];
        else if (args[i] == "--out") out = args[i + 1];
        else if (args[i] == "--then") thenTheme = args[i + 1];
        else if (args[i] == "--width") width = args[i + 1].toInt();
        else if (args[i] == "--height") height = args[i + 1].toInt();
    }

    QTemporaryDir dir;
    const QString tools = dir.filePath("tools"), apps = dir.filePath("apps");

    // Made-up hardware and apps: nothing here is the person's own.
    write(tools + "/hyprctl", "#!/bin/sh\nD=\"$(dirname \"$0\")\"\ncase \"$1\" in\n  -j) case \"$2\" in\n"
          "    monitors) cat \"$D/monitors.json\" ;; workspaces) cat \"$D/workspaces.json\" ;;\n"
          "    clients) cat \"$D/clients.json\" ;; getoption) echo '{\"int\":0}' ;; esac ;;\n  eval) echo ok ;;\nesac\n", true);
    write(tools + "/layout", "#!/bin/sh\nD=\"$(dirname \"$0\")\"\nawk -v w=\"$1\" '$1==w {print $2; f=1} END {if (!f) print \"dwindle\"}' \"$D/layouts.txt\"\n", true);
    write(tools + "/layouts.txt", "11 scrolling\n12 floating\n");
    write(tools + "/monitors.json", R"([
      {"name":"DP-1","make":"Acme","model":"Vision X27","serial":"A1","x":0,"y":0,"width":2560,"height":1440,"scale":1,"activeWorkspace":{"id":1}},
      {"name":"DP-2","make":"Acme","model":"Slate 24","serial":"B2","x":2560,"y":0,"width":1920,"height":1080,"scale":1,"activeWorkspace":{"id":11}}])");
    write(tools + "/workspaces.json", R"([
      {"id":1,"name":"1","monitor":"DP-1","windows":2,"tiledLayout":"dwindle"},
      {"id":2,"name":"2","monitor":"DP-1","windows":1,"tiledLayout":"dwindle"},
      {"id":11,"name":"11","monitor":"DP-2","windows":1,"tiledLayout":"dwindle"},
      {"id":-99,"name":"scratch","monitor":"DP-2","windows":1,"tiledLayout":"dwindle"}])");
    write(tools + "/clients.json", R"([
      {"class":"ghostty","workspace":{"id":1},"mapped":true,"at":[0,0]},
      {"class":"chromium","workspace":{"id":1},"mapped":true,"at":[1280,0]},
      {"class":"obsidian","workspace":{"id":2},"mapped":true,"at":[0,0]},
      {"class":"org.gnome.Nautilus","workspace":{"id":11},"mapped":true,"at":[0,0]}])");
    write(dir.filePath("blocks"), "0\tDP-1\tAcme Vision X27 A1\n1\tDP-2\tAcme Slate 24 B2\n");
    struct App { const char *id, *name, *icon, *cls; };
    for (const App &a : {App{"ghostty", "Terminal", "com.mitchellh.ghostty", "ghostty"}, App{"chromium", "Browser", "chromium", "chromium"},
                         App{"obsidian", "Notes", "obsidian", "obsidian"}, App{"org.gnome.Nautilus", "Files", "org.gnome.Nautilus", "org.gnome.Nautilus"},
                         App{"mpv", "Video", "mpv", "mpv"}, App{"evince", "Documents", "evince", "evince"},
                         App{"libreoffice-writer", "Writer", "libreoffice-writer", "libreoffice-writer"}})
        write(apps + "/" + a.id + ".desktop", QByteArray("[Desktop Entry]\nName=") + a.name + "\nIcon=" + a.icon + "\nExec=" + a.id + "\nStartupWMClass=" + a.cls + "\n");

    const Catalog catalog({apps});
    Hypr::Tools tools_;
    tools_.hyprctl = tools + "/hyprctl";
    tools_.layoutCommand = tools + "/layout";
    tools_.blocksFile = dir.filePath("blocks");
    const SceneStore store(dir.filePath("scenes"));

    auto monitor = [](const QString &make, const QString &model, const QString &serial, int x, int w, int h, const QList<SceneWorkspace> &ws) {
        SceneMonitor m;
        m.identity = make + " " + model + " " + serial; m.make = make; m.model = model; m.serial = serial;
        m.x = x; m.width = w; m.height = h; m.workspaces = ws;
        return m;
    };
    if (state != "empty") {
        Scene coding; coding.name = "Coding";
        coding.monitors = {monitor("Acme", "Vision X27", "A1", 0, 2560, 1440,
                                   {{1, "dwindle", {"ghostty", "chromium"}}, {2, "scrolling", {"ghostty", "ghostty", "obsidian"}}, {3, "dwindle", {}}}),
                           monitor("Acme", "Slate 24", "B2", 2560, 1920, 1080, {{1, "floating", {"org.gnome.Nautilus", "mpv"}}})};
        Scene meeting; meeting.name = "Meeting";
        meeting.monitors = {monitor("Acme", "Vision X27", "A1", 0, 2560, 1440, {{1, "dwindle", {"chromium", "obsidian"}}}),
                            monitor("Acme", "Slate 24", "B2", 2560, 1920, 1080, {{1, "scrolling", {"evince"}}})};
        Scene writing; writing.name = "Writing";
        writing.monitors = {monitor("Acme", "Vision X27", "A1", 0, 2560, 1440, {{1, "dwindle", {"libreoffice-writer", "evince"}}})};
        // A monitor that is not plugged in, for the "missing" state.
        Scene travel; travel.name = "Travel";
        travel.monitors = {monitor("Acme", "Vision X27", "A1", 0, 2560, 1440, {{1, "dwindle", {"chromium", "obsidian", "mpv"}}}),
                           monitor("Acme", "Dock 34", "C3", 2560, 3440, 1440, {{1, "dwindle", {"ghostty"}}, {2, "dwindle", {"mpv"}}})};
        QString error;
        for (const Scene &s : {coding, meeting, writing, travel})
            store.save(s, &error);
    }

    // Language: the shipped catalogs, or a pseudo-translation for long text and right-to-left.
    QString catalogDir = ":/i18n", language = lang;
    if (lang == "long" || lang == "rtl") {
        QJsonObject catalogJson;
        QStringList files = QDir(SRC_DIR).entryList({"*.qml", "*.cpp"}, QDir::Files);
        const QRegularExpression call(R"re((?:qsTr|\btr)\("((?:[^"\\]|\\.)*)")re");
        for (const auto &file : files) {
            QFile f(QString(SRC_DIR) + "/" + file);
            if (!f.open(QIODevice::ReadOnly))
                continue;
            auto it = call.globalMatch(QString::fromUtf8(f.readAll()));
            while (it.hasNext()) {
                const QString key = it.next().captured(1).replace("\\\"", "\"");
                QString text = key;
                if (lang == "long") {
                    // Roughly Danish's habit of longer compounds: half again as long.
                    const QStringList words = key.split(' ');
                    text = key + " " + words.mid(0, qMax(1, words.size() / 2)).join(' ').toLower() + "sammensat";
                } else {
                    text = QString::fromUtf8("مثال ") + key;
                }
                text.replace("%Ln", "%Ln");
                catalogJson[key] = key.contains("%Ln") ? QJsonValue(QJsonObject{{"one", text}, {"other", text}}) : QJsonValue(text);
            }
        }
        catalogDir = dir.filePath("i18n");
        language = lang == "long" ? "da" : "ar";
        write(catalogDir + "/" + language + ".json", QJsonDocument(catalogJson).toJson());
    }
    write(dir.filePath("language"), (language + "\n").toUtf8());

    // The default accent, so the screenshot doesn't depend on the current theme.
    const bool light = themeName == "light";
    write(dir.filePath("theme/theme/colors.toml"),
          light ? "mode = \"light\"\naccent = \"#FFD60A\"\nbackground = \"#eff1f5\"\nlighter_background = \"#dce0e8\"\nforeground = \"#4c4f69\"\nselection = \"#ccd0da\"\n"
                : "mode = \"dark\"\naccent = \"#FFD60A\"\nbackground = \"#1a1b26\"\nlighter_background = \"#24283b\"\nforeground = \"#c0caf5\"\nselection = \"#33467c\"\n");

    QQuickStyle::setStyle("Material");
    Translator translator(dir.filePath("language"), catalogDir);
    app.installTranslator(&translator);
    Theme theme(dir.filePath("theme"));
    Backend backend(store, Hypr(tools_), catalog, nullptr, tools + "/hyprctl");
    backend.setEventSocket(dir.filePath("none.sock")); // never the real session's

    QQmlApplicationEngine engine;
    QObject::connect(&translator, &Translator::changed, &engine, &QQmlEngine::retranslate);
    engine.addImageProvider("appicon", new AppIcons(catalog));
    engine.rootContext()->setContextProperty("translator", &translator);
    engine.rootContext()->setContextProperty("theme", &theme);
    engine.rootContext()->setContextProperty("backend", &backend);
    engine.load(QUrl("qrc:/Main.qml"));
    if (engine.rootObjects().isEmpty())
        return 1;
    auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().first());
    window->resize(width, height);
    window->show();
    wait(700);

    if (state == "list" || state == "dialog" || state == "delete" || state == "overlay" || state == "help" || state == "about")
        backend.select("coding");
    if (state == "missing")
        backend.select("travel");
    if (state == "result") {
        backend.select("meeting");
        backend.restore();
        for (int i = 0; i < 50 && (backend.restoring() || backend.result().isEmpty()); ++i)
            wait(100);
    }
    wait(300);
    if (state == "dialog")
        QMetaObject::invokeMethod(window->findChild<QObject *>("nameDialog"), "openFor", Q_ARG(QVariant, "save"), Q_ARG(QVariant, ""));
    if (state == "delete")
        QMetaObject::invokeMethod(window->findChild<QObject *>("deleteDialog"), "openFor", Q_ARG(QVariant, "Coding"));
    if (state == "overlay")
        QMetaObject::invokeMethod(window->findChild<QObject *>("helpOverlay"), "open");
    if (state == "help")
        QMetaObject::invokeMethod(window->findChild<QObject *>("helpDialog"), "openTab", Q_ARG(QVariant, 0));
    if (state == "about")
        QMetaObject::invokeMethod(window->findChild<QObject *>("helpDialog"), "openTab", Q_ARG(QVariant, 1));
    wait(500);

    // --then <dark|light>: switch the theme while the window is open, the way
    // omarchy-theme-set replaces colors.toml, and save the result as <out>-after.png.
    if (!thenTheme.isEmpty()) {
        const QImage before = window->grabWindow();
        before.save(out);
        const bool toLight = thenTheme == "light";
        QFile::remove(dir.filePath("theme/theme/colors.toml"));
        write(dir.filePath("theme/theme/colors.toml"),
              toLight ? "mode = \"light\"\naccent = \"#1e66f5\"\nbackground = \"#eff1f5\"\nlighter_background = \"#dce0e8\"\nforeground = \"#4c4f69\"\nselection = \"#ccd0da\"\n"
                      : "mode = \"dark\"\naccent = \"#f38d70\"\nbackground = \"#2c2525\"\nlighter_background = \"#3d2f2a\"\nforeground = \"#e6d9db\"\nselection = \"#403e41\"\n");
        wait(900);
        QString after = out;
        after.replace(".png", "-after.png");
        return window->grabWindow().save(after) ? 0 : 1;
    }

    const QImage image = window->grabWindow();
    if (image.isNull() || !image.save(out)) {
        qWarning("could not save %s", qPrintable(out));
        return 1;
    }
    return 0;
}
