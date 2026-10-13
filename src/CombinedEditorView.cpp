#include "CombinedEditorView.h"

#include "Host.h"
#include "EditorAddItemCommand.h"
#include "LuaInterface.h"
#include "MudletApp.h"
#include "VarUnit.h"
#include "TTreeWidget.h"
#include "dlgActionMainArea.h"
#include "dlgAliasMainArea.h"
#include "dlgKeysMainArea.h"
#include "dlgScriptsMainArea.h"
#include "dlgTriggerEditor.h"
#include "dlgTriggerPatternEdit.h"
#include "dlgVarsMainArea.h"
#include "edbee/models/textdocument.h"

#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMenu>
#include <QPainter>
#include <QPushButton>
#include <QScopedValueRollback>
#include <QScrollBar>
#include <QSet>
#include <QSettings>
#include <QSignalBlocker>
#include <QSplitter>
#include <QTimer>
#include <QToolBar>
#include <QTreeWidgetItemIterator>
#include <QVBoxLayout>
#include <algorithm>

namespace {
constexpr int TypeRole = Qt::UserRole + 1;
constexpr int FolderRole = Qt::UserRole + 2;
constexpr int PathRole = Qt::UserRole + 3;
constexpr int ExpansionPathRole = Qt::UserRole + 4;
constexpr int VariableIdRole = Qt::UserRole + 201;
constexpr int VariableSearchPathRole = Qt::UserRole + 202;
char variableWatchKey;

bool tableEmpty(lua_State* state)
{
    lua_pushnil(state);
    if (lua_next(state, -2)) {
        lua_pop(state, 2);
        return false;
    }
    return true;
}

bool pushVariableKey(lua_State* state, TVar* var)
{
    if (var->isReference()) {
        lua_rawgeti(state, LUA_REGISTRYINDEX, var->getName().toInt());
    } else if (var->getKeyType() == LUA_TNUMBER) {
        lua_pushnumber(state, var->getName().toDouble());
    } else if (var->getKeyType() == LUA_TBOOLEAN) {
        lua_pushboolean(state, var->getName().compare(qsl("true"), Qt::CaseInsensitive) == 0);
    } else if (var->getKeyType() == LUA_TSTRING) {
        const auto key = var->getName().toUtf8();
        lua_pushlstring(state, key.constData(), key.size());
    } else {
        return false;
    }
    return true;
}
}

QList<EditorViewType> CombinedEditorView::types()
{
    return {EditorViewType::cmTriggerView,
            EditorViewType::cmAliasView,
            EditorViewType::cmScriptView,
            EditorViewType::cmTimerView,
            EditorViewType::cmKeysView,
            EditorViewType::cmActionView,
            EditorViewType::cmVarsView};
}

QList<EditorViewType> CombinedEditorView::visibleTypes() const
{
    auto result = types();
    if (!includesVariables())
        result.removeAll(EditorViewType::cmVarsView);
    return result;
}

QString CombinedEditorView::typeName(EditorViewType type)
{
    switch (type) {
    case EditorViewType::cmTriggerView:
        //: Singular editor item category, used in the Type column and folder action labels.
        return tr("Trigger");
    case EditorViewType::cmAliasView:
        //: Singular editor item category, used in the Type column and folder action labels.
        return tr("Alias");
    case EditorViewType::cmScriptView:
        //: Singular editor item category, used in the Type column and folder action labels.
        return tr("Script");
    case EditorViewType::cmTimerView:
        //: Singular editor item category, used in the Type column and folder action labels.
        return tr("Timer");
    case EditorViewType::cmKeysView:
        //: Singular editor item category, used in the Type column and folder action labels.
        return tr("Key");
    case EditorViewType::cmActionView:
        //: Singular editor item category, used in the Type column and folder action labels.
        return tr("Button");
    case EditorViewType::cmVarsView:
        //: Singular category for a value in the profile's Lua interpreter.
        return tr("Variable");
    default:
        return {};
    }
}

QString CombinedEditorView::folderName(ItemRef ref) const
{
    if (ref.type == EditorViewType::cmVarsView) {
        //: A Lua table presented alongside independent editor folders.
        return tr("Lua table");
    }
    if (ref.type == EditorViewType::cmActionView) {
        if (auto* action = mpEditor->mpHost->getActionUnit()->getAction(ref.id)) {
            if (!action->mPackageName.isEmpty()) {
                //: Original Button folder that contains a package's or module's toolbars.
                return tr("Button package");
            }
            if (!action->getParent() || !action->getParent()->mPackageName.isEmpty()) {
                //: Original Button folder that defines a toolbar.
                return tr("Button toolbar");
            }
            //: Original Button folder that defines a menu inside a toolbar.
            return tr("Button menu");
        }
    }
    //: Original folder category; %1 is Trigger, Alias, Script, Timer, Key or Button.
    return tr("%1 folder").arg(typeName(ref.type));
}

bool CombinedEditorView::createsToolbar(ItemRef parent) const
{
    if (parent.type != EditorViewType::cmActionView) {
        return false;
    }
    auto* action = mpEditor->mpHost->getActionUnit()->getAction(parent.id);
    // Package containers add a level above toolbars in the native Button hierarchy.
    return !parent.id || (action && !action->mPackageName.isEmpty());
}

QString CombinedEditorView::addLabel(ItemRef parent, bool folder) const
{
    if (parent.type == EditorViewType::cmVarsView) {
        //: Creates a value or table in the selected original Lua table.
        return folder ? tr("Add Lua Table") : tr("Add Variable");
    }
    if (folder && parent.type == EditorViewType::cmActionView) {
        //: Button folders create toolbars at the root or under a package, and menus elsewhere.
        return createsToolbar(parent) ? tr("Add Toolbar") : tr("Add Menu");
    }
    //: Add an item or group of the named editor category.
    return folder ? tr("Add %1 Group").arg(typeName(parent.type)) : tr("Add %1").arg(typeName(parent.type));
}

