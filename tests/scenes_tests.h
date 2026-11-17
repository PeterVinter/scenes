#pragma once

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLocalServer>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QRegularExpression>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QtTest>

#include "appicons.h"
#include "backend.h"
#include "catalog.h"
#include "cli.h"
#include "hypr.h"
#include "hyprevents.h"
#include "plan.h"
#include "restorer.h"
#include "scene.h"
#include "scenestore.h"
#include "theme.h"
#include "translator.h"

namespace {

void write(const QString &path, const QByteArray &contents, bool executable = false)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile file(path);
    QVERIFY2(file.open(QIODevice::WriteOnly), qPrintable(path));
    file.write(contents);
    file.close();
    if (executable)
        QFile::setPermissions(path, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner);
}

QByteArray read(const QString &path)
{
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}

void typeText(QWindow *window, const char *text)
{
    for (const char *c = text; *c; ++c)
        QTest::keyClick(window, *c);
}

const char *IdentityDP = "Acme Displays Slate 24 SN000001";
const char *IdentityHDMI = "Orbit Electronics Vision 32 SN000002";

// A desktop in a temp directory: a fake hyprctl that serves JSON fixtures and
// logs what it is asked to eval, a fake layout command, desktop entries, and
// Omarchy's block file. Nothing touches the real session.
struct Fixture {
    QTemporaryDir dir;
    QString tools, apps, scenes, hyprctl, layouts, blocks, evalLog;

    Fixture()
    {
        tools = dir.filePath("tools");
        apps = dir.filePath("apps");
        scenes = dir.filePath("scenes");
        hyprctl = tools + "/hyprctl";
        layouts = tools + "/layout";
        blocks = dir.filePath("monitor-workspaces");
        evalLog = tools + "/eval.log";

        write(hyprctl,
              "#!/bin/sh\n"
              "D=\"$(dirname \"$0\")\"\n"
              "case \"$1\" in\n"
              "  -j) case \"$2\" in\n"
              "        monitors) cat \"$D/monitors.json\" ;;\n"
              "        workspaces) cat \"$D/workspaces.json\" ;;\n"
              "        clients) cat \"$D/clients.json\" ;;\n"
              "        getoption) echo '{\"int\":0}' ;;\n"
              "      esac ;;\n"
              "  eval) echo \"$2\" >> \"$D/eval.log\"\n"
              "        case \"$2\" in *failme*) echo \"error: refused\" ;; *) echo ok ;; esac ;;\n"
              "esac\n",
              true);
        write(layouts,
              "#!/bin/sh\n"
              "D=\"$(dirname \"$0\")\"\n"
              "awk -v w=\"$1\" '$1==w {print $2; f=1} END {if (!f) print \"dwindle\"}' \"$D/layouts.txt\"\n",
              true);
        write(tools + "/layouts.txt", "12 floating\n1 scrolling\n");

        setMonitors(true, true);
        write(tools + "/workspaces.json", "[\n"
        "          {\"id\":1,\"name\":\"1\",\"monitor\":\"HDMI-A-1\",\"windows\":2,\"tiledLayout\":\"dwindle\"},\n"
        "          {\"id\":2,\"name\":\"2\",\"monitor\":\"HDMI-A-1\",\"windows\":0,\"tiledLayout\":\"dwindle\"},\n"
        "          {\"id\":11,\"name\":\"11\",\"monitor\":\"DP-1\",\"windows\":2,\"tiledLayout\":\"dwindle\"},\n"
        "          {\"id\":12,\"name\":\"12\",\"monitor\":\"DP-1\",\"windows\":1,\"tiledLayout\":\"dwindle\"},\n"
        "          {\"id\":-99,\"name\":\"scratch\",\"monitor\":\"DP-1\",\"windows\":1,\"tiledLayout\":\"dwindle\"},\n"
        "          {\"id\":-98,\"name\":\"special:minimized\",\"monitor\":\"DP-1\",\"windows\":0,\"tiledLayout\":\"dwindle\"}\n"
        "        ]");
        write(tools + "/clients.json", "[\n"
        "          {\"class\":\"firefox\",\"workspace\":{\"id\":1},\"mapped\":true,\"at\":[1000,50]},\n"
        "          {\"class\":\"org.gnome.Nautilus\",\"workspace\":{\"id\":1},\"mapped\":true,\"at\":[100,50]},\n"
        "          {\"class\":\"firefox\",\"workspace\":{\"id\":11},\"mapped\":true,\"at\":[0,0]},\n"
        "          {\"class\":\"mystery\",\"workspace\":{\"id\":11},\"mapped\":true,\"at\":[10,0]},\n"
        "          {\"class\":\"scenes\",\"workspace\":{\"id\":11},\"mapped\":true,\"at\":[20,0]},\n"
        "          {\"class\":\"org.gnome.Nautilus\",\"workspace\":{\"id\":12},\"mapped\":true,\"at\":[0,0]},\n"
        "          {\"class\":\"firefox\",\"workspace\":{\"id\":-98},\"mapped\":true,\"at\":[0,0]},\n"
        "          {\"class\":\"firefox\",\"workspace\":{\"id\":-99},\"mapped\":true,\"at\":[0,0]}\n"
        "        ]");
        write(blocks, QByteArray("0\tHDMI-A-1\t") + IdentityHDMI + "\n1\tDP-1\t" + IdentityDP + "\n");

        write(apps + "/firefox.desktop", "[Desktop Entry]\nName=Firefox\nIcon=firefox\nExec=firefox %u\nStartupWMClass=firefox\n");
        write(apps + "/org.gnome.Nautilus.desktop", "[Desktop Entry]\nName=Files\nIcon=nautilus\nExec=nautilus --new-window %U\n");
        write(apps + "/btop.desktop", "[Desktop Entry]\nName=btop\nExec=btop --utf-force %f\nTerminal=true\n");
        write(apps + "/hidden.desktop", "[Desktop Entry]\nName=Hidden\nExec=hidden\nHidden=true\n");
    }

    void setMonitors(bool dp, bool hdmi)
    {
        QString json = "[";
        if (hdmi)
            json += "{\"name\":\"HDMI-A-1\",\"make\":\"Orbit Electronics\",\"model\":\"Vision 32\",\"serial\":\"SN000002\",\"x\":1920,\"y\":0,\"width\":2560,\"height\":1440,\"scale\":1,\"activeWorkspace\":{\"id\":1}}";
        if (hdmi && dp)
            json += ",";
        if (dp)
            json += "{\"name\":\"DP-1\",\"make\":\"Acme Displays\",\"model\":\"Slate 24\",\"serial\":\"SN000001\",\"x\":0,\"y\":0,\"width\":1920,\"height\":1080,\"scale\":1,\"activeWorkspace\":{\"id\":11}}";
        write(tools + "/monitors.json", (json + "]").toUtf8());
    }

