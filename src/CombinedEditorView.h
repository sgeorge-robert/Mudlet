#ifndef MUDLET_COMBINEDEDITORVIEW_H
#define MUDLET_COMBINEDEDITORVIEW_H

#include "EditorCommand.h"

#include <QHash>
#include <QIcon>
#include <QObject>
#include <QStringList>

class dlgTriggerEditor;
class QAction;
class QMenu;
class QTreeWidget;
class QTreeWidgetItem;
class QWidget;
class QVBoxLayout;

// Rows project the original units; merged folders never own or reparent profile objects.
// Identity, routing and refresh invariants are documented in docs/combined-editor-view.md.
class CombinedEditorView : public QObject
{
    Q_OBJECT

public:
    struct ItemRef
    {
        // IDs are unique only within a type; zero denotes that type's synthetic root.
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
    bool active() const { return mActive; }
    void show();
    void leave();
    void viewChanging(EditorViewType);
    void maintainVisibility();
    void scheduleRefresh();
    void refresh();
    bool addRequested(bool folder);
    bool folderSelected() const;
    bool folderActionRequested();

private:
    friend class CombinedEditorViewTest;
    struct ItemData
    {
        bool exists = false;
        bool folder = false;
        bool temporary = false;
        bool enabled = false;
        bool ancestorsEnabled = true;
        bool valid = true;
        QString name;
        QString error;
        ItemRef parent;
        QList<ItemRef> children;
    };

    static QList<EditorViewType> types();
    static QString typeName(EditorViewType);
    ItemData data(ItemRef) const;
    QString status(ItemRef) const;
    QTreeWidget* originalTree(EditorViewType) const;
    QTreeWidgetItem* originalItem(ItemRef) const;
    ItemRef currentEditorItem() const;
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

    dlgTriggerEditor* mpEditor;
    QTreeWidget* mpTree;
    QWidget* mpSummary;
    QVBoxLayout* mpSummaryLayout;
    QAction* mpAction;
    QIcon mFolderIcon;
    QHash<QTreeWidgetItem*, QList<ItemRef>> mMembers;
    QHash<EditorViewType, QIcon> mTypeIcons;
    QList<int> mSeparateViewSizes;
    QList<int> mCombinedViewSizes;
    bool mActive = false;
    bool mRouting = false;
    bool mRefreshing = false;
    bool mRefreshQueued = false;
};

#endif // MUDLET_COMBINEDEDITORVIEW_H