CombinedEditorView::CombinedEditorView(dlgTriggerEditor* editor)
: QObject(editor)
, mpEditor(editor)
, mpTree(new QTreeWidget(editor->treeWidget_triggers->parentWidget()))
, mpSummary(new QWidget(editor->splitter_right))
, mpSummaryLayout(new QVBoxLayout(mpSummary))
, mpAction(new QAction(tr("Combined"), this))
, mpVariableControls(new QWidget(mpTree->parentWidget()))
, mpIncludeVariables(new QCheckBox(mpVariableControls))
, mpRefreshVariables(new QPushButton(mpVariableControls))
, mpVariableSaved(new QCheckBox(editor->mpVarsMainArea))
{
    setObjectName(qsl("combinedEditorView"));
    QPixmap folder(24, 24);
    folder.fill(Qt::transparent);
    QPainter painter(&folder);
    const QIcon original(qsl(":/icons/folder-blue.png"));
    original.paint(&painter, QRect(0, 0, 18, 18));
    original.paint(&painter, QRect(6, 6, 18, 18));
    painter.end();
    mFolderIcon = QIcon(folder);
    // Hidden category trees may be stale or empty, so they cannot supply these icons.
    mTypeIcons = {{EditorViewType::cmTriggerView, QIcon(qsl(":/icons/tools-wizard.png"))},
                  {EditorViewType::cmAliasView, QIcon(qsl(":/icons/system-users.png"))},
                  {EditorViewType::cmScriptView, QIcon(qsl(":/icons/accessories-text-editor.png"))},
                  {EditorViewType::cmTimerView, QIcon(qsl(":/icons/chronometer.png"))},
                  {EditorViewType::cmKeysView, QIcon(qsl(":/icons/preferences-desktop-keyboard.png"))},
                  {EditorViewType::cmActionView, QIcon(qsl(":/icons/bookmarks.png"))},
                  {EditorViewType::cmVarsView, QIcon(qsl(":/icons/variable.png"))}};
    mpAction->setIcon(mFolderIcon);
    mpAction->setObjectName(qsl("action_combinedEditor"));
    mpAction->setCheckable(true);
    //: Tooltip for the editor's mixed hierarchy, with an optional live Lua data category.
    mpAction->setToolTip(tr("View editor items together, with optional Lua variables"));
    editor->toolBar2->insertAction(editor->toolBar2->actions().first(), mpAction);
    connect(mpAction, &QAction::triggered, this, &CombinedEditorView::show);

    auto* variableLayout = new QHBoxLayout(mpVariableControls);
    //: Enables Lua values and tables in the combined editor hierarchy.
    mpIncludeVariables->setText(tr("Include Variables"));
    //: Reads current Lua data without saving the open draft.
    mpRefreshVariables->setText(tr("Refresh Variables"));
    //: Saves this variable between profile sessions; it is not an activation toggle.
    mpVariableSaved->setText(tr("Save with profile"));
    variableLayout->setContentsMargins(0, 0, 0, 0);
    variableLayout->addWidget(mpIncludeVariables);
    variableLayout->addWidget(mpRefreshVariables);
    mpIncludeVariables->setObjectName(qsl("combinedIncludeVariables"));
    mpRefreshVariables->setObjectName(qsl("combinedRefreshVariables"));
    mpVariableSaved->setObjectName(qsl("combinedVariableSaved"));
    //: Refresh reads live Lua values without saving the open editor draft.
    mpRefreshVariables->setToolTip(tr("Read current Lua variables without saving your draft"));
    mpVariableControls->parentWidget()->layout()->addWidget(mpVariableControls);
    editor->mpVarsMainArea->gridLayout_frame_vars_main_area->addWidget(mpVariableSaved, 2, 0, 1, 3);
    mpVariableControls->hide();
    mpVariableSaved->hide();
    connect(mpRefreshVariables, &QPushButton::clicked, this, &CombinedEditorView::refreshVariables);
    connect(mpIncludeVariables, &QCheckBox::toggled, this, [this](bool include) {
        if (include) {
            refreshVariables();
        } else {
            if (mpEditor->mCurrentView == EditorViewType::cmVarsView) {
                // toggled arrives after the checkbox changes; native saves must still see the variable guard enabled.
                QSignalBlocker blocker(mpIncludeVariables);
                mpIncludeVariables->setChecked(true);
                mpEditor->saveOpenChanges();
                if (mVariableConflict) {
                    return;
                }
                mpIncludeVariables->setChecked(false);
                clearCurrentItems();
                showFolder(nullptr, false);
            }
            refresh();
        }
        maintainVisibility();
    });
    connect(mpVariableSaved, &QCheckBox::clicked, this, &CombinedEditorView::setVariableSaved);

    mpTree->setObjectName(qsl("treeWidget_combined"));
    mpTree->setHeaderLabels({tr("Name"), tr("Type"), tr("State")});
    mpTree->setSelectionMode(QAbstractItemView::SingleSelection);
    mpTree->setDragDropMode(QAbstractItemView::NoDragDrop);
    mpTree->setContextMenuPolicy(Qt::CustomContextMenu);
    mpTree->setIconSize(editor->treeWidget_triggers->iconSize());
    mpTree->header()->setStretchLastSection(false);
    mpTree->header()->setSectionResizeMode(0, QHeaderView::Interactive);
    mpTree->setColumnWidth(0, mpTree->fontMetrics().horizontalAdvance(QChar('M')) * 20 + mpTree->indentation() * 3);
    mpTree->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    mpTree->header()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    mpTree->parentWidget()->layout()->addWidget(mpTree);
    mpTree->hide();
    for (auto* action : editor->treeWidget_triggers->actions()) {
        if (action->shortcut() == QKeySequence(QKeySequence::Copy) || action->shortcut() == QKeySequence(QKeySequence::Paste)) {
            mpTree->addAction(action);
        }
    }
    mpSummary->setObjectName(qsl("combinedFolderSummary"));
    editor->splitter_right->addWidget(mpSummary);
    mpSummary->hide();
    connect(mpTree, &QTreeWidget::currentItemChanged, this, [this](QTreeWidgetItem* item) {
        selectRow(item);
    });
    connect(mpTree, &QTreeWidget::customContextMenuRequested, this, [this](const QPoint& pos) {
        auto* item = mpTree->itemAt(pos);
        if (item)
            mpTree->setCurrentItem(item);
        auto* menu = createMenu(item);
        menu->setAttribute(Qt::WA_DeleteOnClose);
        menu->popup(mpTree->viewport()->mapToGlobal(pos));
    });
    connect(mpTree, &QTreeWidget::itemClicked, this, [this](QTreeWidgetItem* item) {
        if (item && item->data(0, FolderRole).toBool() && !mpSummary->isVisible())
            showFolder(item, true);
    });
    connect(mpTree, &QTreeWidget::itemActivated, this, [this](QTreeWidgetItem* item) {
        if (item && !item->data(0, FolderRole).toBool()) {
            mpEditor->slot_toggleItemOrGroupActiveFlag();
        }
    });
    for (auto type : types()) {
        auto* model = originalTree(type)->model();
        connect(model, &QAbstractItemModel::dataChanged, this, &CombinedEditorView::scheduleRefresh);
        connect(model, &QAbstractItemModel::rowsInserted, this, &CombinedEditorView::scheduleRefresh);
        connect(model, &QAbstractItemModel::rowsRemoved, this, &CombinedEditorView::scheduleRefresh);
        connect(model, &QAbstractItemModel::modelReset, this, &CombinedEditorView::scheduleRefresh);
    }
    connect(editor->mpHost, &Host::signal_itemsChangedByScript, this, &CombinedEditorView::scheduleRefresh);
    connect(editor->mpHost, &Host::signal_toolBarStateChanged, this, &CombinedEditorView::scheduleRefresh);
    connect(editor->mpHost, &Host::signal_triggerToggled, this, &CombinedEditorView::scheduleRefresh);
    connect(editor->mpHost, &Host::signal_aliasToggled, this, &CombinedEditorView::scheduleRefresh);
    connect(editor->mpHost, &Host::signal_scriptToggled, this, &CombinedEditorView::scheduleRefresh);
    connect(editor->mpHost, &Host::signal_timerToggled, this, &CombinedEditorView::scheduleRefresh);
    connect(editor->mpHost, &Host::signal_keyToggled, this, &CombinedEditorView::scheduleRefresh);
    readSettings();
}

CombinedEditorView::~CombinedEditorView()
{
    // Profile teardown can destroy the editor directly, without a close event.
    writeSettings();
}

QString CombinedEditorView::settingsPrefix() const
{
    const auto profile = mpEditor->profileSettingsPrefix();
    return profile.isEmpty() ? QString() : qsl("Editor/combined_view/%1/").arg(profile);
}

void CombinedEditorView::readSettings()
{
    const auto prefix = settingsPrefix();
    auto* settings = MudletApp::getQSettings();
    if (prefix.isEmpty() || !settings) {
        return;
    }
    // Loading a preference must not select an editor or scan live Lua data.
    const QSignalBlocker blocker(mpIncludeVariables);
    mpIncludeVariables->setChecked(settings->value(prefix + qsl("includeVariables"), false).toBool());
    const auto savedSizes = settings->value(prefix + qsl("paneSizes")).toList();
    QList<int> sizes;
    for (const auto& value : savedSizes) {
        bool valid = false;
        const int size = value.toInt(&valid);
        if (!valid || size < 0 || size > QWIDGETSIZE_MAX) {
            sizes.clear();
            break;
        }
        sizes.append(size);
    }
    if (sizes.size() == mpEditor->splitter_main->count() && std::any_of(sizes.cbegin(), sizes.cend(), [](int size) {
            return size > 0;
        })) {
        mCombinedViewSizes = sizes;
    }
    const int columnWidth = settings->value(prefix + qsl("nameColumnWidth"), mpTree->columnWidth(0)).toInt();
    if (columnWidth > 0 && columnWidth <= QWIDGETSIZE_MAX) {
        mpTree->setColumnWidth(0, columnWidth);
    }
    const auto folders = settings->value(prefix + qsl("expandedFolders")).toStringList();
    mExpandedFolders = QSet<QString>(folders.cbegin(), folders.cend());
    mHasExpansionState = settings->contains(prefix + qsl("expandedFolders"));
}

void CombinedEditorView::rememberExpandedFolders()
{
    if (mpTree->topLevelItemCount() == 0) {
        return;
    }
    mHasExpansionState = true;
    // Keep absent paths: a Lua table or package can return after being hidden or loaded later.
    for (QTreeWidgetItemIterator it(mpTree); *it; ++it) {
        const auto path = (*it)->data(0, ExpansionPathRole).toString();
        if (path.isEmpty()) {
            continue;
        }
        if ((*it)->isExpanded()) {
            mExpandedFolders.insert(path);
        } else {
            mExpandedFolders.remove(path);
        }
    }
}

void CombinedEditorView::writeSettings()
{
    rememberExpandedFolders();
    const auto prefix = settingsPrefix();
    auto* settings = MudletApp::getQSettings();
    if (!mHasExpansionState || prefix.isEmpty() || !settings) {
        return;
    }
    if (mActive) {
        mCombinedViewSizes = mpEditor->splitter_main->sizes();
    }
    QVariantList sizes;
    for (int size : mCombinedViewSizes) {
        sizes.append(size);
    }
    auto folders = mExpandedFolders.values();
    folders.sort();
    settings->setValue(prefix + qsl("includeVariables"), includesVariables());
    settings->setValue(prefix + qsl("paneSizes"), sizes);
    settings->setValue(prefix + qsl("nameColumnWidth"), mpTree->columnWidth(0));
    settings->setValue(prefix + qsl("expandedFolders"), folders);
}

