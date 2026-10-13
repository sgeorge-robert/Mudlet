# Combined editor view

Combined shows a profile's triggers, aliases, scripts, timers, keys, and buttons
in one editor tree. Same-name folders at the same displayed parent are consolidated into one
row. The original folders remain separate objects, with their own code, settings,
enabled flags, children, and execution order.

This is an optional feature of this development fork. **Include Variables** adds
Lua values and tables to the same tree; it is off by default and remembered per
profile. Select a category button to return to its normal tree.

## Using the view

Open the editor and select **Combined** in its category toolbar. Select an item
to open its usual editing form. The six automation types use the
same type icons as the category toolbar; optional Variables use the native
variable and table icons. Mixed folder rows use a stacked blue folder
icon, and the Type column identifies either the category or the number of
original folders represented.

Folders appear before individual items at every level. Within the folder and
item sections, traversal follows Trigger, Alias, Script, Timer, Key, Button, Variable order
and retains the original relative order within a type. A merged folder takes the position of
its first occurrence. This is not alphabetical sorting and does not reorder the
objects used by the engine.

### How folders merge

Names must match exactly, including case, under the same displayed parent.
Duplicate sibling folders within one category merge too. The rule is applied
recursively; equally named folders under different displayed parents stay apart.
Individual items never merge, even when their names and types match.

For example, these originals:

```text
Triggers                       Scripts
  Combat                         Combat
    Health warning                 Helpers
    Defences                       Defences
      Shield gained                  Defence table
  Combat
    Legacy warning
```

appear together as:

```text
Combined
  Combat                       3 folders
    Defences                   2 folders
      Shield gained            Trigger
      Defence table            Script
    Health warning             Trigger
    Legacy warning             Trigger
    Helpers                    Script
```

Selecting **Combat** shows a separate button for each original folder. Right-click
the row to choose an original folder's submenu, then edit, enable/disable, copy,
paste, export, create a module, delete, or open it in its regular view. These
actions apply to the chosen original, not all folders represented by the row.
Deleting an original folder still follows the normal editor's subtree deletion
behavior.

Labels such as **Trigger folder 1** and **Trigger folder 2** distinguish original
folders of the same type. Their numbering follows display traversal and can
change after edits. Button/action tooltips show the original ID. Use **Open
Original Folder or Item** when you need the separate hierarchy to identify it.

### Adding items and groups

**Add Item**, **Add Group**, and the context menu ask for the original destination.
If two trigger folders are consolidated, both are offered; the view does not
silently choose one. When an individual item is selected, additions use its
original parent. At the Combined root, all six automation categories are available,
plus Variables when included; Buttons
offer **Add Toolbar** before adding buttons inside it.

If a folder row has no member of the desired type, **Create matching … folders
and add…** creates the missing folder names for that type and then the new item
or group. If multiple matching ancestors exist, choose the intended original
ancestor first. This is an explicit edit to the profile. The newly created
subtree is one undoable addition; it is not created merely by opening the view.

### Keys and Buttons

Keys use their existing binding capture, command, and code fields. Capturing a
key temporarily suspends the usual editor toolbar shortcuts; Escape cancels
capture, and undo/redo uses the native key-binding commands. Browsing or
refreshing Combined does not fire a binding.

Button folders have structural roles in the original hierarchy. The folder
chooser, context menus, and tooltips identify **Button toolbar**, **Button menu**,
or **Button package** (the latter also covers module containers). A single-member
Button folder shows that role in the Type column. These folders can still merge
with same-name folders of other categories, without changing their roles.

At the Button root or directly inside a package container, **Add Group** offers
**Add Toolbar**. Inside a toolbar or menu it offers **Add Menu**. **Add Button**
requires a toolbar or menu as its destination in Combined. Existing root-level
buttons remain visible and editable; this restriction only applies to new
items created with **Add Button**. Paste and import retain native routing.

