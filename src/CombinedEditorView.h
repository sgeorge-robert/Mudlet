#ifndef MUDLET_COMBINEDEDITORVIEW_H
#define MUDLET_COMBINEDEDITORVIEW_H

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
class QVBoxLayout;
class QCheckBox;
class QPushButton;
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
    void showFolder(QTreeWidgetItem*, bool saveChanges);
    void syncSelection();
    void updateActions();
    QMenu* createMenu(QTreeWidgetItem*, bool addOnly = false, bool folder = false);
    void addItem(ItemRef parent, bool folder, const QString& name = {});
    void createHierarchyAndAdd(ItemRef ancestor, const QStringList& path, bool folder);
    void appendMissingParents(QMenu*, QTreeWidgetItem*, bool folder);
    void runFolderAction(ItemRef, const QString&);
    void indexVariables();
    TVar* variable(ItemRef) const;
    QString variablePath(TVar*) const;
    bool watchVariable(TVar*);
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
    QVBoxLayout* mpSummaryLayout;
    QAction* mpAction;
    QWidget* mpVariableControls;
    QCheckBox* mpIncludeVariables;
    QPushButton* mpRefreshVariables;
    QCheckBox* mpVariableSaved;
    QHash<int, QPersistentModelIndex> mVariableRows;
    QPersistentModelIndex mWatchedVariableRow;
    int mNextVariableId = 0;
    quint64 mVariableWatchGeneration = 0;
    int mVariableWatchLength = 0;
    QString mVariableName;
    QString mVariableValue;
    int mVariableKeyType = 0;
    int mVariableValueType = 0;
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