CombinedEditorView::NavigationGuard::NavigationGuard(CombinedEditorView* view)
: mpView(view)
{
    if (mpView) {
        mWasRouting = mpView->mRouting;
        mpView->mRouting = true;
    }
}

CombinedEditorView::NavigationGuard::~NavigationGuard()
{
    if (mpView) {
        mpView->mRouting = mWasRouting;
        // Nested native operations must finish before their final selection is projected.
        if (mpView->active() && !mWasRouting)
            mpView->syncSelection();
    }
}

CombinedEditorView::ItemData CombinedEditorView::data(ItemRef ref) const
{
    ItemData result;
    result.parent = {ref.type, 0};
    auto* host = mpEditor->mpHost.data();
    if (ref.type == EditorViewType::cmVarsView) {
        auto* source = originalItem(ref);
        auto* var = variable(ref);
        if (!source || (ref.id && !var))
            return result;
        result.exists = true;
        result.folder = !ref.id || var->getValueType() == LUA_TTABLE;
        if (var) {
            result.name = var->getName();
            result.mergeable = var->getKeyType() == LUA_TSTRING;
            if (!result.mergeable) {
                //: Distinguishes numeric/boolean/reference keys from a string with the same text.
                result.name = tr("[%1] (%2 key)").arg(result.name, QString::fromLatin1(lua_typename(host->getLuaInterface()->getState(), var->getKeyType())));
            }
            result.hidden = !mpEditor->showHiddenVars && host->getLuaInterface()->getVarUnit()->isHidden(var);
            if (source->parent())
                result.parent.id = source->parent()->data(0, VariableIdRole).toInt();
        }
        for (int i = 0; i < source->childCount(); ++i) {
            const int id = source->child(i)->data(0, VariableIdRole).toInt();
            if (id && mVariableRows.contains(id))
                result.children.append({ref.type, id});
        }
        return result;
    }
    const auto describe = [&](auto* item) {
        if (!item)
            return;
        result.exists = true;
        result.name = item->getName();
        result.folder = item->isFolder();
        result.temporary = item->isTemporary();
        result.enabled = item->shouldBeActive();
        result.ancestorsEnabled = item->ancestorsActive();
        result.valid = item->state();
        result.error = item->getError();
        if (item->getParent())
            result.parent.id = item->getParent()->getID();
        for (auto* child : *item->getChildrenList())
            result.children.append({ref.type, child->getID()});
    };
    if (ref.id == 0) {
        result.exists = true;
        result.folder = true;
        const auto roots = [&](const auto& list) {
            for (auto* item : list)
                result.children.append({ref.type, item->getID()});
        };
        switch (ref.type) {
        case EditorViewType::cmTriggerView:
            roots(host->getTriggerUnit()->getTriggerRootNodeList());
            break;
        case EditorViewType::cmAliasView:
            roots(host->getAliasUnit()->getAliasRootNodeList());
            break;
        case EditorViewType::cmScriptView:
            roots(host->getScriptUnit()->getScriptRootNodeList());
            break;
        case EditorViewType::cmTimerView:
            roots(host->getTimerUnit()->getTimerRootNodeList());
            break;
        case EditorViewType::cmKeysView:
            roots(host->getKeyUnit()->getKeyRootNodeList());
            break;
        case EditorViewType::cmActionView:
            roots(host->getActionUnit()->getActionRootNodeList());
            break;
        default:
            result.exists = false;
        }
        return result;
    }
    switch (ref.type) {
    case EditorViewType::cmTriggerView:
        describe(host->getTriggerUnit()->getTrigger(ref.id));
        break;
    case EditorViewType::cmAliasView:
        describe(host->getAliasUnit()->getAlias(ref.id));
        break;
    case EditorViewType::cmScriptView:
        describe(host->getScriptUnit()->getScript(ref.id));
        break;
    case EditorViewType::cmTimerView:
        describe(host->getTimerUnit()->getTimer(ref.id));
        break;
    case EditorViewType::cmKeysView:
        describe(host->getKeyUnit()->getKey(ref.id));
        break;
    case EditorViewType::cmActionView:
        describe(host->getActionUnit()->getAction(ref.id));
        break;
    default:
        break;
    }
    return result;
}

QString CombinedEditorView::status(ItemRef ref) const
{
    if (ref.type == EditorViewType::cmVarsView) {
        auto* var = variable(ref);
        auto* unit = mpEditor->mpHost->getLuaInterface()->getVarUnit();
        if (!var)
            return {};
        if (!unit->shouldSave(var))
            //: A Lua value that the native Variables editor cannot persist in the profile.
            return tr("Cannot save with profile");
        //: Persistence state of a Lua variable; this is unrelated to item activation.
        return unit->isSaved(var) ? tr("Saved with profile") : tr("Not saved with profile");
    }
    const auto info = data(ref);
    if (!info.valid)
        //: Item state: the original object reports a validation or compilation error.
        return tr("Error");
    if (!info.enabled)
        //: Item state: the object's own enabled flag is off.
        return tr("Disabled");
    if (!info.ancestorsEnabled)
        //: Item state: enabled itself, but prevented from being active by an ancestor.
        return tr("Blocked by parent");
    //: Item state: both the original object and its ancestors are enabled.
    return tr("Enabled");
}

QTreeWidget* CombinedEditorView::originalTree(EditorViewType type) const
{
    switch (type) {
    case EditorViewType::cmTriggerView:
        return mpEditor->treeWidget_triggers;
    case EditorViewType::cmAliasView:
        return mpEditor->treeWidget_aliases;
    case EditorViewType::cmScriptView:
        return mpEditor->treeWidget_scripts;
    case EditorViewType::cmTimerView:
        return mpEditor->treeWidget_timers;
    case EditorViewType::cmKeysView:
        return mpEditor->treeWidget_keys;
    case EditorViewType::cmActionView:
        return mpEditor->treeWidget_actions;
    case EditorViewType::cmVarsView:
        return mpEditor->treeWidget_variables;
    default:
        return nullptr;
    }
}

QTreeWidgetItem* CombinedEditorView::originalItem(ItemRef ref) const
{
    auto* tree = originalTree(ref.type);
    if (!tree)
        return nullptr;
    if (!ref.id)
        return tree->topLevelItem(0);
    if (ref.type == EditorViewType::cmVarsView) {
        const auto index = mVariableRows.value(ref.id);
        return index.isValid() ? tree->itemFromIndex(index) : nullptr;
    }
    for (QTreeWidgetItemIterator it(tree); *it; ++it) {
        if ((*it)->data(0, Qt::UserRole).toInt() == ref.id)
            return *it;
    }
    return nullptr;
}

CombinedEditorView::ItemRef CombinedEditorView::currentEditorItem() const
{
    if (mpEditor->mCurrentView == EditorViewType::cmVarsView && mVariableConflict)
        return {};
    auto* tree = originalTree(mpEditor->mCurrentView);
    if (!tree || !tree->currentItem())
        return {};
    return {mpEditor->mCurrentView, tree->currentItem()->data(0, mpEditor->mCurrentView == EditorViewType::cmVarsView ? VariableIdRole : Qt::UserRole).toInt()};
}

void CombinedEditorView::clearCurrentItems()
{
    mpEditor->mpCurrentTriggerItem = nullptr;
    mpEditor->mpCurrentAliasItem = nullptr;
    mpEditor->mpCurrentScriptItem = nullptr;
    mpEditor->mpCurrentTimerItem = nullptr;
    mpEditor->mpCurrentKeyItem = nullptr;
    mpEditor->mpCurrentActionItem = nullptr;
    mpEditor->mpCurrentVarItem = nullptr;
    mpEditor->mCurrentView = EditorViewType::cmUnknownView;
    mVariableConflict = false;
}

void CombinedEditorView::show()
{
    if (mActive) {
        mpAction->setChecked(true);
        mpTree->setFocus();
        return;
    }
    if (!mActive) {
        mpEditor->saveOpenChanges();
        clearCurrentItems();
        mActive = true;
    }
    mSeparateViewSizes = mpEditor->splitter_main->sizes();
    if (mCombinedViewSizes.isEmpty()) {
        mCombinedViewSizes = mSeparateViewSizes;
        const int left = mpEditor->splitter_main->indexOf(mpEditor->frame_left);
        const int right = mpEditor->splitter_main->indexOf(mpEditor->frame_right);
        const int total = mCombinedViewSizes[left] + mCombinedViewSizes[right];
        const int width = qMax(mCombinedViewSizes[left], qMin(total / 2, mpTree->columnWidth(0) + 190));
        mCombinedViewSizes[left] = width;
        mCombinedViewSizes[right] = total - width;
    }
    mpEditor->splitter_main->setSizes(mCombinedViewSizes);
    mpAction->setChecked(true);
    if (includesVariables())
        refreshVariables();
    refresh();
    showFolder(mpTree->currentItem(), false);
    maintainVisibility();
    mpTree->setFocus();
}

