// scenes — save and restore named setups of the Omarchy desktop.

#include <QGuiApplication>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QUrl>

#include "appicons.h"
#include "backend.h"
#include "catalog.h"
#include "cli.h"
#include "hypr.h"
#include "hyprevents.h"
#include "scenestore.h"
#include "theme.h"
#include "translator.h"

int main(int argc, char *argv[]) {
    QStringList args;
    for (int i = 1; i < argc; ++i)
        args << QString::fromLocal8Bit(argv[i]);
    const bool window = Cli::wantsWindow(args);

    // Commands run without a display. This has to be set before the application exists.
    if (!window && qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM"))
        qputenv("QT_QPA_PLATFORM", "offscreen");

    QGuiApplication app(argc, argv);
    app.setApplicationName("scenes");
    app.setApplicationVersion("0.1.0");

    // Matches the window to scenes.desktop, so the compositor (Wayland app_id)
    // and the launcher pick up the installed icon.
    app.setDesktopFileName("scenes");
    app.setWindowIcon(QIcon::fromTheme("scenes"));

    const Catalog catalog;
    const SceneStore store(qEnvironmentVariable("SCENES_DIR"));

    if (!window) {
        QTextStream out(stdout), err(stderr);
        CliContext context{store, Hypr(), &catalog, "hyprctl", {}};
        const int code = Cli::run(args, context, out, err);
        out.flush();
        err.flush();
        return code;
    }

    QQuickStyle::setStyle("Material");

    // Installed before anything is translated, and every qsTr() binding is
    // re-evaluated when the Omarchy language changes.
    Translator translator;
    app.installTranslator(&translator);

    Theme theme;
    HyprEvents events;
    Backend backend(store, Hypr(), catalog, &events);

    QQmlApplicationEngine engine;
    QObject::connect(&translator, &Translator::changed, &engine, &QQmlEngine::retranslate);
    engine.addImageProvider("appicon", new AppIcons(catalog));
    engine.rootContext()->setContextProperty("translator", &translator);
    engine.rootContext()->setContextProperty("theme", &theme);
    engine.rootContext()->setContextProperty("backend", &backend);
    engine.load(QUrl("qrc:/Main.qml"));
    if (engine.rootObjects().isEmpty())
        return 1;

    return app.exec();
}
