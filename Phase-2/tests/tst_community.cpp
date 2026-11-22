#include "HubClient.h"
#include "Overflow.h"
#include "TeamForgeController.h"

#include <QBuffer>
#include <QDir>
#include <QImage>
#include <QPainter>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQuickItem>
#include <QQuickWindow>
#include <QTemporaryDir>
#include <QSignalSpy>
#include <QTest>
#include <QtQml/QQmlExtensionPlugin>

#include <functional>
#include <memory>

Q_IMPORT_QML_PLUGIN(TeamForgePlugin)

// The Community screens against a real TeamForge server (web/server.py), offscreen.
//
// Needs a local server filled by web/tools/demo_hub_data.py:
//   TEAMFORGE_TEST_SERVER=http://127.0.0.1:8790  (skipped when unset)
//   TEAMFORGE_TEST_EMAIL / TEAMFORGE_TEST_PASSWORD (default: the demo team leader)
//   TEAMFORGE_SHOTS=<folder>                       (optional: a screenshot per screen)
// Signs in as the leader of "Rocket Lab", opens every Community screen, checks what each shows,
// sends a chat message, and fails on any QML warning.
namespace {

QStringList& qmlWarnings()
{
    static QStringList warnings;
    return warnings;
}

QtMessageHandler previousHandler = nullptr;

void collect(QtMsgType type, const QMessageLogContext& context, const QString& message)
{
    // (Windows reports a window larger than the screen as a "setGeometry" warning naming the QML type.)
    if (type != QtDebugMsg && type != QtInfoMsg && !message.startsWith(QLatin1String("setGeometry"))
        && (message.contains(QLatin1String(".qml")) || message.contains(QLatin1String("QML"))))
        qmlWarnings().append(message);
    if (previousHandler)
        previousHandler(type, context, message);
}

} // namespace

class TestCommunity : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void signsInAndShowsCounts();
    void everyScreenOpens_data();
    void everyScreenOpens();
    void chatSendsAndShowsMessages();
    void anonymousInboxHidesTheSender();
    void samePhotoEverywhere();
    void noQmlWarnings();

private:
    bool anyVisibleTextContaining(const QString& part) const;
    void shot(const QString& name);

    std::unique_ptr<QTemporaryDir> dir_;
    std::unique_ptr<QQmlApplicationEngine> engine_;
    QQuickWindow *window_ = nullptr;
    HubClient *hub_ = nullptr;
    int teamId_ = 0;
};

void TestCommunity::initTestCase()
{
    const QByteArray server = qgetenv("TEAMFORGE_TEST_SERVER");
    if (server.isEmpty())
        QSKIP("Set TEAMFORGE_TEST_SERVER to a local server filled by web/tools/demo_hub_data.py");
    previousHandler = qInstallMessageHandler(collect);
    dir_ = std::make_unique<QTemporaryDir>();
    qputenv("TEAMFORGE_DATA_DIR", dir_->path().toUtf8());
    engine_ = std::make_unique<QQmlApplicationEngine>();
    engine_->loadFromModule("TeamForge", "Main");
    QVERIFY(!engine_->rootObjects().isEmpty());
    window_ = qobject_cast<QQuickWindow *>(engine_->rootObjects().first());
    // TEAMFORGE_SHOT_WIDTH / _HEIGHT: e.g. 390 x 844 to check the phone layout.
    const int w = qEnvironmentVariableIntValue("TEAMFORGE_SHOT_WIDTH"), h = qEnvironmentVariableIntValue("TEAMFORGE_SHOT_HEIGHT");
    window_->resize(w > 0 ? w : 1440, h > 0 ? h : 900);
    QVERIFY(QTest::qWaitForWindowExposed(window_));
    hub_ = engine_->singletonInstance<HubClient *>("TeamForge", "Hub");
    QVERIFY(hub_);
    auto *backend = engine_->singletonInstance<TeamForgeController *>("TeamForge", "Backend");
    QVERIFY(backend->enterWorkspace(QStringLiteral("host")));
    window_->setProperty("currentPage", 5); // Community in the host workspace
    QTest::qWait(300);
}

void TestCommunity::cleanupTestCase()
{
    engine_.reset();
    if (previousHandler)
        qInstallMessageHandler(previousHandler);
}

bool TestCommunity::anyVisibleTextContaining(const QString& part) const
{
    bool found = false;
    const std::function<void(QQuickItem *)> visit = [&](QQuickItem *item) {
        if (found || !item->isVisible() || item->opacity() <= 0.0)
            return;
        if (item->property("text").toString().contains(part, Qt::CaseInsensitive))
            found = true;
        for (QQuickItem *child : item->childItems())
            visit(child);
    };
    visit(window_->contentItem());
    return found;
}