void CombinedEditorView::leave()
{
    mCombinedViewSizes = mpEditor->splitter_main->sizes();
    mpEditor->splitter_main->setSizes(mSeparateViewSizes);
    mActive = false;
    mpAction->setChecked(false);
    mpTree->hide();
    mpVariableControls->hide();
    mpVariableSaved->hide();
    mpSummary->hide();
    mpEditor->mpNonCodeWidgets->show();
    for (auto* action : {mpEditor->mSaveItem, mpEditor->mDeleteItem, mpEditor->mpAction_toggleActive, mpEditor->mpExportAction, mpEditor->mpCreateModuleAction})
        action->setEnabled(true);
}

void CombinedEditorView::viewChanging(EditorViewType type)
{
    if (mActive && (!mRouting || !visibleTypes().contains(type)))
        leave();
}

void CombinedEditorView::maintainVisibility()
{
    if (!mActive)
        return;
    for (auto type : types())
        originalTree(type)->hide();
    mpEditor->treeWidget_variables->hide();
    mpTree->show();
    mpVariableControls->show();
    mpRefreshVariables->setEnabled(includesVariables());
    mpEditor->checkBox_displayAllVariables->setVisible(includesVariables());
    updateActions();
}

bool CombinedEditorView::moveSectionFocus(bool forward)
{
    if (!mActive)
        return false;

    struct Section
    {
        QWidget* area;
        QWidget* target;
    };
    QList<Section> sections;
    const auto appendSection = [&sections](QWidget* area, QWidget* preferred = nullptr) {
        if (!area->isVisible() || !area->isEnabled())
            return;
        if (preferred && preferred->isVisible() && preferred->isEnabled()) {
            sections.append(Section{area, preferred});
            return;
        }
        for (auto* child = area->nextInFocusChain(); child != area; child = child->nextInFocusChain()) {
            if (area->isAncestorOf(child) && child->isVisible() && child->isEnabled() && (child->focusPolicy() & Qt::TabFocus)) {
                sections.append(Section{area, child});
                return;
            }
        }
    };
    appendSection(mpTree, mpTree);
    if (mpSummary->isVisible()) {
        for (auto* button : mpSummary->findChildren<QPushButton*>()) {
            if (button->isVisible() && button->isEnabled()) {
                appendSection(mpSummary, button);
                break;
            }
        }
    } else {
        switch (mpEditor->mCurrentView) {
        case EditorViewType::cmTriggerView:
            appendSection(mpEditor->mpTriggersMainArea, mpEditor->mpTriggersMainArea->lineEdit_trigger_name);
            appendSection(mpEditor->mpTriggersMainArea->scrollArea, mpEditor->mTriggerPatternEdit.isEmpty() ? nullptr : mpEditor->mTriggerPatternEdit.first()->singleLineTextEdit_pattern);
            break;
        case EditorViewType::cmAliasView:
            appendSection(mpEditor->mpAliasMainArea, mpEditor->mpAliasMainArea->lineEdit_alias_name);
            break;
        case EditorViewType::cmScriptView:
            appendSection(mpEditor->mpScriptsMainArea, mpEditor->mpScriptsMainArea->lineEdit_script_name);
            break;
        case EditorViewType::cmTimerView:
            appendSection(mpEditor->mpTimersMainArea, mpEditor->mpTimersMainArea->lineEdit_timer_name);
            break;
        case EditorViewType::cmKeysView:
            appendSection(mpEditor->mpKeysMainArea, mpEditor->mpKeysMainArea->lineEdit_key_name);
            break;
        case EditorViewType::cmActionView:
            appendSection(mpEditor->mpActionsMainArea, mpEditor->mpActionsMainArea->lineEdit_action_name);
            break;
        case EditorViewType::cmVarsView:
            appendSection(mpEditor->mpVarsMainArea, mpEditor->mpVarsMainArea->lineEdit_var_name);
            break;
        case EditorViewType::cmUnknownView:
            break;
        }
        appendSection(mpEditor->mpSourceEditorArea, mpEditor->mpSourceEditorEdbee);
    }
    if (sections.isEmpty())
        return true;

    QWidget* focused = QApplication::focusWidget();
    qsizetype next = forward ? 0 : sections.size() - 1;
    // The pattern section is inside the trigger settings; match the inner section first.
    for (qsizetype i = sections.size() - 1; i >= 0; --i) {
        if (focused && (focused == sections[i].area || sections[i].area->isAncestorOf(focused))) {
            next = (i + (forward ? 1 : sections.size() - 1)) % sections.size();
            break;
        }
    }
    sections[next].target->setFocus(forward ? Qt::TabFocusReason : Qt::BacktabFocusReason);
    return true;
}

void CombinedEditorView::scheduleRefresh()
{
    if (!mActive || mRefreshQueued || mRefreshing)
        return;
    mRefreshQueued = true;
    // Coalesce unit/model signals after the current edit finishes instead of rebuilding mid-slot.
    QTimer::singleShot(0, this, [this]() {
        mRefreshQueued = false;
        if (mActive)
            refresh();
    });
}

bool CombinedEditorView::includesVariables() const
{
    return mpIncludeVariables->isChecked();
}

void CombinedEditorView::indexVariables()
{
    if (!includesVariables())
        return;
    auto* tree = mpEditor->treeWidget_variables;
    auto* unit = mpEditor->mpHost->getLuaInterface()->getVarUnit();
    QSignalBlocker blocker(tree);
    QHash<int, QPersistentModelIndex> rows;
    for (QTreeWidgetItemIterator it(tree); *it; ++it) {
        if (!tree->variableForRow(unit, *it))
            continue;
        int id = (*it)->data(0, VariableIdRole).toInt();
        if (!id) {
            id = ++mNextVariableId;
            (*it)->setData(0, VariableIdRole, id);
        }
        rows.insert(id, tree->indexFromItem(*it));
    }
    // Persistent indexes expire when native rows are deleted; IDs are never recycled into stale menus.
    mVariableRows = rows;
}

TVar* CombinedEditorView::variable(ItemRef ref) const
{
    if (ref.type != EditorViewType::cmVarsView || !ref.id)
        return nullptr;
    return mpEditor->treeWidget_variables->variableForRow(mpEditor->mpHost->getLuaInterface()->getVarUnit(), originalItem(ref));
}

QString CombinedEditorView::variablePath(TVar* var) const
{
    QString path;
    if (!var)
        return path;
    for (auto* level : mpEditor->mpHost->getLuaInterface()->varOrder(var))
        path += qsl("/%1:%2:%3").arg(level->getKeyType()).arg(level->getName().size()).arg(level->getName());
    return path;
}

void CombinedEditorView::recordVariableSearchResult(QTreeWidgetItem* result, TVar* var) const
{
    if (mActive && includesVariables())
        result->setData(0, VariableSearchPathRole, variablePath(var));
}

bool CombinedEditorView::variableSearchMatches(QTreeWidgetItem* result, TVar* var) const
{
    const QString path = result->data(0, VariableSearchPathRole).toString();
    // Native display names alone cannot distinguish t[1] from t["1"].
    return path.isEmpty() || path == variablePath(var);
}

bool CombinedEditorView::watchVariable(TVar* var)
{
    mVariableWatchGeneration = 0;
    if (!var)
        return false;
    auto* luaInterface = mpEditor->mpHost->getLuaInterface();
    auto* state = luaInterface->getState();
    const auto path = luaInterface->varOrder(var);
    if (path.isEmpty() || !lua_checkstack(state, 8))
        return false;
    const int top = lua_gettop(state);
    lua_newtable(state);
    const int watch = top + 1;
    lua_newtable(state);
    lua_pushliteral(state, "v");
    lua_setfield(state, -2, "__mode");
    lua_setmetatable(state, watch);
    lua_pushvalue(state, LUA_GLOBALSINDEX);
    int position = 0;
    for (auto* level : path) {
        if (!lua_istable(state, -1)) {
            lua_settop(state, top);
            return false;
        }
        lua_pushvalue(state, -1);
        lua_rawseti(state, watch, ++position);
        if (!pushVariableKey(state, level)) {
            lua_settop(state, top);
            return false;
        }
        lua_rawget(state, -2);
        lua_remove(state, -2);
        if (lua_type(state, -1) != level->getValueType() || (level->getValueType() == LUA_TTABLE && level->pValue && level->pValue != lua_topointer(state, -1))) {
            lua_settop(state, top);
            return false;
        }
    }
    if (lua_istable(state, -1)) {
        // The native form permits recasting an empty table; stale emptiness must not discard new members.
        lua_pushboolean(state, tableEmpty(state));
        lua_rawseti(state, watch, 0);
    }
    lua_rawseti(state, watch, ++position);
    // Weak values detect collected/replaced tables without keeping game objects alive or trusting reused addresses.
    lua_pushlightuserdata(state, &variableWatchKey);
    lua_pushvalue(state, watch);
    lua_rawset(state, LUA_REGISTRYINDEX);
    lua_settop(state, top);
    mVariableWatchGeneration = luaInterface->getVarUnit()->treeGeneration();
    mVariableWatchLength = position;
    return true;
}