    Hypr::Tools hyprTools() const
    {
        Hypr::Tools t;
        t.hyprctl = hyprctl;
        t.layoutCommand = layouts;
        t.blocksFile = blocks;
        return t;
    }
    Hypr hypr() const { return Hypr(hyprTools()); }
    Catalog catalog() const { return Catalog({apps}); }
    SceneStore store() const { return SceneStore(scenes); }
    QStringList evals() const { return QString::fromUtf8(read(evalLog)).split('\n', Qt::SkipEmptyParts); }
};

Scene sampleScene(const QString &name = "Coding")
{
    Scene scene;
    scene.name = name;
    SceneMonitor dp;
    dp.identity = IdentityDP; dp.make = "Acme Displays"; dp.model = "Slate 24"; dp.serial = "SN000001";
    dp.x = 0; dp.y = 0; dp.width = 1920; dp.height = 1080;
    dp.workspaces = {{1, "dwindle", {"firefox"}}, {2, "floating", {"org.gnome.Nautilus"}}};
    SceneMonitor hdmi;
    hdmi.identity = IdentityHDMI; hdmi.model = "Vision 32";
    hdmi.x = 1920; hdmi.width = 2560; hdmi.height = 1440;
    hdmi.workspaces = {{1, "scrolling", {"org.gnome.Nautilus", "firefox", "firefox"}}};
    scene.monitors = {dp, hdmi};
    return scene;
}

// QML warnings are test failures: the real Main.qml is loaded below.
QStringList g_warnings;
QtMessageHandler g_previous = nullptr;
void collectWarnings(QtMsgType type, const QMessageLogContext &context, const QString &message)
{
    if (type == QtWarningMsg || type == QtCriticalMsg)
        g_warnings << message;
    if (g_previous)
        g_previous(type, context, message);
}

} // namespace

class ScenesTests : public QObject {
    Q_OBJECT

private slots:
    void initTestCase()
    {
        // Once, before any QML that imports Qt Quick Controls loads.
        QQuickStyle::setStyle("Material");
        g_previous = qInstallMessageHandler(collectWarnings);
    }
    void cleanupTestCase() { qInstallMessageHandler(g_previous); }

    // --- Scene format ------------------------------------------------------

    void slugifyMakesSafeNames()
    {
        QCOMPARE(Scene::slugify("Coding"), QString("coding"));
        QCOMPARE(Scene::slugify("  My  Meeting / Setup! "), QString("my-meeting-setup"));
        QCOMPARE(Scene::slugify("Skrivbord åäö"), QString("skrivbord-åäö"));
        QCOMPARE(Scene::slugify("../../etc"), QString("etc"));
        QCOMPARE(Scene::slugify("***"), QString());
    }

    void sceneRoundTripsThroughJson()
    {
        const Scene scene = sampleScene();
        Scene loaded;
        QString error;
        QVERIFY2(Scene::fromJson(scene.toJson(), &loaded, &error), qPrintable(error));
        QCOMPARE(loaded.toJson(), scene.toJson());
        QCOMPARE(scene.toJson().value("version").toInt(), 1);
        QCOMPARE(loaded.appCount(), 5);
        QCOMPARE(loaded.workspaceCount(), 3);
    }

    void sceneRejectsBadData()
    {
        auto reject = [](const std::function<void(QJsonObject &)> &damage, const QString &expected) {
            QJsonObject object = sampleScene().toJson();
            damage(object);
            Scene scene;
            QString error;
            QVERIFY(!Scene::fromJson(object, &scene, &error));
            QVERIFY2(error.contains(expected), qPrintable(error));
        };
        reject([](QJsonObject &o) { o["version"] = 2; }, "version");
        reject([](QJsonObject &o) { o["name"] = "  "; }, "name");
        reject([](QJsonObject &o) { o["monitors"] = 3; }, "monitors");
        reject([](QJsonObject &o) {
            QJsonArray monitors = o["monitors"].toArray();
            QJsonObject m = monitors[0].toObject();
            QJsonArray w = m["workspaces"].toArray();
            QJsonObject first = w[0].toObject();
            first["slot"] = 11;
            w[0] = first; m["workspaces"] = w; monitors[0] = m; o["monitors"] = monitors;
        }, "slot");
    }

    // --- Store -------------------------------------------------------------

    void storeSavesLoadsAndLists()
    {
        QTemporaryDir dir;
        SceneStore store(dir.filePath("scenes"));
        QString error;
        QVERIFY2(store.save(sampleScene("Writing"), &error), qPrintable(error));
        QVERIFY2(store.save(sampleScene("coding"), &error), qPrintable(error));
        QVERIFY(store.exists("writing"));

        Scene loaded;
        QVERIFY2(store.load("Writing", &loaded, &error), qPrintable(error));
        QCOMPARE(loaded.name, QString("Writing"));

        QStringList problems;
        const auto scenes = store.list(&problems);
        QVERIFY(problems.isEmpty());
        QCOMPARE(scenes.size(), 2);
        QCOMPARE(scenes[0].name, QString("coding")); // sorted by name, ignoring case

        // Atomic: no temporary files are left beside the scenes.
        QCOMPARE(QDir(store.directory()).entryList(QDir::Files).size(), 2);
        QVERIFY(store.remove("writing", &error));
        QVERIFY(!store.exists("writing"));
    }

    void storeReportsUnreadableFiles()
    {
        QTemporaryDir dir;
        SceneStore store(dir.path());
        write(dir.filePath("broken.json"), "{not json");
        write(dir.filePath("future.json"), "{\"version\": 9, \"name\": \"Future\", \"monitors\": []}");
        QStringList problems;
        QVERIFY(store.list(&problems).isEmpty());
        QCOMPARE(problems.size(), 2);
        QString error;
        Scene scene;
        QVERIFY(!store.load("nothing", &scene, &error));
        QVERIFY(error.contains("no scene named"));
    }

    void failedSaveKeepsTheOldFile()
    {
        QTemporaryDir dir;
        SceneStore store(dir.path());
        QString error;
        QVERIFY(store.save(sampleScene("Keep"), &error));
        const QByteArray before = read(store.pathFor("keep"));
        // A directory where the temp file would go makes the write fail.
        QVERIFY(QFile::setPermissions(dir.path(), QFile::ReadOwner | QFile::ExeOwner));
        Scene changed = sampleScene("Keep");
        changed.monitors.clear();
        const bool saved = store.save(changed, &error);
        QFile::setPermissions(dir.path(), QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner);
        if (saved)
            QSKIP("running as a user that ignores directory permissions");
        QVERIFY(!error.isEmpty());
        QCOMPARE(read(store.pathFor("keep")), before);
    }

    // --- Catalog -----------------------------------------------------------

