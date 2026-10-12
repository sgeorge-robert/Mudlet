/***************************************************************************
 *   Copyright (C) 2026 by Vadim Peretokin - vadim.peretokin@mudlet.org    *
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

// Key capture and the editor action shortcuts have no Lua entry point.

#include <QAction>
#include <QFontDatabase>
#include <QFontInfo>
#include <QTemporaryDir>
#include <QToolBar>
#include <QTreeWidget>
#include <QtTest/QtTest>
#include <chrono>

#include "Host.h"
#include "HostDialogs.h"
#include "KeyUnit.h"
#include "MudletInstanceCoordinator.h"
#include "MudletApp.h"
#include "PortableModeTestHelper.h"
#include "ProfileTestHelper.h"
#include "TKey.h"
#include "TelnetServerStub.h"
#include "ctelnet.h"
#include "dlgKeysMainArea.h"
#include "dlgTriggerEditor.h"
#include "mudlet.h"

#include "GroupedTest.h"

using namespace std::chrono_literals;

class EditorKeyCaptureTest : public QObject
{
    Q_OBJECT

private:
    QTemporaryDir mConfigDir;
    QByteArray mSavedXdg;
    TelnetServerStub* mpServer = nullptr;
    Host* mpHost = nullptr;
    dlgTriggerEditor* mpEditor = nullptr;
    const QString mProfileName = qsl("EditorKeyCapture-Test-Profile");
    const QString mLocalhost = qsl("localhost");

    void deleteProfileDirectory(const QString& profileName)
    {
        QDir dir(MudletApp::getMudletPath(enums::profileHomePath, profileName));
        if (dir.exists()) {
            dir.removeRecursively();
        }
    }

    QMap<QAction*, QList<QKeySequence>> mShortcuts;
    TKey* mpKey = nullptr;

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
        mpEditor->slot_showKeys();
        mpEditor->treeWidget_keys->setCurrentItem(mpEditor->mpKeyBaseItem);
        mpEditor->slot_addNewItem();
        auto* row = mpEditor->treeWidget_keys->currentItem();
        QVERIFY(row);
        mpKey = mpHost->getKeyUnit()->getKey(row->data(0, Qt::UserRole).toInt());
        QVERIFY(mpKey);
        mpEditor->keyGrabCallback(Qt::Key_F9, Qt::NoModifier);
        mpEditor->slot_saveEdits();

        // Real actions can have translated/category-dependent labels and alternate bindings.
        mpEditor->mSaveItem->setText(qsl("Capture test save label"));
        mpEditor->mSaveItem->setShortcuts({QKeySequence(Qt::CTRL | Qt::Key_S), QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_S)});
        for (auto* action : mpEditor->toolBar->actions() + mpEditor->toolBar2->actions()) {
            mShortcuts.insert(action, action->shortcuts());
        }
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

    void init()
    {
        mpEditor->keyGrabCallback(Qt::Key_F9, Qt::NoModifier);
        for (auto it = mShortcuts.cbegin(); it != mShortcuts.cend(); ++it) {
            it.key()->setShortcuts(it.value());
        }
    }

    void test_captureRestoresActionShortcuts_data()
    {
        QTest::addColumn<bool>("cancel");
        QTest::newRow("accept") << false;
        QTest::newRow("cancel") << true;
    }

    void test_captureRestoresActionShortcuts()
    {
        QFETCH(bool, cancel);
        QVERIFY(mpEditor->treeWidget_keys->isVisible());
        QVERIFY(!mpEditor->mAddItem->shortcuts().isEmpty());
        QVERIFY(!mpEditor->mDeleteItem->shortcuts().isEmpty());
        QCOMPARE(mpEditor->mSaveItem->shortcuts().size(), 2);
        QCOMPARE(mpKey->getKeyCode(), Qt::Key_F9);
        QCOMPARE(mpKey->getKeyModifiers(), Qt::NoModifier);
        QVERIFY(!mpEditor->mIsGrabKey);

        auto* grab = mpEditor->mpKeysMainArea->pushButton_key_grabKey;
        QTest::mouseClick(grab, Qt::LeftButton);
        QVERIFY(mpEditor->mIsGrabKey);
        for (auto* action : mShortcuts.keys()) {
            QVERIFY(action->shortcuts().isEmpty());
        }
        QTest::keyClick(grab, cancel ? Qt::Key_Escape : Qt::Key_S, cancel ? Qt::NoModifier : Qt::ControlModifier);
        QVERIFY(!mpEditor->mIsGrabKey);
        QCOMPARE(mpKey->getKeyCode(), cancel ? Qt::Key_F9 : Qt::Key_S);
        QCOMPARE(mpKey->getKeyModifiers(), cancel ? Qt::NoModifier : Qt::ControlModifier);
        for (auto it = mShortcuts.cbegin(); it != mShortcuts.cend(); ++it) {
            QCOMPARE(it.key()->shortcuts(), it.value());
        }
    }
};

#include "EditorKeyCaptureTest.moc"
MUDLET_GROUPED_TEST_MAIN(EditorKeyCaptureTest)