When matching a missing Button hierarchy would create a toolbar, the menu says
so and names the proposed toolbar. Subsequent folders become menus. Under an
existing toolbar, only the missing menus are created. Every addition uses the
native editor's defaults and save/undo behavior. Creating or editing a toolbar
can change the game's toolbar UI; a projection refresh does not rebuild it.
Package containers retain their native restrictions on editable settings.

### State and editing

| State | Meaning |
| --- | --- |
| Enabled | The original reports itself enabled and its ancestors active |
| Disabled | The original's own enabled flag is off |
| Blocked by parent | The original is enabled but an ancestor is inactive |
| Error | The original reports an invalid state; its error is included in the tooltip |
| Mixed | The original folders represented by this row report different states |

Error takes precedence over Disabled, then Blocked by parent. A folder row's
state summarizes its represented originals, not every descendant. These labels
describe the units' existing flags and validity; they are not a promise that an
item will execute. Trigger matching, timer scheduling, key bindings, buttons, and
script execution keep their existing behavior.

Code and settings are saved through the selected original's normal editor.
Changing selection saves pending changes through the usual save path. Saving
script code may execute it according to normal Mudlet behavior. A background
refresh only rebuilds the Combined rows; it does not save or run the draft.

Search, copy/paste, and undo/redo continue to use the original category handlers.
Renaming a folder may merge or split displayed rows; the selection follows the
original being edited when it still exists. If that original is removed while a
draft is open, the draft remains available to copy and its save target is cleared.
Copy that text before navigating away if you want to retain it.

### Keyboard section navigation

**Ctrl+Tab** moves forward through the visible Combined tree, the selected
original's settings, and its code or value editor. **Ctrl+Shift+Tab** moves in
reverse. Triggers have an additional stop at their pattern controls between
settings and code. Hidden or disabled sections are skipped; a Lua table or
function without a value editor cycles between the tree and its settings.
When a Button package hides its name and code fields, navigation reaches its
remaining stylesheet setting.

For a merged folder, these shortcuts move between the tree and the original-folder
buttons. Use ordinary **Tab** or **Shift+Tab** to move among those buttons. At the
Combined root, section navigation stays in the tree. Moving between sections
keeps the same original selected and retains the open draft.

### Variables and Lua tables

Select **Include Variables** to read the profile's Lua variables. Tables with
string keys merge with exactly matching folders at the same displayed parent.
The match uses actual parent levels: a literal dot or slash in a key does not
create another level. Numeric, boolean, and reference keys are labeled with
their key type and stay separate. Values other than tables are individual items
and never merge with folders or one another.

A merged row offers the **Lua table** separately from its editor folders.
**Edit Value and Settings** opens the native Variables form. **Add Variable**,
**Add Lua Table**, and **Delete** apply only to that original Lua destination.
There is no shortcut to create a missing Lua-table hierarchy. Native restrictions
on editing and saving values still apply, including functions and ambiguous keys.
Variables do not acquire item clipboard/export operations or editor undo/redo.

**Save with profile** exposes the native persistence checkbox in the editing
form. A variable's State column describes persistence, not activation. On a mixed
folder, Lua persistence is displayed separately from editor-folder activation.
Disabling a trigger folder does not disable a neighboring Lua table. The native
hidden-variable control and **Show normally hidden variables** filter remain available.

**Refresh Variables** reads a fresh snapshot without saving the open draft.
Ordinary Combined refreshes reuse this snapshot; they do not repeatedly scan Lua.
An unchanged variable keeps its draft and save target across explicit refresh.
Before a variable edit is saved, the current value and each containing table are
checked against what was selected. If a script changed the value, replaced a
table, changed whether a table is empty, or reset the profile, saving is refused
and the draft stays available to copy. Copy it before selecting another item,
then refresh and select the variable
again to load its current value. A navigation attempt that first detects a conflict
stops on the draft; subsequent deliberate navigation can leave it. Refreshing is
not a forced overwrite option. Automatic matching-folder creation is unavailable
under tables with non-string keys, where the new folder could not merge back.