bool CombinedEditorView::variableMatchesWatch() const
{
    auto* luaInterface = mpEditor->mpHost->getLuaInterface();
    if (!mWatchedVariableRow.isValid() || mVariableWatchGeneration != luaInterface->getVarUnit()->treeGeneration())
        return false;
    auto* tree = mpEditor->treeWidget_variables;
    auto* var = tree->variableForRow(luaInterface->getVarUnit(), tree->itemFromIndex(mWatchedVariableRow));
    if (!var)
        return false;
    const auto path = luaInterface->varOrder(var);
    auto* state = luaInterface->getState();
    if (path.size() + 1 != mVariableWatchLength || !lua_checkstack(state, 8))
        return false;
    const int top = lua_gettop(state);
    lua_pushlightuserdata(state, &variableWatchKey);
    lua_rawget(state, LUA_REGISTRYINDEX);
    const int watch = top + 1;
    if (!lua_istable(state, watch)) {
        lua_settop(state, top);
        return false;
    }
    lua_pushvalue(state, LUA_GLOBALSINDEX);
    int position = 0;
    for (auto* level : path) {
        lua_rawgeti(state, watch, ++position);
        const bool sameTable = lua_istable(state, -2) && lua_rawequal(state, -1, -2);
        lua_pop(state, 1);
        if (!sameTable || !pushVariableKey(state, level)) {
            lua_settop(state, top);
            return false;
        }
        // Raw lookup keeps inspecting a variable from invoking __index or other user code.
        lua_rawget(state, -2);
        lua_remove(state, -2);
    }
    lua_rawgeti(state, watch, ++position);
    bool same = lua_rawequal(state, -1, -2);
    lua_pop(state, 1);
    if (same && lua_istable(state, -1)) {
        const bool empty = tableEmpty(state);
        lua_rawgeti(state, watch, 0);
        same = empty == static_cast<bool>(lua_toboolean(state, -1));
    }
    lua_settop(state, top);
    return same;
}

bool CombinedEditorView::variableDraftChanged() const
{
    auto* form = mpEditor->mpVarsMainArea;
    return mVariableName != form->lineEdit_var_name->text() || mVariableValue != mpEditor->mpSourceEditorEdbeeDocument->text()
           || mVariableKeyType != form->comboBox_variable_key_type->currentData().toInt() || mVariableValueType != form->comboBox_variable_value_type->currentData().toInt();
}

void CombinedEditorView::variableSelected()
{
    if (!mActive || !includesVariables() || !mpEditor->mpCurrentVarItem)
        return;
    auto* tree = mpEditor->treeWidget_variables;
    auto* var = tree->variableForRow(mpEditor->mpHost->getLuaInterface()->getVarUnit(), mpEditor->mpCurrentVarItem);
    if (!var)
        return;
    indexVariables();
    mWatchedVariableRow = tree->indexFromItem(mpEditor->mpCurrentVarItem);
    mVariableConflict = false;
    if (!watchVariable(var)) {
        rejectVariableEdit();
        return;
    }
    auto* form = mpEditor->mpVarsMainArea;
    mVariableName = form->lineEdit_var_name->text();
    mVariableValue = mpEditor->mpSourceEditorEdbeeDocument->text();
    mVariableKeyType = form->comboBox_variable_key_type->currentData().toInt();
    mVariableValueType = form->comboBox_variable_value_type->currentData().toInt();
    updateActions();
    scheduleRefresh();
}

void CombinedEditorView::rejectVariableEdit()
{
    mVariableConflict = true;
    mpEditor->mpCurrentVarItem = nullptr;
    //: A Lua value or one of its containing tables changed externally while its editor was open.
    mpEditor->showWarning(tr("This variable changed, was replaced, or is no longer available in this snapshot. Your draft has not been written and is still available to copy. "
                             "Refresh Variables, then select the variable again to load its current value. Copy your draft before leaving it."));
    updateActions();
}

bool CombinedEditorView::variableSaveAllowed()
{
    if (!mActive || !includesVariables() || mAddingVariable)
        return true;
    if (mVariableConflict)
        return false;
    if (!variableMatchesWatch()) {
        rejectVariableEdit();
        return false;
    }
    return true;
}

bool CombinedEditorView::variableNavigationAllowed()
{
    if (!mActive || mpEditor->mCurrentView != EditorViewType::cmVarsView || !mpEditor->mpCurrentVarItem)
        return true;
    return variableSaveAllowed();
}

void CombinedEditorView::updateVariableControls()
{
    const bool editing = mActive && includesVariables() && mpEditor->mCurrentView == EditorViewType::cmVarsView && !mpSummary->isVisible();
    mpVariableSaved->setVisible(editing);
    if (!editing)
        return;
    auto* unit = mpEditor->mpHost->getLuaInterface()->getVarUnit();
    auto* var = variable(currentEditorItem());
    QSignalBlocker blocker(mpVariableSaved);
    mpVariableSaved->setChecked(var && unit->isSaved(var));
    mpVariableSaved->setEnabled(var && !mVariableConflict && unit->shouldSave(var));
    mpVariableSaved->setToolTip(var ? unit->getUnsaveableReason(var) : QString());
}

void CombinedEditorView::setVariableSaved(bool saved)
{
    if (!variableSaveAllowed()) {
        updateVariableControls();
        return;
    }
    auto* source = originalItem(currentEditorItem());
    if (!source)
        return;
    source->setCheckState(0, saved ? Qt::Checked : Qt::Unchecked);
    mpEditor->slot_variableChanged(source);
    refresh();
}

void CombinedEditorView::refreshVariables()
{
    if (!mActive || !includesVariables() || mReadingVariables)
        return;
    auto* tree = mpEditor->treeWidget_variables;
    auto* unit = mpEditor->mpHost->getLuaInterface()->getVarUnit();
    const bool editing = mpEditor->mCurrentView == EditorViewType::cmVarsView && !mpSummary->isVisible();
    auto* oldVar = mWatchedVariableRow.isValid() ? tree->variableForRow(unit, tree->itemFromIndex(mWatchedVariableRow)) : nullptr;
    const QString path = variablePath(oldVar);
    const bool draft = editing && (mVariableConflict || variableDraftChanged());
    const bool unchanged = editing && !mVariableConflict && variableMatchesWatch();
    QScopedValueRollback reading(mReadingVariables, true);
    QSignalBlocker blocker(tree);
    QSignalBlocker selectionBlocker(tree->selectionModel());
    mpEditor->repopulateVars();
    indexVariables();
    QTreeWidgetItem* source = nullptr;
    for (auto it = mVariableRows.cbegin(); it != mVariableRows.cend(); ++it) {
        if (!path.isEmpty() && variablePath(variable({EditorViewType::cmVarsView, it.key()})) == path) {
            source = tree->itemFromIndex(it.value());
            break;
        }
    }
    if (editing) {
        if (draft && (!unchanged || !source)) {
            rejectVariableEdit();
        } else if (source) {
            tree->setCurrentItem(source);
            mpEditor->mpCurrentVarItem = source;
            if (draft) {
                mWatchedVariableRow = tree->indexFromItem(source);
                if (!watchVariable(tree->variableForRow(mpEditor->mpHost->getLuaInterface()->getVarUnit(), source)))
                    rejectVariableEdit();
            } else {
                mpEditor->slot_variableSelected(source);
            }
        } else {
            rejectVariableEdit();
        }
    }
    refresh();
    maintainVisibility();
}