void TestCommunity::shot(const QString& name)
{
    const QString folder = qEnvironmentVariable("TEAMFORGE_SHOTS");
    if (folder.isEmpty())
        return;
    QDir().mkpath(folder);
    window_->grabWindow().save(folder + QLatin1Char('/') + name + QStringLiteral(".png"));
}

void TestCommunity::signsInAndShowsCounts()
{
    QVERIFY(anyVisibleTextContaining(QStringLiteral("Sign in to TeamForge")));
    shot(QStringLiteral("00-sign-in"));
    const QString email = qEnvironmentVariable("TEAMFORGE_TEST_EMAIL", QStringLiteral("lee@demo.test"));
    const QString password = qEnvironmentVariable("TEAMFORGE_TEST_PASSWORD", QStringLiteral("demo-password-1"));
    hub_->signIn(qEnvironmentVariable("TEAMFORGE_TEST_SERVER"), email, password);
    QTRY_VERIFY_WITH_TIMEOUT(hub_->connected(), 10000);
    QCOMPARE(hub_->profile().value(QStringLiteral("name")).toString(), QStringLiteral("Lee Sharma"));
    QTRY_VERIFY_WITH_TIMEOUT(hub_->pendingDecisions() >= 1, 5000); // Oscar's application
    QVERIFY(hub_->anonymousInbox() >= 1);
    const QVariantList teams = hub_->summary().value(QStringLiteral("teams")).toList();
    QVERIFY(!teams.isEmpty());
    for (const QVariant& team : teams)
        if (team.toMap().value(QStringLiteral("name")).toString() == QLatin1String("Rocket Lab"))
            teamId_ = team.toMap().value(QStringLiteral("teamId")).toInt();
    QVERIFY(teamId_ > 0);
}

void TestCommunity::everyScreenOpens_data()
{
    QTest::addColumn<QString>("route");
    QTest::addColumn<QString>("expected");
    QTest::newRow("home") << "home" << "Waiting for your decision";
    QTest::newRow("teams") << "teams" << "Rocket Lab";
    QTest::newRow("team") << "teams/{team}" << "CampusConnect for the hackathon";
    QTest::newRow("team members") << "teams/{team}/members" << "Mia Rawat";
    QTest::newRow("team applications") << "teams/{team}/applications" << "Oscar Negi";
    QTest::newRow("events") << "events" << "CampusConnect";
    QTest::newRow("event") << "events/req-001" << "48-hour hackathon";
    QTest::newRow("applications") << "applications" << "Oscar Negi";
    QTest::newRow("my applications") << "applications?tab=mine" << "No applications yet";
    QTest::newRow("anonymous") << "anonymous" << "Do you still need a tester";
    QTest::newRow("notifications") << "notifications" << "New application to Rocket Lab";
    QTest::newRow("profile") << "profile" << "Community profile";
    QTest::newRow("people") << "people" << "Dev Admin";
}

void TestCommunity::everyScreenOpens()
{
    QFETCH(QString, route);
    QFETCH(QString, expected);
    route.replace(QStringLiteral("{team}"), QString::number(teamId_));
    hub_->setRoute(route);
    QTRY_VERIFY2_WITH_TIMEOUT(anyVisibleTextContaining(expected), qPrintable(route + " shows " + expected), 6000);
    QTest::qWait(400); // let skeletons and fades finish for the screenshot
    if (window_->width() <= 430) { // phone sizes: nothing may stick out sideways
        const QStringList over = horizontalOverflow(window_);
        QVERIFY2(over.isEmpty(), qPrintable(route + ": " + over.join(QStringLiteral("; "))));
    }
    shot(QString::fromLatin1(QTest::currentDataTag()).replace(QLatin1Char(' '), QLatin1Char('-')));
}

void TestCommunity::chatSendsAndShowsMessages()
{
    hub_->setRoute(QStringLiteral("messages/%1").arg(teamId_));
    HubChatModel *chat = hub_->chat();
    QTRY_COMPARE_WITH_TIMEOUT(chat->teamId(), teamId_, 5000);
    QTRY_VERIFY_WITH_TIMEOUT(chat->rowCount() >= 4 && chat->live(), 6000);
    // The newest page holds the demo conversation, or (after earlier runs against the same server
    // pushed it past the first page) messages from those runs. Checked in the model, not on screen:
    // on a short phone screen it may be scrolled out of view.
    bool hasKnownMessage = false;
    for (int row = 0; row < chat->rowCount(); ++row) {
        const QString body = chat->data(chat->index(row), HubChatModel::BodyRole).toString();
        hasKnownMessage |= body.contains(QStringLiteral("Pushed the login screen mock-up"))
                        || body.contains(QStringLiteral("Hello from the offscreen test"));
    }
    QVERIFY(hasKnownMessage);
    const int before = chat->rowCount();
    QCOMPARE(chat->send(QStringLiteral("   ")), QStringLiteral("Message is required."));
    QCOMPARE(chat->send(QStringLiteral("Hello from the offscreen test")), QString());
    QTRY_COMPARE_WITH_TIMEOUT(chat->rowCount(), before + 1, 5000);
    QVERIFY(chat->data(chat->index(chat->rowCount() - 1), HubChatModel::MineRole).toBool());
    QTRY_VERIFY_WITH_TIMEOUT(anyVisibleTextContaining(QStringLiteral("Hello from the offscreen test")), 3000);
    QTest::qWait(400);
    shot(QStringLiteral("chat"));
}