    void catalogMapsWindowClassesToDesktopIds()
    {
        Fixture f;
        const Catalog catalog = f.catalog();
        QCOMPARE(catalog.idForClass("firefox"), QString("firefox"));
        QCOMPARE(catalog.idForClass("FireFox"), QString("firefox"));
        QCOMPARE(catalog.idForClass("org.gnome.Nautilus"), QString("org.gnome.Nautilus"));
        QCOMPARE(catalog.idForClass("nautilus"), QString("org.gnome.Nautilus")); // last component of the id
        QCOMPARE(catalog.idForClass("mystery"), QString());
        QVERIFY(!catalog.contains("hidden"));
        QCOMPARE(catalog.entry("firefox").name, QString("Firefox"));
    }

    void catalogPrefersTheUsersOwnEntry()
    {
        Fixture f;
        write(f.dir.filePath("mine/firefox.desktop"), "[Desktop Entry]\nName=My Firefox\nExec=ff\n");
        const Catalog catalog({f.dir.filePath("mine"), f.apps});
        QCOMPARE(catalog.entry("firefox").name, QString("My Firefox"));
    }

    void catalogSplitsTerminalCommands()
    {
        Fixture f;
        const Catalog catalog = f.catalog();
        QCOMPARE(catalog.terminalCommand("btop"), (QStringList{"btop", "--utf-force"}));
        QVERIFY(catalog.terminalCommand("firefox").isEmpty());
        QCOMPARE(Catalog::splitExec("sh -c \"echo \\\"hi there\\\"\" %U"), (QStringList{"sh", "-c", "echo \"hi there\"", "%U"}));
    }

    // --- Reading the desktop ----------------------------------------------

    void captureReadsMonitorsWorkspacesAndApps()
    {
        Fixture f;
        CaptureReport report;
        QString error;
        const Scene scene = f.hypr().capture("Now", f.catalog(), &report, &error);
        QVERIFY2(error.isEmpty(), qPrintable(error));

        // Monitors are bound by identity, left to right.
        QCOMPARE(scene.monitors.size(), 2);
        QCOMPARE(scene.monitors[0].identity, QString(IdentityDP));
        QCOMPARE(scene.monitors[1].identity, QString(IdentityHDMI));
        QCOMPARE(scene.monitors[1].x, 1920);
        QCOMPARE(scene.monitors[1].width, 2560);

        // The DP-1 block is 11-20: workspace 11 is slot 1, 12 is slot 2. 13 is empty and left out.
        const auto dp = scene.monitors[0].workspaces;
        QCOMPARE(dp.size(), 2);
        QCOMPARE(dp[0].slot, 1);
        QCOMPARE(dp[0].apps, (QStringList{"firefox"}));
        QCOMPARE(dp[0].layout, QString("dwindle"));
        QCOMPARE(dp[1].slot, 2);
        QCOMPARE(dp[1].layout, QString("floating")); // from Omarchy's layout command
        QCOMPARE(dp[1].apps, (QStringList{"org.gnome.Nautilus"}));

        // Apps in reading order across the screen: Files (x=100) before Firefox (x=1000).
        const auto hdmi = scene.monitors[1].workspaces;
        QCOMPARE(hdmi.size(), 1);
        QCOMPARE(hdmi[0].apps, (QStringList{"org.gnome.Nautilus", "firefox"}));
        QCOMPARE(hdmi[0].layout, QString("scrolling"));
    }

    void captureSkipsNamedWorkspacesAndSaysSo()
    {
        Fixture f;
        CaptureReport report;
        QString error;
        const Scene scene = f.hypr().capture("Now", f.catalog(), &report, &error);
        // "scratch" reports a negative id; "special:minimized" belongs to Omarchy.
        QCOMPARE(report.skippedNamed, (QStringList{"scratch"}));
        QCOMPARE(report.unmappedWindows, 1);
        QCOMPARE(report.unmappedClasses, (QStringList{"mystery"}));
        QCOMPARE(scene.appCount(), 4); // the scenes app's own window is never captured
    }

    void captureFailsLoudlyWithoutHyprland()
    {
        Fixture f;
        Hypr::Tools tools = f.hyprTools();
        tools.hyprctl = f.dir.filePath("missing");
        CaptureReport report;
        QString error;
        f.hypr(); // keep the fixture alive
        Hypr(tools).capture("x", f.catalog(), &report, &error);
        QVERIFY(error.contains("missing"));
    }

    void captureUsesTheActiveWorkspaceWhenThereIsNoBlockFile()
    {
        Fixture f;
        QFile::remove(f.blocks);
        CaptureReport report;
        QString error;
        const Scene scene = f.hypr().capture("Now", f.catalog(), &report, &error);
        QCOMPARE(scene.monitors[0].workspaces[0].slot, 1); // DP-1 shows workspace 11, so block 1
    }

    // --- Plans -------------------------------------------------------------

    void planOpensOnlyWhatIsMissing()
    {
        Fixture f;
        const Hypr hypr = f.hypr();
        QString error;
        const Scene scene = sampleScene();
        const Plan plan = buildPlan(scene, hypr.monitors(&error), hypr.clients(&error), hypr.blocks(), f.catalog(),
                                    [&](int id) { return hypr.layout(id); });
        // DP-1 slot 1 (ws 11) has Firefox already; slot 2 (ws 12) has Files already.
        // HDMI slot 1 (ws 1) wants Files, Firefox, Firefox: has Files and one Firefox, so one Firefox is missing.
        QCOMPARE(plan.launches.size(), 1);
        QCOMPARE(plan.launches[0].desktopId, QString("firefox"));
        QCOMPARE(plan.launches[0].workspace, 1);
        QCOMPARE(plan.alreadyRunning, 4);
        QVERIFY(plan.unplaced.isEmpty());
        QVERIFY(plan.missingApps.isEmpty());
        // Workspace 12 is floating (scene wants floating: fine). Workspace 1 is scrolling (fine). Workspace 11 wants dwindle.
        QVERIFY(plan.layouts.isEmpty());
    }

    void planReportsLayoutsItCannotSet()
    {
        Fixture f;
        write(f.tools + "/layouts.txt", "11 scrolling\n12 floating\n1 scrolling\n");
        const Hypr hypr = f.hypr();
        QString error;
        const Plan plan = buildPlan(sampleScene(), hypr.monitors(&error), hypr.clients(&error), hypr.blocks(), f.catalog(),
                                    [&](int id) { return hypr.layout(id); });
        QCOMPARE(plan.layouts.size(), 1);
        QCOMPARE(plan.layouts[0].workspace, 11);
        QCOMPARE(plan.layouts[0].wanted, QString("dwindle"));
        QCOMPARE(plan.layouts[0].current, QString("scrolling"));
        QVERIFY(plan.describe().join("\n").contains("no command to set the layout"));
        QVERIFY(f.evals().isEmpty()); // reading never writes
    }

