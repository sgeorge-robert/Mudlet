#include "CombinedEditorView.h"

#include "Host.h"
#include "EditorAddItemCommand.h"
#include "TTreeWidget.h"
#include "dlgAliasMainArea.h"
#include "dlgScriptsMainArea.h"
#include "dlgTriggerEditor.h"

#include <QAction>
#include <QHeaderView>
#include <QLabel>
#include <QMenu>
#include <QPainter>
#include <QPushButton>
#include <QScopedValueRollback>
#include <QScrollBar>
#include <QSet>
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
}

QList<EditorViewType> CombinedEditorView::types()
{
    return {EditorViewType::cmTriggerView, EditorViewType::cmAliasView, EditorViewType::cmScriptView, EditorViewType::cmTimerView};
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
    default:
        return {};
    }
}

CombinedEditorView::CombinedEditorView(dlgTriggerEditor* editor)
: QObject(editor)
, mpEditor(editor)
, mpTree(new QTreeWidget(editor->treeWidget_triggers->parentWidget()))
, mpSummary(new QWidget(editor->splitter_right))
, mpSummaryLayout(new QVBoxLayout(mpSummary))
, mpAction(new QAction(tr("Combined"), this))
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
                  {EditorViewType::cmTimerView, QIcon(qsl(":/icons/chronometer.png"))}};
    mpAction->setIcon(mFolderIcon);
    mpAction->setObjectName(qsl("action_combinedEditor"));
    mpAction->setCheckable(true);
    mpAction->setToolTip(tr("View triggers, aliases, scripts and timers together"));
    editor->toolBar2->insertAction(editor->toolBar2->actions().first(), mpAction);
    connect(mpAction, &QAction::triggered, this, &CombinedEditorView::show);

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
    connect(editor->mpHost, &Host::signal_triggerToggled, this, &CombinedEditorView::scheduleRefresh);
    connect(editor->mpHost, &Host::signal_aliasToggled, this, &CombinedEditorView::scheduleRefresh);
    connect(editor->mpHost, &Host::signal_scriptToggled, this, &CombinedEditorView::scheduleRefresh);
    connect(editor->mpHost, &Host::signal_timerToggled, this, &CombinedEditorView::scheduleRefresh);
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
    default:
        break;
    }
    return result;
}

QString CombinedEditorView::status(ItemRef ref) const
{
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
    for (QTreeWidgetItemIterator it(tree); *it; ++it) {
        if ((*it)->data(0, Qt::UserRole).toInt() == ref.id)
            return *it;
    }
    return nullptr;
}

CombinedEditorView::ItemRef CombinedEditorView::currentEditorItem() const
{
    auto* tree = originalTree(mpEditor->mCurrentView);
    if (!tree || !tree->currentItem())
        return {};
    return {mpEditor->mCurrentView, tree->currentItem()->data(0, Qt::UserRole).toInt()};
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
    mpSummary->hide();
    mpEditor->mpNonCodeWidgets->show();
    for (auto* action : {mpEditor->mSaveItem, mpEditor->mDeleteItem, mpEditor->mpAction_toggleActive, mpEditor->mpExportAction, mpEditor->mpCreateModuleAction})
        action->setEnabled(true);
}

void CombinedEditorView::viewChanging(EditorViewType type)
{
    if (mActive && (!mRouting || !types().contains(type)))
        leave();
}

