---
name: scenes
description: Save and restore named setups of the Omarchy desktop (Scenes). Use when asked to save the current desktop as a scene, bring back a setup like "Coding" or "Meeting", check whether a scene can be restored, or list saved scenes.
---

# Scenes

`scenes` records, per physical monitor and per workspace, the layout and the
apps open on it, and brings them back. Scenes are JSON files in
`~/.config/scenes/<slug>.json`. Every command takes `--json`, exits 0 or 1,
and writes errors to stderr. None of them needs a display.

```sh
scenes list --json
scenes save "Coding" [--force] --json     # --force replaces an existing scene
scenes check "Coding" --json              # monitors connected? apps installed?
scenes restore "Coding" --dry-run --json  # exactly what would launch where
scenes restore "Coding" --json
```

## Rules

- Run `restore --dry-run` first and read it before a real restore.
- Restore only adds: it opens apps that are missing from a workspace. It never
  closes or moves a window that is already open.
- Monitors are matched by make, model and serial. A scene's monitor that is not
  connected is reported under `unplaced_monitors` and its workspaces are not
  moved onto another monitor. Say so to the user and let them decide.
- Layouts are shown but not applied: Omarchy has no command to set the layout
  of a specific workspace. `layouts_not_applied` lists the differences. Do not
  edit Omarchy's state files to work around it.
- Named workspaces (negative ids) are skipped when saving; the skipped names
  are in `skipped_named_workspaces`.
- Exit status 1 means something was not done: a failed launch, a missing
  monitor, an app that is not installed, or an unreadable scene.
- Never write to `~/.config/scenes` by hand; use `scenes save`.