Turning off **Include Variables** saves a pending variable edit through the same
conflict check. If it conflicts with live Lua data, the option stays checked,
Variables remain visible, and the draft remains available to copy. With no
conflict, the edit is saved before Variables are hidden.

Search respects the editor's existing **Include Variables** option. With
Variables included in Combined, a variable result opens its native form while
keeping the combined tree visible. Native handling of hidden values, shared
tables, cycles, and traversal limits is retained.

### Remembered preferences

Combined remembers **Include Variables**, its left/right pane widths, the Name
column width, and expanded folders for each profile. Closing the editor or
profile saves these preferences in the local `Mudlet.ini`. Regular views retain
their own layout behavior; reopening the editor does not automatically enter
Combined or read Lua variables.

Folder expansion follows the displayed path, so it survives new original IDs
when a profile is loaded again. Temporarily hidden Lua tables and packages
loaded later can recover their expansion state. A collapsed Combined root also
stays collapsed. Numeric and string Lua keys remain distinct; tables reached
through reference keys cannot retain expansion across restarts because those
keys have no stable identity between Lua sessions.

These preferences do not save a selection, an unfinished draft, or Lua values.
Opening Combined with Variables included reads their current values. Use the
native **Save with profile** control to persist supported variables.

## Current limits

- One row can be selected at a time. Use regular views for bulk selection.
- Drag-and-drop is disabled because a merged row can represent several different
  parents. Use regular views to move or reorder original objects.
- Temporary editor objects are omitted. Lua locals are not exposed by the native
  Variables browser and are not added by Combined.
- There is no operation that treats a merged folder as a single real object or
  applies an action to all its originals at once.
- Remembered preferences are local UI settings; Combined does not introduce a
  persisted mixed hierarchy or new profile XML fields.
