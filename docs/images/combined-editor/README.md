# Combined editor screenshots

These unmodified window captures show the actual Qt editor on Windows. They
were captured on **2026-10-15** from the MCP-enabled Debug build, including the
original-folder tabs and all four complexity cleanups recorded in
[the fork notes](../../../FORK.md#validation-record).

At capture time, `codex/combined-editor-view` was at `621c95438` with uncommitted
Combined editor changes; that commit alone does not contain the full captured
implementation. The captured implementation is recorded in
[`a81b4dd18`](https://github.com/sgeorge-robert/Mudlet/commit/a81b4dd18f7b2f98690e24416ffbb727a7432aa1).
The executable reported `5.0.0-dev-621c95438`, with SHA-256
`7fdd4de0fb609dc1f6dc1d8df7a29a0ca3c2425814b0f775c2534768c2647eec`.

The separate **Combined Editor Demo** profile was loaded offline and populated
with synthetic names, Lua code, and values. No game account, conversation, or
personal profile data is included. These are feature demonstrations, not a claim
that the complete manual validation checklist has passed.

| Image | What it demonstrates |
| --- | --- |
| [overview.jpg](overview.jpg) | Six automation types in one tree, nested mixed folders before items, type icons, enabled states, and the usual script editor |
| [original-folders.jpg](original-folders.jpg) | Seven original folders represented by one Combat row, compact tabs for each original, separate trigger-folder identities, disabled-state labels, and the first trigger folder's native settings and code |

Both images are 1920 × 1032 JPEGs saved directly from the capture tool, without
cropping, resizing, retouching, or generated UI elements.

To refresh the gallery, use a disposable offline profile with two `Combat`
trigger folders and matching alias, script, timer, key, and button folders.
Disable the second trigger folder and the timer folder. Add matching `Defences`
subfolders to Triggers, Scripts, and Buttons, then add a few clearly named items
under each original. In Combined, expand those folders and select a script for
the overview; select `Combat` to show the original-folder tabs and the active
original's settings. Keep every label and code line visible, and capture only
the demo editor window.

Use a separate profile directory when running alongside a personal Mudlet
session. Check each capture for unrelated windows or personal content before
saving it here. Update this note if the demonstrated behavior or source revision
changes. The [feature guide](../../combined-editor-view.md) remains the reference
for current behavior and limitations.