void CombinedEditorView::maintainVisibility()
{
    if (!mActive)
        return;
    for (auto type : types())
        originalTree(type)->hide();
    mpEditor->treeWidget_keys->hide();
    mpEditor->treeWidget_actions->hide();
    mpEditor->treeWidget_variables->hide();
    mpTree->show();
    updateActions();
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

void CombinedEditorView::appendChildren(QTreeWidgetItem* parent, const QList<ItemRef>& sources, const QString& path)
{
    QHash<QString, QTreeWidgetItem*> folders;
    QList<ItemRef> children;
    for (auto type : types()) {
        for (const auto& source : sources) {
            if (source.type == type)
                children.append(data(source).children);
        }
    }
    for (const auto& ref : children) {
        const auto info = data(ref);
        if (!info.exists || info.temporary)
            continue;
        auto* row = info.folder ? folders.value(info.name, nullptr) : nullptr;
        if (!row) {
            row = new QTreeWidgetItem(QStringList{info.name, typeName(ref.type), status(ref)});
            row->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
            row->setData(0, Qt::UserRole, ref.id);
            row->setData(0, TypeRole, static_cast<int>(ref.type));
            row->setData(0, FolderRole, info.folder);
            // Length prefixes distinguish literal slashes in names from hierarchy separators.
            row->setData(0, PathRole, info.folder ? path + qsl("/%1:%2").arg(info.name.size()).arg(info.name) : qsl("item/%1/%2").arg(static_cast<int>(ref.type)).arg(ref.id));
            if (info.folder) {
                // Move only display rows ahead of leaves; unit sibling order controls execution.
                parent->insertChild(folders.size(), row);
                folders.insert(info.name, row);
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
        for (const auto& ref : members) {
            const auto info = data(ref);
            const QString state = status(ref);
            states.insert(state);
            //: %1 is an item type, %2 its profile-local ID, %3 its enabled/error state.
            descriptions.append(tr("%1 #%2: %3").arg(typeName(ref.type)).arg(ref.id).arg(state));
            if (!info.valid)
                descriptions.append(info.error);
        }
        if (row->data(0, FolderRole).toBool()) {
            row->setIcon(0, mFolderIcon);
            if (members.size() > 1)
                //: Number of original folders consolidated into one display row.
                row->setText(1, tr("%n folders", nullptr, members.size()));
            //: State label when the represented original folders have different states.
            row->setText(2, states.size() == 1 ? *states.cbegin() : tr("Mixed"));
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
    // Rebuilding the projection must not select an editor, save a draft or execute script code.
    QSignalBlocker blocker(mpTree);
    const auto selected = mMembers.value(mpTree->currentItem());
    const auto editing = currentEditorItem();
    const bool firstBuild = mpTree->topLevelItemCount() == 0;
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
    root->setIcon(0, mFolderIcon);
    QList<ItemRef> roots;
    for (auto type : types())
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
        row->setExpanded((row == root && firstBuild) || wasExpanded || expanded.contains(row->data(0, PathRole).toString()));
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
    if (!editable) {
        mpEditor->mSaveItem->setText(tr("Save Item"));
        mpEditor->mDeleteItem->setText(tr("Delete Item"));
    }
    mpEditor->mAddItem->setText(tr("Add Item"));
    mpEditor->mAddGroup->setText(tr("Add Group"));
}

void CombinedEditorView::showFolder(QTreeWidgetItem* row, bool saveChanges)
{
    if (saveChanges) {
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
                                         : tr("Choose an original folder to edit its code and settings. Right-click for folder actions."),
                            mpSummary);
    help->setWordWrap(true);
    mpSummaryLayout->addWidget(help);
    QHash<int, int> ordinals;
    for (auto ref : mMembers.value(row)) {
        if (!ref.id)
            continue;
        const int ordinal = ++ordinals[static_cast<int>(ref.type)];
        //: %1 is the item type, %2 distinguishes same-type original folders, %3 is their enabled/error state.
        auto* button = new QPushButton(tr("%1 folder %2 — %3").arg(typeName(ref.type)).arg(ordinal).arg(status(ref)), mpSummary);
        //: Tooltip; %1 is the original folder's numeric ID within its category.
        button->setToolTip(tr("Edit original folder #%1").arg(ref.id));
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
    if (mpEditor->mNeedUpdateData)
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
        for (auto type : types())
            members.append({type, 0});
    }
    QHash<int, int> ordinals;
    for (auto ref : members) {
        const auto info = data(ref);
        if (!info.exists)
            continue;
        const int ordinal = ++ordinals[static_cast<int>(ref.type)];
        //: A submenu for one original folder; %2 distinguishes duplicate names within the same type.
        const QString label = ref.id ? tr("%1 folder %2 — %3").arg(typeName(ref.type)).arg(ordinal).arg(status(ref)) : typeName(ref.type);
        QMenu* target = members.size() > 1 || info.folder ? menu->addMenu(label) : menu;
        target->setToolTipsVisible(true);
        const auto action = [&](const QString& text, const QString& operation) {
            auto* entry = target->addAction(text);
            entry->setData(QVariantList{static_cast<int>(ref.type), ref.id, operation});
            //: Tooltip; %1 is the original object's numeric ID within its category.
            entry->setToolTip(tr("Original item #%1").arg(ref.id));
            // A queued refresh can replace every row while the menu is open; resolve the ID on use.
            connect(entry, &QAction::triggered, this, [this, ref, operation]() {
                runFolderAction(ref, operation);
            });
        };
        if (addOnly) {
            auto* entry = target->addAction(folder ? tr("Add %1 Group").arg(typeName(ref.type)) : tr("Add %1").arg(typeName(ref.type)));
            const ItemRef parent = info.folder ? ref : info.parent;
            entry->setData(QVariantList{static_cast<int>(parent.type), parent.id, folder ? qsl("addGroup") : qsl("add")});
            connect(entry, &QAction::triggered, this, [this, parent, folder]() {
                addItem(parent, folder);
            });
            continue;
        }
        if (ref.id) {
            action(tr("Edit Code and Settings"), qsl("edit"));
            action(tr("Open Original Folder or Item"), qsl("original"));
            target->addSeparator();
        }
        action(tr("Paste"), qsl("paste"));
        action(tr("Add %1").arg(typeName(ref.type)), qsl("add"));
        action(tr("Add %1 Group").arg(typeName(ref.type)), qsl("addGroup"));
        if (ref.id) {
            target->addSeparator();
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
    if (!data(parent).exists || !data(parent).folder || !editItem(parent))
        return;
    NavigationGuard navigation(this);
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
        default:
            break;
        }
    }
}

void CombinedEditorView::appendMissingParents(QMenu* menu, QTreeWidgetItem* row, bool folder)
{
    for (auto type : types()) {
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
        //: Explicitly creates missing folders in one category, then adds an item beneath them.
        auto* missing = menu->addMenu(tr("Create matching %1 folders and add…").arg(typeName(type)));
        for (int i = 0; i < destinations.size(); ++i) {
            const auto destination = destinations.at(i);
            auto* entry = missing->addAction(destination.id ? tr("Under %1 folder %2").arg(typeName(type)).arg(i + 1) : tr("At %1 root").arg(typeName(type)));
            entry->setToolTip(path.join(qsl(" / ")));
            connect(entry, &QAction::triggered, this, [this, destination, path, folder]() {
                createHierarchyAndAdd(destination, path, folder);
            });
        }
    }
}

void CombinedEditorView::createHierarchyAndAdd(ItemRef ancestor, const QStringList& path, bool folder)
{
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