void CombinedEditorView::appendChildren(QTreeWidgetItem* parent, const QList<ItemRef>& sources, const QString& path)
{
    QHash<QString, QTreeWidgetItem*> folders;
    QList<ItemRef> children;
    for (auto type : visibleTypes()) {
        for (const auto& source : sources) {
            if (source.type == type)
                children.append(data(source).children);
        }
    }
    for (const auto& ref : children) {
        const auto info = data(ref);
        if (!info.exists || info.temporary || info.hidden)
            continue;
        const QString mergeKey = info.mergeable ? qsl("name/") + info.name : qsl("variable/%1").arg(ref.id);
        auto* row = info.folder ? folders.value(mergeKey, nullptr) : nullptr;
        if (!row) {
            row = new QTreeWidgetItem(QStringList{info.name, typeName(ref.type), status(ref)});
            row->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
            row->setData(0, Qt::UserRole, ref.id);
            row->setData(0, TypeRole, static_cast<int>(ref.type));
            row->setData(0, FolderRole, info.folder);
            // Length prefixes distinguish literal slashes in names from hierarchy separators.
            row->setData(0, PathRole, info.folder ? path + qsl("/%1:%2").arg(info.name.size()).arg(info.name) : qsl("item/%1/%2").arg(static_cast<int>(ref.type)).arg(ref.id));
            if (ref.type == EditorViewType::cmVarsView && (!info.folder || !info.mergeable))
                row->setData(0, PathRole, qsl("variable/") + variablePath(variable(ref)));
            if (info.folder) {
                bool stablePath = true;
                if (ref.type == EditorViewType::cmVarsView) {
                    // Reference keys use temporary Lua registry handles, which cannot survive a restart.
                    for (auto* key : mpEditor->mpHost->getLuaInterface()->varOrder(variable(ref))) {
                        if (key->isReference()) {
                            stablePath = false;
                            break;
                        }
                    }
                }
                if (stablePath) {
                    row->setData(0, ExpansionPathRole, row->data(0, PathRole));
                }
                // Move only display rows ahead of leaves; unit sibling order controls execution.
                parent->insertChild(folders.size(), row);
                folders.insert(mergeKey, row);
            } else {
                parent->addChild(row);
            }
        }
        mMembers[row].append(ref);
    }
    for (int i = 0; i < parent->childCount(); ++i) {
        auto* row = parent->child(i);
        const auto members = mMembers.value(row);
        QStringList descriptions;
        QSet<QString> states;
        QString variableState;
        for (const auto& ref : members) {
            const auto info = data(ref);
            const QString state = status(ref);
            if (ref.type == EditorViewType::cmVarsView) {
                variableState = state;
                descriptions.append((info.folder ? folderName(ref) : typeName(ref.type)) + qsl(": ") + state);
                if (auto* var = variable(ref))
                    descriptions.append(mpEditor->mpHost->getLuaInterface()->getVarUnit()->getUnsaveableReason(var));
                continue;
            }
            states.insert(state);
            //: %1 is an item or folder type, %2 its profile-local ID, %3 its enabled/error state.
            descriptions.append(tr("%1 #%2: %3").arg(info.folder ? folderName(ref) : typeName(ref.type)).arg(ref.id).arg(state));
            if (!info.valid)
                descriptions.append(info.error);
        }
        if (row->data(0, FolderRole).toBool()) {
            row->setIcon(0, members.size() == 1 && members.first().type == EditorViewType::cmVarsView ? QIcon(qsl(":/icons/table.png")) : mFolderIcon);
            if (members.size() > 1) {
                //: Number of original folders, or folders and a Lua table, consolidated into one display row.
                row->setText(1, variableState.isEmpty() ? tr("%n folders", nullptr, members.size()) : tr("%n sources", nullptr, members.size()));
            } else if (members.first().type == EditorViewType::cmActionView || members.first().type == EditorViewType::cmVarsView) {
                row->setText(1, folderName(members.first()));
            }
            //: State label when the represented original folders have different states.
            QString state = states.isEmpty() ? variableState : states.size() == 1 ? *states.cbegin() : tr("Mixed");
            if (!states.isEmpty() && !variableState.isEmpty()) {
                //: Activation of editor folders followed by the independent persistence state of a Lua table.
                state = tr("%1; Lua: %2").arg(state, variableState);
            }
            row->setText(2, state);
        } else {
            row->setIcon(0, mTypeIcons.value(members.first().type));
        }
        row->setToolTip(0, descriptions.join(QChar('\n')));
        row->setData(0, Qt::AccessibleDescriptionRole, descriptions.join(qsl("; ")));
        appendChildren(row, members, row->data(0, PathRole).toString());
    }
}

void CombinedEditorView::refresh()
{
    if (!mActive || mRefreshing)
        return;
    QScopedValueRollback refreshing(mRefreshing, true);
    indexVariables();
    // Rebuilding the projection must not select an editor, save a draft or execute script code.
    QSignalBlocker blocker(mpTree);
    const auto selected = mMembers.value(mpTree->currentItem());
    const QString selectedPath = mpTree->currentItem() ? mpTree->currentItem()->data(0, PathRole).toString() : QString();
    const auto editing = currentEditorItem();
    const bool firstBuild = mpTree->topLevelItemCount() == 0;
    rememberExpandedFolders();
    QSet<QString> expanded;
    QSet<QPair<EditorViewType, int>> expandedMembers;
    for (QTreeWidgetItemIterator it(mpTree); *it; ++it) {
        if ((*it)->isExpanded()) {
            expanded.insert((*it)->data(0, PathRole).toString());
            for (auto ref : mMembers.value(*it)) {
                if (ref.id)
                    expandedMembers.insert({ref.type, ref.id});
            }
        }
    }
    const int scroll = mpTree->verticalScrollBar()->value();
    mpTree->clear();
    mMembers.clear();
    auto* root = new QTreeWidgetItem(mpTree, {tr("Combined")});
    root->setData(0, FolderRole, true);
    root->setData(0, PathRole, qsl("root"));
    root->setData(0, ExpansionPathRole, qsl("root"));
    root->setIcon(0, mFolderIcon);
    QList<ItemRef> roots;
    for (auto type : visibleTypes())
        roots.append({type, 0});
    mMembers.insert(root, roots);
    appendChildren(root, roots, qsl("root"));
    QTreeWidgetItem* selection = root;
    QTreeWidgetItem* editingRow = nullptr;
    for (QTreeWidgetItemIterator it(mpTree); *it; ++it) {
        auto* row = *it;
        const auto members = mMembers.value(row);
        const bool wasExpanded = std::any_of(members.cbegin(), members.cend(), [&](ItemRef ref) {
            return expandedMembers.contains({ref.type, ref.id});
        });
        row->setExpanded((row == root && firstBuild && !mHasExpansionState) || wasExpanded || expanded.contains(row->data(0, PathRole).toString())
                         || mExpandedFolders.contains(row->data(0, ExpansionPathRole).toString()));
        if (row->data(0, PathRole).toString() == selectedPath)
            selection = row;
        if (editing.id && members.contains(editing))
            editingRow = row;
        if (row != root && std::any_of(selected.cbegin(), selected.cend(), [&](ItemRef ref) {
                return members.contains(ref);
            }))
            selection = row;
    }
    // A renamed merged folder can split; follow the edited original rather than a sibling member.
    if (editingRow)
        selection = editingRow;
    mpTree->setCurrentItem(selection);
    mpTree->verticalScrollBar()->setValue(scroll);
    if (mpSummary->isVisible())
        showFolder(selection, false);
    if (editing.id && !data(editing).exists) {
        clearCurrentItems();
        //: The source editor retains an unsaved draft after its original object was deleted externally.
        mpEditor->showWarning(tr("The item being edited was removed. Its text is still available to copy."));
    }
    maintainVisibility();
}

bool CombinedEditorView::folderSelected() const
{
    return mActive && (!mpTree->currentItem() || mpTree->currentItem()->data(0, FolderRole).toBool());
}

void CombinedEditorView::updateActions()
{
    const bool editable = mpEditor->mCurrentView != EditorViewType::cmUnknownView;
    for (auto* action : {mpEditor->mSaveItem, mpEditor->mDeleteItem, mpEditor->mpAction_toggleActive, mpEditor->mpExportAction, mpEditor->mpCreateModuleAction})
        action->setEnabled(editable);
    if (mpEditor->mCurrentView == EditorViewType::cmVarsView) {
        mpEditor->mpAction_toggleActive->setEnabled(false);
        mpEditor->mpExportAction->setEnabled(false);
        mpEditor->mpCreateModuleAction->setEnabled(false);
        auto* var = variable(currentEditorItem());
        mpEditor->mSaveItem->setEnabled(!mVariableConflict && var && var->getValueType() != LUA_TFUNCTION);
        mpEditor->mDeleteItem->setEnabled(!mVariableConflict && var);
    }
    if (!editable) {
        mpEditor->mSaveItem->setText(tr("Save Item"));
        mpEditor->mDeleteItem->setText(tr("Delete Item"));
    }
    mpEditor->mAddItem->setText(tr("Add Item"));
    mpEditor->mAddGroup->setText(tr("Add Group"));
    updateVariableControls();
    mpEditor->slot_updateUndoRedoButtonStates();
}

