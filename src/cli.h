#pragma once

#include <QString>
#include <QStringList>
#include <QTextStream>

#include "catalog.h"
#include "hypr.h"
#include "scenestore.h"

// What the commands act on; tests swap each piece for a fake.
struct CliContext {
    SceneStore store;
    Hypr hypr;
    const Catalog *catalog = nullptr;
    QString hyprctl = "hyprctl";
    QString skillDirectory; // Where `skill install` writes; empty means ~/.claude/skills/scenes.
};

namespace Cli {

// `open` and no arguments start the window; everything else is a command that
// needs no display.
bool wantsWindow(const QStringList &args);

// Exit code 0 or 1. Results go to `out` (JSON with --json), errors to `err`.
int run(const QStringList &args, CliContext &context, QTextStream &out, QTextStream &err);

} // namespace Cli
