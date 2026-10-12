# Mudlet development fork

This fork, [sgeorge-robert/Mudlet](https://github.com/sgeorge-robert/Mudlet), develops
editor improvements on top of [Mudlet/Mudlet](https://github.com/Mudlet/Mudlet).
The current feature is an optional Combined view for triggers, aliases, scripts,
and timers. It is under active development and has no fork release yet.

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
based on upstream commit `290022ed`. The feature's source, tests, and documentation
are published together on this branch.

- Add the Combined editor view, with same-name folders consolidated for display
  while the original objects, parents, and execution order stay in their units.
- Expose each original folder's code, settings, and actions separately, including
  duplicate folders of the same type.
- Add explicit destination selection and optional creation of a missing typed
  folder hierarchy as one undoable operation.
- Keep selection, search, clipboard actions, and live refresh connected to the
  existing type-specific editors.
- Display folders before leaves at every level and distinct icons for all four
  item types, including items whose original tree has not been populated.

See the [feature guide](docs/combined-editor-view.md) for exact behavior and limits.
Browsing Combined does not alter the profile's storage format. Deliberate edits,
including creating matching folders, modify the original objects through the
normal editor operations.

## Validation record

The latest behavior validation was performed on **2026-10-12** on Windows using
MSYS2 CLANG64, Clang 22.1.8, Qt 6.11.2, and the `windows-debug` configuration.

| Check | Result and scope |
| --- | --- |
| Application build | Successful; the regular local development executable was rebuilt and launched |
| Combined editor tests | 15 behavioral test methods in `CombinedEditorViewTest` passed |
| Related editor checks | The selected 17 CTest cases, including Combined, passed; these are a subset of the repository's tests |
| Regression proof | The folder-order and missing-icon checks failed before their fixes; earlier checks also detected deliberately disabled feature paths |
| Desktop inspection | Mixed-folder navigation and original-folder controls were inspected in a disposable offline profile; type icons were also inspected in the regular build |
| User testing | The fork owner reported using the feature successfully and identified the ordering and icon issues addressed above |

The exact test selection and reproduction instructions are in the
[testing section](docs/combined-editor-view.md#testing). Local build logs are
ignored artifacts, not required inputs to the test procedure. The documentation
and comment review does not constitute another functional test run.

The full repository suite has not been rerun after this feature. The Windows
debug result does not establish Linux, macOS, release-build, sanitizer, or
large-profile performance results. Those remain follow-up work, alongside a
fresh-clone build and fork-specific CI validation before distributing a release.

## Maintaining this fork

Keep a change small enough to explain its behavior and verify its effect. For
Combined, maintain the [identity and routing invariants](docs/combined-editor-view.md#implementation-and-invariants)
and update its guide whenever behavior or limitations change.

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