    void planNeverMovesAWorkspaceToAnotherMonitor()
    {
        Fixture f;
        f.setMonitors(false, true); // the DP-1 monitor is unplugged
        const Hypr hypr = f.hypr();
        QString error;
        const Plan plan = buildPlan(sampleScene(), hypr.monitors(&error), hypr.clients(&error), hypr.blocks(), f.catalog(), nullptr);
        QCOMPARE(plan.unplaced.size(), 1);
        QCOMPARE(plan.unplaced[0].identity, QString(IdentityDP));
        QCOMPARE(plan.unplaced[0].workspaceSlots, (QList<int>{1, 2}));
        QCOMPARE(plan.unplaced[0].appCount, 2);
        for (const auto &launch : plan.launches)
            QCOMPARE(launch.monitor, QString("Vision 32")); // nothing was redirected
        QVERIFY(plan.hasProblems());
    }

    void planSkipsAppsThatAreNotInstalled()
    {
        Fixture f;
        Scene scene = sampleScene();
        scene.monitors[1].workspaces[0].apps = {"gone.app", "firefox"};
        const Hypr hypr = f.hypr();
        QString error;
        const Plan plan = buildPlan(scene, hypr.monitors(&error), {}, hypr.blocks(), f.catalog(), nullptr);
        QCOMPARE(plan.missingApps, (QStringList{"gone.app"}));
        QVERIFY(!plan.describe().filter("gone.app").isEmpty());
    }

    void dryRunNamesEveryLaunch()
    {
        Fixture f;
        Plan plan;
        plan.launches = {{"firefox", "Firefox", 12, "Slate 24"}};
        QCOMPARE(plan.describe(), (QStringList{"launch firefox on workspace 12 (Slate 24)"}));
        QCOMPARE(Plan().describe(), (QStringList{"nothing to do"}));
    }

    // --- Launching ---------------------------------------------------------

    void launchExpressionsQuoteForLuaAndShell()
    {
        Fixture f;
        const Catalog catalog = f.catalog();
        QCOMPARE(Restorer::luaQuote("a\"b\\c"), QString("\"a\\\"b\\\\c\""));
        QCOMPARE(Restorer::shellQuote("it's"), QString("'it'\\''s'"));
        QCOMPARE(Restorer::launchExpression({"firefox", "Firefox", 12, ""}, catalog),
                 QString("hl.exec_cmd(\"uwsm-app -- gtk-launch 'firefox.desktop'\", {workspace=\"12 silent\"})"));
        QCOMPARE(Restorer::launchExpression({"btop", "btop", 3, ""}, catalog),
                 QString("hl.exec_cmd(\"omarchy-launch-tui 'btop' '--utf-force'\", {workspace=\"3 silent\"})"));
    }

    void restorerLaunchesAsyncAndReportsFailures()
    {
        Fixture f;
        // The second app's id makes the fake hyprctl refuse it.
        write(f.apps + "/failme.desktop", "[Desktop Entry]\nName=Failme\nExec=failme\n");
        const Catalog catalog = f.catalog();
        Plan plan;
        plan.launches = {{"firefox", "Firefox", 12, "x"}, {"failme", "Failme", 13, "x"}, {"org.gnome.Nautilus", "Files", 1, "x"}};
        Restorer restorer(plan, catalog, f.hyprctl);
        QSignalSpy progress(&restorer, &Restorer::progress), finished(&restorer, &Restorer::finished);
        restorer.start();
        QVERIFY(finished.isEmpty()); // async: nothing happened yet
        QTRY_COMPARE(finished.count(), 1);
        QCOMPARE(progress.count(), 3);
        QCOMPARE(restorer.launched(), 2);
        QCOMPARE(restorer.failures().size(), 1);
        QVERIFY(restorer.failures()[0].startsWith("Failme: "));
        QCOMPARE(f.evals().size(), 3);
        QVERIFY(f.evals()[0].contains("gtk-launch 'firefox.desktop'"));
        QVERIFY(f.evals()[0].contains("workspace=\"12 silent\""));
    }

    void restorerReportsAMissingHyprctl()
    {
        Fixture f;
        const Catalog catalog = f.catalog();
        Plan plan;
        plan.launches = {{"firefox", "Firefox", 12, "x"}};
        Restorer restorer(plan, catalog, f.dir.filePath("no-such-hyprctl"));
        QSignalSpy finished(&restorer, &Restorer::finished);
        restorer.start();
        QTRY_COMPARE(finished.count(), 1);
        QCOMPARE(restorer.launched(), 0);
        QCOMPARE(restorer.failures().size(), 1);
    }

    // --- Events ------------------------------------------------------------

    void eventsAreDebouncedFilteredAndReconnect()
    {
        // A short path: unix sockets are limited to about 100 characters.
        QTemporaryDir dir(QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation) + "/scenes-test-XXXXXX");
        QVERIFY(dir.isValid());
        const QString path = dir.filePath("s.sock");
        QLocalServer server;
        QVERIFY(server.listen(path));

        HyprEvents events(path);
        QSignalSpy changed(&events, &HyprEvents::desktopChanged);
        QTRY_VERIFY(server.hasPendingConnections());
        QLocalSocket *client = server.nextPendingConnection();
        QTRY_COMPARE(changed.count(), 1); // looks again on connect

        client->write("openwindow>>80a,1,foot,foot\nworkspace>>2\nmonitoradded>>DP-2\n");
        client->flush();
        QTRY_COMPARE(changed.count(), 2); // a burst is one change
        QTest::qWait(400);
        QCOMPARE(changed.count(), 2);

        client->write("activewindow>>foot,foot\nsubmap>>x\n"); // not about the desktop's shape
        client->flush();
        QTest::qWait(400);
        QCOMPARE(changed.count(), 2);

        client->write("configreloaded>>\n");
        client->flush();
        QTRY_COMPARE(changed.count(), 3);

        client->disconnectFromServer();
        QTRY_VERIFY(!events.connected());
        QTRY_VERIFY2(server.hasPendingConnections(), "did not reconnect");
        QTRY_COMPARE(changed.count(), 4); // and looks again, as events may have been missed
        QTRY_VERIFY(events.connected());
    }

    void eventNamesThatMatter()
    {
        for (const char *name : {"openwindow", "closewindow", "workspace", "monitoradded", "monitorremoved", "configreloaded"})
            QVERIFY(HyprEvents::relevant(name));
        QVERIFY(!HyprEvents::relevant("activewindow"));
    }

    // --- CLI ---------------------------------------------------------------