void CombinedEditorView::showFolder(QTreeWidgetItem* row, bool saveChanges)
{
    if (saveChanges) {
        if (!variableNavigationAllowed())
            return;
        mpEditor->saveOpenChanges();
        clearCurrentItems();
    }
    while (auto* item = mpSummaryLayout->takeAt(0)) {
        delete item->widget();
        delete item;
    }
    auto* title = new QLabel(row ? row->text(0) : tr("Combined"), mpSummary);
    title->setTextFormat(Qt::PlainText);
    mpSummaryLayout->addWidget(title);
    const bool rootSelected = !row || !row->parent();
    //: Instructions above the combined tree's folder chooser or its root overview.
    auto* help = new QLabel(rootSelected ? tr("Select an item to edit it. Use Add Item or right-click to choose its type and original folder.")
                                         : tr("Choose an original folder or Lua table to edit its settings. Right-click for actions on that original."),
                            mpSummary);
    help->setWordWrap(true);
    mpSummaryLayout->addWidget(help);
    QHash<int, int> ordinals;
    for (auto ref : mMembers.value(row)) {
        if (!ref.id)
            continue;
        const int ordinal = ++ordinals[static_cast<int>(ref.type)];
        //: %1 is the folder type/role, %2 distinguishes same-type originals, %3 is their enabled/error state.
        auto* button = new QPushButton(tr("%1 %2 — %3").arg(folderName(ref)).arg(ordinal).arg(status(ref)), mpSummary);
        //: Tooltip; %1 is the original folder's numeric ID within its category.
        button->setToolTip(ref.type == EditorViewType::cmVarsView ? data(ref).name : tr("Edit original folder #%1").arg(ref.id));
        connect(button, &QPushButton::clicked, this, [this, ref]() {
            editItem(ref);
        });
        mpSummaryLayout->addWidget(button);
    }
    mpSummaryLayout->addStretch();
    mpEditor->mpNonCodeWidgets->hide();
    mpEditor->mpSourceEditorArea->hide();
    mpSummary->show();
    updateActions();
}

void CombinedEditorView::selectRow(QTreeWidgetItem* row)
{
    if (!mActive || mRefreshing || !row)
        return;
    if (row->data(0, FolderRole).toBool())
        showFolder(row, true);
    else if (!mMembers.value(row).isEmpty())
        editItem(mMembers.value(row).first());
    // Native selection handlers focus the code pane; arrow navigation must stay in this tree.
    mpTree->setFocus(Qt::OtherFocusReason);
}

bool CombinedEditorView::editItem(ItemRef ref, bool originalView)
{
    if (!data(ref).exists) {
        scheduleRefresh();
        return false;
    }
    if (!variableNavigationAllowed())
        return false;
    QScopedValueRollback routing(mRouting, true);
    if (currentEditorItem() == ref && mpEditor->mCurrentView == ref.type && !mpSummary->isVisible() && !mpEditor->mNeedUpdateData) {
        if (originalView) {
            leave();
            originalTree(ref.type)->show();
        }
        return true;
    }
    // Save against the old type/ID before switching forms; rebuilding invalidates native row pointers.
    mpEditor->saveOpenChanges();
    clearCurrentItems();
    if (mpEditor->mNeedUpdateData && ref.type != EditorViewType::cmVarsView)
        mpEditor->rebuildStaleTrees();
    clearCurrentItems();
    auto* source = originalItem(ref);
    if (!source || !data(ref).exists)
        return false;
    if (originalView)
        leave();
    mpSummary->hide();
    mpEditor->mpNonCodeWidgets->show();
    mpEditor->changeView(ref.type);
    auto* tree = originalTree(ref.type);
    // Invoke the native selection handler once, after both current item and category agree.
    QSignalBlocker blocker(tree);
    QSignalBlocker selectionBlocker(tree->selectionModel());
    tree->clearSelection();
    tree->setCurrentItem(source);
    switch (ref.type) {
    case EditorViewType::cmTriggerView:
        mpEditor->slot_triggerSelected(source);
        break;
    case EditorViewType::cmAliasView:
        mpEditor->slot_aliasSelected(source);
        break;
    case EditorViewType::cmScriptView:
        mpEditor->slot_scriptsSelected(source);
        break;
    case EditorViewType::cmTimerView:
        mpEditor->slot_timerSelected(source);
        break;
    case EditorViewType::cmKeysView:
        mpEditor->slot_keySelected(source);
        break;
    case EditorViewType::cmActionView:
        mpEditor->slot_actionSelected(source);
        break;
    case EditorViewType::cmVarsView:
        if (ref.id && !watchVariable(variable(ref))) {
            rejectVariableEdit();
            return false;
        }
        mpEditor->slot_variableSelected(source);
        break;
    default:
        return false;
    }
    tree->scrollToItem(source);
    maintainVisibility();
    return true;
}

void CombinedEditorView::syncSelection()
{
    if (!mActive)
        return;
    indexVariables();
    const auto selected = currentEditorItem();
    if (selected.id)
        mpSummary->hide();
    refresh();
    QSignalBlocker blocker(mpTree);
    for (QTreeWidgetItemIterator it(mpTree); *it; ++it) {
        if (mMembers.value(*it).contains(selected)) {
            mpTree->setCurrentItem(*it);
            mpTree->scrollToItem(*it);
            break;
        }
    }
    if (selected.id) {
        mpSummary->hide();
        mpEditor->mpNonCodeWidgets->show();
    } else {
        clearCurrentItems();
        showFolder(mpTree->currentItem(), false);
    }
    maintainVisibility();
}

QMenu* CombinedEditorView::createMenu(QTreeWidgetItem* row, bool addOnly, bool folder)
{
    auto* menu = new QMenu(mpTree);
    QList<ItemRef> members = mMembers.value(row);
    if (!row) {
        for (auto type : visibleTypes())
            members.append({type, 0});
    }
    QHash<int, int> ordinals;
    for (auto ref : members) {
        const auto info = data(ref);
        if (!info.exists)
            continue;
        const int ordinal = ++ordinals[static_cast<int>(ref.type)];
        const ItemRef parent = info.folder ? ref : info.parent;
        //: A submenu for one original folder; %1 is its type/role, %2 distinguishes same-type duplicates.
        const QString label = ref.id ? tr("%1 %2 — %3").arg(folderName(ref)).arg(ordinal).arg(status(ref)) : typeName(ref.type);
        QMenu* target = members.size() > 1 || info.folder ? menu->addMenu(label) : menu;
        target->setToolTipsVisible(true);
        const auto action = [&](const QString& text, const QString& operation) {
            auto* entry = target->addAction(text);
            entry->setData(QVariantList{static_cast<int>(ref.type), ref.id, operation});
            //: Tooltip; %1 is the original object's numeric ID within its category.
            entry->setToolTip(tr("Original item #%1").arg(ref.id));
            if (operation == qsl("add") && createsToolbar(parent)) {
                entry->setEnabled(false);
                //: A button must be added to an existing toolbar or menu, not directly to a package or the root.
                entry->setToolTip(tr("Choose a toolbar or menu to add a button."));
            }
            // A queued refresh can replace every row while the menu is open; resolve the ID on use.
            connect(entry, &QAction::triggered, this, [this, ref, operation]() {
                runFolderAction(ref, operation);
            });
        };
        if (addOnly) {
            auto* entry = target->addAction(addLabel(parent, folder));
            if (!folder && createsToolbar(parent)) {
                entry->setEnabled(false);
                //: A button must be added to an existing toolbar or menu, not directly to a package or the root.
                entry->setToolTip(tr("Choose a toolbar or menu to add a button."));
            }
            entry->setData(QVariantList{static_cast<int>(parent.type), parent.id, folder ? qsl("addGroup") : qsl("add")});
            connect(entry, &QAction::triggered, this, [this, parent, folder]() {
                addItem(parent, folder);
            });
            continue;
        }
        if (ref.id) {
            //: Opens the native value editor for Variables or code editor for other categories.
            action(ref.type == EditorViewType::cmVarsView ? tr("Edit Value and Settings") : tr("Edit Code and Settings"), qsl("edit"));
            action(tr("Open Original Folder or Item"), qsl("original"));
            target->addSeparator();
        }
        if (ref.type != EditorViewType::cmVarsView)
            action(tr("Paste"), qsl("paste"));
        action(addLabel(parent, false), qsl("add"));
        action(addLabel(parent, true), qsl("addGroup"));
        if (ref.id) {
            target->addSeparator();
            if (ref.type == EditorViewType::cmVarsView) {
                action(tr("Delete"), qsl("delete"));
                continue;
            }
            action(info.enabled ? tr("Disable") : tr("Enable"), qsl("toggle"));
            action(tr("Copy"), qsl("copy"));

            action(tr("Export"), qsl("export"));
            action(tr("Create Module"), qsl("module"));
            action(tr("Delete"), qsl("delete"));
        }
    }
    if (row && row->data(0, FolderRole).toBool())
        appendMissingParents(menu, row, folder);
    return menu;
}