- Large-profile performance and platform coverage beyond the Windows development
  build remain unverified. See the [fork validation record](../FORK.md#validation-record).

## Implementation and invariants

| Source | Responsibility |
| --- | --- |
| [CombinedEditorView.h](../src/CombinedEditorView.h) | Original references, projection state, and navigation guard |
| [CombinedEditorView.cpp](../src/CombinedEditorView.cpp) | Row merging, display ordering, state labels, original-folder menus, dispatch, and view preferences |
| [dlgTriggerEditor.cpp](../src/dlgTriggerEditor.cpp) | Hooks into category changes, editing actions, search, import/paste, undo, clean resets, and settings writes |
| [ActionUnit.cpp](../src/ActionUnit.cpp) and [Host.h](../src/Host.h) | A toolbar-state notification for the projection, separate from structural item changes |
| [CombinedEditorViewTest.cpp](../test/functional_tests/CombinedEditorViewTest.cpp) | UI regression cases in the shared `functional_editor_tests` binary |
| [EditorKeyCaptureTest.cpp](../test/functional_tests/EditorKeyCaptureTest.cpp) | Independent regular-view regression for restoring shortcuts after binding capture |

### Integration boundaries

Keep projection logic in `CombinedEditorView`. Its friendship with
`dlgTriggerEditor` gives it access to the existing forms, trees, and commands;
it does not own a second set of editable objects or a second execution engine.
The shared editor needs these integration points:

| Boundary | Combined behavior | Regular-view behavior |
| --- | --- | --- |
| Category selection, menus, and search | Route to an exact original while retaining the combined tree, or explicitly leave Combined | Existing category handlers remain responsible for their trees and forms |
| Saving and navigation | Check live Lua values before saving a variable draft or leaving it | Guards allow native operations when Combined is inactive |
| Refresh and undo | Coalesce projection updates, preserve drafts, and rebuild native rows only when needed | Existing structural-change and clean-reset handling remains in place |
| Variable snapshots | Retain native variable rows through unrelated tree rebuilds; rescan Lua on explicit refresh | Native Variables refresh behavior applies outside Combined |
| Section shortcuts | Cycle through visible Combined sections without saving | Existing section handlers run when Combined is inactive |
| Toolbar visibility | Listen to `Host::signal_toolBarStateChanged()` to update state labels | The notification does not mark native trees stale or request a rebuild |
| View preferences | Forward editor settings writes to the Combined controller; it also saves on direct teardown | Native settings keys and reads are unchanged; inactive Combined saves its cached pane widths |

Typed variable search metadata is attached only to searches made with Variables
included in Combined. It stays with those results even after leaving Combined,
so following an existing result still distinguishes numeric and string keys.
Searches made in regular views retain their native matching behavior.

One shared fix intentionally also changes regular views: key-binding capture
preserves each action's full shortcut list and restores it on acceptance or
Escape. Restoring shortcuts by action label loses Add/Delete, alternative
bindings, and actions whose labels differ from the lookup map. This fix is kept
in a separate commit and tested in the regular Keys view without opening Combined.
Its integration with Combined selection and undo has a separate feature test.

These hooks do not change unit ownership, parent relationships, execution order,
or profile storage. A new integration should pass through an existing native
operation, with its behavior outside Combined checked explicitly.

### Identity and ownership

`ItemRef` is a type plus an ID: a profile-local item ID for the six automation
types, or an expiring native-row ID for Variables. An ID alone is ambiguous across
types; ID zero is a synthetic category root. `mMembers` maps each displayed row
to all its original references. No combined folder is registered with a unit.
The model units continue to own the objects and their parent/child relationships.

Rows are discarded during refresh. Menu callbacks capture references, not row or
object pointers, and resolve them again before acting. Undo can recreate objects
and remap their IDs through the native command system; these references are not
permanent identifiers suitable for storing outside that system.

Folder paths are display-state keys only. Length-prefixed names keep a literal
slash inside a name distinct from a hierarchy separator. Within an editor,
expansion restoration uses both those paths and original membership; selection
restoration gives the currently edited original priority when a merged folder
splits. Only stable paths are persisted between editor instances, never original
IDs or Lua registry handles.

### Preference storage

The controller reads and writes its own QSettings keys under
`Editor/combined_view/profiles/<sanitized profile name>/`. It reuses the editor's
profile naming convention. Signal blocking prevents the restored Include
Variables preference from selecting a form or scanning Lua during construction.
Saving preferences never invokes an item save or a Lua write.

Expansion paths are cached on the display rows, so saving does not depend on a
still-valid variable snapshot. Paths absent from the current projection are
retained for hidden tables and later package loads; paths for visible collapsed
folders are removed. A separate flag distinguishes a saved all-collapsed tree
from a profile opening Combined for the first time. An editor that never opened
Combined leaves the previously recorded expansion state intact. Reference-key
tables and their descendants have no persistent expansion key.

The controller records Combined's current pane widths while active and uses its
cached widths after leaving. Native editor settings writes call the controller;
its destructor also saves because profile teardown can bypass the close event.
Both paths run before the source widgets are destroyed.

### Native editor routing

`editItem()` saves the old selection before changing category, clears stale native
row pointers, rebuilds native trees when needed, and selects the exact original.
Signals are blocked while synchronizing selection, then the corresponding native
handler is called once. That handler owns the editing form and existing save logic.

The private `saveAndClearCurrentItems()` helper keeps the save-before-clear order
shared by entering Combined, opening a folder summary, and selecting an original.
Callers must approve navigation before using it while Combined is active; the
helper neither checks conflicts itself nor selects or rebuilds a tree. Keep the
Include Variables toggle's save path separate: it must keep an already reported
conflict visible, whereas deliberate navigation may leave that retained draft.
After a native tree rebuild, `editItem()` clears any default selection's save
target before resolving the requested original again.

`NavigationGuard` distinguishes native operations that temporarily switch a
category from a user's request to leave Combined. Nested guards defer selection
synchronization until the outer operation finishes. Do not bypass this routing
when adding another toolbar action or shortcut.

The editor's section shortcuts delegate to `moveSectionFocus()` only while
Combined is active. It builds the focus cycle from visible, enabled widgets,
without changing selection or invoking an item save. Trigger pattern controls
are nested inside the settings form, so focus matching checks the inner section
first. The regular category views retain their existing shortcut handlers.

### Refresh and undo

Model and unit signals schedule one deferred refresh. Key toggles use the existing
Host signal; `ActionUnit::setToolBarActive()` emits `signal_toolBarStateChanged()`
after updating toolbars, so Lua visibility changes also update the projection.
This notification is separate from `signal_itemsChangedByScript()`, whose native
editor listener marks its trees stale. A visibility change does not invalidate
those trees or trigger an unrelated draft save on re-entry.
`refresh()` reads the units
and rebuilds only the projection with tree signals blocked; it must not reload
the editing form, save pending code, or execute Lua. Native clean resets are
deferred while Combined is active so an open draft survives. Native trees are
rebuilt when an operation needs them.

Variable references use monotonically assigned view-local IDs and persistent
indexes into native rows. They are not persistent profile IDs. Rebuilding the
native variable tree expires those references; an older menu cannot act on the
replacement rows. Typed, length-prefixed Lua paths are used only to find a new
row after explicit refresh, and a live-value check decides whether an open draft
may be rebound to it.

The variable edit watch uses raw Lua lookups and weak references to containing
tables and the selected value. This avoids invoking lookup metamethods, retaining
otherwise collectible game tables, or treating a recycled pointer as the same
table. A VarUnit generation check fences off profile resets before any old TVar
pointer can be used. The native Variables tree is retained when unrelated editor
trees are rebuilt. Keep conflict checks on both Save and navigation routes.

Each profile's Lua state holds one watch in the registry under the address of
`variableWatchKey`. For a path of N keys, slots 1 through N hold the containing
tables, starting with the globals table; slot N+1 holds the selected value. Slot
`VariableWatchEmptyTableSlot` (0) records whether a selected table was empty.
The C++ `mVariableWatchLength` records N+1 explicitly because collection can leave
holes in the weak-value table. Both watch operations restore the incoming Lua
stack top; comparison reads the watch without replacing it or writing game data.

Type icons come directly from resources because hidden category trees may be
stale or unpopulated. Folder-first ordering changes only the display rows. Neither
mechanism should become dependent on first visiting a regular category view.

Creating a missing hierarchy temporarily directs the native add commands into
a construction undo stack. The real stack receives one `EditorAddItemCommand`
for the first created folder; its subtree snapshot supports undo and redo. A
macro of separate add commands is insufficient because parent IDs can change
when restored. Keep the subtree regression test when changing this path.

## Testing

The 45 feature test methods exercise opening the view; recursive and same-type
merging; folder-first order without native reordering; distinct icons before
native trees are populated; explicit parent menus and folder code; draft-safe
refresh; deleted-item drafts; enabled/error states; missing-hierarchy undo/redo;
rename split/merge; search and view switching; alias/timer form routing; ambiguous
ancestor choices; clipboard actions; and keyboard focus across types. Keys add
binding capture/cancel, shortcut restoration, property undo, saving, search,
parent-state updates, and missing-hierarchy checks. Buttons add native folder
roles, unchanged toolbar widgets during refresh, draft preservation, search,
clipboard/delete undo, explicit toolbar creation, and Lua visibility updates.
A regular-view control verifies that these visibility updates do not invalidate
native trees, change their selection, or save an open script draft.
Variables add optional inclusion, typed-key merging and search, native value and
persistence controls, explicit refresh, stale-menu rejection, live-value/table
conflicts, category-navigation conflicts, hidden values, table creation, read-only
values, safe recasting after a table gains contents, and snapshot invalidation.
Disabling Variables is checked both with a conflicting live update and with an
ordinary draft, including conflict recovery and re-enabling the view.
Section-navigation checks send Ctrl+Tab and Ctrl+Shift+Tab through all seven
item types, trigger patterns, original-folder choices, and the Combined root.
They verify draft retention, skipped hidden/disabled editors, package-container
stylesheet access, and the original view's shortcut routing.
Five preference tests recreate the editor to check Include Variables, fresh Lua
values without draft saves, pane and column widths, stable folder paths with new
original IDs, hidden tables, numeric/string keys, an unopened view, a collapsed
root, and independent profile settings. They also exercise direct destruction
without a close event and verify that regular-view widths remain separate.

`EditorKeyCaptureTest` independently covers accepted and cancelled binding
capture in the regular Keys view. It verifies restoration of all toolbar action
shortcuts, including Add/Delete and an action with a changed label and two bindings.

These are C++ functional tests because the projection, selection, and private
editor widgets have no Lua entry point. They share the editor test executable,
while CTest launches each test class in a separate process. Test setup uses a
temporary configuration directory and a localhost server with an assigned port.

### Reproducing the Windows checks

Provision the dependencies using the [platform instructions](platform-builds.md)
and [build skill](../.agents/skills/build-mudlet/SKILL.md). In an MSYS2 **CLANG64**
shell, from the repository root, prepare the environment used by the local checks:

```bash
export CC=clang CXX=clang++ MINGW_BASE_DIR="$MSYSTEM_PREFIX"
export LUA_PATH="$(luarocks --lua-version 5.1 path --lr-path)"
export LUA_CPATH="$(luarocks --lua-version 5.1 path --lr-cpath)"
export QT_RCC_SOURCE_DATE_OVERRIDE=1
export QT_QPA_FONTDIR="$(cygpath -am src/fonts/ttf-bitstream-vera-1.10)"
```

For a fresh test build, use a separate directory that has no portable profile
marker. This avoids the local development application's portable configuration:

```bash
cmake --preset windows-debug -B build-combined-tests -DUSE_MCPSERVER=ON
cmake --build build-combined-tests --target functional_editor_tests functional_package_tests functional_profile_tests --parallel 4
cp src/*.aff src/*.dic build-combined-tests/test/functional_tests/
ctest --test-dir build-combined-tests -R '^CombinedEditorViewTest$' --verbose
```

The same commands work against an already configured test build by substituting
its directory. The recorded validation used the existing `build` directory and
temporarily removed its development portable marker through a local helper; the
separate-directory recipe above has not yet been validated from a fresh clone.
If a portable marker is found, the test deliberately skips setup rather than
using the real profile directory. The verbose feature run exposes `SKIP`/`QSKIP`; a skip
does not validate the feature. A marker in the global Mudlet configuration can
also override a separate test directory.

The related editor and toolbar regression selection is:

```bash
ctest --test-dir build-combined-tests --parallel 1 --output-on-failure \
  -R '^(CombinedEditorViewTest|Editor.*Test|dlgTriggerEditorUndoRedoTest|TriggerEditor.*Test|TreeWidgetItemMoveTest|ScriptEventHandlerLifetimeTest|VariableEditorWriteBackTest|SavedVariableFenceTest|XMLexportVariablesTest|ColorTriggerDialogTest|TriggerPatternListLayoutTest|ActionBarPlacementTest|ActionSelfRemovalTest|ActionToolbarMenuTest)$'
```

This is a targeted selection, not the full functional or Lua suite. Obtain the
result from CTest's final summary and process exit code. Preserve failed output
when debugging; when a behavior changes, show that the corresponding test detects
the broken behavior before trusting its success with the fix.

The Windows test selection runs serially because some existing profile fixtures
allow only one second to establish their local test connection. Under concurrent
startup, they can fail setup before exercising the editor. Build parallelism is
still limited to four jobs.

### Manual checks

Use a disposable offline profile for deletion and undo checks:

1. Create same-name trigger, alias, script, timer, key, and Button folders, including two
   trigger folders with that name. Add both nested folders and individual items.
   Open Combined before visiting every regular view; verify all item icons appear.
2. Confirm folders precede items at the root and inside nested folders. Compare
   the regular views to confirm their original order and parents are unchanged.
3. Select the merged folder. Open each original's code/settings, and add an item
   through each duplicate parent's submenu. Verify it belongs to the chosen parent.
4. Disable one original folder. Verify its enabled child reports Blocked by parent
   and a differently enabled sibling folder produces a Mixed row.
5. Add a type missing from a nested merged folder. Choose the matching ancestor,
   undo once, and redo once; check the entire new hierarchy and its parent.
6. Leave a draft open while adding another item through Lua or a package action.
   Verify refresh preserves the draft. Remove the edited original in this test
   profile; verify the warning, retained text, and disabled save action.
7. Rename an original folder to split the merged row, undo to merge it again,
   follow a search result, and copy/paste an item. Check the selected original and
   that Combined remains visible. Use the arrow keys to navigate across types.
8. Capture an arrow key and Ctrl+S for a key binding. Cancel capture with Escape,
   undo/redo a binding change, and confirm editor shortcuts resume afterward.
9. Inspect Button toolbar, menu, and package-container originals. Verify their
   usual settings and code remain reachable where the native editor permits them.
   Create matching Button folders and verify the named toolbar/menu hierarchy.
10. With a Button draft open, call `hideToolBar()` and `showToolBar()` in the
    disposable profile. Check state labels and confirm the draft stays unsaved.
11. Save and reopen the test profile. Check the objects in both Combined and the
   regular views; opening Combined should require no profile conversion.
12. Enable **Include Variables** with a Lua table matching an existing folder.
    Check the independent Lua-table chooser and persistence controls. Verify
    numeric key `1` stays separate from string key `"1"`, including search results.
13. Edit a variable, refresh without saving, and check that its draft remains.
    Change its live value from a script, try Save and a category button, and
    verify the live value is retained and the conflicting draft remains to copy.
    Repeat by replacing its parent table with a new table holding the same value.
14. Select an empty table, prepare to change its type, then add a member through
    Lua. Verify saving cannot replace that populated table. Toggle hidden-variable
    visibility and switch to the original Variables view to compare its controls.
15. Edit a variable, change its live value from a script, then uncheck **Include
    Variables**. Verify the option stays checked, the form and draft remain visible,
    and Lua retains the script's value. Copy the draft, refresh, and select the
    variable again to recover. Also turn Variables off with a nonconflicting edit;
    verify it saves and is still present when Variables are enabled again.
16. From the Combined tree, use **Ctrl+Tab** and **Ctrl+Shift+Tab** to cycle through
    each type's settings and code/value editor. Check the extra trigger-pattern
    stop, a Lua table without a value editor, merged-folder buttons, and the root.
    Use ordinary Tab among folder choices. Verify the original selection and
    unsaved text remain intact, then compare navigation in a regular category view.
17. In the regular Scripts view, leave an unsaved draft open, hide/show a toolbar
    through Lua, then return focus to the editor. Check that the same item and
    draft remain and the draft has not been saved. In the regular Keys view,
    capture a binding and cancel another capture with Escape; verify Save, Add,
    and Delete shortcuts still work afterward.
18. Set Include Variables, resize Combined's panes and Name column, and expand
    several folders. Close and reopen the profile, then restart Mudlet; verify
    the preferences return when opening Combined. Repeat with the root collapsed
    and with Variables temporarily hidden. Check that another profile keeps its
    own preferences, regular views keep their layout behavior, and Variables
    show current Lua values rather than values from the earlier session.

Record the build, platform, checks performed, and any failures when repeating
these steps. This checklist describes what to exercise, not a claim that every
step has been manually completed on every platform.