private:
    struct CliRun { int code; QString out, err; };
    CliRun runCli(Fixture &f, const QStringList &args, const Catalog &catalog)
    {
        CliRun run;
        QTextStream out(&run.out), err(&run.err);
        CliContext context{f.store(), f.hypr(), &catalog, f.hyprctl, f.dir.filePath("skill")};
        run.code = Cli::run(args, context, out, err);
        out.flush();
        err.flush();
        return run;
    }

private slots:
    void cliSavesListsAndChecks()
    {
        Fixture f;
        const Catalog catalog = f.catalog();
        auto saved = runCli(f, {"save", "Coding", "--json"}, catalog);
        QCOMPARE(saved.code, 0);
        const QJsonObject json = QJsonDocument::fromJson(saved.out.toUtf8()).object();
        QCOMPARE(json["apps"].toInt(), 4);
        QCOMPARE(json["skipped_named_workspaces"].toArray().size(), 1);
        QVERIFY(saved.err.contains("scratch"));

        QCOMPARE(runCli(f, {"save", "Coding"}, catalog).code, 1); // never replaces without --force
        QVERIFY(runCli(f, {"save", "Coding"}, catalog).err.contains("--force"));
        QCOMPARE(runCli(f, {"save", "Coding", "--force"}, catalog).code, 0);

        const auto listed = runCli(f, {"list", "--json"}, catalog);
        QCOMPARE(QJsonDocument::fromJson(listed.out.toUtf8()).object()["scenes"].toArray().size(), 1);

        const auto checked = runCli(f, {"check", "coding", "--json"}, catalog);
        QCOMPARE(checked.code, 0);
        QVERIFY(QJsonDocument::fromJson(checked.out.toUtf8()).object()["restorable"].toBool());

        f.setMonitors(false, true);
        const auto missing = runCli(f, {"check", "coding"}, catalog);
        QCOMPARE(missing.code, 1);
        QVERIFY(missing.err.contains("Slate 24"));
    }

    void cliRejectsBadInput()
    {
        Fixture f;
        const Catalog catalog = f.catalog();
        QCOMPARE(runCli(f, {"restore", "nothing"}, catalog).code, 1);
        QVERIFY(runCli(f, {"restore", "nothing"}, catalog).err.contains("no scene named"));
        QCOMPARE(runCli(f, {"list", "--bogus"}, catalog).code, 1);
        QCOMPARE(runCli(f, {"frobnicate"}, catalog).code, 1);
        QCOMPARE(runCli(f, {"save", "***"}, catalog).code, 1);
        QCOMPARE(runCli(f, {"--help"}, catalog).code, 0);
        QVERIFY(Cli::wantsWindow({}));
        QVERIFY(Cli::wantsWindow({"open"}));
        QVERIFY(!Cli::wantsWindow({"list"}));
    }

    void cliDryRunPrintsExactlyWhatWouldLaunch()
    {
        Fixture f;
        const Catalog catalog = f.catalog();
        QString error;
        QVERIFY(f.store().save(sampleScene("Coding"), &error));
        const auto run = runCli(f, {"restore", "Coding", "--dry-run"}, catalog);
        QCOMPARE(run.code, 0);
        QCOMPARE(run.out, QString("launch firefox on workspace 1 (Vision 32)\n"));
        QVERIFY(f.evals().isEmpty()); // a dry run changes nothing

        const auto json = runCli(f, {"restore", "Coding", "--dry-run", "--json"}, catalog);
        const QJsonObject object = QJsonDocument::fromJson(json.out.toUtf8()).object();
        QVERIFY(object["dry_run"].toBool());
        QCOMPARE(object["launch"].toArray().size(), 1);
        QCOMPARE(object["launch"].toArray()[0].toObject()["workspace"].toInt(), 1);
        QVERIFY(f.evals().isEmpty());
    }

    void cliRestoreLaunchesAndExitsOneOnProblems()
    {
        Fixture f;
        const Catalog catalog = f.catalog();
        QString error;
        QVERIFY(f.store().save(sampleScene("Coding"), &error));
        auto run = runCli(f, {"restore", "Coding"}, catalog);
        QCOMPARE(run.code, 0);
        QCOMPARE(f.evals().size(), 1);
        QVERIFY(f.evals()[0].contains("workspace=\"1 silent\""));

        // A scene for a monitor that is gone: exits 1 and says which.
        f.setMonitors(false, true);
        run = runCli(f, {"restore", "Coding"}, catalog);
        QCOMPARE(run.code, 1);
        QVERIFY(run.err.contains("Slate 24 is not connected"));
    }

    void cliInstallsTheBundledSkill()
    {
        Fixture f;
        const Catalog catalog = f.catalog();
        const auto run = runCli(f, {"skill", "install"}, catalog);
        QCOMPARE(run.code, 0);
        const QByteArray skill = read(f.dir.filePath("skill/SKILL.md"));
        QVERIFY(skill.startsWith("---\nname: scenes"));
        QVERIFY(skill.contains("restore"));
        QVERIFY(skill.contains("no command to set the layout"));
    }

    // --- Backend -----------------------------------------------------------

    void backendSavesSelectsRenamesDeletesAndUndoes()
    {
        Fixture f;
        const Catalog catalog = f.catalog();
        Backend backend(f.store(), f.hypr(), catalog, nullptr, f.hyprctl);
        QTRY_VERIFY(backend.view().value("monitors").toList().size() == 2);
        QVERIFY(backend.view().value("current").toBool());

        QVERIFY(!backend.saveCurrent("  ").isEmpty());
        QCOMPARE(backend.saveCurrent("Coding"), QString());
        QCOMPARE(backend.selected(), QString("coding"));
        QVERIFY(!backend.saveCurrent("coding").isEmpty()); // same name, however spelled
        QCOMPARE(backend.scenes().size(), 1);

        // The view draws the saved scene, with every monitor connected.
        const auto monitors = backend.view().value("monitors").toList();
        QCOMPARE(monitors.size(), 2);
        QVERIFY(monitors[0].toMap().value("connected").toBool());

        QCOMPARE(backend.rename("Writing"), QString());
        QCOMPARE(backend.selected(), QString("writing"));
        QVERIFY(!f.store().exists("coding"));
        QVERIFY(backend.canUndo());
        QCOMPARE(backend.undo(), QString());
        QCOMPARE(backend.selected(), QString("coding"));
        QVERIFY(f.store().exists("coding") && !f.store().exists("writing"));
        QCOMPARE(backend.redo(), QString());
        QVERIFY(f.store().exists("writing"));

        QCOMPARE(backend.remove(), QString());
        QVERIFY(!f.store().exists("writing"));
        QCOMPARE(backend.selected(), QString()); // back on the current desktop
        QCOMPARE(backend.undo(), QString());
        QVERIFY(f.store().exists("writing")); // delete undone
        QCOMPARE(backend.selected(), QString("writing"));
    }

    void backendFlagsMonitorsThatAreGone()
    {
        Fixture f;
        QString error;
        QVERIFY(f.store().save(sampleScene("Coding"), &error));
        f.setMonitors(false, true);
        const Catalog catalog = f.catalog();
        Backend backend(f.store(), f.hypr(), catalog, nullptr, f.hyprctl);
        QTRY_VERIFY(backend.view().value("monitors").toList().size() == 1);
        backend.select("coding");
        const auto monitors = backend.view().value("monitors").toList();
        QCOMPARE(monitors.size(), 2);
        QVERIFY(!monitors[0].toMap().value("connected").toBool()); // DP-1's Slate 24
        QVERIFY(monitors[1].toMap().value("connected").toBool());
    }

    void backendArrowsWalkThePinnedRowThenTheScenes()
    {
        Fixture f;
        QString error;
        QVERIFY(f.store().save(sampleScene("Alpha"), &error));
        QVERIFY(f.store().save(sampleScene("Beta"), &error));
        const Catalog catalog = f.catalog();
        Backend backend(f.store(), f.hypr(), catalog, nullptr, f.hyprctl);
        QCOMPARE(backend.selected(), QString());
        backend.selectOffset(1);
        QCOMPARE(backend.selected(), QString("alpha"));
        backend.selectOffset(1);
        backend.selectOffset(1);
        QCOMPARE(backend.selected(), QString("beta")); // stops at the end
        backend.selectOffset(-5);
        QCOMPARE(backend.selected(), QString());
    }

    void backendRestoreRunsAsyncAndSummarizes()
    {
        Fixture f;
        QString error;
        Scene scene = sampleScene("Coding");
        scene.monitors[1].workspaces[0].apps = {"org.gnome.Nautilus", "firefox", "firefox", "gone.app"};
        QVERIFY(f.store().save(scene, &error));
        write(f.tools + "/layouts.txt", "11 scrolling\n12 floating\n1 scrolling\n");
        const Catalog catalog = f.catalog();
        Backend backend(f.store(), f.hypr(), catalog, nullptr, f.hyprctl);
        backend.select("coding");

        QSignalSpy finished(&backend, &Backend::restoringChanged);
        backend.restore();
        QVERIFY(backend.restoring());
        QTRY_VERIFY(!backend.restoring());

        const QVariantMap result = backend.result();
        QCOMPARE(result.value("scene").toString(), QString("coding"));
        QCOMPARE(result.value("launched").toInt(), 1);
        QCOMPARE(result.value("alreadyRunning").toInt(), 4);
        QCOMPARE(result.value("missing").toStringList(), (QStringList{"gone.app"}));
        QCOMPARE(result.value("layouts").toList().size(), 1); // reported, never applied
        QCOMPARE(f.evals().size(), 1);

        backend.dismissResult();
        QVERIFY(backend.result().isEmpty());
    }

    void backendRestoreWithMissingMonitorMovesNothing()
    {
        Fixture f;
        QString error;
        QVERIFY(f.store().save(sampleScene("Coding"), &error));
        f.setMonitors(false, true);
        const Catalog catalog = f.catalog();
        Backend backend(f.store(), f.hypr(), catalog, nullptr, f.hyprctl);
        backend.select("coding");
        backend.restore();
        QTRY_VERIFY(!backend.restoring());
        const QVariantList unplaced = backend.result().value("unplaced").toList();
        QCOMPARE(unplaced.size(), 1);
        QCOMPARE(unplaced[0].toMap().value("label").toString(), QString("Slate 24"));
        QCOMPARE(unplaced[0].toMap().value("workspaces").toInt(), 2);
        for (const auto &line : f.evals())
            QVERIFY(!line.contains("Slate 24")); // nothing was redirected to the other monitor
    }

    void backendKeepsWorkingWhenHyprlandIsGone()
    {
        Fixture f;
        QFile::remove(f.hyprctl);
        const Catalog catalog = f.catalog();
        Backend backend(f.store(), f.hypr(), catalog, nullptr, f.hyprctl);
        QTRY_VERIFY(!backend.currentError().isEmpty());
        QVERIFY(!backend.saveCurrent("Nope").isEmpty());
    }

    void backendFollowsEventsLive()
    {
        Fixture f;
        QTemporaryDir dir(QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation) + "/scenes-test-XXXXXX");
        QLocalServer server;
        QVERIFY(server.listen(dir.filePath("s.sock")));
        HyprEvents events(dir.filePath("s.sock"));
        const Catalog catalog = f.catalog();
        Backend backend(f.store(), f.hypr(), catalog, &events, f.hyprctl);
        QTRY_VERIFY(server.hasPendingConnections());
        QLocalSocket *client = server.nextPendingConnection();
        QTRY_COMPARE(backend.view().value("monitors").toList().size(), 2);

        f.setMonitors(false, true);
        client->write("monitorremoved>>DP-1\n");
        client->flush();
        QTRY_COMPARE(backend.view().value("monitors").toList().size(), 1);
    }

    // --- Theme and language ------------------------------------------------

    void themeFallsBackToDark()
    {
        QTemporaryDir dir;
        Theme theme(dir.path());
        QCOMPARE(theme.accent(), QString("#FFD60A"));
        QCOMPARE(theme.accentForeground(), QString("black"));
        QVERIFY(!theme.light());
        QCOMPARE(theme.radius(), 0);
    }

    void themeReadsAccentAndColors()
    {
        QTemporaryDir dir;
        write(dir.filePath("theme/colors.toml"),
              "mode = \"light\"\naccent = \"#112233\"\nbackground = \"#eff1f5\"\nlighter_background = \"#dce0e8\"\n"
              "foreground = \"#4c4f69\"\nselection = \"#ccd0da\"\n");
        Theme theme(dir.path());
        QCOMPARE(theme.accent(), QString("#112233"));
        QCOMPARE(theme.accentForeground(), QString("white"));
        QVERIFY(theme.light());
        QCOMPARE(theme.background(), QString("#eff1f5"));
        QCOMPARE(theme.surface(), QString("#dce0e8"));
        QCOMPARE(theme.foreground(), QString("#4c4f69"));
        QVERIFY(theme.secondary() != theme.foreground());
    }

    void themeFollowsALiveSwitchToLight()
    {
        QTemporaryDir dir;
        write(dir.filePath("theme/colors.toml"), "mode = \"dark\"\naccent = \"#112233\"\nbackground = \"#000000\"\nforeground = \"#ffffff\"\n");
        Theme theme(dir.path());
        QSignalSpy changed(&theme, &Theme::changed);
        QVERIFY(!theme.light());
        // A theme switch replaces the file.
        QFile::remove(dir.filePath("theme/colors.toml"));
        write(dir.filePath("theme/colors.toml"), "mode = \"light\"\naccent = \"#abcdef\"\nbackground = \"#ffffff\"\nforeground = \"#000000\"\n");
        QTRY_VERIFY(theme.light());
        QCOMPARE(theme.accent(), QString("#abcdef"));
        QVERIFY(changed.count() >= 1);
    }

    void themeFollowsHyprlandRounding()
    {
        QTemporaryDir dir;
        const QString response = dir.filePath("response.json"), command = dir.filePath("hyprctl");
        write(command, "#!/bin/sh\ncat '" + response.toUtf8() + "'\n", true);
        write(response, "{\"int\":0}");
        Theme theme(dir.path(), nullptr, command);
        QCOMPARE(theme.radius(), 0);
        write(response, "{\"int\":6}");
        QTRY_COMPARE(theme.radius(), 6);
    }

    void translatorFollowsTheLanguageFile()
    {
        QTemporaryDir dir;
        write(dir.path() + "/language", "pt-BR\n");
        write(dir.path() + "/pt.json", "{\"Save\": \"Guardar\", \"Open\": \"Abrir\"}");
        write(dir.path() + "/pt-BR.json", "{\"Save\": \"Salvar\"}");
        Translator translator(dir.path() + "/language", dir.path());
        QCOMPARE(translator.language(), QString("pt-BR"));
        QCOMPARE(translator.translate("Main", "Save"), QString("Salvar"));
        QCOMPARE(translator.translate("Main", "Open"), QString("Abrir"));
        QVERIFY(translator.translate("Main", "Quit").isNull());
    }

    void translatorFallsBackToEnglish()
    {
        QTemporaryDir dir;
        Translator translator(dir.path() + "/language", dir.path());
        QCOMPARE(translator.language(), QString("en"));
        QVERIFY(translator.translate("Main", "Save").isNull());
    }

    void translatorPicksPluralForms()
    {
        QCOMPARE(Translator::pluralCategory("ru", 21), QString("one"));
        QCOMPARE(Translator::pluralCategory("ar", 11), QString("many"));
        QCOMPARE(Translator::pluralCategory("ja", 1), QString("other"));
        QCOMPARE(Translator::pluralCategory("sv", 1), QString("one"));
        QCOMPARE(Translator::pluralCategory("sv", 2), QString("other"));
    }

    // --- Catalogs ----------------------------------------------------------

    // Every visible string in the QML and C++ is in the Swedish catalog, and
    // every counted string has its plural forms in both.
    void catalogsCoverEveryString()
    {
        auto load = [](const QString &name) {
            return QJsonDocument::fromJson(read(QString(SRC_DIR) + "/i18n/" + name + ".json")).object();
        };
        const QJsonObject en = load("en"), sv = load("sv");
        QVERIFY(!sv.isEmpty());

        QStringList sources;
        for (const auto &file : QDir(SRC_DIR).entryList({"*.qml", "*.cpp"}, QDir::Files))
            if (file != "cli.cpp") // command-line text is for agents and stays English
                sources << QString::fromUtf8(read(QString(SRC_DIR) + "/" + file));
        const QRegularExpression call("(?:qsTr|\\btr)\\(\"((?:[^\"\\\\]|\\\\.)*)\"");
        QSet<QString> keys;
        for (const auto &source : sources) {
            auto it = call.globalMatch(source);
            while (it.hasNext())
                keys.insert(it.next().captured(1).replace("\\\"", "\""));
        }
        QVERIFY(keys.size() > 30);

        QStringList missing;
        for (const auto &key : keys) {
            if (!sv.contains(key))
                missing << key;
            if (key.contains("%Ln")) {
                for (const auto *catalog : {&en, &sv}) {
                    const QJsonObject forms = catalog->value(key).toObject();
                    if (!forms.contains("one") || !forms.contains("other"))
                        missing << "plural forms: " + key;
                }
            }
            // A translation keeps the placeholders its source has.
            if (sv.value(key).isString())
                for (const char *placeholder : {"%1", "%2", "%3"})
                    if (key.contains(placeholder) && !sv.value(key).toString().contains(placeholder))
                        missing << QString("placeholder %1 lost in: %2").arg(placeholder, key);
        }
        QVERIFY2(missing.isEmpty(), qPrintable(missing.join("\n")));
        // Nothing stale either.
        for (auto it = sv.begin(); it != sv.end(); ++it)
            QVERIFY2(keys.contains(it.key()), qPrintable("unused: " + it.key()));
    }

    void swedishRendersCountedStrings()
    {
        QTemporaryDir dir;
        write(dir.path() + "/language", "sv\n");
        Translator translator(dir.path() + "/language", ":/i18n");
        QCOMPARE(translator.translate("x", "%Ln apps", nullptr, 1), QString("%Ln app"));
        QCOMPARE(translator.translate("x", "%Ln apps", nullptr, 3), QString("%Ln appar"));
        QCOMPARE(translator.translate("x", "Restore"), QString("Återställ"));
    }

    // --- The real window ---------------------------------------------------

    void qmlDrivesTheWindow()
    {
        g_warnings.clear();
        Fixture f;
        QString error;
        QVERIFY(f.store().save(sampleScene("Coding"), &error));
        write(f.dir.filePath("language"), "en\n");
        write(f.dir.filePath("theme/theme/colors.toml"), "mode = \"dark\"\naccent = \"#FFD60A\"\n");

        const Catalog catalog = f.catalog();
        Translator translator(f.dir.filePath("language"), ":/i18n");
        Theme theme(f.dir.filePath("theme"));
        Backend backend(f.store(), f.hypr(), catalog, nullptr, f.hyprctl);
        QQmlApplicationEngine engine;
        engine.addImageProvider("appicon", new AppIcons(catalog));
        engine.rootContext()->setContextProperty("translator", &translator);
        engine.rootContext()->setContextProperty("theme", &theme);
        engine.rootContext()->setContextProperty("backend", &backend);
        engine.load(QUrl("qrc:/Main.qml"));
        QVERIFY(!engine.rootObjects().isEmpty());
        auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().first());
        QVERIFY(window);
        window->show();
        QVERIFY(QTest::qWaitForWindowExposed(window));
        window->requestActivate();
        QTRY_VERIFY(window->isActive());
        QTRY_COMPARE(backend.view().value("monitors").toList().size(), 2);

        auto *nameDialog = window->findChild<QObject *>("nameDialog");
        auto *deleteDialog = window->findChild<QObject *>("deleteDialog");
        auto *help = window->findChild<QObject *>("helpOverlay");
        QVERIFY(nameDialog && deleteDialog && help);
        auto visible = [](QObject *o) { return o->property("visible").toBool(); };

        // N opens the name dialog, and its field takes focus.
        QTest::keyClick(window, Qt::Key_N);
        QTRY_VERIFY(visible(nameDialog));
        auto *field = nameDialog->findChild<QObject *>("nameField");
        QVERIFY(field);
        QTRY_VERIFY(field->property("activeFocus").toBool());

        // Single keys are letters while typing: Q does not quit, N does not reopen.
        typeText(window, "qrn");
        QCOMPARE(field->property("text").toString(), QString("qrn"));
        QVERIFY(window->isVisible());
        QTest::keyClick(window, Qt::Key_Escape);
        QTRY_VERIFY(!visible(nameDialog));

        // Save a scene with the keyboard alone.
        QTest::keyClick(window, Qt::Key_N);
        QTRY_VERIFY(visible(nameDialog));
        QTRY_VERIFY(field->property("activeFocus").toBool());
        typeText(window, "Meeting");
        QTest::keyClick(window, Qt::Key_Return);
        QTRY_VERIFY(!visible(nameDialog));
        QVERIFY(f.store().exists("meeting"));
        QCOMPARE(backend.selected(), QString("meeting"));

        // Up and Down move through the list.
        QTest::keyClick(window, Qt::Key_Up);
        QCOMPARE(backend.selected(), QString("coding"));
        QTest::keyClick(window, Qt::Key_Up);
        QCOMPARE(backend.selected(), QString());
        QTest::keyClick(window, Qt::Key_Down);
        QCOMPARE(backend.selected(), QString("coding"));

        // Space restores the selection.
        QTest::keyClick(window, Qt::Key_Space);
        QTRY_VERIFY(backend.result().contains("launched"));
        QCOMPARE(backend.result().value("launched").toInt(), 1);

        // R renames; Escape leaves the name alone.
        QTest::keyClick(window, Qt::Key_R);
        QTRY_VERIFY(visible(nameDialog));
        QTRY_COMPARE(field->property("text").toString(), QString("Coding"));
        QTest::keyClick(window, Qt::Key_Escape);
        QTRY_VERIFY(!visible(nameDialog));
        QCOMPARE(backend.selectedName(), QString("Coding"));

        // Delete asks first, and Escape takes the answer that loses nothing.
        QTest::keyClick(window, Qt::Key_Delete);
        QTRY_VERIFY(visible(deleteDialog));
        QTest::keyClick(window, Qt::Key_Escape);
        QTRY_VERIFY(!visible(deleteDialog));
        QVERIFY(f.store().exists("coding"));
        // So does Enter on the focused Cancel button.
        QTest::keyClick(window, Qt::Key_Delete);
        QTRY_VERIFY(visible(deleteDialog));
        QTRY_VERIFY2(deleteDialog->findChild<QObject *>("cancelButton")->property("activeFocus").toBool(),
                     "Cancel should hold the focus when the dialog opens");
        QTest::keyClick(window, Qt::Key_Return);
        QTRY_VERIFY(!visible(deleteDialog));
        QVERIFY(f.store().exists("coding"));

        // Confirming deletes, and Ctrl+Z brings it back.
        QMetaObject::invokeMethod(deleteDialog, "openFor", Q_ARG(QVariant, QVariant("Coding")));
        QTRY_VERIFY(visible(deleteDialog));
        const QList<QObject *> buttons = deleteDialog->findChildren<QObject *>();
        QObject *deleteButton = nullptr;
        for (auto *candidate : buttons)
            if (candidate->property("text").toString() == "Delete" && candidate->metaObject()->indexOfMethod("click()") >= 0)
                deleteButton = candidate;
        QVERIFY(deleteButton);
        QMetaObject::invokeMethod(deleteButton, "click");
        QTRY_VERIFY(!f.store().exists("coding"));
        QTRY_VERIFY(!visible(deleteDialog)); // shortcuts come back once the dialog has faded out
        QTest::keyClick(window, Qt::Key_Z, Qt::ControlModifier);
        QTRY_VERIFY(f.store().exists("coding"));

        // ? shows the shortcuts and Escape hides them; Q only quits when nothing is open.
        QTest::keyClick(window, Qt::Key_Question, Qt::ShiftModifier);
        QTRY_VERIFY(visible(help));
        QTest::keyClick(window, Qt::Key_Q);
        QVERIFY(window->isVisible());
        QTest::keyClick(window, Qt::Key_Escape);
        QTRY_VERIFY(!visible(help));

        QTest::keyClick(window, Qt::Key_Q);
        QTRY_VERIFY(!window->isVisible());
        QVERIFY2(g_warnings.isEmpty(), qPrintable(g_warnings.join("\n")));
    }

    // Longer words, a light theme and right-to-left must not break the layout
    // or log a QML warning.
    void qmlSurvivesLongTextLightThemeAndRtl()
    {
        g_warnings.clear();
        Fixture f;
        QString error;
        QVERIFY(f.store().save(sampleScene("En ganska lång scen med ett mycket långt namn som inte får bryta layouten"), &error));
        write(f.dir.filePath("language"), "ar\n");
        write(f.dir.filePath("ar.json"), "{\"Scenes\": \"المشاهد\", \"Restore\": \"استعادة جميع التطبيقات المفقودة على أسطح المكتب\"}");
        write(f.dir.filePath("theme/theme/colors.toml"),
              "mode = \"light\"\naccent = \"#1e66f5\"\nbackground = \"#eff1f5\"\nlighter_background = \"#dce0e8\"\nforeground = \"#4c4f69\"\nselection = \"#ccd0da\"\n");

        const Catalog catalog = f.catalog();
        Translator translator(f.dir.filePath("language"), f.dir.path());
        QCOMPARE(QGuiApplication::layoutDirection(), Qt::RightToLeft);
        Theme theme(f.dir.filePath("theme"));
        QVERIFY(theme.light());
        Backend backend(f.store(), f.hypr(), catalog, nullptr, f.hyprctl);
        QQmlApplicationEngine engine;
        engine.addImageProvider("appicon", new AppIcons(catalog));
        engine.rootContext()->setContextProperty("translator", &translator);
        engine.rootContext()->setContextProperty("theme", &theme);
        engine.rootContext()->setContextProperty("backend", &backend);
        engine.load(QUrl("qrc:/Main.qml"));
        QVERIFY(!engine.rootObjects().isEmpty());
        auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().first());
        window->show();
        QVERIFY(QTest::qWaitForWindowExposed(window));
        QTRY_COMPARE(backend.view().value("monitors").toList().size(), 2);
        backend.select(Scene::slugify("En ganska lång scen med ett mycket långt namn som inte får bryta layouten"));
        QTest::qWait(300);
        window->resize(720, 480); // the smallest the window goes
        QTest::qWait(300);
        QVERIFY2(g_warnings.isEmpty(), qPrintable(g_warnings.join("\n")));

        // Put the language back for the tests that follow.
        write(f.dir.filePath("language"), "en\n");
        QTRY_COMPARE(translator.language(), QString("en"));
        QCOMPARE(QGuiApplication::layoutDirection(), Qt::LeftToRight);
    }
};

