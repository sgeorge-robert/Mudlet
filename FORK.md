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
owner has reported normal operation of the rebuilt preview after all four
complexity cleanups. This is basic manual verification; the complete
[manual checklist](docs/combined-editor-view.md#manual-checks) has not been signed off.

- Add the Combined editor view, with same-name folders consolidated for display
  while the original objects, parents, and execution order stay in their units.
- Expose each original folder's code, settings, and actions separately, including
  duplicate folders of the same type.
- Replace the full-width original-folder buttons with compact tabs that remain
  above the editor. Open the first original directly, identify duplicate originals
  separately, and retain the existing save, conflict, and keyboard-navigation rules.
- Add explicit destination selection and optional creation of a missing typed
  folder hierarchy as one undoable operation.
- Keep selection, search, clipboard actions, and live refresh connected to the
  existing type-specific editors.
- Add Ctrl/Shift selection with bulk Copy/Delete for individual items of one
  automation category. Retain the active form and selected identities separately,
  deduplicate selected subtrees, and reuse native deletion undo. Folder batches,
  mixed categories, and Variables bulk actions remain unavailable.
- Display folders before leaves at every level and distinct icons for all six
  item types, including items whose original tree has not been populated.
- Identify filter-chain triggers and offset timers with native state icons and
  subtype labels. Keep mixed-folder icons while identifying each original in
  its tab and tooltip; refresh the indicators without saving drafts.
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
- Apply tree-icon size changes immediately and expose the native toolbar-recovery
  commands in Combined's context menu, preserving selection and unsaved drafts.
- Use typed original-folder actions and separate folder navigation from the
  root/empty panel. Retain the guidance widgets across refreshes, with unchanged
  native editing, saving, and execution behavior.
- Share Copy/Delete eligibility checks within each update, while revalidating
  captured targets when an action runs. Keep Variables watch state separate from
  the draft baseline so refresh can renew one without replacing the other.

See the [feature guide](docs/combined-editor-view.md) for exact behavior and limits.
Drag-and-drop, folder-inclusive bulk Copy/Delete, and bulk Variables deletion are
[intentional boundaries](docs/combined-editor-view.md#intentional-boundaries), not
pending parity work. Use regular category views for those operations.
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

The latest behavior validation was performed on **2026-10-15** on Windows using
MSYS2 CLANG64, Clang 22.1.8, Qt 6.11.2, and the `windows-debug` configuration.

| Check | Result and scope |
| --- | --- |
| Application build | Windows Debug build and command-line version check passed with MCP enabled, including all four complexity cleanups. Runtime MCP access remains controlled by Mudlet's preferences |
| Combined editor tests | All 63 behavioral test methods passed, including Keys, Buttons, Variables, section shortcuts, regular-view toolbar isolation, preference persistence, live-table recast protection, icon-size updates, toolbar recovery, original-folder tabs, subtype indicators, and scoped bulk Copy/Delete; 68 QTest passes including data rows and setup/cleanup, with no skipped tests |
| Related regression checks | The latest cleanup passed three CTest cases: Combined, native Variables write-back, and editor reopening, totaling 93 QTest passes with no failures or skips. Earlier rounds covered additional editor/toolbar, variable/profile, and MCP checks; the MCP bridge skipped three POSIX-only checks on Windows. Detailed results below identify each run's scope |
| Regression proof | New Variables tests failed before integration. Separate negative runs caught stale native rows, navigation bypassing the save guard, numeric/string search ambiguity, recasting a table after it gained contents, and turning off Include Variables overwriting a newer Lua value. Three section-navigation cases failed before routing shortcuts through Combined; the original-view control passed. Keys/Buttons, folder-order, and icon checks also caught their missing fixes. The new toolbar-isolation test failed with the broad notification; accepted and cancelled key-capture checks both failed with the upstream shortcut function |
| Desktop inspection | Earlier inspections covered automation icons, Key/Button roles, and the Variables form and controls. The initial folder-tab layout was also checked through offscreen Qt renders at a narrow width. Offline Windows captures refreshed on 2026-10-15 show the current mixed tree, enabled and parent-blocked states, and all seven original-folder tabs with the first original's native settings and code open |
| User testing | The fork owner tested earlier previews and reported normal operation of the rebuilt preview after all four complexity cleanups. This confirms basic manual verification, without claiming that every step in the manual checklist was exercised |

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
The controller's save/selection cleanup was checked with the same seven CTest
cases before and after: Combined, regular key capture, undo/redo, editor reopening,
item creation, search, and native variable write-back. Both runs reported the same
208 QTest passes, including setup/cleanup and data rows, with no failures or skips.
The MCP-enabled `functional_editor_tests` target and main application built
successfully. The application was relinked after stopping the MCP bridge processes
that held it open. Refreshing CMake's build metadata updated its version label to
`5.0.0-dev-734b252b6`; command-line version/help checks passed and exposed the MCP
bridge option. The cleanup changes only the Combined controller and documentation,
with no intended behavior change.
During the earlier Keys/Buttons work, a parallel run hit two fixture connection
timeouts before test cases ran; both passed when retried alone, and the subsequent
serial run passed. Documentation/comment review is separate from these tests.

On 2026-10-13, the live-table recast fix was checked against the unchanged
production source from `621c95438`. Three isolated cases reproduced loss of Lua
table contents through Save, navigation, and refresh when members arrived after
the snapshot but before selection. A fourth check caught the incorrectly enabled
type control; the unchanged empty-table conversion control passed. The six
related editor cases also passed before the fix. With the Combined-only guard,
the same seven CTest cases passed serially (42.35 seconds): 213 QTest passes,
including 52 for Combined, with zero failures or skips. Populated-table renaming
and growth after selection remain covered. The MCP-enabled application rebuilt
successfully, and version/help checks passed. The guide's manual checklist now
includes pre-selection table growth. The fork owner reported that the build
appeared to work well on 2026-10-14; this feedback does not establish completion
of the specific table-growth checks.

On 2026-10-14, the icon-size and toolbar-recovery checks reproduced three failures
before integration: Combined retained its old icon size, and both recovery
commands were missing. The regular editor reopening and window-position checks
passed before the fix. After the Combined-only changes, the same three CTest
cases passed serially (36.06 seconds): 76 QTest passes, including 54 for Combined,
with zero failures or skips. The checks cover icon-size changes while Combined
is active or inactive and restoring each hidden, moved toolbar without changing
selection or saving the open draft. The MCP-enabled application rebuilt
successfully, and its command-line version check passed.

The initial folder-tab update was also checked on 2026-10-14. Seven checks reported
missing tabs against the pre-tab controller; the native reopening and window
positioning controls passed. An early-failure path in a new test also exposed
missing checkbox cleanup in the fixture, which was corrected. The implemented
tabs passed all three selected CTest cases serially (35.32 seconds): 80 QTest passes, including 58 for
Combined, with zero failures or skips. The checks cover duplicate destinations,
Overview saves, unchanged drafts during refresh and folder merges/splits,
deleted originals, variable-conflict rollback, snapshot expiry, and keyboard
navigation. Narrow-window renders confirmed readable scrolling tabs and Overview
content after queued widget updates. Temporary capture code was removed from the
final tests. Native script-saving and execution semantics are unchanged.
The MCP-enabled application rebuilt successfully and passed its command-line
version check.

Overview was then removed on 2026-10-14 so selecting a combined folder opens
its first original directly. Four checks failed against the prior controller;
the regular editor reopening and window-position checks passed on both versions.
With the change, all three selected CTest cases passed serially (43.39 seconds):
79 QTest passes, including 57 for Combined, with zero failures or skips. The
checks cover immediate editing, same-folder draft retention, duplicate originals,
refresh, variable conflicts, and keyboard navigation. The removed Overview-only
data row accounts for the lower pass count. Native script-saving behavior is
unchanged. The MCP-enabled application rebuilt successfully and passed its
command-line version check.

The subtype-indicator update was checked on 2026-10-14. All three new checks
failed against the prior controller, while its existing 57 Combined passes and
the two regular editor suites remained green. With the update, all three selected
CTest cases passed serially (34.03 seconds): 82 QTest passes, including 60 for
Combined, with zero failures or skips. The checks compare native filter-chain
and offset-timer icon resources across enabled, disabled, blocked, and error
states, resolve icons before visiting native trees, cover subtype transitions,
and preserve original identities through folder merges/splits. Passive refresh retains an
unrelated script draft without saving or executing it. Production changes are
confined to Combined's labels and icons. The MCP-enabled application rebuilt
successfully and passed its command-line version check.

The scoped multi-selection update was checked on 2026-10-15. All eight new tests
and the updated selection-mode expectation failed against the prior controller;
the remaining existing checks passed. The final six CTest cases passed serially
(49.38 seconds): 220 QTest passes, including 68 for Combined, with zero failures
or skips. This includes native clipboard, undo/redo, key-capture, editor-window,
and reshow regression suites. The gesture test exercises window-level Ctrl/Shift
input and preserves the selection through a context menu. Other checks cover
all six Copy categories, unselected drafts, stale menus, passive refresh, native
delete undo, and selected ancestor/child deduplication. Production logic stays
in the Combined controller except for two delegation checks in the shared
Copy/Delete entry points. Native saving and execution behavior are unchanged.
The MCP-enabled application rebuilt successfully and passed its version check.

The first complexity-cleanup pass was checked on 2026-10-15. Production changes
are confined to `CombinedEditorView.cpp` and its header: a private `FolderAction`
enum replaces string dispatch, and `selectFolder()` is separate from the
non-saving `showEmptyPanel()` path. The shared editor source is byte-for-byte
unchanged by this pass. Existing test helpers use typed action metadata; no
behavioral tests were added or removed. The pre-cleanup controller and header
matched the recorded passing multi-selection baseline.

All five regular-editor regression cases passed. Combined's first run reached
its 60-second CTest limit near the end without an assertion failure; that timeout
is retained in the local log. Retrying only Combined with the same executable
and unchanged limit passed in 30.93 seconds, with 68 QTest passes and no failures
or skips. The six completed suites therefore account for 220 QTest passes. No
test limits or assertions were relaxed. Formatting and diff checks passed, and
the MCP-enabled application rebuilt successfully and passed its version check.
The [maintenance priorities](docs/combined-editor-view.md#complexity-review-and-maintenance-priorities)
record the completed cleanups.

The Copy/Delete eligibility cleanup was checked on 2026-10-15. Toolbar updates
and batch menus reuse one local validation result for both actions. Delete keeps
its active-item requirement, and invocation revalidates the captured originals;
no eligibility is cached between calls. Production changes are confined to the
Combined controller and header. The shared editor and existing test source are
byte-for-byte unchanged by this pass.

All three focused CTest cases passed serially in 38.98 seconds: 194 QTest passes
across Combined, native clipboard, and native undo/redo, with zero failures or
skips. Existing coverage includes invalid batches, deselected active drafts, and
stale captured menus. The MCP-enabled application rebuilt successfully and passed
its command-line version check.

The Variables-state cleanup was checked on 2026-10-15. Private `VariableWatch`
and `VariableDraftBaseline` records group fields with separate refresh lifetimes.
The controller implementation was mechanically compared with the prior source:
only the seven field references and formatting changed. Field defaults are
unchanged, and the shared editor and existing test source are byte-for-byte
unchanged by this pass.

All three focused CTest cases passed serially in 30.32 seconds: 93 QTest passes
across Combined, native Variables write-back, and editor reopening, with zero
failures or skips. Existing coverage checks draft retention, replaced tables,
live-value conflicts, and snapshot invalidation. The MCP-enabled application
rebuilt successfully and passed its command-line version check. All four items
from the complexity review are complete.

The fork owner manually checked the rebuilt preview after these cleanups and
reported that everything appeared to be working normally.

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
