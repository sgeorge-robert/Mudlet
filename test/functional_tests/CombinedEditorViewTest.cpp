/***************************************************************************
 *   Copyright (C) 2026 by Vadim Peretokin - vadim.peretokin@mudlet.org    *
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

// The combined tree, selection routing and folder menus have no Lua entry point.

#include <QAction>
#include <QMenu>
#include <QClipboard>
#include <QPushButton>
#include <QTreeWidgetItemIterator>
#include "CombinedEditorView.h"
#include "ScriptUnit.h"
#include "TScript.h"
#include "TAlias.h"
#include "TTimer.h"
#include "TLuaInterpreter.h"
#include "dlgScriptsMainArea.h"
#include "edbee/models/textdocument.h"
#include <QTemporaryDir>
#include <QTreeWidget>
#include <QtTest/QtTest>
#include <chrono>

#include "Host.h"
#include "HostDialogs.h"
#include "MudletInstanceCoordinator.h"
#include "MudletApp.h"
#include "PortableModeTestHelper.h"
#include "ProfileTestHelper.h"
#include "TriggerUnit.h"
#include "TelnetServerStub.h"
#include "ctelnet.h"
#include "dlgTriggerEditor.h"
#include "mudlet.h"

#include "GroupedTest.h"

using namespace std::chrono_literals;

class CombinedEditorViewTest : public QObject
{
    Q_OBJECT

private:
    QTemporaryDir mConfigDir;
    QByteArray mSavedXdg;
    TelnetServerStub* mpServer = nullptr;
    Host* mpHost = nullptr;
    dlgTriggerEditor* mpEditor = nullptr;
    const QString mProfileName = qsl("CombinedEditorView-Test-Profile");
    const QString mLocalhost = qsl("localhost");

    void deleteProfileDirectory(const QString& profileName)
    {
        QDir dir(MudletApp::getMudletPath(enums::profileHomePath, profileName));
        if (dir.exists()) {
            dir.removeRecursively();
        }
    }


    CombinedEditorView* view() const { return mpEditor->mpCombinedView; }
    using Ref = CombinedEditorView::ItemRef;

    TTrigger* trigger(const QString& name, TTrigger* parent = nullptr, bool folder = true)
    {
        auto* item = new TTrigger(parent, mpHost);
        item->setName(name);
        item->setIsFolder(folder);
        if (!folder)
            item->setRegexCodeList({qsl("qaCombinedNeverMatch")}, {REGEX_SUBSTRING});
        item->setScript({});
        item->registerTrigger();
        item->setIsActive(true);
        return item;
    }

    TScript* script(const QString& name, TScript* parent = nullptr, bool folder = true)
    {
        auto* item = new TScript(parent, mpHost);
        item->setName(name);
        item->setIsFolder(folder);
        item->registerScript();
        item->setScript({});
        item->setIsActive(true);
        return item;
    }

    QTreeWidgetItem* row(Ref ref) const
    {
        for (QTreeWidgetItemIterator it(view()->mpTree); *it; ++it) {
            if (view()->mMembers.value(*it).contains(ref))
                return *it;
        }
        return nullptr;
    }

    void showCombined()
    {
        emit mpHost->signal_itemsChangedByScript();
        view()->show();
        view()->refresh();
    }

    void select(Ref ref)
    {
        auto* item = row(ref);
        QVERIFY(item);
        view()->mpTree->setCurrentItem(item);
    }

    QAction* menuAction(QMenu* menu, Ref ref, const QString& operation) const
    {
        for (auto* action : menu->actions()) {
            if (action->data().toList() == QVariantList{static_cast<int>(ref.type), ref.id, operation})
                return action;
            if (action->menu()) {
                if (auto* found = menuAction(action->menu(), ref, operation))
                    return found;
            }
        }
        return nullptr;
    }

private slots:
    void initTestCase()
    {
        if (portableMarkerPresent()) {
            QSKIP("portable.txt present - it takes precedence over XDG_CONFIG_HOME, so the config dir cannot be redirected");
        }

        QVERIFY(mConfigDir.isValid());
        QVERIFY(QDir().mkpath(qsl("%1/mudlet/profiles").arg(mConfigDir.path())));
        mSavedXdg = qgetenv("XDG_CONFIG_HOME");
        qputenv("XDG_CONFIG_HOME", mConfigDir.path().toUtf8());

        mpServer = new TelnetServerStub(qApp);
        mpServer->start(mLocalhost, 0);
        QVERIFY2(mpServer->isListening(), qPrintable(qsl("TelnetServerStub failed to start: %1").arg(mpServer->errorString())));
        mudlet::start();
        mudlet::self()->setupConfig();
        QCOMPARE(MudletApp::getMudletPath(enums::mainPath), qsl("%1/mudlet").arg(mConfigDir.path()));
        mudlet::self()->takeOwnershipOfInstanceCoordinator(std::make_unique<MudletInstanceCoordinator>("MudletInstanceCoordinator"));
        mudlet::self()->init();
        mudlet::self()->setStorePasswordsSecurely(false);
        deleteProfileDirectory(mProfileName);

        mpHost = TestProfile::create(mProfileName, mLocalhost, QString::number(mpServer->serverPort()));
        QVERIFY2(mpHost, "No active host available for the test.");
        QSignalSpy connectedSpy(&(mpHost->mTelnet), &cTelnet::signal_connected);
        QVERIFY2(connectedSpy.wait(1s), "Could not connect with the host.");

        mudlet::self()->slot_showScriptDialog();
        QTest::qWait(100ms);
        mpEditor = HostDialogs::of(mpHost).mpEditorDialog;
        QVERIFY2(mpEditor, "the editor dialog was not created");
    }

    void cleanupTestCase()
    {
        if (mpHost) {
            if (auto* pEditor = HostDialogs::of(mpHost).mpEditorDialog.data()) {
                HostDialogs::of(mpHost).mpEditorDialog = nullptr;
                delete pEditor;
            }
        }
        mpEditor = nullptr;
        mpHost = nullptr;
        delete mpServer;
        mpServer = nullptr;
        if (mudlet::self()) {
            deleteProfileDirectory(mProfileName);
            delete mudlet::self();
        }
        mSavedXdg.isNull() ? qunsetenv("XDG_CONFIG_HOME") : qputenv("XDG_CONFIG_HOME", mSavedXdg);
    }

    void test_combinedViewCanBeOpenedWithoutChangingTheProfile()
    {
        const auto triggersBefore = mpHost->getTriggerUnit()->getTriggerRootNodeList();
        auto* action = mpEditor->findChild<QAction*>(qsl("action_combinedEditor"));
        QVERIFY2(action, "The editor must offer a Combined view");
        action->trigger();
        auto* tree = mpEditor->findChild<QTreeWidget*>(qsl("treeWidget_combined"));
        QVERIFY(tree);
        QVERIFY(tree->isVisible());
        QCOMPARE(tree->selectionMode(), QAbstractItemView::SingleSelection);
        QCOMPARE(tree->dragDropMode(), QAbstractItemView::NoDragDrop);
        QVERIFY(triggersBefore == mpHost->getTriggerUnit()->getTriggerRootNodeList());
    }
    void test_mergesFoldersAndPreservesObjectsAndOrder()
    {
        auto* first = trigger(qsl("qaMerge"));
        auto* second = trigger(qsl("qaMerge"));
        auto* other = script(qsl("qaMerge"));
        auto* child1 = trigger(qsl("Nested"), first);
        auto* child2 = trigger(qsl("Nested"), second);
        auto* child3 = script(qsl("Nested"), other);
        auto* leaf1 = trigger(qsl("Same leaf"), child1, false);
        auto* leaf2 = trigger(qsl("Same leaf"), child2, false);
        auto* leaf3 = script(qsl("Same leaf"), child3, false);
        const auto before = mpHost->getTriggerUnit()->getTriggerRootNodeList();
        const auto firstChildren = *first->getChildrenList();
        showCombined();
        auto* merged = row({EditorViewType::cmTriggerView, first->getID()});
        QVERIFY(merged);
        QCOMPARE(view()->mMembers.value(merged).size(), 3);
        QCOMPARE(merged, row({EditorViewType::cmTriggerView, second->getID()}));
        QCOMPARE(merged, row({EditorViewType::cmScriptView, other->getID()}));
        QCOMPARE(merged->childCount(), 1);
        auto* nested = merged->child(0);
        QCOMPARE(view()->mMembers.value(nested).size(), 3);
        QCOMPARE(nested->childCount(), 3);
        QCOMPARE(nested->child(0), row({EditorViewType::cmTriggerView, leaf1->getID()}));
        QCOMPARE(nested->child(1), row({EditorViewType::cmTriggerView, leaf2->getID()}));
        QCOMPARE(nested->child(2), row({EditorViewType::cmScriptView, leaf3->getID()}));
        QVERIFY(before == mpHost->getTriggerUnit()->getTriggerRootNodeList());
        QVERIFY(firstChildren == *first->getChildrenList());
        QCOMPARE(leaf1->getParent(), child1);
        QCOMPARE(leaf2->getParent(), child2);
        QCOMPARE(leaf3->getParent(), child3);
    }

    void test_foldersComeFirstWithoutChangingNativeOrder()
    {
        auto* rootLeaf = trigger(qsl("qaOrderRootLeaf"), nullptr, false);
        auto* triggers = trigger(qsl("qaOrder"));
        auto* firstLeaf = trigger(qsl("Z leaf"), triggers, false);
        auto* firstFolder = trigger(qsl("Z folder"), triggers);
        auto* secondLeaf = trigger(qsl("A leaf"), triggers, false);
        auto* secondFolder = trigger(qsl("A folder"), triggers);
        auto* scripts = script(qsl("qaOrder"));
        auto* scriptLeaf = script(qsl("Script leaf"), scripts, false);
        auto* scriptFolder = script(qsl("Script folder"), scripts);
        trigger(qsl("Nested leaf"), firstFolder, false);
        trigger(qsl("Nested folder"), firstFolder);
        const auto rootsBefore = mpHost->getTriggerUnit()->getTriggerRootNodeList();
        const auto childrenBefore = *triggers->getChildrenList();

        showCombined();
        auto* merged = row({EditorViewType::cmTriggerView, triggers->getID()});
        QVERIFY(merged);
        const QList<Ref> expected{{EditorViewType::cmTriggerView, firstFolder->getID()},
                                  {EditorViewType::cmTriggerView, secondFolder->getID()},
                                  {EditorViewType::cmScriptView, scriptFolder->getID()},
                                  {EditorViewType::cmTriggerView, firstLeaf->getID()},
                                  {EditorViewType::cmTriggerView, secondLeaf->getID()},
                                  {EditorViewType::cmScriptView, scriptLeaf->getID()}};
        QCOMPARE(merged->childCount(), expected.size());
        for (int i = 0; i < expected.size(); ++i)
            QCOMPARE(merged->child(i), row(expected[i]));
        auto* root = merged->parent();
        QVERIFY(root->indexOfChild(merged) < root->indexOfChild(row({EditorViewType::cmTriggerView, rootLeaf->getID()})));
        auto* nested = row({EditorViewType::cmTriggerView, firstFolder->getID()});
        QCOMPARE(nested->child(0)->text(0), qsl("Nested folder"));
        QCOMPARE(nested->child(1)->text(0), qsl("Nested leaf"));
        QVERIFY(rootsBefore == mpHost->getTriggerUnit()->getTriggerRootNodeList());
        QVERIFY(childrenBefore == *triggers->getChildrenList());
    }

    void test_typeIconsDoNotDependOnPopulatedOriginalTrees()
    {
        auto* triggerLeaf = trigger(qsl("qaIconTrigger"), nullptr, false);
        auto* aliasLeaf = new TAlias(qsl("qaIconAlias"), mpHost);
        aliasLeaf->setRegexCode(qsl("^qaIconAlias$"));
        aliasLeaf->registerAlias();
        auto* scriptLeaf = script(qsl("qaIconScript"), nullptr, false);
        auto* timerLeaf = new TTimer(qsl("qaIconTimer"), QTime(1, 0), mpHost);
        mpHost->getTimerUnit()->registerTimer(timerLeaf);
        const QList<Ref> refs{{EditorViewType::cmTriggerView, triggerLeaf->getID()},
                              {EditorViewType::cmAliasView, aliasLeaf->getID()},
                              {EditorViewType::cmScriptView, scriptLeaf->getID()},
                              {EditorViewType::cmTimerView, timerLeaf->getID()}};
        for (auto ref : refs)
            QVERIFY(!view()->originalItem(ref));
        showCombined();
        QList<QImage> icons;
        for (auto ref : refs) {
            auto* item = row(ref);
            QVERIFY(item);
            QVERIFY2(!item->icon(0).isNull(), "Every item needs a type icon even before its original tree is populated");
            const auto pixels = item->icon(0).pixmap(24, 24).toImage();
            QVERIFY(!pixels.isNull());
            for (const auto& previous : icons)
                QVERIFY2(pixels != previous, "Different item types need visually distinct icons");
            icons.append(pixels);
        }
        view()->refresh();
        for (int i = 0; i < refs.size(); ++i)
            QCOMPARE(row(refs[i])->icon(0).pixmap(24, 24).toImage(), icons[i]);
    }

    void test_menuChoosesEachOriginalParentAndFolderCode()
    {
        auto* first = trigger(qsl("qaDestinations"));
        auto* second = trigger(qsl("qaDestinations"));
        auto* scriptFolder = script(qsl("qaDestinations"));
        const Ref firstRef{EditorViewType::cmTriggerView, first->getID()};
        const Ref secondRef{EditorViewType::cmTriggerView, second->getID()};
        const Ref scriptRef{EditorViewType::cmScriptView, scriptFolder->getID()};
        showCombined();
        select(firstRef);
        QVERIFY(view()->mpSummary->isVisible());
        QCOMPARE(view()->mpSummary->findChildren<QPushButton*>().size(), 3);
        std::unique_ptr<QMenu> menu(view()->createMenu(row(firstRef)));
        QVERIFY(menuAction(menu.get(), firstRef, qsl("add")));
        auto* addSecond = menuAction(menu.get(), secondRef, qsl("add"));
        QVERIFY(addSecond);
        QVERIFY(first->getChildrenList()->empty());
        QVERIFY(second->getChildrenList()->empty());
        addSecond->trigger();
        QVERIFY(first->getChildrenList()->empty());
        QCOMPARE(second->getChildrenList()->size(), 1U);
        QCOMPARE(view()->currentEditorItem().id, second->getChildrenList()->front()->getID());
        QVERIFY(view()->active());
        auto* editScript = menuAction(menu.get(), scriptRef, qsl("edit"));
        QVERIFY(editScript);
        editScript->trigger();
        QVERIFY(view()->currentEditorItem() == scriptRef);
        QVERIFY(!view()->mpSummary->isVisible());
        const QString code = qsl("-- original script folder code\n");
        mpEditor->mpSourceEditorEdbeeDocument->setText(code);
        mpEditor->slot_saveEdits();
        QCOMPARE(scriptFolder->getScript(), code);
        QCOMPARE(first->getScript(), QString());
    }

    void test_refreshPreservesDraftAndDoesNotExecuteScript()
    {
        auto* item = script(qsl("qaDraft"), nullptr, false);
        const Ref ref{EditorViewType::cmScriptView, item->getID()};
        showCombined();
        select(ref);
        const QString draft = qsl("qaCombinedExecutions = (qaCombinedExecutions or 0) + 1\n");
        mpEditor->mpSourceEditorEdbeeDocument->setText(draft);
        QVERIFY(item->getScript().isEmpty());
        auto* added = trigger(qsl("qaLiveAddition"));
        emit mpHost->signal_itemsChangedByScript();
        QTRY_VERIFY(row({EditorViewType::cmTriggerView, added->getID()}));
        QCOMPARE(mpEditor->mpSourceEditorEdbeeDocument->text(), draft);
        QVERIFY(item->getScript().isEmpty());
        QVERIFY(mpHost->getLuaInterpreter()->compileAndExecuteScript(qsl("assert(qaCombinedExecutions == nil)")));
        mpEditor->doCleanReset();
        QTest::qWait(10);
        QVERIFY(view()->active());
        QCOMPARE(mpEditor->mpSourceEditorEdbeeDocument->text(), draft);
        view()->show();
        QVERIFY(!view()->mpSummary->isVisible());
        QCOMPARE(mpEditor->mpSourceEditorEdbeeDocument->text(), draft);
        mpEditor->slot_saveEdits();
        QCOMPARE(item->getScript(), draft);
        QVERIFY(mpHost->getLuaInterpreter()->compileAndExecuteScript(qsl("assert(qaCombinedExecutions == 1)")));
        view()->refresh();
        QVERIFY(mpHost->getLuaInterpreter()->compileAndExecuteScript(qsl("assert(qaCombinedExecutions == 1)")));
    }

    void test_deletedItemRetainsDraftWithoutSavingToAnotherItem()
    {
        auto* item = script(qsl("qaDeletedDraft"), nullptr, false);
        const Ref ref{EditorViewType::cmScriptView, item->getID()};
        auto* survivor = script(qsl("qaSurvivor"), nullptr, false);
        showCombined();
        select(ref);
        const QString draft = qsl("-- copy this unsaved draft\n");
        mpEditor->mpSourceEditorEdbeeDocument->setText(draft);
        delete item;
        emit mpHost->signal_itemsChangedByScript();
        QTRY_VERIFY(!row(ref));
        QCOMPARE(mpEditor->mpSourceEditorEdbeeDocument->text(), draft);
        QVERIFY(mpEditor->mpSourceEditorEdbee->isEnabled());
        QVERIFY(!mpEditor->mSaveItem->isEnabled());
        mpEditor->slot_saveEdits();
        QVERIFY(survivor->getScript().isEmpty());
    }

    void test_enabledStateDistinguishesParentBlockingAndErrors()
    {
        auto* first = trigger(qsl("qaStates"));
        auto* second = trigger(qsl("qaStates"));
        auto* child = trigger(qsl("qaBlocked"), first, false);
        first->setIsActive(false);
        QVERIFY(child->shouldBeActive());
        QVERIFY(!child->ancestorsActive());
        showCombined();
        QCOMPARE(row({EditorViewType::cmTriggerView, first->getID()})->text(2), qsl("Mixed"));
        QCOMPARE(row({EditorViewType::cmTriggerView, child->getID()})->text(2), qsl("Blocked by parent"));
        QVERIFY(child->shouldBeActive());
        auto* broken = script(qsl("qaBroken"), nullptr, false);
        QVERIFY(!broken->setScript(qsl("function (")));
        emit mpHost->signal_itemsChangedByScript();
        QTRY_VERIFY(row({EditorViewType::cmScriptView, broken->getID()}));
        QCOMPARE(row({EditorViewType::cmScriptView, broken->getID()})->text(2), qsl("Error"));
        QVERIFY(second->shouldBeActive());
    }

    void test_createsMissingHierarchyAsOneUndoableAction()
    {
        auto* root = trigger(qsl("qaMissing"));
        trigger(qsl("Inner"), root);
        showCombined();
        select({EditorViewType::cmTriggerView, root->getID()});
        const auto before = mpHost->getScriptUnit()->getScriptRootNodeList();
        const int undoBefore = mpEditor->mpUndoStack->index();
        view()->createHierarchyAndAdd({EditorViewType::cmScriptView, 0}, {qsl("qaMissing"), qsl("Inner")}, false);
        QVERIFY(view()->active());
        const auto selected = view()->currentEditorItem();
        QCOMPARE(selected.type, EditorViewType::cmScriptView);
        auto* created = mpHost->getScriptUnit()->getScript(selected.id);
        QVERIFY(created);
        QVERIFY(!created->isFolder());
        QVERIFY(created->getParent());
        QCOMPARE(created->getParent()->getName(), qsl("Inner"));
        QVERIFY(created->getParent()->getParent());
        QCOMPARE(created->getParent()->getParent()->getName(), qsl("qaMissing"));
        QCOMPARE(mpEditor->mpUndoStack->index(), undoBefore + 1);
        mpEditor->mpUndoStack->undo();
        QVERIFY(view()->active());
        QVERIFY(before == mpHost->getScriptUnit()->getScriptRootNodeList());
        mpEditor->mpUndoStack->redo();
        QVERIFY(view()->active());
        QCOMPARE(mpHost->getScriptUnit()->getScriptRootNodeList().size(), before.size() + 1);
    }

    void test_renameSplitsAndUndoMergesWithoutLeavingCombined()
    {
        auto* first = trigger(qsl("qaRename"));
        auto* other = script(qsl("qaRename"));
        const Ref triggerRef{EditorViewType::cmTriggerView, first->getID()};
        const Ref scriptRef{EditorViewType::cmScriptView, other->getID()};
        showCombined();
        QCOMPARE(row(triggerRef), row(scriptRef));
        select(triggerRef);
        row(triggerRef)->setExpanded(true);
        QVERIFY(view()->editItem(triggerRef));
        mpEditor->mpTriggersMainArea->lineEdit_trigger_name->setText(qsl("qaRenamed"));
        mpEditor->slot_saveProperty_TriggerName();
        QTRY_VERIFY(row(triggerRef) != row(scriptRef));
        QCOMPARE(first->getName(), qsl("qaRenamed"));
        QCOMPARE(view()->mpTree->currentItem(), row(triggerRef));
        QVERIFY(row(triggerRef)->isExpanded());
        mpEditor->mpUndoStack->undo();
        QVERIFY(view()->active());
        QCOMPARE(first->getName(), qsl("qaRename"));
        QCOMPARE(row(triggerRef), row(scriptRef));
    }

    void test_searchAndOriginalViewNavigation()
    {
        auto* item = script(qsl("qaCombinedSearchNeedle"), nullptr, false);
        const Ref ref{EditorViewType::cmScriptView, item->getID()};
        showCombined();
        mpEditor->comboBox_searchTerms->clear();
        mpEditor->comboBox_searchTerms->addItem(item->getName());
        mpEditor->slot_searchMudletItems(0);
        auto* result = mpEditor->treeWidget_searchResults->topLevelItem(0);
        QVERIFY(result);
        mpEditor->slot_itemSelectedInSearchResults(result);
        QVERIFY(view()->active());
        QVERIFY(view()->currentEditorItem() == ref);
        QCOMPARE(view()->mpTree->currentItem(), row(ref));
        QVERIFY(!mpEditor->treeWidget_scripts->isVisible());
        mpEditor->slot_showScripts();
        QVERIFY(!view()->active());
        QVERIFY(mpEditor->treeWidget_scripts->isVisible());
        QVERIFY(!view()->mpTree->isVisible());
    }
    void test_aliasAndTimerSelectionUseTheirOriginalEditors()
    {
        auto* alias = new TAlias(qsl("qaAliasLeaf"), mpHost);
        alias->setRegexCode(qsl("^qaAliasLeaf$"));
        alias->registerAlias();
        auto* timer = new TTimer(qsl("qaTimerLeaf"), QTime(1, 0), mpHost);
        mpHost->getTimerUnit()->registerTimer(timer);
        showCombined();
        select({EditorViewType::cmAliasView, alias->getID()});
        QCOMPARE(mpEditor->mCurrentView, EditorViewType::cmAliasView);
        mpEditor->mpSourceEditorEdbeeDocument->setText(qsl("-- alias code\n"));
        select({EditorViewType::cmTimerView, timer->getID()});
        QCOMPARE(alias->getScript(), qsl("-- alias code\n"));
        QCOMPARE(mpEditor->mCurrentView, EditorViewType::cmTimerView);
        mpEditor->mpSourceEditorEdbeeDocument->setText(qsl("-- timer code\n"));
        mpEditor->slot_saveEdits();
        QCOMPARE(timer->getScript(), qsl("-- timer code\n"));
        QCOMPARE(alias->getScript(), qsl("-- alias code\n"));
        QVERIFY(view()->active());
    }

    void test_missingHierarchyMenuOffersEachMatchingAncestor()
    {
        auto* parent = trigger(qsl("qaPartial"));
        auto* child = trigger(qsl("Inner"), parent);
        auto* first = script(qsl("qaPartial"));
        auto* second = script(qsl("qaPartial"));
        showCombined();
        auto* childRow = row({EditorViewType::cmTriggerView, child->getID()});
        QVERIFY(childRow);
        std::unique_ptr<QMenu> menu(view()->createMenu(childRow, true));
        QMenu* choices = nullptr;
        for (auto* action : menu->actions()) {
            if (action->text() == qsl("Create matching Script folders and add…"))
                choices = action->menu();
        }
        QVERIFY(choices);
        QCOMPARE(choices->actions().size(), 2);
        QVERIFY(first->getChildrenList()->empty());
        QVERIFY(second->getChildrenList()->empty());
        choices->actions().at(1)->trigger();
        QVERIFY(first->getChildrenList()->empty());
        QCOMPARE(second->getChildrenList()->size(), 1U);
        auto* addedFolder = static_cast<TScript*>(second->getChildrenList()->front());
        QCOMPARE(addedFolder->getName(), qsl("Inner"));
        QCOMPARE(addedFolder->getChildrenList()->size(), 1U);
        QVERIFY(!addedFolder->getChildrenList()->front()->isFolder());
    }

    void test_clipboardShortcutsAndPasteKeepCombinedView()
    {
        auto* folder = script(qsl("qaClipboard"));
        auto* leaf = script(qsl("qaClipboardLeaf"), folder, false);
        showCombined();
        const Ref ref{EditorViewType::cmScriptView, leaf->getID()};
        select(ref);
        QAction* copy = nullptr;
        QAction* paste = nullptr;
        for (auto* action : view()->mpTree->actions()) {
            if (action->shortcut() == QKeySequence(QKeySequence::Copy))
                copy = action;
            if (action->shortcut() == QKeySequence(QKeySequence::Paste))
                paste = action;
        }
        QVERIFY(copy);
        QVERIFY(paste);
        QCOMPARE(copy->shortcutContext(), Qt::WidgetWithChildrenShortcut);
        QApplication::clipboard()->clear();
        copy->trigger();
        QVERIFY(QApplication::clipboard()->text().contains(qsl("qaClipboardLeaf")));
        QCOMPARE(folder->getChildrenList()->size(), 1U);
        paste->trigger();
        QVERIFY(view()->active());
        QCOMPARE(folder->getChildrenList()->size(), 2U);
        QVERIFY(view()->currentEditorItem().id != leaf->getID());
        QCOMPARE(view()->mpTree->currentItem(), row(view()->currentEditorItem()));
        QVERIFY(mpEditor->mpSourceEditorArea->isVisible());
    }
    void test_keyboardNavigationAcrossTypesKeepsTreeFocus()
    {
        auto* triggers = trigger(qsl("qaKeyboard"));
        auto* triggerLeaf = trigger(qsl("First"), triggers, false);
        auto* aliases = new TAlias(qsl("qaKeyboard"), mpHost);
        aliases->setIsFolder(true);
        aliases->registerAlias();
        auto* aliasLeaf = new TAlias(aliases, mpHost);
        aliasLeaf->setName(qsl("Second"));
        aliasLeaf->setRegexCode(qsl("^qaKeyboard$"));
        aliasLeaf->registerAlias();
        auto* scripts = script(qsl("qaKeyboard"));
        auto* scriptLeaf = script(qsl("Third"), scripts, false);
        showCombined();
        auto* tree = view()->mpTree;
        auto* firstRow = row({EditorViewType::cmTriggerView, triggerLeaf->getID()});
        QVERIFY(firstRow);
        firstRow->parent()->setExpanded(true);
        tree->scrollToItem(firstRow);
        QTest::qWait(10);
        firstRow = row({EditorViewType::cmTriggerView, triggerLeaf->getID()});
        QVERIFY(firstRow);
        tree->scrollToItem(firstRow);
        const QRect visibleRow = tree->visualItemRect(firstRow).intersected(tree->viewport()->rect());
        QVERIFY(!visibleRow.isEmpty());
        tree->setFocus();
        QTest::mouseClick(tree->viewport(), Qt::LeftButton, Qt::NoModifier, visibleRow.center());
        QVERIFY((view()->currentEditorItem() == Ref{EditorViewType::cmTriggerView, triggerLeaf->getID()}));
        QCOMPARE(QApplication::focusWidget(), tree);
        QTest::keyClick(tree, Qt::Key_Down);
        QVERIFY((view()->currentEditorItem() == Ref{EditorViewType::cmAliasView, aliasLeaf->getID()}));
        QCOMPARE(QApplication::focusWidget(), tree);
        QTest::keyClick(tree, Qt::Key_Down);
        QVERIFY((view()->currentEditorItem() == Ref{EditorViewType::cmScriptView, scriptLeaf->getID()}));
        QCOMPARE(QApplication::focusWidget(), tree);
    }
};

#include "CombinedEditorViewTest.moc"
MUDLET_GROUPED_TEST_MAIN(CombinedEditorViewTest)