void CombinedEditorView::runFolderAction(ItemRef ref, const QString& operation)
{
    if (!data(ref).exists) {
        scheduleRefresh();
        return;
    }
    if (operation == qsl("original")) {
        editItem(ref, true);
        return;
    }
    if (operation == qsl("add") || operation == qsl("addGroup")) {
        const auto info = data(ref);
        addItem(info.folder ? ref : info.parent, operation == qsl("addGroup"));
        return;
    }
    if (!editItem(ref))
        return;
    if (ref.type == EditorViewType::cmVarsView && operation != qsl("edit") && !variableSaveAllowed())
        return;
    NavigationGuard navigation(this);
    if (operation == qsl("toggle"))
        mpEditor->slot_toggleItemOrGroupActiveFlag();
    else if (operation == qsl("delete"))
        mpEditor->slot_deleteItemOrGroup();
    else if (operation == qsl("copy"))
        mpEditor->slot_copyXml();
    else if (operation == qsl("paste"))
        mpEditor->slot_pasteXml();
    else if (operation == qsl("export"))
        mpEditor->slot_export();
    else if (operation == qsl("module"))
        mpEditor->slot_createModule();
}

bool CombinedEditorView::addRequested(bool folder)
{
    if (!mActive || mRouting)
        return false;
    auto* menu = createMenu(mpTree->currentItem(), true, folder);
    menu->setAttribute(Qt::WA_DeleteOnClose);
    menu->popup(mpTree->viewport()->mapToGlobal(mpTree->visualItemRect(mpTree->currentItem()).bottomLeft()));
    return true;
}

bool CombinedEditorView::folderActionRequested()
{
    if (!mActive || mRouting || !folderSelected())
        return false;
    auto* menu = createMenu(mpTree->currentItem());
    menu->setAttribute(Qt::WA_DeleteOnClose);
    menu->popup(mpTree->viewport()->mapToGlobal(mpTree->visualItemRect(mpTree->currentItem()).bottomLeft()));
    return true;
}

void CombinedEditorView::addItem(ItemRef parent, bool folder, const QString& name)
{
    if (!folder && createsToolbar(parent)) {
        return;
    }
    if (!data(parent).exists || !data(parent).folder || !editItem(parent))
        return;
    if (parent.type == EditorViewType::cmVarsView && parent.id && !variableSaveAllowed())
        return;
    NavigationGuard navigation(this);
    QScopedValueRollback addingVariable(mAddingVariable, parent.type == EditorViewType::cmVarsView);
    switch (parent.type) {
    case EditorViewType::cmTriggerView:
        mpEditor->addTrigger(folder);
        break;
    case EditorViewType::cmAliasView:
        mpEditor->addAlias(folder);
        break;
    case EditorViewType::cmScriptView:
        mpEditor->addScript(folder);
        break;
    case EditorViewType::cmTimerView:
        mpEditor->addTimer(folder);
        break;
    case EditorViewType::cmKeysView:
        mpEditor->addKey(folder);
        break;
    case EditorViewType::cmActionView:
        mpEditor->addAction(folder);
        break;
    case EditorViewType::cmVarsView:
        mpEditor->addVar(folder);
        indexVariables();
        variableSelected();
        break;
    default:
        return;
    }
    // An empty folder name is valid; only a null string means keep the native default name.
    if (!name.isNull()) {
        switch (parent.type) {
        case EditorViewType::cmTriggerView:
            mpEditor->mpTriggersMainArea->lineEdit_trigger_name->setText(name);
            mpEditor->slot_saveProperty_TriggerName();
            break;
        case EditorViewType::cmAliasView:
            mpEditor->mpAliasMainArea->lineEdit_alias_name->setText(name);
            mpEditor->slot_saveProperty_AliasName();
            break;
        case EditorViewType::cmScriptView:
            mpEditor->mpScriptsMainArea->lineEdit_script_name->setText(name);
            mpEditor->slot_saveProperty_ScriptName();
            break;
        case EditorViewType::cmTimerView:
            mpEditor->mpTimersMainArea->lineEdit_timer_name->setText(name);
            mpEditor->slot_saveProperty_TimerName();
            break;
        case EditorViewType::cmKeysView:
            mpEditor->mpKeysMainArea->lineEdit_key_name->setText(name);
            mpEditor->slot_saveProperty_KeyName();
            break;
        case EditorViewType::cmActionView:
            mpEditor->mpActionsMainArea->lineEdit_action_name->setText(name);
            mpEditor->slot_saveProperty_ActionName();
            break;
        default:
            break;
        }
    }
}

void CombinedEditorView::appendMissingParents(QMenu* menu, QTreeWidgetItem* row, bool folder)
{
    for (auto* ancestor = row; ancestor; ancestor = ancestor->parent()) {
        for (auto ref : mMembers.value(ancestor)) {
            if (!data(ref).mergeable)
                return;
        }
    }
    for (auto type : visibleTypes()) {
        // Missing Lua tables would change live data; this shortcut only constructs editor folders.
        if (type == EditorViewType::cmVarsView)
            continue;
        const auto represented = mMembers.value(row);
        if (std::any_of(represented.cbegin(), represented.cend(), [type](ItemRef ref) {
                return ref.type == type;
            }))
            continue;
        QStringList path;
        auto* ancestor = row;
        QList<ItemRef> destinations;
        while (ancestor && ancestor->data(0, FolderRole).toBool()) {
            for (auto ref : mMembers.value(ancestor))
                if (ref.type == type)
                    destinations.append(ref);
            if (!destinations.isEmpty())
                break;
            path.prepend(ancestor->text(0));
            ancestor = ancestor->parent();
        }
        if (destinations.isEmpty())
            continue;
        const bool newToolbar = std::any_of(destinations.cbegin(), destinations.cend(), [this](ItemRef ref) {
            return createsToolbar(ref);
        });
        //: Explicitly creates missing folders in one category, then adds an item beneath them. Button roots create real toolbars.
        auto* missing = menu->addMenu(newToolbar ? tr("Create matching Button toolbar and menus and add…") : tr("Create matching %1 folders and add…").arg(typeName(type)));
        for (int i = 0; i < destinations.size(); ++i) {
            const auto destination = destinations.at(i);
            //: Destination for a missing hierarchy; %1 identifies the original folder type/role.
            QString label = destination.id ? tr("Under %1 %2").arg(folderName(destination)).arg(i + 1) : tr("At %1 root").arg(typeName(type));
            if (createsToolbar(destination)) {
                //: Creating this matching Button hierarchy creates a real toolbar; %1 is its name, %2 identifies a package container.
                label = destination.id ? tr("Create toolbar “%1” under Button package %2").arg(path.first()).arg(i + 1) : tr("Create toolbar “%1” at Button root").arg(path.first());
            }
            auto* entry = missing->addAction(label);
            entry->setToolTip(path.join(qsl(" / ")));
            connect(entry, &QAction::triggered, this, [this, destination, path, folder]() {
                createHierarchyAndAdd(destination, path, folder);
            });
        }
    }
}

void CombinedEditorView::createHierarchyAndAdd(ItemRef ancestor, const QStringList& path, bool folder)
{
    if (ancestor.type == EditorViewType::cmVarsView && !path.isEmpty())
        return;
    if (path.isEmpty()) {
        addItem(ancestor, folder);
        return;
    }
    if (!data(ancestor).exists || !editItem(ancestor))
        return;
    NavigationGuard navigation(this);
    const ItemRef originalParent = ancestor;
    ItemRef firstCreated;
    {
        // One add command snapshots the entire new subtree; separate adds lose parent IDs on redo.
        EditorUndoStack constructionHistory;
        QScopedValueRollback history(mpEditor->mpUndoStack, &constructionHistory);
        for (const auto& name : path) {
            addItem(ancestor, true, name);
            const auto created = currentEditorItem();
            if (created == ancestor || !data(created).exists)
                break;
            if (!firstCreated.id)
                firstCreated = created;
            ancestor = created;
        }
        if (firstCreated.id)
            addItem(ancestor, folder);
    }
    if (firstCreated.id) {
        auto* command = new EditorAddItemCommand(
                firstCreated.type, firstCreated.id, originalParent.id ? originalParent.id : -1, data(originalParent).children.indexOf(firstCreated), true, path.first(), mpEditor->mpHost);
        //: Undo/redo history entry for creating a missing typed folder hierarchy and its item together.
        command->setText(tr("Create matching folders and item"));
        mpEditor->mpUndoStack->pushCommand(command);
    }
}
