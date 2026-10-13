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
#include <QCheckBox>
#include <QMenu>
#include <QClipboard>
#include <QPushButton>
#include <QFontDatabase>
#include <QFontInfo>
#include <QSettings>
#include <QSplitter>
#include <QTreeWidgetItemIterator>
#include "CombinedEditorView.h"
#include "ScriptUnit.h"
#include "TScript.h"
#include "TAlias.h"
#include "TAction.h"
#include "TEasyButtonBar.h"
#include "TMainConsole.h"
#include "TTimer.h"
#include "TKey.h"
#include "TLuaInterpreter.h"
#include "LuaInterface.h"
#include "VarUnit.h"
#include "dlgVarsMainArea.h"
#include "dlgKeysMainArea.h"
#include "dlgActionMainArea.h"
#include "dlgAliasMainArea.h"
#include "dlgScriptsMainArea.h"
#include "dlgTriggerPatternEdit.h"
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

    TKey* key(const QString& name, TKey* parent = nullptr, bool folder = true)
    {
        auto* item = new TKey(parent, mpHost);
        item->setName(name);
        item->setIsFolder(folder);
        item->setKeyCode(Qt::Key_F12);
        item->setKeyModifiers(Qt::ControlModifier);
        item->setScript({});
        item->registerKey();
        item->setIsActive(true);
        return item;
    }

    TAction* button(const QString& name, TAction* parent = nullptr, bool folder = true)
    {
        auto* item = new TAction(parent, mpHost);
        item->setName(name);
        item->setIsFolder(folder);
        item->registerAction();
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

    bool includeVariables()
    {
        showCombined();
        auto* include = mpEditor->findChild<QCheckBox*>(qsl("combinedIncludeVariables"));
        if (!include)
            return false;
        include->setChecked(true);
        auto* refresh = mpEditor->findChild<QPushButton*>(qsl("combinedRefreshVariables"));
        if (!refresh)
            return false;
        refresh->click();
        return true;
    }

    Ref variable(const QStringList& names, int keyType = LUA_TSTRING) const
    {
        auto* unit = mpHost->getLuaInterface()->getVarUnit();
        for (QTreeWidgetItemIterator it(view()->mpTree); *it; ++it) {
            for (auto ref : view()->mMembers.value(*it)) {
                if (ref.type != EditorViewType::cmVarsView || !ref.id)
                    continue;
                auto* var = mpEditor->treeWidget_variables->variableForRow(unit, view()->originalItem(ref));
                if (var && unit->shortVarName(var) == names && var->getKeyType() == keyType)
                    return ref;
            }
        }
        return {};
    }

    bool focusWithin(QWidget* area) const
    {
        auto* focused = QApplication::focusWidget();
        return focused && (focused == area || area->isAncestorOf(focused));
    }

    void sectionShortcut(bool forward)
    {
        QVERIFY(QApplication::focusWidget());
        const auto modifiers = forward ? Qt::ControlModifier : Qt::ControlModifier | Qt::ShiftModifier;
        QTest::keyClick(QApplication::focusWidget(), Qt::Key_Tab, modifiers);
        QCoreApplication::processEvents();
    }

    void recreateEditor(bool clearPreferences = false, bool closeFirst = true)
    {
        if (closeFirst) {
            mpEditor->close();
        }
        HostDialogs::of(mpHost).mpEditorDialog = nullptr;
        delete mpEditor;
        if (clearPreferences) {
            MudletApp::getQSettings()->remove(qsl("Editor/combined_view/profiles/%1").arg(MudletApp::sanitizeForPath(mProfileName)));
        }
        MudletApp::getQSettings()->sync();
        mpEditor = new dlgTriggerEditor(mpHost);
        HostDialogs::of(mpHost).mpEditorDialog = mpEditor;
        mpEditor->show();
        mpEditor->slot_showTriggers();
        QTest::qWait(50ms);
    }

private slots:
    void initTestCase()
    {
        if (portableMarkerPresent()) {
            QSKIP("portable.txt present - it takes precedence over XDG_CONFIG_HOME, so the config dir cannot be redirected");
        }

#ifdef INCLUDE_FONTS
        QVERIFY(QFontDatabase::addApplicationFont(qsl(":/fonts/ttf-bitstream-vera-1.10/VeraMono.ttf")) != -1);
        QCOMPARE(QFontInfo(QFont(qsl("Bitstream Vera Sans Mono"))).family(), qsl("Bitstream Vera Sans Mono"));
#endif

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
        auto* keyLeaf = key(qsl("qaIconKey"), nullptr, false);
        auto* buttonLeaf = button(qsl("qaIconButton"), nullptr, false);
        const QList<Ref> refs{{EditorViewType::cmTriggerView, triggerLeaf->getID()},
                              {EditorViewType::cmAliasView, aliasLeaf->getID()},
                              {EditorViewType::cmScriptView, scriptLeaf->getID()},
                              {EditorViewType::cmTimerView, timerLeaf->getID()},
                              {EditorViewType::cmKeysView, keyLeaf->getID()},
                              {EditorViewType::cmActionView, buttonLeaf->getID()}};
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

    void test_keysMergeWithoutChangingBindingsAndRefreshTheirState()
    {
        auto* first = key(qsl("qaKeyMerge"));
        auto* second = key(qsl("qaKeyMerge"));
        auto* leaf = key(qsl("qaKeyLeaf"), first, false);
        leaf->setCommand(qsl("qaKeyCommand"));
        auto* timers = new TTimer(qsl("qaKeyMerge"), QTime(1, 0), mpHost);
        timers->setIsFolder(true);
        mpHost->getTimerUnit()->registerTimer(timers);
        auto* timer = new TTimer(timers, mpHost);
        timer->setName(qsl("qaEarlierTimer"));
        mpHost->getTimerUnit()->registerTimer(timer);
        const Ref firstRef{EditorViewType::cmKeysView, first->getID()};
        const Ref leafRef{EditorViewType::cmKeysView, leaf->getID()};
        const auto roots = mpHost->getKeyUnit()->getKeyRootNodeList();
        showCombined();
        QVERIFY(row(firstRef));
        QCOMPARE(row(firstRef), row({EditorViewType::cmKeysView, second->getID()}));
        QCOMPARE(row(firstRef), row({EditorViewType::cmTimerView, timers->getID()}));
        QCOMPARE(row(firstRef)->child(0), row({EditorViewType::cmTimerView, timer->getID()}));
        QCOMPARE(row(firstRef)->child(1), row(leafRef));
        QVERIFY(!row(leafRef)->icon(0).isNull());
        QCOMPARE(row(leafRef)->text(1), qsl("Key"));
        QCOMPARE(row(leafRef)->text(2), qsl("Enabled"));
        QVERIFY(roots == mpHost->getKeyUnit()->getKeyRootNodeList());
        QCOMPARE(leaf->getParent(), first);
        QCOMPARE(leaf->getCommand(), qsl("qaKeyCommand"));
        QCOMPARE(leaf->getKeyCode(), Qt::Key_F12);
        QCOMPARE(leaf->getKeyModifiers(), Qt::ControlModifier);
        first->setIsActive(false);
        emit mpHost->signal_keyToggled(first->getID());
        QTRY_COMPARE(row(leafRef)->text(2), qsl("Blocked by parent"));
        QVERIFY(leaf->shouldBeActive());
        QVERIFY(!leaf->setScript(qsl("function (")));
        emit mpHost->signal_itemsChangedByScript();
        QTRY_COMPARE(row(leafRef)->text(2), qsl("Error"));
    }

    void test_keyDraftSearchAndSaveStayWithTheOriginalItem()
    {
        auto* item = key(qsl("qaCombinedKeySearchNeedle"), nullptr, false);
        auto* other = trigger(qsl("qaKeyDraftOther"), nullptr, false);
        const Ref ref{EditorViewType::cmKeysView, item->getID()};
        showCombined();
        QVERIFY(row(ref));
        select(ref);
        QVERIFY(mpEditor->mpKeysMainArea->isVisible());
        QVERIFY(!mpEditor->treeWidget_keys->isVisible());
        const QString draft = qsl("qaCombinedKeyExecutions = (qaCombinedKeyExecutions or 0) + 1\n");
        mpEditor->mpSourceEditorEdbeeDocument->setText(draft);
        emit mpHost->signal_itemsChangedByScript();
        QTest::qWait(10);
        QCOMPARE(mpEditor->mpSourceEditorEdbeeDocument->text(), draft);
        QVERIFY(item->getScript().isEmpty());
        select({EditorViewType::cmTriggerView, other->getID()});
        QCOMPARE(item->getScript(), draft);
        QVERIFY(other->getScript().isEmpty());
        QVERIFY(mpHost->getLuaInterpreter()->compileAndExecuteScript(qsl("assert(qaCombinedKeyExecutions == nil)")));
        mpEditor->comboBox_searchTerms->clear();
        mpEditor->comboBox_searchTerms->addItem(item->getName());
        mpEditor->slot_searchMudletItems(0);
        auto* result = mpEditor->treeWidget_searchResults->topLevelItem(0);
        QVERIFY(result);
        mpEditor->slot_itemSelectedInSearchResults(result);
        QVERIFY(view()->active());
        QVERIFY(view()->currentEditorItem() == ref);
        QCOMPARE(view()->mpTree->currentItem(), row(ref));
        QCOMPARE(mpEditor->mpSourceEditorEdbeeDocument->text(), draft);
        mpEditor->slot_showKeys();
        QVERIFY(!view()->active());
        QVERIFY(mpEditor->treeWidget_keys->isVisible());
    }

    void test_keyCaptureAndUndoKeepCombinedSelection()
    {
        auto* item = key(qsl("qaCombinedCapture"), nullptr, false);
        const Ref ref{EditorViewType::cmKeysView, item->getID()};
        showCombined();
        QVERIFY(row(ref));
        select(ref);
        auto* grab = mpEditor->mpKeysMainArea->pushButton_key_grabKey;
        const auto saveShortcut = mpEditor->mSaveItem->shortcut();
        const auto addShortcut = mpEditor->mAddItem->shortcut();
        const auto deleteShortcut = mpEditor->mDeleteItem->shortcut();
        QVERIFY(!saveShortcut.isEmpty());
        QVERIFY(!addShortcut.isEmpty());
        QVERIFY(!deleteShortcut.isEmpty());
        QTest::mouseClick(grab, Qt::LeftButton);
        QVERIFY(mpEditor->mIsGrabKey);
        QTest::keyClick(grab, Qt::Key_S, Qt::ControlModifier);
        QVERIFY(!mpEditor->mIsGrabKey);
        QCOMPARE(item->getKeyCode(), Qt::Key_S);
        QCOMPARE(item->getKeyModifiers(), Qt::ControlModifier);
        QCOMPARE(mpEditor->mSaveItem->shortcut(), saveShortcut);
        QCOMPARE(mpEditor->mAddItem->shortcut(), addShortcut);
        QCOMPARE(mpEditor->mDeleteItem->shortcut(), deleteShortcut);
        QVERIFY(view()->currentEditorItem() == ref);
        mpEditor->mpUndoStack->undo();
        QVERIFY(view()->active());
        QCOMPARE(item->getKeyCode(), Qt::Key_F12);
        mpEditor->mpUndoStack->redo();
        QCOMPARE(item->getKeyCode(), Qt::Key_S);
        QTest::mouseClick(grab, Qt::LeftButton);
        QTest::keyClick(view()->mpTree, Qt::Key_Down);
        QVERIFY(!mpEditor->mIsGrabKey);
        QCOMPARE(item->getKeyCode(), Qt::Key_Down);
        QVERIFY(view()->currentEditorItem() == ref);
        QTest::mouseClick(grab, Qt::LeftButton);
        QTest::keyClick(grab, Qt::Key_Escape);
        QVERIFY(!mpEditor->mIsGrabKey);
        QCOMPARE(item->getKeyCode(), Qt::Key_Down);
        QCOMPARE(mpEditor->mSaveItem->shortcut(), saveShortcut);
        QCOMPARE(mpEditor->mAddItem->shortcut(), addShortcut);
        QCOMPARE(mpEditor->mDeleteItem->shortcut(), deleteShortcut);
    }

    void test_keyAddsChooseOriginalParentsAndUndoMissingHierarchy()
    {
        auto* first = key(qsl("qaKeyAdd"));
        auto* second = key(qsl("qaKeyAdd"));
        const Ref ref{EditorViewType::cmKeysView, second->getID()};
        showCombined();
        QVERIFY(row(ref));
        std::unique_ptr<QMenu> menu(view()->createMenu(row(ref)));
        auto* add = menuAction(menu.get(), ref, qsl("add"));
        QVERIFY(add);
        add->trigger();
        QCOMPARE(first->getChildrenList()->size(), 0U);
        QCOMPARE(second->getChildrenList()->size(), 1U);
        QCOMPARE(mpHost->getKeyUnit()->getKey(view()->currentEditorItem().id)->getParent(), second);
        QVERIFY(view()->editItem(ref));
        const int undoBefore = mpEditor->mpUndoStack->index();
        view()->createHierarchyAndAdd(ref, {qsl("Nested"), qsl("Inner")}, false);
        auto* created = mpHost->getKeyUnit()->getKey(view()->currentEditorItem().id);
        QVERIFY(created);
        QCOMPARE(created->getParent()->getName(), qsl("Inner"));
        QCOMPARE(created->getParent()->getParent()->getName(), qsl("Nested"));
        QCOMPARE(created->getParent()->getParent()->getParent(), second);
        QCOMPARE(mpEditor->mpUndoStack->index(), undoBefore + 1);
        mpEditor->mpUndoStack->undo();
        QCOMPARE(second->getChildrenList()->size(), 1U);
        mpEditor->mpUndoStack->redo();
        QCOMPARE(second->getChildrenList()->size(), 2U);
        QVERIFY(view()->active());
    }

    void test_buttonFoldersMergeAndKeepTheirNativeRoles()
    {
        auto* triggers = trigger(qsl("qaButtonMerge"));
        auto* keys = key(qsl("qaButtonMerge"));
        auto* keyLeaf = key(qsl("qaButtonEarlierKey"), keys, false);
        auto* toolbar = button(qsl("qaButtonMerge"));
        auto* duplicate = button(qsl("qaButtonMerge"));
        auto* leaf = button(qsl("qaButtonLeaf"), toolbar, false);
        auto* menu = button(qsl("qaButtonMenu"), toolbar);
        auto* package = button(qsl("qaButtonPackage"));
        package->mPackageName = qsl("qaButtonPackage");
        auto* packagedToolbar = button(qsl("qaPackagedToolbar"), package);
        const Ref toolbarRef{EditorViewType::cmActionView, toolbar->getID()};
        showCombined();
        QVERIFY(row(toolbarRef));
        auto* merged = row(toolbarRef);
        QCOMPARE(merged, row({EditorViewType::cmActionView, duplicate->getID()}));
        QCOMPARE(merged, row({EditorViewType::cmTriggerView, triggers->getID()}));
        QCOMPARE(merged->child(0), row({EditorViewType::cmActionView, menu->getID()}));
        QCOMPARE(merged->child(1), row({EditorViewType::cmKeysView, keyLeaf->getID()}));
        QCOMPARE(merged->child(2), row({EditorViewType::cmActionView, leaf->getID()}));
        QCOMPARE(leaf->getParent(), toolbar);
        QCOMPARE(packagedToolbar->getParent(), package);
        QCOMPARE(row({EditorViewType::cmActionView, menu->getID()})->text(1), qsl("Button menu"));
        QCOMPARE(row({EditorViewType::cmActionView, package->getID()})->text(1), qsl("Button package"));
        QCOMPARE(row({EditorViewType::cmActionView, packagedToolbar->getID()})->text(1), qsl("Button toolbar"));
        select(toolbarRef);
        QStringList choices;
        for (auto* choice : view()->mpSummary->findChildren<QPushButton*>()) {
            choices.append(choice->text());
        }
        QVERIFY(choices.contains(qsl("Button toolbar 1 — Enabled")));
        QVERIFY(choices.contains(qsl("Button toolbar 2 — Enabled")));
        QVERIFY(view()->editItem(toolbarRef));
        QVERIFY(mpEditor->mpActionsMainArea->groupBox_action_bar->isVisible());
        QVERIFY(view()->editItem({EditorViewType::cmActionView, menu->getID()}));
        QVERIFY(!mpEditor->mpActionsMainArea->groupBox_action_bar->isVisible());
        QVERIFY(mpEditor->mpActionsMainArea->groupBox_action_button_appearance->isVisible());
        QVERIFY(view()->editItem({EditorViewType::cmActionView, package->getID()}));
        QVERIFY(!mpEditor->mpSourceEditorArea->isVisible());
        QVERIFY(view()->editItem({EditorViewType::cmActionView, packagedToolbar->getID()}));
        QVERIFY(mpEditor->mpActionsMainArea->groupBox_action_bar->isVisible());
    }

    void test_buttonRefreshPreservesDraftAndToolbarWidgets()
    {
        auto* toolbar = button(qsl("qaButtonDraftBar"));
        toolbar->mLocation = 2;
        toolbar->setButtonColumns(3);
        auto* leaf = button(qsl("qaButtonDraftLeaf"), toolbar, false);
        leaf->setIsPushDownButton(true);
        leaf->setCommandButtonDown(qsl("qaDown"));
        leaf->setCommandButtonUp(qsl("qaUp"));
        auto* other = key(qsl("qaButtonDraftOther"), nullptr, false);
        const Ref ref{EditorViewType::cmActionView, leaf->getID()};
        showCombined();
        QVERIFY(row(ref));
        select(ref);
        QVERIFY(mpEditor->mpActionsMainArea->isVisible());
        QVERIFY(!mpEditor->treeWidget_actions->isVisible());
        QCOMPARE(mpEditor->mpActionsMainArea->lineEdit_action_button_command_down->text(), qsl("qaDown"));
        QCOMPARE(mpEditor->mpActionsMainArea->lineEdit_action_button_command_up->text(), qsl("qaUp"));
        const QString draft = qsl("qaCombinedButtonExecutions = (qaCombinedButtonExecutions or 0) + 1\n");
        mpEditor->mpSourceEditorEdbeeDocument->setText(draft);
        mpHost->getActionUnit()->updateAllToolbars();
        QPointer<TEasyButtonBar> bar = mpHost->mainConsoleView()->actionEasyButtonBar(toolbar);
        QVERIFY(bar);
        QPointer<QPushButton> drawnButton;
        for (auto* widget : bar->findChildren<QPushButton*>()) {
            if (widget->text() == leaf->getName()) {
                drawnButton = widget;
            }
        }
        QVERIFY(drawnButton);
        const bool changed = toolbar->isDataChanged();
        emit mpHost->signal_itemsChangedByScript();
        QTest::qWait(10);
        view()->refresh();
        QVERIFY(bar);
        QVERIFY(drawnButton);
        QCOMPARE(mpHost->mainConsoleView()->actionEasyButtonBar(toolbar), bar.data());
        QCOMPARE(toolbar->isDataChanged(), changed);
        QCOMPARE(toolbar->mLocation, 2);
        QCOMPARE(toolbar->getButtonColumns(), 3);
        QVERIFY(leaf->getScript().isEmpty());
        QCOMPARE(mpEditor->mpSourceEditorEdbeeDocument->text(), draft);
        select({EditorViewType::cmKeysView, other->getID()});
        QCOMPARE(leaf->getScript(), draft);
        QVERIFY(other->getScript().isEmpty());
        QVERIFY(leaf->isPushDownButton());
        QCOMPARE(leaf->getCommandButtonUp(), qsl("qaUp"));
        QVERIFY(mpHost->getLuaInterpreter()->compileAndExecuteScript(qsl("assert(qaCombinedButtonExecutions == nil)")));
    }

    void test_buttonSearchClipboardAndDeleteUndoUseOriginals()
    {
        auto* first = button(qsl("qaButtonEdit"));
        auto* second = button(qsl("qaButtonEdit"));
        auto* leaf = button(qsl("qaCombinedButtonSearchNeedle"), second, false);
        const Ref ref{EditorViewType::cmActionView, leaf->getID()};
        showCombined();
        QVERIFY(row(ref));
        mpEditor->comboBox_searchTerms->clear();
        mpEditor->comboBox_searchTerms->addItem(leaf->getName());
        mpEditor->slot_searchMudletItems(0);
        auto* result = mpEditor->treeWidget_searchResults->topLevelItem(0);
        QVERIFY(result);
        mpEditor->slot_itemSelectedInSearchResults(result);
        QVERIFY(view()->active());
        QVERIFY(view()->currentEditorItem() == ref);
        QCOMPARE(view()->mpTree->currentItem(), row(ref));
        std::unique_ptr<QMenu> menu(view()->createMenu(row(ref)));
        auto* copy = menuAction(menu.get(), ref, qsl("copy"));
        auto* paste = menuAction(menu.get(), ref, qsl("paste"));
        QVERIFY(copy);
        QVERIFY(paste);
        copy->trigger();
        QVERIFY(QApplication::clipboard()->text().contains(leaf->getName()));
        paste->trigger();
        QVERIFY(view()->active());
        QCOMPARE(first->getChildrenList()->size(), 0U);
        QCOMPARE(second->getChildrenList()->size(), 2U);
        mpEditor->slot_deleteItemOrGroup();
        QCOMPARE(second->getChildrenList()->size(), 1U);
        mpEditor->mpUndoStack->undo();
        QCOMPARE(second->getChildrenList()->size(), 2U);
        QVERIFY(view()->active());
        mpEditor->slot_showActions();
        QVERIFY(!view()->active());
        QVERIFY(mpEditor->treeWidget_actions->isVisible());
    }

    void test_buttonAddsNameToolbarCreationAndChooseDuplicateParents()
    {
        auto* first = button(qsl("qaButtonAdd"));
        auto* second = button(qsl("qaButtonAdd"));
        auto* missing = trigger(qsl("qaCreatedToolbar"));
        trigger(qsl("Menu"), missing);
        auto* package = button(qsl("qaButtonAddPackage"));
        package->mPackageName = qsl("qaButtonAddPackage");
        const Ref rootRef{EditorViewType::cmActionView, 0};
        const Ref secondRef{EditorViewType::cmActionView, second->getID()};
        showCombined();
        QVERIFY(row(secondRef));
        std::unique_ptr<QMenu> rootMenu(view()->createMenu(nullptr));
        auto* addRootButton = menuAction(rootMenu.get(), rootRef, qsl("add"));
        auto* addToolbar = menuAction(rootMenu.get(), rootRef, qsl("addGroup"));
        QVERIFY(addRootButton);
        QVERIFY(!addRootButton->isEnabled());
        QVERIFY(addToolbar);
        QCOMPARE(addToolbar->text(), qsl("Add Toolbar"));
        std::unique_ptr<QMenu> packageMenu(view()->createMenu(row({EditorViewType::cmActionView, package->getID()})));
        auto* addPackagedToolbar = menuAction(packageMenu.get(), {EditorViewType::cmActionView, package->getID()}, qsl("addGroup"));
        QVERIFY(addPackagedToolbar);
        QCOMPARE(addPackagedToolbar->text(), qsl("Add Toolbar"));
        QVERIFY(!menuAction(packageMenu.get(), {EditorViewType::cmActionView, package->getID()}, qsl("add"))->isEnabled());
        std::unique_ptr<QMenu> menu(view()->createMenu(row(secondRef)));
        auto* add = menuAction(menu.get(), secondRef, qsl("add"));
        auto* addMenu = menuAction(menu.get(), secondRef, qsl("addGroup"));
        QVERIFY(add);
        QVERIFY(addMenu);
        QCOMPARE(addMenu->text(), qsl("Add Menu"));
        add->trigger();
        QCOMPARE(first->getChildrenList()->size(), 0U);
        QCOMPARE(second->getChildrenList()->size(), 1U);
        QCOMPARE(mpHost->getActionUnit()->getAction(view()->currentEditorItem().id)->getParent(), second);
        QVERIFY(view()->editItem(rootRef));
        std::unique_ptr<QMenu> missingMenu(view()->createMenu(row({EditorViewType::cmTriggerView, missing->getID()})));
        QMenu* toolbarCreation = nullptr;
        for (auto* action : missingMenu->actions()) {
            if (action->text() == qsl("Create matching Button toolbar and menus and add…")) {
                toolbarCreation = action->menu();
            }
        }
        QVERIFY(toolbarCreation);
        QCOMPARE(toolbarCreation->actions().first()->text(), qsl("Create toolbar “qaCreatedToolbar” at Button root"));
        const auto before = mpHost->getActionUnit()->getActionRootNodeList();
        const int undoBefore = mpEditor->mpUndoStack->index();
        view()->createHierarchyAndAdd(rootRef, {qsl("qaCreatedToolbar"), qsl("Menu")}, false);
        auto* created = mpHost->getActionUnit()->getAction(view()->currentEditorItem().id);
        QVERIFY(created);
        QCOMPARE(created->getParent()->getName(), qsl("Menu"));
        QCOMPARE(created->getParent()->getParent()->getName(), qsl("qaCreatedToolbar"));
        QVERIFY(!created->getParent()->getParent()->getParent());
        QCOMPARE(mpEditor->mpUndoStack->index(), undoBefore + 1);
        mpEditor->mpUndoStack->undo();
        QVERIFY(before == mpHost->getActionUnit()->getActionRootNodeList());
        mpEditor->mpUndoStack->redo();
        QCOMPARE(mpHost->getActionUnit()->getActionRootNodeList().size(), before.size() + 1);
        QVERIFY(view()->active());
    }

    void test_buttonLuaVisibilityRefreshesStateWithoutSavingDraft()
    {
        auto* toolbar = button(qsl("qaCombinedLuaToolbar"));
        auto* leaf = button(qsl("qaCombinedLuaButton"), toolbar, false);
        const Ref toolbarRef{EditorViewType::cmActionView, toolbar->getID()};
        const Ref leafRef{EditorViewType::cmActionView, leaf->getID()};
        showCombined();
        QVERIFY(row(leafRef));
        select(leafRef);
        const QString draft = qsl("-- unsaved button draft\n");
        mpEditor->mpSourceEditorEdbeeDocument->setText(draft);
        QCOMPARE(row(toolbarRef)->text(2), qsl("Enabled"));
        QVERIFY(mpHost->getLuaInterpreter()->compileAndExecuteScript(qsl("assert(hideToolBar('qaCombinedLuaToolbar'))")));
        QTRY_COMPARE(row(toolbarRef)->text(2), qsl("Disabled"));
        QCOMPARE(row(leafRef)->text(2), qsl("Blocked by parent"));
        QVERIFY(leaf->getScript().isEmpty());
        QCOMPARE(mpEditor->mpSourceEditorEdbeeDocument->text(), draft);
        QVERIFY(mpHost->getLuaInterpreter()->compileAndExecuteScript(qsl("assert(showToolBar('qaCombinedLuaToolbar'))")));
        QTRY_COMPARE(row(toolbarRef)->text(2), qsl("Enabled"));
        QCOMPARE(row(leafRef)->text(2), qsl("Enabled"));
        QVERIFY(leaf->getScript().isEmpty());
        QCOMPARE(mpEditor->mpSourceEditorEdbeeDocument->text(), draft);
    }
    void test_toolbarVisibilityDoesNotInvalidateOriginalEditor()
    {
        auto* toolbar = button(qsl("qaNativeToolbarNotification"));
        button(qsl("qaNativeToolbarButton"), toolbar, false);
        auto* leaf = script(qsl("qaNativeToolbarDraft"), nullptr, false);
        showCombined();
        QVERIFY(view()->editItem({EditorViewType::cmScriptView, leaf->getID()}, true));
        QCoreApplication::processEvents();
        QVERIFY(!view()->active());
        QVERIFY(!mpEditor->mNeedUpdateData);
        auto* selected = mpEditor->treeWidget_scripts->currentItem();
        QVERIFY(selected);
        QCOMPARE(selected->data(0, Qt::UserRole).toInt(), leaf->getID());
        const QString draft = qsl("-- original view unsaved draft\n");
        mpEditor->mpSourceEditorEdbeeDocument->setText(draft);
        QSignalSpy structuralChanges(mpHost, &Host::signal_itemsChangedByScript);
        QVERIFY(structuralChanges.isValid());
        QVERIFY(toolbar->isActive());

        for (const auto& operation : {qsl("hideToolBar"), qsl("showToolBar")}) {
            QVERIFY(mpHost->getLuaInterpreter()->compileAndExecuteScript(qsl("assert(%1('qaNativeToolbarNotification'))").arg(operation)));
            QCOMPARE(toolbar->isActive(), operation == qsl("showToolBar"));
            QCoreApplication::processEvents();
            QCOMPARE(structuralChanges.count(), 0);
            QVERIFY(!mpEditor->mNeedUpdateData);
            QCOMPARE(mpEditor->treeWidget_scripts->currentItem(), selected);
            QCOMPARE(mpEditor->mpSourceEditorEdbeeDocument->text(), draft);
            QVERIFY(leaf->getScript().isEmpty());
        }
    }

    void test_variablesAreOptionalAndOnlyStringTablesMerge()
    {
        auto* folder = trigger(qsl("qaCombinedVars"));
        auto* numericName = trigger(qsl("1"), folder);
        auto* leaf = trigger(qsl("number"), folder, false);
        QVERIFY(mpHost->getLuaInterpreter()->compileAndExecuteScript(qsl("qaCombinedVars = {['1'] = {}, [1] = {}, number = 17, nested = {}}")));
        showCombined();
        auto* include = mpEditor->findChild<QCheckBox*>(qsl("combinedIncludeVariables"));
        QVERIFY(include);
        QVERIFY(!include->isChecked());
        QVERIFY(!variable({qsl("qaCombinedVars")}).id);
        QVERIFY(includeVariables());
        const auto table = variable({qsl("qaCombinedVars")});
        const auto stringTable = variable({qsl("qaCombinedVars"), qsl("1")});
        const auto numberTable = variable({qsl("qaCombinedVars"), qsl("1")}, LUA_TNUMBER);
        QVERIFY(table.id && stringTable.id && numberTable.id);
        QCOMPARE(row(table), row({EditorViewType::cmTriggerView, folder->getID()}));
        QCOMPARE(row(stringTable), row({EditorViewType::cmTriggerView, numericName->getID()}));
        QVERIFY(row(numberTable) != row(stringTable));
        std::unique_ptr<QMenu> numericMenu(view()->createMenu(row(numberTable)));
        for (auto* action : numericMenu->actions())
            QVERIFY(!action->text().startsWith(qsl("Create matching")));
        QVERIFY(row(variable({qsl("qaCombinedVars"), qsl("number")})) != row({EditorViewType::cmTriggerView, leaf->getID()}));
        QVERIFY(!row(variable({qsl("qaCombinedVars"), qsl("number")}))->icon(0).isNull());
        QVERIFY(mpHost->getLuaInterpreter()->compileAndExecuteScript(qsl("assert(qaCombinedVars.number == 17 and qaCombinedVars[1] ~= qaCombinedVars['1'])")));
    }

    void test_variableEditsUseNativeFormsAndPreserveOtherDrafts()
    {
        auto* item = script(qsl("qaVariableNeighbor"), nullptr, false);
        QVERIFY(mpHost->getLuaInterpreter()->compileAndExecuteScript(qsl("qaCombinedEdit = {value = 'before'}")));
        QVERIFY(includeVariables());
        const Ref scriptRef{EditorViewType::cmScriptView, item->getID()};
        select(scriptRef);
        mpEditor->mpSourceEditorEdbeeDocument->setText(qsl("-- unrelated script draft"));
        const auto ref = variable({qsl("qaCombinedEdit"), qsl("value")});
        QVERIFY(ref.id);
        select(ref);
        QCOMPARE(item->getScript(), qsl("-- unrelated script draft"));
        QCOMPARE(mpEditor->mCurrentView, EditorViewType::cmVarsView);
        QCOMPARE(mpEditor->mpSourceEditorEdbeeDocument->text(), qsl("before"));
        QVERIFY(!mpEditor->mpUndoAction->isEnabled());
        QVERIFY(!mpEditor->mpAction_toggleActive->isEnabled());
        QVERIFY(!mpEditor->mpExportAction->isEnabled());
        mpEditor->mpSourceEditorEdbeeDocument->setText(qsl("after"));
        mpEditor->slot_saveSelectedItem();
        QVERIFY(mpHost->getLuaInterpreter()->compileAndExecuteScript(qsl("assert(qaCombinedEdit.value == 'after')")));
        QVERIFY(view()->active());
        auto* saved = mpEditor->findChild<QCheckBox*>(qsl("combinedVariableSaved"));
        QVERIFY(saved && saved->isVisible());
        saved->click();
        QVERIFY(mpHost->getLuaInterface()->getVarUnit()->savedVars.contains(qsl("qaCombinedEdit.value")));
        QVERIFY(row(ref)->text(2).contains(qsl("Saved")));
        select(scriptRef);
        QCOMPARE(mpEditor->mpSourceEditorEdbeeDocument->text(), qsl("-- unrelated script draft"));
    }

    void test_variableRefreshPreservesDraftAndRejectsExternalChanges()
    {
        QVERIFY(mpHost->getLuaInterpreter()->compileAndExecuteScript(qsl("qaCombinedRefresh = {value = 'before'}")));
        QVERIFY(includeVariables());
        auto ref = variable({qsl("qaCombinedRefresh"), qsl("value")});
        QVERIFY(ref.id);
        select(ref);
        mpEditor->mpSourceEditorEdbeeDocument->setText(qsl("my draft"));
        auto* refresh = mpEditor->findChild<QPushButton*>(qsl("combinedRefreshVariables"));
        QVERIFY(refresh);
        refresh->click();
        QCOMPARE(mpEditor->mpSourceEditorEdbeeDocument->text(), qsl("my draft"));
        QVERIFY(mpHost->getLuaInterpreter()->compileAndExecuteScript(qsl("assert(qaCombinedRefresh.value == 'before')")));
        mpEditor->slot_saveSelectedItem();
        QVERIFY(mpHost->getLuaInterpreter()->compileAndExecuteScript(qsl("assert(qaCombinedRefresh.value == 'my draft')")));
        mpEditor->mpSourceEditorEdbeeDocument->setText(qsl("second draft"));
        QVERIFY(mpHost->getLuaInterpreter()->compileAndExecuteScript(qsl("qaCombinedRefresh.value = 'from Lua'")));
        mpEditor->slot_saveSelectedItem();
        QVERIFY(mpHost->getLuaInterpreter()->compileAndExecuteScript(qsl("assert(qaCombinedRefresh.value == 'from Lua')")));
        QCOMPARE(mpEditor->mpSourceEditorEdbeeDocument->text(), qsl("second draft"));
        refresh->click();
        QCOMPARE(mpEditor->mpSourceEditorEdbeeDocument->text(), qsl("second draft"));
        QVERIFY(!mpEditor->mSaveItem->isEnabled());
        QVERIFY(!mpEditor->mpCurrentVarItem);
    }

    void test_replacedTablesAndStaleMenusCannotModifyNewTargets()
    {
        QVERIFY(mpHost->getLuaInterpreter()->compileAndExecuteScript(qsl("qaCombinedReplace = {value = 'same'}")));
        QVERIFY(includeVariables());
        const auto ref = variable({qsl("qaCombinedReplace"), qsl("value")});
        const auto table = variable({qsl("qaCombinedReplace")});
        QVERIFY(ref.id && table.id);
        select(ref);
        mpEditor->mpSourceEditorEdbeeDocument->setText(qsl("draft"));
        std::unique_ptr<QMenu> menu(view()->createMenu(row(table)));
        auto* add = menuAction(menu.get(), table, qsl("add"));
        QVERIFY(add);
        QVERIFY(mpHost->getLuaInterpreter()->compileAndExecuteScript(qsl("qaCombinedReplace = {value = 'same'}; collectgarbage('collect')")));
        mpEditor->slot_saveSelectedItem();
        QVERIFY(mpHost->getLuaInterpreter()->compileAndExecuteScript(qsl("assert(qaCombinedReplace.value == 'same')")));
        mpEditor->findChild<QPushButton*>(qsl("combinedRefreshVariables"))->click();
        add->trigger();
        QVERIFY(mpHost->getLuaInterpreter()->compileAndExecuteScript(qsl("assert(qaCombinedReplace.variable_name == nil)")));
        QCOMPARE(mpEditor->mpSourceEditorEdbeeDocument->text(), qsl("draft"));
        // Deliberately select the fresh row after retaining the conflict text above.
        const auto fresh = variable({qsl("qaCombinedReplace"), qsl("value")});
        QVERIFY(fresh.id);
        QVERIFY(view()->editItem(fresh));
    }

    void test_variableMenusAddOnlyToTheChosenTableAndKeepNativeLimits()
    {
        auto* folder = trigger(qsl("qaCombinedAddVars"));
        QVERIFY(mpHost->getLuaInterpreter()->compileAndExecuteScript(qsl("qaCombinedAddVars = {}; qaCombinedReadOnly = function() end")));
        QVERIFY(includeVariables());
        const auto table = variable({qsl("qaCombinedAddVars")});
        QVERIFY(table.id);
        std::unique_ptr<QMenu> menu(view()->createMenu(row(table)));
        auto* add = menuAction(menu.get(), table, qsl("add"));
        QVERIFY(add);
        QVERIFY(!menuAction(menu.get(), table, qsl("toggle")));
        QVERIFY(!menuAction(menu.get(), table, qsl("paste")));
        QVERIFY(!menuAction(menu.get(), table, qsl("export")));
        add->trigger();
        QCOMPARE(folder->getChildrenList()->size(), 0U);
        QVERIFY(mpHost->getLuaInterpreter()->compileAndExecuteScript(qsl("assert(qaCombinedAddVars.variable_name ~= nil)")));
        const auto function = variable({qsl("qaCombinedReadOnly")});
        QVERIFY(function.id);
        select(function);
        QVERIFY(!mpEditor->mpSourceEditorEdbee->isEnabled());
        auto* saved = mpEditor->findChild<QCheckBox*>(qsl("combinedVariableSaved"));
        QVERIFY(saved && !saved->isEnabled());
        auto* onlyTriggers = trigger(qsl("qaNoImplicitLuaTable"));
        showCombined();
        std::unique_ptr<QMenu> missing(view()->createMenu(row({EditorViewType::cmTriggerView, onlyTriggers->getID()})));
        for (auto* action : missing->actions())
            QVERIFY(!action->text().contains(qsl("matching Variable")));
        QVERIFY(mpHost->getLuaInterpreter()->compileAndExecuteScript(qsl("assert(qaNoImplicitLuaTable == nil)")));
    }

    void test_variableSearchAndHiddenControlsStayInCombined()
    {
        QVERIFY(mpHost->getLuaInterpreter()->compileAndExecuteScript(qsl("qaCombinedSearchVariable = 'needle'")));
        QVERIFY(includeVariables());
        mpEditor->mSearchOptions |= enums::EditorSearchOptionIncludeVariables;
        mpEditor->comboBox_searchTerms->clear();
        mpEditor->comboBox_searchTerms->addItem(qsl("qaCombinedSearchVariable"));
        mpEditor->slot_searchMudletItems(0);
        auto* result = mpEditor->treeWidget_searchResults->topLevelItem(0);
        QVERIFY(result);
        mpEditor->slot_itemSelectedInSearchResults(result);
        QVERIFY(view()->active());
        QCOMPARE(mpEditor->mCurrentView, EditorViewType::cmVarsView);
        QCOMPARE(mpEditor->mpSourceEditorEdbeeDocument->text(), qsl("needle"));
        mpEditor->mpVarsMainArea->checkBox_variable_hidden->click();
        QTRY_VERIFY(!variable({qsl("qaCombinedSearchVariable")}).id);
        mpEditor->checkBox_displayAllVariables->setChecked(true);
        QTRY_VERIFY(variable({qsl("qaCombinedSearchVariable")}).id);
        mpEditor->slot_showVariables();
        QVERIFY(!view()->active());
        QVERIFY(mpEditor->treeWidget_variables->isVisible());
    }

    void test_switchingToOriginalViewCannotOverwriteChangedLua()
    {
        QVERIFY(mpHost->getLuaInterpreter()->compileAndExecuteScript(qsl("qaCombinedNavigation = 'before'")));
        QVERIFY(includeVariables());
        const auto ref = variable({qsl("qaCombinedNavigation")});
        QVERIFY(ref.id);
        select(ref);
        mpEditor->mpSourceEditorEdbeeDocument->setText(qsl("draft"));
        QVERIFY(mpHost->getLuaInterpreter()->compileAndExecuteScript(qsl("qaCombinedNavigation = 'from Lua'")));
        mpEditor->slot_showScripts();
        QVERIFY(mpHost->getLuaInterpreter()->compileAndExecuteScript(qsl("assert(qaCombinedNavigation == 'from Lua')")));
        QVERIFY(view()->active());
        QCOMPARE(mpEditor->mpSourceEditorEdbeeDocument->text(), qsl("draft"));
    }

    void test_disablingVariablesCannotOverwriteChangedLua()
    {
        QVERIFY(mpHost->getLuaInterpreter()->compileAndExecuteScript(qsl("qaCombinedIncludeToggle = {value = 'before'}")));
        QVERIFY(includeVariables());
        const auto ref = variable({qsl("qaCombinedIncludeToggle"), qsl("value")});
        QVERIFY(ref.id);
        select(ref);
        QCOMPARE(mpEditor->mpSourceEditorEdbeeDocument->text(), qsl("before"));
        QVERIFY(mpEditor->mSaveItem->isEnabled());
        mpEditor->mpSourceEditorEdbeeDocument->setText(qsl("my draft"));
        QVERIFY(mpHost->getLuaInterpreter()->compileAndExecuteScript(qsl("qaCombinedIncludeToggle.value = 'from Lua'")));
        auto* include = mpEditor->findChild<QCheckBox*>(qsl("combinedIncludeVariables"));
        auto* refresh = mpEditor->findChild<QPushButton*>(qsl("combinedRefreshVariables"));
        QVERIFY(include && include->isChecked());
        QVERIFY(refresh && refresh->isEnabled());

        include->click();

        QVERIFY(mpHost->getLuaInterpreter()->compileAndExecuteScript(qsl("assert(qaCombinedIncludeToggle.value == 'from Lua')")));
        QVERIFY(include->isChecked());
        QVERIFY(view()->active() && view()->mpTree->isVisible());
        QCOMPARE(mpEditor->mCurrentView, EditorViewType::cmVarsView);
        QVERIFY(mpEditor->mpVarsMainArea->isVisible());
        QVERIFY(!view()->mpSummary->isVisible());
        QCOMPARE(mpEditor->mpSourceEditorEdbeeDocument->text(), qsl("my draft"));
        QVERIFY(!mpEditor->mpCurrentVarItem);
        QVERIFY(!mpEditor->mSaveItem->isEnabled());
        QVERIFY(refresh->isEnabled());

        include->click();
        QVERIFY(include->isChecked());
        QCOMPARE(mpEditor->mpSourceEditorEdbeeDocument->text(), qsl("my draft"));
        QVERIFY(mpHost->getLuaInterpreter()->compileAndExecuteScript(qsl("assert(qaCombinedIncludeToggle.value == 'from Lua')")));
        refresh->click();
        QCOMPARE(mpEditor->mpSourceEditorEdbeeDocument->text(), qsl("my draft"));
        const auto fresh = variable({qsl("qaCombinedIncludeToggle"), qsl("value")});
        QVERIFY(fresh.id);
        QVERIFY(view()->editItem(fresh));
        QCOMPARE(mpEditor->mpSourceEditorEdbeeDocument->text(), qsl("from Lua"));
        include->click();
        QVERIFY(!include->isChecked());
        QVERIFY(mpHost->getLuaInterpreter()->compileAndExecuteScript(qsl("assert(qaCombinedIncludeToggle.value == 'from Lua')")));
    }

    void test_disablingVariablesSavesUncontestedDraft()
    {
        QVERIFY(mpHost->getLuaInterpreter()->compileAndExecuteScript(qsl("qaCombinedIncludeSave = 'before'")));
        QVERIFY(includeVariables());
        const auto ref = variable({qsl("qaCombinedIncludeSave")});
        QVERIFY(ref.id);
        select(ref);
        QCOMPARE(mpEditor->mpSourceEditorEdbeeDocument->text(), qsl("before"));
        mpEditor->mpSourceEditorEdbeeDocument->setText(qsl("my draft"));
        auto* include = mpEditor->findChild<QCheckBox*>(qsl("combinedIncludeVariables"));
        auto* refresh = mpEditor->findChild<QPushButton*>(qsl("combinedRefreshVariables"));
        QVERIFY(include && include->isChecked());
        QVERIFY(refresh && refresh->isEnabled());

        include->click();

        QVERIFY(mpHost->getLuaInterpreter()->compileAndExecuteScript(qsl("assert(qaCombinedIncludeSave == 'my draft')")));
        QVERIFY(!include->isChecked());
        QVERIFY(view()->active());
        QCOMPARE(mpEditor->mCurrentView, EditorViewType::cmUnknownView);
        QVERIFY(!refresh->isEnabled());
        QVERIFY(!variable({qsl("qaCombinedIncludeSave")}).id);
        include->click();
        const auto fresh = variable({qsl("qaCombinedIncludeSave")});
        QVERIFY(fresh.id);
        select(fresh);
        QCOMPARE(mpEditor->mpSourceEditorEdbeeDocument->text(), qsl("my draft"));
    }

    void test_variableSearchDistinguishesNumberAndStringKeys()
    {
        QVERIFY(mpHost->getLuaInterpreter()->compileAndExecuteScript(qsl("qaCombinedSearchKeys = {[1] = 'numberKeyNeedle', ['1'] = 'stringKeyNeedle'}")));
        QVERIFY(includeVariables());
        mpEditor->mSearchOptions |= enums::EditorSearchOptionIncludeVariables;
        for (const auto& entry : QList<QPair<QString, int>>{{qsl("numberKeyNeedle"), LUA_TNUMBER}, {qsl("stringKeyNeedle"), LUA_TSTRING}}) {
            mpEditor->comboBox_searchTerms->clear();
            mpEditor->comboBox_searchTerms->addItem(entry.first);
            mpEditor->slot_searchMudletItems(0);
            auto* result = mpEditor->treeWidget_searchResults->topLevelItem(0);
            QVERIFY(result);
            mpEditor->slot_itemSelectedInSearchResults(result);
            QVERIFY(view()->active());
            const auto ref = variable({qsl("qaCombinedSearchKeys"), qsl("1")}, entry.second);
            QVERIFY(ref.id);
            QVERIFY(view()->currentEditorItem() == ref);
        }
    }

    void test_variableRecastAndSnapshotResetRetainLiveData()
    {
        QVERIFY(mpHost->getLuaInterpreter()->compileAndExecuteScript(qsl("qaCombinedGrowing = {}; qaCombinedSnapshot = 'before'")));
        QVERIFY(includeVariables());
        const auto table = variable({qsl("qaCombinedGrowing")});
        QVERIFY(table.id);
        QVERIFY(view()->editItem(table));
        auto* type = mpEditor->mpVarsMainArea->comboBox_variable_value_type;
        QVERIFY(type->isEnabled());
        type->setCurrentIndex(type->findData(LUA_TSTRING));
        mpEditor->mpSourceEditorEdbeeDocument->setText(qsl("replacement"));
        QVERIFY(mpHost->getLuaInterpreter()->compileAndExecuteScript(qsl("qaCombinedGrowing.arrived = 'late'")));
        mpEditor->slot_saveSelectedItem();
        QVERIFY(mpHost->getLuaInterpreter()->compileAndExecuteScript(qsl("assert(type(qaCombinedGrowing) == 'table' and qaCombinedGrowing.arrived == 'late')")));
        QCOMPARE(mpEditor->mpSourceEditorEdbeeDocument->text(), qsl("replacement"));
        const auto snapshot = variable({qsl("qaCombinedSnapshot")});
        QVERIFY(snapshot.id);
        QVERIFY(view()->editItem(snapshot));
        mpEditor->mpSourceEditorEdbeeDocument->setText(qsl("snapshot draft"));
        mpHost->getLuaInterface()->getVars(false);
        view()->refresh();
        mpEditor->slot_saveSelectedItem();
        QVERIFY(mpHost->getLuaInterpreter()->compileAndExecuteScript(qsl("assert(qaCombinedSnapshot == 'before')")));
        QCOMPARE(mpEditor->mpSourceEditorEdbeeDocument->text(), qsl("snapshot draft"));
        QVERIFY(!mpEditor->mSaveItem->isEnabled());
        QVERIFY(!mpEditor->mpCurrentVarItem);
    }

    void test_sectionShortcutsCycleThroughAllItemTypes()
    {
        auto* triggerLeaf = trigger(qsl("qaSectionTrigger"), nullptr, false);
        auto* aliasLeaf = new TAlias(qsl("qaSectionAlias"), mpHost);
        aliasLeaf->setIsFolder(false);
        aliasLeaf->setRegexCode(qsl("^qaSectionAlias$"));
        aliasLeaf->registerAlias();
        auto* scriptLeaf = script(qsl("qaSectionScript"), nullptr, false);
        auto* timerLeaf = new TTimer(qsl("qaSectionTimer"), QTime(1, 0), mpHost);
        timerLeaf->setIsFolder(false);
        mpHost->getTimerUnit()->registerTimer(timerLeaf);
        auto* keyLeaf = key(qsl("qaSectionKey"), nullptr, false);
        auto* buttonLeaf = button(qsl("qaSectionButton"), button(qsl("qaSectionToolbar")), false);
        QVERIFY(mpHost->getLuaInterpreter()->compileAndExecuteScript(qsl("qaSectionVariable = 'before'")));
        QVERIFY(includeVariables());
        const auto var = variable({qsl("qaSectionVariable")});
        QVERIFY(var.id);
        const QList<QPair<Ref, QWidget*>> cases{{{EditorViewType::cmTriggerView, triggerLeaf->getID()}, mpEditor->mpTriggersMainArea->lineEdit_trigger_name},
                                                {{EditorViewType::cmAliasView, aliasLeaf->getID()}, mpEditor->mpAliasMainArea->lineEdit_alias_name},
                                                {{EditorViewType::cmScriptView, scriptLeaf->getID()}, mpEditor->mpScriptsMainArea->lineEdit_script_name},
                                                {{EditorViewType::cmTimerView, timerLeaf->getID()}, mpEditor->mpTimersMainArea->lineEdit_timer_name},
                                                {{EditorViewType::cmKeysView, keyLeaf->getID()}, mpEditor->mpKeysMainArea->lineEdit_key_name},
                                                {{EditorViewType::cmActionView, buttonLeaf->getID()}, mpEditor->mpActionsMainArea->lineEdit_action_name},
                                                {var, mpEditor->mpVarsMainArea->lineEdit_var_name}};
        auto* tree = view()->mpTree;
        for (const auto& entry : cases) {
            QVERIFY(view()->editItem(entry.first));
            QCoreApplication::processEvents();
            const QString original = mpEditor->mpSourceEditorEdbeeDocument->text();
            const QString draft = qsl("-- section navigation draft");
            mpEditor->mpSourceEditorEdbeeDocument->setText(draft);
            mpEditor->activateWindow();
            tree->setFocus();
            QTRY_VERIFY_WITH_TIMEOUT(focusWithin(tree), 1000);
            sectionShortcut(true);
            QVERIFY(focusWithin(entry.second));
            if (entry.first.type == EditorViewType::cmTriggerView) {
                sectionShortcut(true);
                QVERIFY(focusWithin(mpEditor->mTriggerPatternEdit.first()->singleLineTextEdit_pattern));
            }
            sectionShortcut(true);
            QVERIFY(focusWithin(mpEditor->mpSourceEditorEdbee));
            sectionShortcut(true);
            QVERIFY(focusWithin(tree));
            sectionShortcut(false);
            QVERIFY(focusWithin(mpEditor->mpSourceEditorEdbee));
            if (entry.first.type == EditorViewType::cmTriggerView) {
                sectionShortcut(false);
                QVERIFY(focusWithin(mpEditor->mTriggerPatternEdit.first()->singleLineTextEdit_pattern));
            }
            sectionShortcut(false);
            QVERIFY(focusWithin(entry.second));
            sectionShortcut(false);
            QVERIFY(focusWithin(tree));
            QVERIFY(view()->active());
            QVERIFY(view()->currentEditorItem() == entry.first);
            QVERIFY(!view()->originalTree(entry.first.type)->isVisible());
            QCOMPARE(mpEditor->mpSourceEditorEdbeeDocument->text(), draft);
            QVERIFY(scriptLeaf->getScript().isEmpty());
            QVERIFY(mpHost->getLuaInterpreter()->compileAndExecuteScript(qsl("assert(qaSectionVariable == 'before')")));
            mpEditor->mpSourceEditorEdbeeDocument->setText(original);
        }
    }

    void test_sectionShortcutsReachFolderChoicesAndHandleRoot()
    {
        auto* folder = trigger(qsl("qaSectionFolder"));
        script(qsl("qaSectionFolder"));
        QVERIFY(mpHost->getLuaInterpreter()->compileAndExecuteScript(qsl("qaSectionFolder = {value = 'before'}")));
        QVERIFY(includeVariables());
        select({EditorViewType::cmTriggerView, folder->getID()});
        QCoreApplication::processEvents();
        auto* tree = view()->mpTree;
        QVERIFY(view()->mpSummary->isVisible());
        const auto choices = view()->mpSummary->findChildren<QPushButton*>();
        QCOMPARE(choices.size(), 3);
        const QPointer<QPushButton> firstChoice = choices.first();
        QTRY_VERIFY_WITH_TIMEOUT(firstChoice && firstChoice->isVisible(), 1000);
        mpEditor->activateWindow();
        tree->setFocus();
        QTRY_VERIFY_WITH_TIMEOUT(focusWithin(tree), 1000);
        sectionShortcut(true);
        QVERIFY(firstChoice);
        QCOMPARE(QApplication::focusWidget(), static_cast<QWidget*>(firstChoice.data()));
        QTest::keyClick(QApplication::focusWidget(), Qt::Key_Tab);
        QVERIFY(focusWithin(choices.at(1)));
        sectionShortcut(true);
        QVERIFY(focusWithin(tree));
        sectionShortcut(false);
        QVERIFY(focusWithin(choices.first()));
        sectionShortcut(false);
        QVERIFY(focusWithin(tree));
        QCOMPARE(mpEditor->mCurrentView, EditorViewType::cmUnknownView);
        QVERIFY(mpHost->getLuaInterpreter()->compileAndExecuteScript(qsl("assert(qaSectionFolder.value == 'before')")));

        tree->setCurrentItem(tree->topLevelItem(0));
        QCoreApplication::processEvents();
        QVERIFY(view()->mpSummary->findChildren<QPushButton*>().isEmpty());
        tree->setFocus();
        sectionShortcut(true);
        QVERIFY(focusWithin(tree));
        sectionShortcut(false);
        QVERIFY(focusWithin(tree));
    }

    void test_sectionShortcutsSkipUnavailableEditors()
    {
        QVERIFY(mpHost->getLuaInterpreter()->compileAndExecuteScript(qsl("qaSectionTable = {value = 'before'}; qaSectionFunction = function() end; qaSectionDisabled = 'before'")));
        QVERIFY(includeVariables());
        auto* tree = view()->mpTree;
        auto* name = mpEditor->mpVarsMainArea->lineEdit_var_name;
        for (const QString& value : {qsl("qaSectionTable"), qsl("qaSectionFunction"), qsl("qaSectionDisabled")}) {
            const auto ref = variable({value});
            QVERIFY(ref.id);
            QVERIFY(view()->editItem(ref));
            QCoreApplication::processEvents();
            if (value == qsl("qaSectionDisabled")) {
                QVERIFY(mpEditor->mpSourceEditorArea->isVisible());
                mpEditor->mpSourceEditorEdbee->setEnabled(false);
            } else {
                QVERIFY(!mpEditor->mpSourceEditorArea->isVisible());
            }
            tree->setFocus();
            sectionShortcut(true);
            QVERIFY(focusWithin(name));
            sectionShortcut(true);
            QVERIFY(focusWithin(tree));
            sectionShortcut(false);
            QVERIFY(focusWithin(name));
            sectionShortcut(false);
            QVERIFY(focusWithin(tree));
        }
        mpEditor->mpSourceEditorEdbee->setEnabled(true);
        auto* package = button(qsl("qaSectionPackage"));
        package->mPackageName = qsl("qaSectionPackage");
        showCombined();
        QVERIFY(view()->editItem({EditorViewType::cmActionView, package->getID()}));
        QCoreApplication::processEvents();
        QVERIFY(!mpEditor->mpActionsMainArea->lineEdit_action_name->isVisible());
        QVERIFY(!mpEditor->mpSourceEditorArea->isVisible());
        auto* stylesheet = mpEditor->mpActionsMainArea->plainTextEdit_action_css;
        QVERIFY(stylesheet->isVisible());
        tree->setFocus();
        sectionShortcut(true);
        QVERIFY(focusWithin(stylesheet));
        sectionShortcut(true);
        QVERIFY(focusWithin(tree));
        sectionShortcut(false);
        QVERIFY(focusWithin(stylesheet));
        sectionShortcut(false);
        QVERIFY(focusWithin(tree));
        mpEditor->mpActionsMainArea->setEnabled(false);
        sectionShortcut(true);
        QVERIFY(focusWithin(tree));
        sectionShortcut(false);
        QVERIFY(focusWithin(tree));
        mpEditor->mpActionsMainArea->setEnabled(true);
    }

    void test_sectionShortcutsKeepOriginalViewRouting()
    {
        auto* item = script(qsl("qaSectionOriginal"), nullptr, false);
        showCombined();
        QVERIFY(view()->editItem({EditorViewType::cmScriptView, item->getID()}, true));
        QCoreApplication::processEvents();
        QVERIFY(!view()->active());
        auto* tree = mpEditor->treeWidget_scripts;
        auto* name = mpEditor->mpScriptsMainArea->lineEdit_script_name;
        QVERIFY(tree->isVisible());
        tree->setFocus();
        sectionShortcut(true);
        QVERIFY(focusWithin(name));
        sectionShortcut(true);
        QVERIFY(focusWithin(mpEditor->mpSourceEditorEdbee));
        sectionShortcut(true);
        QVERIFY(focusWithin(tree));
        sectionShortcut(false);
        QVERIFY(focusWithin(mpEditor->mpSourceEditorEdbee));
        sectionShortcut(false);
        QVERIFY(focusWithin(name));
        sectionShortcut(false);
        QVERIFY(focusWithin(tree));
        QVERIFY(!view()->active());
    }

    void test_variablePreferenceRestoresWithoutSavingOrReusingLuaValues()
    {
        recreateEditor(true);
        QVERIFY(!view()->includesVariables());
        QVERIFY(mpHost->getLuaInterpreter()->compileAndExecuteScript(qsl("qaPreferenceValue = {value = 17}")));
        QVERIFY(includeVariables());
        select(variable({qsl("qaPreferenceValue"), qsl("value")}));
        mpEditor->mpSourceEditorEdbeeDocument->setText(qsl("23"));
        mpEditor->writeSettings();
        QCOMPARE(mpEditor->mpSourceEditorEdbeeDocument->text(), qsl("23"));
        QVERIFY(mpHost->getLuaInterpreter()->compileAndExecuteScript(qsl("assert(qaPreferenceValue.value == 17)")));
        mpEditor->mpSourceEditorEdbeeDocument->setText(qsl("17"));

        recreateEditor();
        QVERIFY(!view()->active());
        QCOMPARE(view()->mpTree->topLevelItemCount(), 0);
        QVERIFY(view()->mVariableRows.isEmpty());
        QVERIFY(view()->includesVariables());
        QVERIFY(mpHost->getLuaInterpreter()->compileAndExecuteScript(qsl("qaPreferenceValue.value = 29")));
        showCombined();
        select(variable({qsl("qaPreferenceValue"), qsl("value")}));
        QCOMPARE(mpEditor->mpSourceEditorEdbeeDocument->text(), qsl("29"));
        view()->mpIncludeVariables->setChecked(false);
        recreateEditor();
        QVERIFY(!view()->includesVariables());
    }

    void test_combinedPanePreferencesDoNotReplaceRegularViewWidths()
    {
        recreateEditor(true);
        mpEditor->resize(1600, 900);
        QCoreApplication::processEvents();
        const auto nativeBefore = mpEditor->splitter_main->sizes();
        showCombined();
        view()->mpTree->setColumnWidth(0, 510);
        mpEditor->splitter_main->setSizes({610, 990});
        QCoreApplication::processEvents();
        const auto combinedSizes = mpEditor->splitter_main->sizes();
        QVERIFY(combinedSizes != nativeBefore);
        recreateEditor();
        const auto reopenedNativeSizes = mpEditor->splitter_main->sizes();
        QVERIFY(!view()->active());
        showCombined();
        QCOMPARE(view()->mpTree->columnWidth(0), 510);
        QCOMPARE(mpEditor->splitter_main->sizes(), combinedSizes);
        mpEditor->slot_showTriggers();
        QCOMPARE(mpEditor->splitter_main->sizes(), reopenedNativeSizes);
        mpEditor->splitter_main->setSizes({1100, 500});
        QVERIFY(mpEditor->splitter_main->sizes() != combinedSizes);
        recreateEditor();
        showCombined();
        QCOMPARE(mpEditor->splitter_main->sizes(), combinedSizes);
        QCOMPARE(view()->mpTree->columnWidth(0), 510);
    }

    void test_folderPreferencesFollowDisplayPathsAcrossEditorRecreation()
    {
        recreateEditor(true);
        auto* literal = trigger(qsl("qaPreferences/a"));
        auto* nested = trigger(qsl("child"), literal);
        trigger(qsl("leaf"), nested, false);
        script(qsl("qaPreferences/a"));
        auto* separate = trigger(qsl("qaPreferences"));
        auto* separateChild = trigger(qsl("a"), separate);
        trigger(qsl("other"), separateChild, false);
        showCombined();
        row({EditorViewType::cmTriggerView, literal->getID()})->setExpanded(true);
        row({EditorViewType::cmTriggerView, nested->getID()})->setExpanded(true);
        row({EditorViewType::cmTriggerView, separate->getID()})->setExpanded(false);
        mpEditor->close();
        literal->setName(qsl("qaPreferences-renamed"));
        auto* replacement = trigger(qsl("qaPreferences/a"));
        auto* replacementChild = trigger(qsl("child"), replacement);
        trigger(qsl("new leaf"), replacementChild, false);
        QVERIFY(replacement->getID() != literal->getID());

        recreateEditor(false, false);
        showCombined();
        QVERIFY(row({EditorViewType::cmTriggerView, replacement->getID()})->isExpanded());
        QVERIFY(row({EditorViewType::cmTriggerView, replacementChild->getID()})->isExpanded());
        QVERIFY(!row({EditorViewType::cmTriggerView, separate->getID()})->isExpanded());
        QVERIFY(!row({EditorViewType::cmTriggerView, literal->getID()})->isExpanded());
    }

    void test_hiddenAndUnopenedFolderPreferencesSurviveWithoutForcingRootOpen()
    {
        recreateEditor(true);
        QVERIFY(mpHost->getLuaInterpreter()->compileAndExecuteScript(qsl("qaPreferenceTables = {nested = {value = 1}, [1] = {value = 2}, ['1'] = {value = 3}}")));
        QVERIFY(includeVariables());
        view()->mpTree->collapseAll();
        view()->mpTree->topLevelItem(0)->setExpanded(true);
        row(variable({qsl("qaPreferenceTables")}))->setExpanded(true);
        row(variable({qsl("qaPreferenceTables"), qsl("nested")}))->setExpanded(true);
        row(variable({qsl("qaPreferenceTables"), qsl("1")}, LUA_TNUMBER))->setExpanded(true);
        QVERIFY(!row(variable({qsl("qaPreferenceTables"), qsl("1")}))->isExpanded());
        view()->mpIncludeVariables->setChecked(false);
        recreateEditor();
        QVERIFY(!view()->active());
        QVERIFY(!view()->includesVariables());
        recreateEditor();
        QVERIFY(includeVariables());
        QVERIFY(row(variable({qsl("qaPreferenceTables")}))->isExpanded());
        QVERIFY(row(variable({qsl("qaPreferenceTables"), qsl("nested")}))->isExpanded());
        QVERIFY(row(variable({qsl("qaPreferenceTables"), qsl("1")}, LUA_TNUMBER))->isExpanded());
        QVERIFY(!row(variable({qsl("qaPreferenceTables"), qsl("1")}))->isExpanded());
        view()->mpTree->collapseAll();
        recreateEditor(false, false);
        showCombined();
        QVERIFY(!view()->mpTree->topLevelItem(0)->isExpanded());
    }

    void test_combinedPreferencesBelongToTheirProfile()
    {
        recreateEditor(true);
        QVERIFY(includeVariables());
        view()->mpTree->setColumnWidth(0, 570);
        mpEditor->close();
        auto* otherHost = TestProfile::create(qsl("CombinedPreferences-Other-Profile"), mLocalhost, QString::number(mpServer->serverPort()));
        QVERIFY(otherHost);
        QSignalSpy connectedSpy(&(otherHost->mTelnet), &cTelnet::signal_connected);
        QVERIFY(connectedSpy.wait(1s));
        mudlet::self()->slot_showScriptDialog();
        auto* otherEditor = HostDialogs::of(otherHost).mpEditorDialog.data();
        QVERIFY(otherEditor);
        auto* otherView = otherEditor->mpCombinedView;
        QVERIFY(!otherView->includesVariables());
        otherView->show();
        QVERIFY(otherView->mpTree->columnWidth(0) != 570);
        otherView->mpTree->setColumnWidth(0, 350);
        otherEditor->close();

        recreateEditor(false, false);
        QVERIFY(view()->includesVariables());
        showCombined();
        QCOMPARE(view()->mpTree->columnWidth(0), 570);
        QVERIFY(!otherView->includesVariables());
        QCOMPARE(otherView->mpTree->columnWidth(0), 350);
    }
};

#include "CombinedEditorViewTest.moc"
MUDLET_GROUPED_TEST_MAIN(CombinedEditorViewTest)
