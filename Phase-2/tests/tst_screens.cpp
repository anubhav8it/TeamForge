#include "TeamForgeController.h"

#include <QDir>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQuickItem>
#include <QQuickWindow>
#include <QTemporaryDir>
#include <QTest>
#include <QtQml/QQmlExtensionPlugin>

#include <functional>
#include <memory>

Q_IMPORT_QML_PLUGIN(TeamForgePlugin)

// A screenshot of every main screen, for checking the design by eye (skipped unless
// TEAMFORGE_SHOTS names a folder). Use a real window (QT_QPA_PLATFORM=windows) for correct text;
// offscreen rendering scrambles glyphs. Also fails on any QML warning while the screens load.
namespace {

QStringList& warnings()
{
    static QStringList list;
    return list;
}
QtMessageHandler previous = nullptr;

void collect(QtMsgType type, const QMessageLogContext& context, const QString& message)
{
    if (type != QtDebugMsg && type != QtInfoMsg && !message.startsWith(QLatin1String("setGeometry"))
        && (message.contains(QLatin1String(".qml")) || message.contains(QLatin1String("QML"))))
        warnings().append(message);
    if (previous)
        previous(type, context, message);
}

} // namespace

class TestScreens : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void workspaceChoice();
    void onboarding();
    void participant();
    void host();
    void developer();
    void noQmlWarnings();

private:
    QQuickItem *itemWithText(const QString& text) const;
    void shot(const QString& name);
    void page(int index, const QString& name);

    QString folder_;
    std::unique_ptr<QTemporaryDir> dir_;
    std::unique_ptr<QQmlApplicationEngine> engine_;
    QQuickWindow *window_ = nullptr;
    TeamForgeController *backend_ = nullptr;
};

void TestScreens::initTestCase()
{
    folder_ = qEnvironmentVariable("TEAMFORGE_SHOTS");
    if (folder_.isEmpty())
        QSKIP("Set TEAMFORGE_SHOTS to a folder for the screenshots");
    QDir().mkpath(folder_);
    previous = qInstallMessageHandler(collect);
    dir_ = std::make_unique<QTemporaryDir>();
    qputenv("TEAMFORGE_DATA_DIR", dir_->path().toUtf8());
    engine_ = std::make_unique<QQmlApplicationEngine>();
    engine_->loadFromModule("TeamForge", "Main");
    QVERIFY(!engine_->rootObjects().isEmpty());
    window_ = qobject_cast<QQuickWindow *>(engine_->rootObjects().first());
    const QSize size(qEnvironmentVariableIntValue("TEAMFORGE_SHOT_WIDTH") > 0 ? qEnvironmentVariableIntValue("TEAMFORGE_SHOT_WIDTH") : 1440,
                     qEnvironmentVariableIntValue("TEAMFORGE_SHOT_HEIGHT") > 0 ? qEnvironmentVariableIntValue("TEAMFORGE_SHOT_HEIGHT") : 900);
    window_->resize(size);
    QVERIFY(QTest::qWaitForWindowExposed(window_));
    backend_ = engine_->singletonInstance<TeamForgeController *>("TeamForge", "Backend");
}

void TestScreens::cleanupTestCase()
{
    engine_.reset();
    if (previous)
        qInstallMessageHandler(previous);
}

QQuickItem *TestScreens::itemWithText(const QString& text) const
{
    QQuickItem *found = nullptr;
    const std::function<void(QQuickItem *)> visit = [&](QQuickItem *item) {
        if (found || !item->isVisible() || item->opacity() <= 0.0)
            return;
        if (item->property("text").toString() == text)
            found = item;
        for (QQuickItem *child : item->childItems())
            visit(child);
    };
    visit(window_->contentItem());
    return found;
}

void TestScreens::shot(const QString& name)
{
    QTest::qWait(700); // entrances and fades finish
    QVERIFY(window_->grabWindow().save(folder_ + QLatin1Char('/') + name + QStringLiteral(".png")));
}

void TestScreens::page(int index, const QString& name)
{
    window_->setProperty("currentPage", index);
    shot(name);
}

void TestScreens::workspaceChoice()
{
    QTRY_VERIFY(itemWithText(QStringLiteral("PARTICIPANT")));
    QTest::qWait(1200);
    shot(QStringLiteral("01-workspaces"));
    QQuickItem *host = itemWithText(QStringLiteral("HOST"));
    QVERIFY(host);
    QTest::mouseMove(window_, host->mapToScene(QPointF(host->width() / 2, host->height())).toPoint());
    shot(QStringLiteral("02-workspaces-hover"));
}

void TestScreens::onboarding()
{
    QVERIFY(backend_->enterWorkspace(QStringLiteral("participant")));
    QTRY_VERIFY(itemWithText(QStringLiteral("Create your profile")));
    shot(QStringLiteral("03-onboarding-profile"));
    // A name, then on to Skills.
    std::function<QQuickItem *(QQuickItem *)> nameField = [&](QQuickItem *item) -> QQuickItem * {
        if (item->isVisible() && item->property("placeholderText").toString() == QLatin1String("Your full name"))
            return item;
        for (QQuickItem *child : item->childItems())
            if (QQuickItem *hit = nameField(child))
                return hit;
        return nullptr;
    };
    QQuickItem *field = nameField(window_->contentItem());
    QVERIFY(field);
    field->setProperty("text", QStringLiteral("Asha Rao"));
    QQuickItem *next = itemWithText(QStringLiteral("Continue"));
    QVERIFY(next);
    QTest::mouseClick(window_, Qt::LeftButton, {}, next->mapToScene(QPointF(next->width() / 2, next->height() / 2)).toPoint());
    QTRY_VERIFY(itemWithText(QStringLiteral("Skills")));
    shot(QStringLiteral("04-onboarding-skills"));
    backend_->leaveWorkspace();
}

void TestScreens::participant()
{
    QVERIFY(backend_->enterWorkspace(QStringLiteral("participant")));
    QVERIFY(backend_->claimProfile(QStringLiteral("tf-001")));
    QTest::qWait(300);
    page(0, QStringLiteral("10-participant-home"));
    page(1, QStringLiteral("11-participant-profile"));
    page(2, QStringLiteral("12-participant-opportunities"));
    page(3, QStringLiteral("13-participant-community"));
    backend_->leaveWorkspace();
}

void TestScreens::host()
{
    QVERIFY(backend_->enterWorkspace(QStringLiteral("host")));
    page(0, QStringLiteral("20-host-overview"));
    page(1, QStringLiteral("21-host-projects"));
    page(2, QStringLiteral("22-host-applicants"));
    page(3, QStringLiteral("23-host-participants"));
    page(4, QStringLiteral("24-host-matching"));
    backend_->leaveWorkspace();
}

void TestScreens::developer()
{
    QVERIFY(backend_->enterWorkspace(QStringLiteral("developer")));
    for (int i = 0; i < 5; ++i)
        page(i, QStringLiteral("3%1-developer-%2").arg(i).arg(i));
    backend_->leaveWorkspace();
}

void TestScreens::noQmlWarnings()
{
    QVERIFY2(warnings().isEmpty(), qPrintable(warnings().join(QLatin1Char('\n'))));
}

QTEST_MAIN(TestScreens)
#include "tst_screens.moc"
