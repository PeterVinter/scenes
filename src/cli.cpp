// Commands for agents and scripts, after Hype's CLI: each takes --json, exits 0
// or 1, and writes its errors to stderr. The window is behind `scenes open`.
#include "cli.h"

#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>

#include "plan.h"
#include "restorer.h"

namespace Cli {
namespace {

const char *usageText = R"(Scenes saves and restores named setups of the Omarchy desktop.

Usage:
  scenes open                          Open the window (the default)
  scenes list [--json]                 List the saved scenes
  scenes save <name> [--force] [--json]
                                       Save the current desktop as a scene
  scenes restore <name> [--dry-run] [--json]
                                       Open a scene's missing apps on its workspaces
  scenes check <name> [--json]         Check a scene can be restored here
  scenes skill install [--json]        Install the agent skill for these commands

Restore only adds: it opens missing apps and never closes or moves a window.
Layouts are read and shown but not applied, because Omarchy has no command to
set the layout of a given workspace.
Exit status is 0 on success and 1 on any error or unrestorable scene.)";

struct Options {
    QStringList positional;
    bool json = false, dryRun = false, force = false, help = false;
    QString error;
};

Options parse(const QStringList &args)
{
    Options options;
    for (const auto &arg : args) {
        if (arg == "--json") options.json = true;
        else if (arg == "--dry-run") options.dryRun = true;
        else if (arg == "--force") options.force = true;
        else if (arg == "--help" || arg == "-h") options.help = true;
        else if (arg.startsWith("--")) options.error = QString("unknown option %1").arg(arg);
        else options.positional.append(arg);
    }
    return options;
}

void emitJson(QTextStream &out, const QJsonObject &object)
{
    out << QJsonDocument(object).toJson(QJsonDocument::Compact) << "\n";
}

int fail(QTextStream &err, const QString &message)
{
    err << "scenes: " << message << "\n";
    return 1;
}

QJsonArray strings(const QStringList &list) { return QJsonArray::fromStringList(list); }

int list(CliContext &c, const Options &o, QTextStream &out, QTextStream &err)
{
    QStringList problems;
    const auto scenes = c.store.list(&problems);
    QJsonArray array;
    for (const auto &scene : scenes)
        array.append(QJsonObject{{"name", scene.name}, {"slug", scene.slug()}, {"path", c.store.pathFor(scene.slug())},
                                 {"monitors", scene.monitors.size()}, {"workspaces", scene.workspaceCount()},
                                 {"apps", scene.appCount()}});
    if (o.json) {
        emitJson(out, {{"scenes", array}, {"problems", strings(problems)}});
    } else if (scenes.isEmpty()) {
        out << "no scenes saved yet; `scenes save <name>` saves the current desktop\n";
    } else {
        for (const auto &scene : scenes)
            out << scene.name << "\t" << scene.appCount() << " apps, " << scene.workspaceCount() << " workspaces, "
                << scene.monitors.size() << " monitors\n";
    }
    for (const auto &problem : problems)
        err << "scenes: " << problem << "\n";
    return problems.isEmpty() ? 0 : 1;
}

int save(CliContext &c, const Options &o, QTextStream &out, QTextStream &err)
{
    if (o.positional.size() != 1)
        return fail(err, "save takes one scene name");
    const QString name = o.positional.first().trimmed();
    if (Scene::slugify(name).isEmpty())
        return fail(err, "the scene needs a name with at least one letter or digit");
    // An existing scene is replaced only on request.
    if (c.store.exists(Scene::slugify(name)) && !o.force)
        return fail(err, QString("a scene called \"%1\" already exists; pass --force to replace it").arg(name));

    CaptureReport report;
    QString error;
    const Scene scene = c.hypr.capture(name, *c.catalog, &report, &error);
    if (!error.isEmpty())
        return fail(err, "can't read the desktop: " + error);
    if (!c.store.save(scene, &error))
        return fail(err, error);

    if (o.json) {
        emitJson(out, {{"name", name}, {"path", c.store.pathFor(scene.slug())}, {"monitors", scene.monitors.size()},
                       {"workspaces", scene.workspaceCount()}, {"apps", scene.appCount()},
                       {"skipped_named_workspaces", strings(report.skippedNamed)},
                       {"unmapped_windows", report.unmappedWindows},
                       {"unmapped_classes", strings(report.unmappedClasses)}});
    } else {
        out << "saved " << name << ": " << scene.appCount() << " apps, " << scene.workspaceCount() << " workspaces, "
            << scene.monitors.size() << " monitors -> " << c.store.pathFor(scene.slug()) << "\n";
    }
    if (!report.skippedNamed.isEmpty())
        err << "scenes: warning: skipped named workspaces (" << report.skippedNamed.join(", ")
            << "): Hyprland gives them negative ids, which can't be targeted safely\n";
    if (report.unmappedWindows > 0)
        err << "scenes: warning: windows without a desktop entry were left out: " << report.unmappedWindows << " ("
            << report.unmappedClasses.join(", ") << ")\n";
    return 0;
}

// Reads what a plan needs from the running desktop.
bool makePlan(CliContext &c, const Scene &scene, Plan *plan, QString *error)
{
    const QJsonArray monitors = c.hypr.monitors(error);
    if (monitors.isEmpty())
        return false;
    const QJsonArray clients = c.hypr.clients(error);
    if (!error->isEmpty())
        return false;
    QHash<int, QString> layouts;
    *plan = buildPlan(scene, monitors, clients, c.hypr.blocks(), *c.catalog, [&](int id) {
        if (!layouts.contains(id))
            layouts.insert(id, c.hypr.layout(id));
        return layouts.value(id);
    });
    return true;
}

int restore(CliContext &c, const Options &o, QTextStream &out, QTextStream &err)
{
    if (o.positional.size() != 1)
        return fail(err, "restore takes one scene name");
    Scene scene;
    QString error;
    if (!c.store.load(o.positional.first(), &scene, &error))
        return fail(err, error);
    Plan plan;
    if (!makePlan(c, scene, &plan, &error))
        return fail(err, "can't read the desktop: " + error);

    QStringList failures;
    int launched = 0;
    bool unverified = false;
    if (!o.dryRun && !plan.launches.isEmpty()) {
        Restorer restorer(plan, *c.catalog, c.hyprctl, c.eventSocket);
        restorer.setWindowTimeout(c.windowTimeout);
        QEventLoop loop;
        QObject::connect(&restorer, &Restorer::finished, &loop, &QEventLoop::quit);
        if (!o.json)
            QObject::connect(&restorer, &Restorer::progress, [&](int done, int total, const QString &app) {
                out << "[" << done + 1 << "/" << total << "] " << app << "\n";
                out.flush();
            });
        restorer.start();
        loop.exec();
        failures = restorer.failures();
        launched = restorer.launched();
        unverified = !restorer.verified();
    }

    if (o.json) {
        QJsonObject result = plan.toJson();
        result["scene"] = scene.name;
        result["dry_run"] = o.dryRun;
        result["launched"] = launched;
        result["failed"] = strings(failures);
        result["unverified"] = unverified;
        emitJson(out, result);
    } else if (o.dryRun) {
        for (const auto &line : plan.describe())
            out << line << "\n";
    } else {
        out << "restored " << scene.name << ": opened " << launched << " apps, " << plan.alreadyRunning
            << " were already open\n";
    }

    // Problems always reach stderr, so an agent reading only errors sees them.
    if (unverified)
        err << "scenes: warning: Hyprland's event socket is not reachable, so windows could not be confirmed or moved to their workspaces\n";
    for (const auto &failure : failures)
        err << "scenes: failed to start " << failure << "\n";
    for (const auto &monitor : plan.unplaced)
        err << "scenes: monitor " << monitor.label << " is not connected; workspaces not placed: "
            << monitor.workspaceSlots.size() << "\n";
    for (const auto &app : plan.missingApps)
        err << "scenes: " << app << " is not installed\n";
    if (!plan.layouts.isEmpty())
        err << "scenes: warning: layouts that differ and were not applied: " << plan.layouts.size()
            << " (Omarchy has no command to set the layout of a given workspace)\n";
    return failures.isEmpty() && !plan.hasProblems() ? 0 : 1;
}

int check(CliContext &c, const Options &o, QTextStream &out, QTextStream &err)
{
    if (o.positional.size() != 1)
        return fail(err, "check takes one scene name");
    Scene scene;
    QString error;
    if (!c.store.load(o.positional.first(), &scene, &error))
        return fail(err, error);
    const QJsonArray monitors = c.hypr.monitors(&error);
    if (monitors.isEmpty())
        return fail(err, "can't read the desktop: " + error);

    // Nothing is running yet as far as a check is concerned, so only the
    // monitors and the installed apps decide the answer.
    const Plan plan = buildPlan(scene, monitors, {}, c.hypr.blocks(), *c.catalog, nullptr);
    const QJsonObject json = plan.toJson();
    if (o.json) {
        emitJson(out, {{"scene", scene.name}, {"valid", true}, {"restorable", !plan.hasProblems()},
                       {"unplaced_monitors", json["unplaced_monitors"]}, {"apps_not_installed", json["apps_not_installed"]}});
    } else {
        out << scene.name << ": valid, " << (plan.hasProblems() ? "not fully restorable here" : "restorable here") << "\n";
    }
    for (const auto &monitor : plan.unplaced)
        err << "scenes: monitor " << monitor.label << " is not connected\n";
    for (const auto &app : plan.missingApps)
        err << "scenes: " << app << " is not installed\n";
    return plan.hasProblems() ? 1 : 0;
}

int skillInstall(CliContext &c, const Options &o, QTextStream &out, QTextStream &err)
{
    QFile source(":/skill/SKILL.md");
    if (!source.open(QIODevice::ReadOnly))
        return fail(err, "the bundled skill is missing from this build");
    const QString directory = c.skillDirectory.isEmpty() ? QDir::homePath() + "/.claude/skills/scenes" : c.skillDirectory;
    if (!QDir().mkpath(directory))
        return fail(err, QString("can't create %1").arg(directory));
    QSaveFile target(directory + "/SKILL.md");
    target.setDirectWriteFallback(false);
    if (!target.open(QIODevice::WriteOnly) || target.write(source.readAll()) < 0 || !target.commit())
        return fail(err, QString("can't write %1: %2").arg(target.fileName(), target.errorString()));
    if (o.json)
        emitJson(out, {{"path", target.fileName()}});
    else
        out << "installed " << target.fileName() << "\n";
    return 0;
}

} // namespace

bool wantsWindow(const QStringList &args)
{
    return args.isEmpty() || args.first() == "open";
}

int run(const QStringList &args, CliContext &context, QTextStream &out, QTextStream &err)
{
    const QString command = args.value(0);
    if (command == "help" || command == "--help" || command == "-h") {
        out << usageText << "\n";
        return 0;
    }
    const Options options = parse(args.mid(command == "skill" ? 2 : 1));
    if (!options.error.isEmpty())
        return fail(err, options.error);
    if (options.help) {
        out << usageText << "\n";
        return 0;
    }
    if (command == "list") return list(context, options, out, err);
    if (command == "save") return save(context, options, out, err);
    if (command == "restore") return restore(context, options, out, err);
    if (command == "check") return check(context, options, out, err);
    if (command == "skill" && args.value(1) == "install") return skillInstall(context, options, out, err);
    err << "scenes: unknown command \"" << args.join(' ') << "\"\n\n" << usageText << "\n";
    return 1;
}

} // namespace Cli
