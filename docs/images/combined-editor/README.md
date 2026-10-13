# Combined editor screenshots

These unmodified window captures show the actual Qt editor on Windows, using the
Combined editor code committed as `179790080` on `codex/combined-editor-view`.
They were captured on 2026-10-12 from the MCP-enabled Debug build validated in
[the fork notes](../../../FORK.md#validation-record).

The separate **Combined Editor Demo** profile was loaded offline and populated
with synthetic names, Lua code, and values. No game account, conversation, or
personal profile data is included. These are feature demonstrations, not a claim
that the complete manual validation checklist has passed.

| Image | What it demonstrates |
| --- | --- |
| [overview.jpg](overview.jpg) | Six automation types in one tree, nested mixed folders before items, type icons, enabled states, and the usual script editor |
| [original-folders.jpg](original-folders.jpg) | Seven original folders represented by one Combat row, with separate choices and states for both trigger folders |

To refresh the gallery, use a disposable offline profile with two `Combat`
trigger folders and matching alias, script, timer, key, and button folders.
Disable the second trigger folder and the timer folder. Add matching `Defences`
subfolders to Triggers, Scripts, and Buttons, then add a few clearly named items
under each original. In Combined, expand those folders and select a script for
the overview; select `Combat` for the original-folder choices. Keep every label
and code line visible, and capture only the demo editor window.

Use a separate profile directory when running alongside a personal Mudlet
session. Check each capture for unrelated windows or personal content before
saving it here. Update this note if the demonstrated behavior or source revision
changes. The [feature guide](../../combined-editor-view.md) remains the reference
for current behavior and limitations.
