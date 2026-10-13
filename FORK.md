# Mudlet development fork

This fork, [sgeorge-robert/Mudlet](https://github.com/sgeorge-robert/Mudlet), develops
editor improvements on top of [Mudlet/Mudlet](https://github.com/Mudlet/Mudlet).
The current feature is an optional Combined view for triggers, aliases, scripts,
timers, keys, and buttons, with optional Lua variables and tables. It is under
active development and has no fork release yet.

Upstream's credits and project documentation remain in [README.md](README.md).
Its badges and download links describe upstream, not a validation or release of
this fork's additions.

## AI-assisted development

Development of this fork involves **OpenAI Codex**. Codex has contributed to the
Combined editor's design discussions, implementation, automated regression tests,
code review, and documentation. Codex has generated and revised code and prose
as part of this work.

Samuel George directs the feature decisions, tests the application interactively,
and is responsible for accepting and publishing changes. The
[validation record](#validation-record) distinguishes automated checks from
manual testing and records the areas that still need validation.

AI-assisted commits include an `Assisted-by: Codex:MODEL_VERSION` trailer naming
the model used, alongside the human submitter's authorized `Signed-off-by` trailer
under the repository's [contribution guidance](docs/CONTRIBUTING.md).
This disclosure describes this fork's contributions; upstream code retains its
existing attribution and notices.

## Unreleased changes

Development branch: [codex/combined-editor-view](https://github.com/sgeorge-robert/Mudlet/tree/codex/combined-editor-view),
based on upstream commit `290022ed`. This branch includes the Combined editor's
source, tests, documentation, and Codex disclosure, including the Keys/Buttons,
Variables, section-navigation, and per-profile preference extensions. The fork
owner confirmed basic manual verification on 2026-10-12; the complete manual
checklist has not been signed off.

- Add the Combined editor view, with same-name folders consolidated for display
  while the original objects, parents, and execution order stay in their units.
- Expose each original folder's code, settings, and actions separately, including
  duplicate folders of the same type.
- Add explicit destination selection and optional creation of a missing typed
  folder hierarchy as one undoable operation.
- Keep selection, search, clipboard actions, and live refresh connected to the
  existing type-specific editors.
- Display folders before leaves at every level and distinct icons for all six
  item types, including items whose original tree has not been populated.
- Include Keys with their native binding capture, commands, code, and undo.
- Include Buttons with their native toolbar, menu, and package-container roles.
  Name toolbar creation explicitly and keep all settings with their originals.
- Refresh toolbar enabled-state labels after Lua visibility changes without
  saving a draft or rebuilding toolbar widgets during projection refresh. Use a
  separate state notification so regular editor trees are not marked stale.
- Optionally include Lua variables and merge tables with string keys into matching
  folder rows. Keep numeric and string keys distinct, including search results.
- Expose native variable editing, hidden status, and profile-saving controls.
  Keep variable persistence separate from folder activation and retain native
  restrictions on undo, clipboard/export operations, and supported value types.
- Refresh Lua snapshots explicitly without saving drafts. Refuse stale writes
  after live-value changes, table replacement, snapshot invalidation, or a change
  in table emptiness; keep conflicting drafts available to copy.
- Route Ctrl+Tab and Ctrl+Shift+Tab through Combined's visible sections, including
  trigger patterns and original-folder choices. Skip hidden or disabled controls
  while preserving the selected original and its draft.
- Remember Include Variables, Combined pane and Name column widths, and expanded
  folders per profile. Keep these UI preferences separate from regular-view
  settings, original object storage, and Lua value persistence.

See the [feature guide](docs/combined-editor-view.md) for exact behavior and limits.
Browsing Combined does not alter the profile's storage format. Deliberate edits,
including creating matching folders, modify the original objects through the
normal editor operations.

The shared key-capture shortcut fix is separate from these extensions. It restores
every editor action's bindings after accepting or cancelling capture, including
Add/Delete and alternate shortcuts, in both regular and Combined views. Its
[independent regression test](test/functional_tests/EditorKeyCaptureTest.cpp)
opens only the regular Keys view.
Commit `21c8b82cc` keeps this fix and its test separate from the Combined extensions.

## Validation record

The latest behavior validation was performed on **2026-10-12** on Windows using
MSYS2 CLANG64, Clang 22.1.8, Qt 6.11.2, and the `windows-debug` configuration.

| Check | Result and scope |
| --- | --- |
| Application build | Successful; the regular development executable includes per-profile Combined preferences and optional MCP support. The earlier Variables and section-navigation work was also reviewed in a separate offline preview profile |
| Combined editor tests | All 45 behavioral test methods passed, including Keys, Buttons, Variables, section shortcuts, regular-view toolbar isolation, and preference persistence; no skipped tests |
| Related regression checks | 25 distinct selected CTest cases passed across editor/toolbar, variable/profile, and MCP checks. The latest run passed MCP server, MCP bridge, and Combined; three POSIX-only bridge checks were skipped on Windows. These are a subset of the repository's tests |
| Regression proof | New Variables tests failed before integration. Separate negative runs caught stale native rows, navigation bypassing the save guard, numeric/string search ambiguity, recasting a table after it gained contents, and turning off Include Variables overwriting a newer Lua value. Three section-navigation cases failed before routing shortcuts through Combined; the original-view control passed. Keys/Buttons, folder-order, and icon checks also caught their missing fixes. The new toolbar-isolation test failed with the broad notification; accepted and cancelled key-capture checks both failed with the upstream shortcut function |
| Desktop inspection | Six automation icons and native Key/Button roles were inspected previously. The Variables preview was checked for optional inclusion, an eight-original Combat folder, separate numeric/string table keys, Lua-table settings, the native value form, and the final placement of Save with profile beside the other settings |
| User testing | The fork owner tested the original four-category feature, reviewed the Variables/section-navigation preview, and confirmed basic manual verification on 2026-10-12. This does not establish completion of every manual check |

The exact test selection and reproduction instructions are in the
[testing section](docs/combined-editor-view.md#testing). Local build logs are
ignored artifacts, not required inputs to the test procedure. The Variables work
passed all 20 editor/toolbar cases serially (140.24 seconds), followed by four
focused cases covering Combined, native variable write-back, saved-variable
fences, and XML export (89.98 seconds). Two cases overlap, giving 22 distinct
checks. The final layout adjustment was followed by another Combined test run.
The subsequent Include Variables guard fix passed Combined and native variable
write-back checks serially (16.63 seconds). Its regression test reproduced the
overwrite before the fix; the companion normal-save test passed on both versions.
The section-navigation fix passed six selected CTest cases serially: Combined,
autocomplete focus, banner view switching, trigger disclosure, trigger-pattern
layout, and native variable write-back. The Combined run includes forward and
reverse shortcuts across all seven item types, merged-folder choices, the root,
hidden/disabled editors, and Button package stylesheets.
The boundary cleanup passed 10 selected cases serially (123.75 seconds): Combined,
regular key capture, undo/redo, editor reopening, item creation, search, native
variable write-back, toolbar placement, action self-removal, and toolbar menus.
Combined reported 42 QTest passes including setup/cleanup; the independent key
capture test reported four, including acceptance and cancellation. All 10 cases
reported zero skips. The failure run restored the upstream shortcut function and
retained the broad toolbar notification; it did not build the entire upstream tree.
The preference update passed six selected cases serially (43.77 seconds): Combined,
banner view switching, regular key capture, editor reopening, window positioning,
and native variable write-back. Combined reported 47 QTest passes including
setup/cleanup; all six cases reported zero skips. Before implementing persistence,
all five new preference tests failed on the missing behavior while the existing
40 feature methods passed. This comparison used the local pre-preference source,
not a full upstream build. Preference checks recreate editor instances and cover
direct teardown, hidden folders, profile separation, and fresh Lua values; a full
application restart remains on the manual checklist.
With MCP enabled, the application and all three selected CTest cases passed
serially (47.61 seconds). QTest reported 77 passes for the server, 77 passes and
three Windows skips for the bridge, and 47 passes for Combined, including
setup/cleanup. The skipped bridge checks require POSIX filesystem permissions;
they do not establish Windows access-control coverage. The first build reused
stale Qt metadata from the MCP-disabled configuration and failed to link.
Regenerating the core and application automoc files resolved that failure without
changing MCP source code.
During the earlier Keys/Buttons work, a parallel run hit two fixture connection
timeouts before test cases ran; both passed when retried alone, and the subsequent
serial run passed. Documentation/comment review is separate from these tests.

The full repository suite has not been rerun after this feature. The Windows
debug result does not establish Linux, macOS, release-build, sanitizer, or
large-profile performance results. Those remain follow-up work, alongside a
fresh-clone build and fork-specific CI validation before distributing a release.

## Maintaining this fork

Keep a change small enough to explain its behavior and verify its effect. For
Combined, maintain the [identity and routing invariants](docs/combined-editor-view.md#implementation-and-invariants)
and update its guide whenever behavior or limitations change.
The [integration boundaries](docs/combined-editor-view.md#integration-boundaries)
identify the shared editor hooks, their behavior outside Combined, and the shared
shortcut fix. Keep unrelated native-editor fixes in their own commits and tests.

Use comments for reasons a future maintainer cannot infer from the code: object
identity, Qt signal timing, draft preservation, and undo constraints. Keep usage,
architecture, and validation records in the documentation rather than repeating
function names in comments. Existing translator comments are part of maintaining
the UI and should stay attached to their strings.

When behavior changes, reproduce the problem, add or adapt a focused check, and
confirm that the check fails without the fix. Run the relevant regression cases
and record the platform, build configuration, scope, and outcome. Do not describe
a skipped test or an upstream CI badge as a passing fork check.

Preserve upstream notices and attribution. Follow the existing
[coding and contribution guidance](docs/CONTRIBUTING.md) and
[repository instructions](docs/ai-instructions.md) for code style, formatting,
and commit attribution. Keep unrelated game packages, personal profiles,
credentials, machine-specific launchers, and build outputs outside a feature's
commit. Review the explicit file list before committing.

After bringing in upstream changes, review the editor hooks and rerun the
feature checks, especially selection, save routing, and undo. A successful merge
alone does not demonstrate compatibility.

For a useful bug report, include the platform/build, the type of selected item,
the original folder structure (including duplicate names), the action taken,
expected and actual behavior, and whether the regular category view has the
same problem. A small disposable profile or package is preferable to a personal
game profile.

## Development entry points

- [Platform build instructions](docs/platform-builds.md) and the
  [build skill](.agents/skills/build-mudlet/SKILL.md) describe the supported toolchains.
- [Combined editor source](src/CombinedEditorView.cpp) contains the projection and routing controller.
- [Combined editor tests](test/functional_tests/CombinedEditorViewTest.cpp) exercise the UI and original objects together.
- [Manual checks](docs/combined-editor-view.md#manual-checks) cover the main interactive workflows.

The local `build-tools/` helpers and `Mudlet*.cmd` launchers are ignored convenience
files. They are not available from a fresh clone; use the tracked instructions
and CMake targets documented above when reproducing a build.

Local development builds include Mudlet's optional Model Context Protocol (MCP)
support. Configure with `-DUSE_MCPSERVER=ON`; Qt HttpServer and WebSockets are
required as described in the [platform instructions](docs/platform-builds.md#mcp-server-extra-qt-modules).
The local build helper and Qt Creator kit supply the flag. This does not change
upstream's default build options.

In an MCP-enabled build, the runtime server is controlled separately under
**Preferences → General → AI assistant access (MCP)**, using **Let an AI assistant
run Lua in Mudlet**. The server listens on localhost and requires its generated
access token. The `TMCPServerTest` and `TMCPBridgeTest` CTest cases cover the
server and the `mudlet --mcp-bridge` client connection.