void TestCommunity::anonymousInboxHidesTheSender()
{
    hub_->setRoute(QStringLiteral("anonymous"));
    QTRY_VERIFY_WITH_TIMEOUT(anyVisibleTextContaining(QStringLiteral("Anonymous → Rocket Lab")), 5000);
    QVERIFY(!anyVisibleTextContaining(QStringLiteral("Oscar Negi")));
    QVERIFY(!anyVisibleTextContaining(QStringLiteral("oscar@demo.test")));
}

// One identity: a photo uploaded once shows for the account wherever it appears, found by the
// participant profile it uses (sidebar, home, host lists) and by its Community profile (chat).
void TestCommunity::samePhotoEverywhere()
{
    // A generated test picture (not a person): 200 x 200, two colours.
    QImage picture(200, 200, QImage::Format_RGB32);
    picture.fill(QColor("#3B82F6"));
    QPainter painter(&picture);
    painter.fillRect(50, 50, 100, 100, QColor("#F0434A"));
    painter.end();
    QByteArray png;
    QBuffer buffer(&png);
    buffer.open(QIODevice::WriteOnly);
    picture.save(&buffer, "PNG");
    QSignalSpy ready(hub_, &HubClient::avatarSourceReady);
    hub_->avatarBytes(png);
    QCOMPARE(ready.count(), 1);
    const QString cropped = hub_->cropAvatar(25, 25, 150);
    QVERIFY(cropped.startsWith(QStringLiteral("data:image/jpeg;base64,")) || cropped.startsWith(QStringLiteral("data:image/png;base64,")));
    const QString before = hub_->profile().value(QStringLiteral("avatar")).toString(); // from an earlier run, if any
    hub_->uploadPhoto(cropped);
    hub_->syncIdentity(QStringLiteral("Lee Sharma"), QStringLiteral("B.Tech CSE"), QStringLiteral("tf-001"));
    // the account's identity, by participant profile id, gets the new photo reference ...
    QTRY_VERIFY_WITH_TIMEOUT(hub_->identityForStudent(QStringLiteral("tf-001")).value(QStringLiteral("avatar")).toString()
                                 .startsWith(QStringLiteral("/api/hub/photos/"))
                             && hub_->identityForStudent(QStringLiteral("tf-001")).value(QStringLiteral("avatar")).toString() != before, 8000);
    const QString ref = hub_->identityForStudent(QStringLiteral("tf-001")).value(QStringLiteral("avatar")).toString();
    // ... and the picture itself arrives once and is shared by every avatar
    hub_->photo(ref);
    QTRY_VERIFY_WITH_TIMEOUT(hub_->photo(ref).startsWith(QStringLiteral("data:image/")), 8000);
    // the chat shows the same reference for the author
    hub_->setRoute(QStringLiteral("messages/%1").arg(teamId_));
    HubChatModel *chat = hub_->chat();
    QVERIFY(chat->send(QStringLiteral("Photo check")).isEmpty());
    QTRY_VERIFY_WITH_TIMEOUT(chat->rowCount() > 0
                                 && chat->data(chat->index(chat->rowCount() - 1), HubChatModel::AuthorAvatarRole).toString() == ref, 8000);
    // a sample participant record without an account has no identity: initials, no photo
    QVERIFY(hub_->identityForStudent(QStringLiteral("tf-555")).isEmpty());
    QTest::qWait(600);
    shot(QStringLiteral("chat-with-photo"));
    hub_->setRoute(QStringLiteral("home"));
    QTest::qWait(800);
    shot(QStringLiteral("home-with-photo"));
}

void TestCommunity::noQmlWarnings()
{
    QVERIFY2(qmlWarnings().isEmpty(), qPrintable(qmlWarnings().join(QLatin1Char('\n'))));
}

QTEST_MAIN(TestCommunity)
#include "tst_community.moc"
