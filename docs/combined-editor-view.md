# Combined editor view

Combined shows a profile's triggers, aliases, scripts, and timers in one editor
tree. Same-name folders at the same displayed parent are consolidated into one
row. The original folders remain separate objects, with their own code, settings,
enabled flags, children, and execution order.

This is an optional feature of this development fork. Keys, variables, and buttons
remain in their existing views. Select a category button to return to its normal
tree at any time.

## Using the view

Open the editor and select **Combined** in its category toolbar. Select an item
to open its usual editing form. Triggers, aliases, scripts, and timers use the
same type icons as the category toolbar. Folder rows use a stacked blue folder
icon, and the Type column identifies either the category or the number of
original folders represented.

Folders appear before individual items at every level. Within the folder and
item sections, traversal follows Trigger, Alias, Script, Timer order and retains
the original relative order within a type. A merged folder takes the position of
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
original parent. At the Combined root, all four types are available.

If a folder row has no member of the desired type, **Create matching … folders
and add…** creates the missing folder names for that type and then the new item
or group. If multiple matching ancestors exist, choose the intended original
ancestor first. This is an explicit edit to the profile. The newly created
subtree is one undoable addition; it is not created merely by opening the view.

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
item will execute. Trigger matching, timer scheduling, and script execution keep
their existing behavior.

Code and settings are saved through the selected original's normal editor.
Changing selection saves pending changes through the usual save path. Saving
script code may execute it according to normal Mudlet behavior. A background
refresh only rebuilds the Combined rows; it does not save or run the draft.

Search, copy/paste, and undo/redo continue to use the original category handlers.
Renaming a folder may merge or split displayed rows; the selection follows the
original being edited when it still exists. If that original is removed while a
draft is open, the draft remains available to copy and its save target is cleared.
Copy that text before navigating away if you want to retain it.

## Current limits

- One row can be selected at a time. Use regular views for bulk selection.
- Drag-and-drop is disabled because a merged row can represent several different
  parents. Use regular views to move or reorder original objects.
- Temporary objects are omitted, as are keys, variables, and buttons.
- There is no operation that treats a merged folder as a single real object or
  applies an action to all its originals at once.
- Combined keeps its tree state and pane width in the live editor instance; it
  does not introduce a persisted mixed hierarchy or new profile fields.
- Large-profile performance and platform coverage beyond the Windows development
  build remain unverified. See the [fork validation record](../FORK.md#validation-record).

## Implementation and invariants

| Source | Responsibility |
| --- | --- |
| [CombinedEditorView.h](../src/CombinedEditorView.h) | Original references, projection state, and navigation guard |
| [CombinedEditorView.cpp](../src/CombinedEditorView.cpp) | Row merging, display ordering, state labels, original-folder menus, and dispatch |
| [dlgTriggerEditor.cpp](../src/dlgTriggerEditor.cpp) | Hooks into category changes, editing actions, search, import/paste, undo, and clean resets |
| [CombinedEditorViewTest.cpp](../test/functional_tests/CombinedEditorViewTest.cpp) | UI regression cases in the shared `functional_editor_tests` binary |

### Identity and ownership

`ItemRef` is a type plus a profile-local ID. An ID alone is ambiguous across
types; ID zero is a synthetic category root. `mMembers` maps each displayed row
to all its original references. No combined folder is registered with a unit.
The model units continue to own the objects and their parent/child relationships.

Rows are discarded during refresh. Menu callbacks capture references, not row or
object pointers, and resolve them again before acting. Undo can recreate objects
and remap their IDs through the native command system; these references are not
permanent identifiers suitable for storing outside that system.

Folder paths are display-state keys only. Length-prefixed names keep a literal
slash inside a name distinct from a hierarchy separator. Expansion restoration
uses both those paths and original membership; selection restoration gives the
currently edited original priority when a merged folder splits.

### Native editor routing

`editItem()` saves the old selection before changing category, clears stale native
row pointers, rebuilds native trees when needed, and selects the exact original.
Signals are blocked while synchronizing selection, then the corresponding native
handler is called once. That handler owns the editing form and existing save logic.

`NavigationGuard` distinguishes native operations that temporarily switch a
category from a user's request to leave Combined. Nested guards defer selection
synchronization until the outer operation finishes. Do not bypass this routing
when adding another toolbar action or shortcut.

### Refresh and undo

Model and unit signals schedule one deferred refresh. `refresh()` reads the units
and rebuilds only the projection with tree signals blocked; it must not reload
the editing form, save pending code, or execute Lua. Native clean resets are
deferred while Combined is active so an open draft survives. Native trees are
rebuilt when an operation needs them.

Type icons come directly from resources because hidden category trees may be
stale or unpopulated. Folder-first ordering changes only the display rows. Neither
mechanism should become dependent on first visiting a regular category view.

Creating a missing hierarchy temporarily directs the native add commands into
a construction undo stack. The real stack receives one `EditorAddItemCommand`
for the first created folder; its subtree snapshot supports undo and redo. A
macro of separate add commands is insufficient because parent IDs can change
when restored. Keep the subtree regression test when changing this path.

## Testing

The 15 feature test methods exercise opening the view; recursive and same-type
merging; folder-first order without native reordering; distinct icons before
native trees are populated; explicit parent menus and folder code; draft-safe
refresh; deleted-item drafts; enabled/error states; missing-hierarchy undo/redo;
rename split/merge; search and view switching; alias/timer form routing; ambiguous
ancestor choices; clipboard actions; and keyboard focus across types.

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
cmake --preset windows-debug -B build-combined-tests
cmake --build build-combined-tests --target functional_editor_tests --parallel 4
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

The related regression selection used for the recorded 17-case run is:

```bash
ctest --test-dir build-combined-tests --parallel 4 --output-on-failure \
  -R '^(CombinedEditorViewTest|Editor.*Test|dlgTriggerEditorUndoRedoTest|TriggerEditor.*Test|TreeWidgetItemMoveTest|ScriptEventHandlerLifetimeTest|VariableEditorWriteBackTest|ColorTriggerDialogTest|TriggerPatternListLayoutTest)$'
```

This is a targeted selection, not the full functional or Lua suite. Obtain the
result from CTest's final summary and process exit code. Preserve failed output
when debugging; when a behavior changes, show that the corresponding test detects
the broken behavior before trusting its success with the fix.

### Manual checks

Use a disposable offline profile for deletion and undo checks:

1. Create same-name trigger, alias, script, and timer folders, including two
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
8. Save and reopen the test profile. Check the objects in both Combined and the
   regular views; opening Combined should require no profile conversion.

Record the build, platform, checks performed, and any failures when repeating
these steps. This checklist describes what to exercise, not a claim that every
step has been manually completed on every platform.
