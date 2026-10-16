#ifndef MUDLET_COMBINEDEDITORVIEW_H
#define MUDLET_COMBINEDEDITORVIEW_H

/***************************************************************************
 *   Copyright (C) 2026 by Samuel George - sgeorge.robert@gmail.com          *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 ***************************************************************************/

#include "EditorCommand.h"

#include <QHash>
#include <QIcon>
#include <QObject>
#include <QPersistentModelIndex>
#include <QSet>
#include <QStringList>

class dlgTriggerEditor;
class QAction;
class QMenu;
class QTreeWidget;
class QTreeWidgetItem;
class QWidget;
class QLabel;
class QCheckBox;
class QPushButton;
class QTabBar;
class TVar;

// Rows project the original units; merged folders never own or reparent profile objects.
// Identity, routing and refresh invariants are documented in docs/combined-editor-view.md.
class CombinedEditorView : public QObject
{
    Q_OBJECT

public:
    struct ItemRef
    {
        // IDs are unique only within a type; zero denotes that type's synthetic root.
        // Variable IDs expire when their native snapshot rows are rebuilt.
        EditorViewType type = EditorViewType::cmUnknownView;
        int id = 0;
        bool operator==(const ItemRef&) const = default;
    };

    // Native handlers may switch categories internally without leaving Combined.
    class NavigationGuard
    {
    public:
        explicit NavigationGuard(CombinedEditorView*);
        ~NavigationGuard();

    private:
        CombinedEditorView* mpView;
        bool mWasRouting = false;
    };

    explicit CombinedEditorView(dlgTriggerEditor*);
    ~CombinedEditorView() override;
    void writeSettings();
    bool active() const { return mActive; }
    void show();
    void leave();
    void viewChanging(EditorViewType);
    void maintainVisibility();
    bool moveSectionFocus(bool forward);
    void scheduleRefresh();
    void refresh();
    bool addRequested(bool folder);
    bool folderSelected() const;
    bool folderActionRequested();
    bool selectionActionRequested(bool deleting);
    bool includesVariables() const;
    bool readingVariables() const { return mReadingVariables; }
    void refreshVariables();
    void variableSelected();
    bool variableSaveAllowed();
    bool variableNavigationAllowed();
    void recordVariableSearchResult(QTreeWidgetItem*, TVar*) const;
    bool variableSearchMatches(QTreeWidgetItem*, TVar*) const;

private:
    friend class CombinedEditorViewTest;
    enum class FolderAction { Edit, OpenOriginal, AddItem, AddGroup, Toggle, Copy, Paste, Export, CreateModule, Delete };

    struct SelectionProblems
    {
        QString copy;
        QString deletion;
    };

    // Refresh can renew the live watch while retaining the draft's comparison baseline.
    struct VariableWatch
    {
        QPersistentModelIndex row;
        quint64 generation = 0;
        int length = 0;
    };

    struct VariableDraftBaseline
    {
        QString name;
        QString value;
        int keyType = 0;
        int valueType = 0;
    };

    struct ItemData
    {
        bool exists = false;
        bool folder = false;
        bool mergeable = true;
        bool temporary = false;
        bool hidden = false;
        bool enabled = false;
        bool ancestorsEnabled = true;
        bool valid = true;
        QString name;
        QString error;
        ItemRef parent;
        QList<ItemRef> children;
    };

    static QList<EditorViewType> types();
    QList<EditorViewType> visibleTypes() const;
    static QString typeName(EditorViewType);
    QString itemTypeName(ItemRef) const;
    QIcon subtypeIcon(ItemRef) const;
    QString folderName(ItemRef) const;
    bool createsToolbar(ItemRef parent) const;
    QString addLabel(ItemRef parent, bool folder) const;
    ItemData data(ItemRef) const;
    QString status(ItemRef) const;
    QTreeWidget* originalTree(EditorViewType) const;
    QTreeWidgetItem* originalItem(ItemRef) const;
    ItemRef currentEditorItem() const;
    void saveAndClearCurrentItems();
    void clearCurrentItems();
    void appendChildren(QTreeWidgetItem*, const QList<ItemRef>&, const QString&);
    void selectRow(QTreeWidgetItem*);
    bool editItem(ItemRef, bool originalView = false);
    void selectFolder(QTreeWidgetItem*);
    void showEmptyPanel(QTreeWidgetItem*);
    void updateFolderTabs();
    void selectFolderTab(int index);
    void syncSelection();
    void updateActions();
    bool singleCurrentSelection() const;
    QList<ItemRef> selectedItemRefs() const;
    SelectionProblems selectionProblems(const QList<ItemRef>&) const;
    void runSelectionAction(const QList<ItemRef>&, bool deleting);
    QMenu* createMenu(QTreeWidgetItem*, bool addOnly = false, bool folder = false);
    void addItem(ItemRef parent, bool folder, const QString& name = {});
    void createHierarchyAndAdd(ItemRef ancestor, const QStringList& path, bool folder);
    void appendMissingParents(QMenu*, QTreeWidgetItem*, bool folder);
    void runFolderAction(ItemRef, FolderAction);
    void indexVariables();
    TVar* variable(ItemRef) const;
    QString variablePath(TVar*) const;
    bool watchVariable(TVar*, bool* emptyTable = nullptr);
    bool variableMatchesWatch() const;
    bool variableDraftChanged() const;
    void rejectVariableEdit();
    void updateVariableControls();
    void setVariableSaved(bool);
    QString settingsPrefix() const;
    void readSettings();
    void rememberExpandedFolders();

    dlgTriggerEditor* mpEditor;
    QTreeWidget* mpTree;
    QWidget* mpSummary;
    QLabel* mpSummaryTitle;
    QTabBar* mpFolderTabs;
    QAction* mpAction;
    QAction* mpCopyAction = nullptr;
    QAction* mpPasteAction = nullptr;
    QWidget* mpVariableControls;
    QCheckBox* mpIncludeVariables;
    QPushButton* mpRefreshVariables;
    QCheckBox* mpVariableSaved;
    QHash<int, QPersistentModelIndex> mVariableRows;
    int mNextVariableId = 0;
    VariableWatch mVariableWatch;
    VariableDraftBaseline mVariableDraftBaseline;
    QIcon mFolderIcon;
    QHash<QTreeWidgetItem*, QList<ItemRef>> mMembers;
    QHash<EditorViewType, QIcon> mTypeIcons;
    QList<int> mSeparateViewSizes;
    QList<int> mCombinedViewSizes;
    QSet<QString> mExpandedFolders;
    bool mHasExpansionState = false;
    bool mActive = false;
    bool mRouting = false;
    bool mRefreshing = false;
    bool mRefreshQueued = false;
    bool mReadingVariables = false;
    bool mAddingVariable = false;
    bool mVariableConflict = false;
};

#endif // MUDLET_COMBINEDEDITORVIEW_H
