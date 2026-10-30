#include "TeamForgeController.h"

#include <QAbstractItemModel>
#include <QFileInfo>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQuickItem>
#include <QQuickWindow>
#include <QTemporaryDir>
#include <QTest>
#include <QtQml/QQmlExtensionPlugin>

#include <cmath>
#include <functional>
#include <memory>

Q_IMPORT_QML_PLUGIN(TeamForgePlugin)

// Loads the real application (Main.qml) offscreen against a fresh data folder and drives it with
// Qt mouse events: the three workspaces, what each one shows, hover states and tooltips.
class TestUi : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void startsOnTheWorkspaceChoice();
    void entryLogoRespondsToPointer();
    void hostMatchingRowsAreScannable();
    void hoverStatesAndTooltipsRespond();
    void participantSeesNoHostOrDeveloperControls();
    void developerToolsOnlyInDeveloperWorkspace();
    void developerSeesEveryRecord();
    void hostReviewsApplicants();
    void motivationalQuotesFollowTheJourney();
    void resumeImportFillsTheDraft();
    void interestNoteReachesTheHost();
    void noAvailabilityAnywhere();

private:
    // Visible items (window content and overlay) whose `text` property equals `text`.
    QList<QQuickItem *> itemsWithText(const QString& text) const;
    QQuickItem *itemWithText(const QString& text) const;
    // First visible item that has a property called `name`.
    QQuickItem *itemWithProperty(const char *name) const;
    bool anyVisibleText(const QString& text) const { return !itemsWithText(text).isEmpty(); }
    // First visible text containing `part` (case-insensitive), or empty.
    QString visibleTextContaining(const QString& part) const;
    bool toolTipShown(const QString& text) const;
    void goToPage(int index);
    // The quote on screen in the visible MotivationQuote for `moment` ({} if none is visible).
    QVariantMap visibleQuote(const QString& moment) const;
    QObject *objectOfType(const char *typeName) const;

    std::unique_ptr<QTemporaryDir> dir_;
    std::unique_ptr<QQmlApplicationEngine> engine_;
    QQuickWindow *window_ = nullptr;
    TeamForgeController *backend_ = nullptr;
};

void TestUi::initTestCase()
{
    dir_ = std::make_unique<QTemporaryDir>();
    QVERIFY(dir_->isValid());
    // An empty folder: the app seeds it from the samples bundled in the module.
    qputenv("TEAMFORGE_DATA_DIR", dir_->path().toUtf8());
    engine_ = std::make_unique<QQmlApplicationEngine>();
    engine_->loadFromModule("TeamForge", "Main");
    QVERIFY(!engine_->rootObjects().isEmpty());
    window_ = qobject_cast<QQuickWindow *>(engine_->rootObjects().first());
    QVERIFY(window_);
    window_->resize(1600, 900);
    QVERIFY(QTest::qWaitForWindowExposed(window_));
    backend_ = engine_->singletonInstance<TeamForgeController *>("TeamForge", "Backend");
    QVERIFY(backend_);
    QVERIFY(backend_->students().size() >= 500);
}

void TestUi::cleanupTestCase()
{
    engine_.reset();
}

QList<QQuickItem *> TestUi::itemsWithText(const QString& text) const
{
    QList<QQuickItem *> found;
    const std::function<void(QQuickItem *)> visit = [&](QQuickItem *item) {
        if (!item->isVisible() || item->opacity() <= 0.0)
            return;
        if (item->property("text").toString() == text)
            found.append(item);
        for (QQuickItem *child : item->childItems())
            visit(child);
    };
    visit(window_->contentItem());
    return found;
}

QQuickItem *TestUi::itemWithText(const QString& text) const
{
    const QList<QQuickItem *> items = itemsWithText(text);
    return items.isEmpty() ? nullptr : items.first();
}

QString TestUi::visibleTextContaining(const QString& part) const
{
    QString found;
    const std::function<void(QQuickItem *)> visit = [&](QQuickItem *item) {
        if (!found.isEmpty() || !item->isVisible() || item->opacity() <= 0.0)
            return;
        const QString text = item->property("text").toString();
        if (text.contains(part, Qt::CaseInsensitive))
            found = text;
        for (QQuickItem *child : item->childItems())
            visit(child);
    };
    visit(window_->contentItem());
    return found;
}

QQuickItem *TestUi::itemWithProperty(const char *name) const
{
    QQuickItem *found = nullptr;
    const std::function<void(QQuickItem *)> visit = [&](QQuickItem *item) {
        if (found || !item->isVisible())
            return;
        if (item->property(name).isValid()) {
            found = item;
            return;
        }
        for (QQuickItem *child : item->childItems())
            visit(child);
    };
    visit(window_->contentItem());
    return found;
}

bool TestUi::toolTipShown(const QString& text) const
{
    // Attached tooltips share one ToolTip popup per engine.
    for (QObject *object : engine_->findChildren<QObject *>()) {
        if (QByteArray(object->metaObject()->className()).contains("ToolTip")
            && object->property("visible").toBool() && object->property("text").toString() == text)
            return true;
    }
    for (QObject *object : window_->findChildren<QObject *>()) {
        if (QByteArray(object->metaObject()->className()).contains("ToolTip")
            && object->property("visible").toBool() && object->property("text").toString() == text)
            return true;
    }
    return false;
}

QVariantMap TestUi::visibleQuote(const QString& moment) const
{
    QVariantMap found;
    const std::function<void(QQuickItem *)> visit = [&](QQuickItem *item) {
        if (!found.isEmpty() || !item->isVisible())
            return;
        if (item->property("moment").toString() == moment && item->property("shown").isValid()) {
            found = item->property("shown").toMap();
            return;
        }
        for (QQuickItem *child : item->childItems())
            visit(child);
    };
    visit(window_->contentItem());
    return found;
}

QObject *TestUi::objectOfType(const char *typeName) const
{
    for (QObject *object : engine_->rootObjects().first()->findChildren<QObject *>()) {
        if (QByteArray(object->metaObject()->className()).startsWith(typeName))
            return object;
    }
    return nullptr;
}

void TestUi::goToPage(int index)
{
    window_->setProperty("currentPage", index);
    QTest::qWait(300); // page fade-in
}

void TestUi::startsOnTheWorkspaceChoice()
{
    QCOMPARE(backend_->workspace(), QString());
    // The cards arrive with the entrance animation.
    QTRY_VERIFY(anyVisibleText("Build the right team."));
    QVERIFY(anyVisibleText("TEAMFORGE"));
    for (const char *choice : {"PARTICIPANT", "HOST", "DEVELOPER"})
        QTRY_VERIFY2(anyVisibleText(QString::fromLatin1(choice)), choice);
    for (const char *line : {"Build your profile and discover opportunities", "Create projects and build teams",
                             "Monitor, test and manage TeamForge"})
        QTRY_VERIFY2(anyVisibleText(QString::fromLatin1(line)), line);
}

void TestUi::entryLogoRespondsToPointer()
{
    QCOMPARE(backend_->workspace(), QString());
    QQuickItem *logo = itemWithProperty("spinDirection"); // LogoMark3D
    QVERIFY(logo);
    QTRY_VERIFY(logo->opacity() > 0.99);
    // Pointing to the right of the logo tilts it towards the pointer; leaving rests it.
    const QPointF centre = logo->mapToScene(QPointF(logo->width() / 2, logo->height() / 2));
    QTest::mouseMove(window_, centre.toPoint());
    QTest::mouseMove(window_, QPoint(window_->width() - 10, int(centre.y())));
    QTRY_VERIFY(logo->property("tiltY").toDouble() > 3.0);
    // A click spins it a full turn and it comes back to rest.
    QTest::mouseClick(window_, Qt::LeftButton, {}, centre.toPoint());
    QTRY_VERIFY(std::abs(logo->property("spin").toDouble()) > 30.0);
    QTRY_VERIFY_WITH_TIMEOUT(logo->property("spin").toDouble() == 0.0, 3000);
}

void TestUi::hostMatchingRowsAreScannable()
{
    QVERIFY(backend_->enterWorkspace("host"));
    goToPage(4); // Matching
    // Every row: name, a few skills, score, "Why this match?" and "+ Add"; no reason sentences.
    QTRY_VERIFY(itemsWithText("Why this match?").size() >= 5);
    QVERIFY(itemsWithText("+ Add").size() >= 5);
    QVERIFY(anyVisibleText("Anubhav Bisht"));
    for (QQuickItem *item : itemsWithText("Explain"))
        QFAIL(qPrintable("Old 'Explain' label still visible: " + item->objectName()));
    const std::function<void(QQuickItem *)> noReasons = [&](QQuickItem *item) {
        if (!item->isVisible())
            return;
        const QString text = item->property("text").toString();
        QVERIFY2(!text.contains("missing skills") && !text.contains("sessions ·") && !text.startsWith("Fills "),
                 qPrintable(text));
        for (QQuickItem *child : item->childItems())
            noReasons(child);
    };
    noReasons(window_->contentItem());
}

void TestUi::hoverStatesAndTooltipsRespond()
{
    QVERIFY(backend_->enterWorkspace("host"));
    goToPage(4); // Matching

    // A button's hover state follows the pointer.
    QQuickItem *suggest = itemWithText("Suggest team");
    QVERIFY(suggest);
    QVERIFY(!suggest->property("hovered").toBool());
    const QPoint inside = suggest->mapToScene(QPointF(suggest->width() / 2, suggest->height() / 2)).toPoint();
    QTest::mouseMove(window_, QPoint(5, 5));
    QTest::mouseMove(window_, inside);
    QTRY_VERIFY(suggest->property("hovered").toBool());
    // ... and its tooltip appears after the delay.
    QTRY_VERIFY_WITH_TIMEOUT(toolTipShown("Fill the team automatically with the best-matching people"), 3000);
    QTest::mouseMove(window_, QPoint(5, 5));
    QTRY_VERIFY(!suggest->property("hovered").toBool());
    QTRY_VERIFY(!toolTipShown("Fill the team automatically with the best-matching people"));

    // A sidebar entry highlights on hover (its colour changes).
    QQuickItem *navLabel = itemWithText("Projects");
    QVERIFY(navLabel);
    // The NavItem is the nearest ancestor with a `selected` property.
    QQuickItem *nav = navLabel;
    while (nav && !nav->property("selected").isValid())
        nav = nav->parentItem();
    QVERIFY(nav);
    const QColor before = nav->property("color").value<QColor>();
    QTest::mouseMove(window_, nav->mapToScene(QPointF(nav->width() / 2, nav->height() / 2)).toPoint());
    QTRY_VERIFY(nav->property("color").value<QColor>() != before);
    QTest::mouseMove(window_, QPoint(window_->width() - 5, window_->height() - 5));
    QTRY_COMPARE(nav->property("color").value<QColor>(), before);
}

void TestUi::participantSeesNoHostOrDeveloperControls()
{
    QVERIFY(backend_->enterWorkspace("participant"));
    goToPage(0);
    // No profile yet: onboarding.
    QTRY_VERIFY(anyVisibleText("Create your profile"));
    QVERIFY(anyVisibleText("Profile"));
    for (const char *hidden : {"Suggest team", "Save team", "+ New", "Rebuild SkillIndex", "Reset seed data",
                               "Show system tools", "Preview host", "Accept", "Weekly sessions"})
        QVERIFY2(!anyVisibleText(QString::fromLatin1(hidden)), hidden);
    QVERIFY(backend_->useDemoParticipant());
    goToPage(2);
    QTRY_VERIFY(itemsWithText("View").size() >= 3);
    QVERIFY(anyVisibleText("Express interest"));
    QVERIFY(anyVisibleText("Under review")); // the demo participant's seeded request
    QVERIFY(!anyVisibleText("+ Add"));

    // Expressing interest from a card turns it into a status.
    const int before = int(backend_->myInterests().size());
    QQuickItem *express = itemWithText("Express interest");
    QVERIFY(express);
    QTest::mouseClick(window_, Qt::LeftButton, {},
                      express->mapToScene(QPointF(express->width() / 2, express->height() / 2)).toPoint());
    QTRY_COMPARE(int(backend_->myInterests().size()), before + 1);
    QTRY_VERIFY(itemsWithText("Interested").size() >= 1);
    goToPage(0);
    QTRY_VERIFY(anyVisibleText("Your interests"));
}

void TestUi::developerToolsOnlyInDeveloperWorkspace()
{
    QVERIFY(backend_->enterWorkspace("developer"));
    goToPage(0);
    QTRY_VERIFY(anyVisibleText("Participant database"));
    QVERIFY(anyVisibleText("DATA STATUS") && anyVisibleText("SYSTEM STATUS"));
    QVERIFY(!anyVisibleText("Rebuild SkillIndex")); // technical tools are not on the overview
    goToPage(4);
    QTRY_VERIFY(anyVisibleText("Reset seed data"));
    QVERIFY(anyVisibleText("Preview host"));
    QVERIFY(anyVisibleText("Show system tools"));
    QVERIFY(!anyVisibleText("Rebuild SkillIndex")); // folded away until asked for

    QVERIFY(backend_->previewWorkspace("host", "req-002"));
    goToPage(0);
    QTRY_VERIFY(anyVisibleText("DEVELOPER PREVIEW"));
    QVERIFY(anyVisibleText("Back to Developer"));
    QVERIFY(anyVisibleText("CivicLens"));
    for (const char *hidden : {"Reset seed data", "Show system tools", "Rebuild SkillIndex"})
        QVERIFY2(!anyVisibleText(QString::fromLatin1(hidden)), hidden);

    // Participant preview needs no onboarding: the chosen (here, default demo) profile is used.
    QVERIFY(backend_->previewWorkspace("participant"));
    goToPage(0);
    QTRY_VERIFY(anyVisibleText("DEVELOPER PREVIEW"));
    QVERIFY(!anyVisibleText("Create your profile"));
    QTRY_VERIFY(itemsWithText("Why this match?").size() >= 1);
    backend_->endPreview();
    QCOMPARE(backend_->workspace(), QStringLiteral("developer"));
}

void TestUi::developerSeesEveryRecord()
{
    QVERIFY(backend_->enterWorkspace("developer"));
    const int total = int(backend_->students().size());
    goToPage(1);
    // The record count and the virtualised list cover every participant, not a page of them.
    QTRY_VERIFY(anyVisibleText("participant records"));
    QVERIFY(anyVisibleText(QString::number(total)));
    QQuickItem *list = nullptr;
    const std::function<void(QQuickItem *)> visit = [&](QQuickItem *item) {
        if (!item->isVisible())
            return;
        if (QByteArray(item->metaObject()->className()).contains("ListView") && item->property("count").toInt() == total)
            list = item;
        for (QQuickItem *child : item->childItems())
            visit(child);
    };
    visit(window_->contentItem());
    QVERIFY2(list, "no list with every participant");

    goToPage(3);
    QTRY_VERIFY(anyVisibleText("SKILLS"));
    QVERIFY(backend_->skillCount() >= 500);
    QVERIFY(anyVisibleText(QString::number(backend_->skillCount())));
    QVERIFY(anyVisibleText("PYTHON") || anyVisibleText("Python"));
}

void TestUi::hostReviewsApplicants()
{
    QVERIFY(backend_->enterWorkspace("host"));
    backend_->setCurrentRequirementId("req-001");
    goToPage(2); // Applicants
    QTRY_VERIFY(itemsWithText("Accept").size() >= 1);
    const auto acceptedCount = [&] {
        int n = 0;
        for (const QVariant& r : backend_->projectInterests("req-001"))
            n += r.toMap().value("status").toString() == "accepted" ? 1 : 0;
        return n;
    };
    const int before = acceptedCount();
    QQuickItem *accept = itemWithText("Accept");
    QTest::mouseClick(window_, Qt::LeftButton, {},
                      accept->mapToScene(QPointF(accept->width() / 2, accept->height() / 2)).toPoint());
    QTRY_COMPARE(acceptedCount(), before + 1);

    // Accepted people are offered in Matching, not added to the team.
    goToPage(4);
    QVERIFY(backend_->acceptedCandidates().size() >= 1);
    QCOMPARE(backend_->currentTeam().value("size").toInt(), 0);
}

void TestUi::motivationalQuotesFollowTheJourney()
{
    const auto words = [](const QVariantMap& quote) {
        return int(quote.value("text").toString().split(' ', Qt::SkipEmptyParts).size());
    };

    // Onboarding, step 01: a growth quote.
    QVERIFY(backend_->enterWorkspace("participant"));
    backend_->forgetMyProfile();
    goToPage(0);
    QTRY_VERIFY(anyVisibleText("Create your profile"));
    QTRY_COMPARE(visibleQuote("register").value("category").toString(), QString("growth"));
    QCOMPARE(words(visibleQuote("register")), 10);

    // Step 02, skills: the quote follows the selection without touching the draft.
    QObject *editor = objectOfType("ProfileEditor");
    QVERIFY(editor);
    editor->setProperty("draftName", "Quote Tester");
    QVERIFY(QMetaObject::invokeMethod(editor, "next"));
    QTRY_COMPARE(editor->property("step").toInt(), 1);
    QTRY_COMPARE(visibleQuote("skills").value("category").toString(), QString("first_skill"));
    QVariant added;
    QVERIFY(QMetaObject::invokeMethod(editor, "addSkill", Q_RETURN_ARG(QVariant, added), Q_ARG(QVariant, "python")));
    QTRY_COMPARE(visibleQuote("skills").value("category").toString(), QString("single_skill"));
    QVERIFY(QMetaObject::invokeMethod(editor, "addSkill", Q_RETURN_ARG(QVariant, added), Q_ARG(QVariant, "react")));
    QTRY_COMPARE(visibleQuote("skills").value("category").toString(), QString("multi_skill"));
    const QString multi = visibleQuote("skills").value("text").toString();
    QCOMPARE(words(visibleQuote("skills")), 10);
    // A third skill keeps the same rule, so the quote stays put.
    QVERIFY(QMetaObject::invokeMethod(editor, "addSkill", Q_RETURN_ARG(QVariant, added), Q_ARG(QVariant, "sql")));
    QTest::qWait(400);
    QCOMPARE(visibleQuote("skills").value("text").toString(), multi);
    QVariant skills;
    QVERIFY(QMetaObject::invokeMethod(editor, "skillList", Q_RETURN_ARG(QVariant, skills)));
    QCOMPARE(skills.toList().size(), 3);
    QVERIFY(anyVisibleText("Continue"));

    // The opportunity drawer: the team preview, then the join quote above Express interest.
    QVERIFY(backend_->useDemoParticipant());
    goToPage(2);
    QString openId;
    for (const QVariant& entry : backend_->opportunities()) {
        if (entry.toMap().value("interestStatus").toString().isEmpty()) {
            openId = entry.toMap().value("id").toString();
            break;
        }
    }
    QVERIFY(!openId.isEmpty());
    QObject *drawer = objectOfType("OpportunityDrawer");
    QVERIFY(drawer);
    QVERIFY(QMetaObject::invokeMethod(drawer, "show", Q_ARG(QVariant, openId)));
    QTRY_VERIFY(anyVisibleText("Team preview"));
    QTRY_COMPARE(drawer->property("position").toReal(), 1.0); // fully slid in
    QTRY_VERIFY(!visibleQuote("join").isEmpty());
    const QString joinCategory = visibleQuote("join").value("category").toString();
    QVERIFY(joinCategory == "collaboration" || joinCategory == "teamwork");
    QCOMPARE(words(visibleQuote("join")), 10);
    // Express interest is clickable at once: no animation has to finish first.
    auto *drawerContent = drawer->property("contentItem").value<QQuickItem *>();
    QVERIFY(drawerContent);
    QQuickItem *express = nullptr;
    for (QQuickItem *button : itemsWithText("Express interest")) {
        if (drawerContent->isAncestorOf(button))
            express = button; // the drawer's footer, not a card behind it
    }
    QVERIFY(express);
    const QPointF corner = express->mapToScene(QPointF(express->width(), express->height()));
    QVERIFY(corner.x() <= window_->width() && corner.y() <= window_->height()); // in view
    const int before = int(backend_->myInterests().size());
    QTest::mouseClick(window_, Qt::LeftButton, {},
                      express->mapToScene(QPointF(express->width() / 2, express->height() / 2)).toPoint());
    QTRY_COMPARE(int(backend_->myInterests().size()), before + 1);
    QTRY_VERIFY(visibleQuote("join").isEmpty()); // the status takes the quote's place
    QVERIFY(QMetaObject::invokeMethod(drawer, "close"));

    // Host: the new-project form.
    QVERIFY(backend_->enterWorkspace("host"));
    goToPage(1);
    QTRY_VERIFY(itemWithText("+ New"));
    QQuickItem *create = itemWithText("+ New");
    QTest::mouseClick(window_, Qt::LeftButton, {},
                      create->mapToScene(QPointF(create->width() / 2, create->height() / 2)).toPoint());
    QTRY_VERIFY(!visibleQuote("host_project").isEmpty());
    const QString hostCategory = visibleQuote("host_project").value("category").toString();
    QVERIFY(hostCategory == "leadership" || hostCategory == "project_building" || hostCategory == "innovation");
}

void TestUi::resumeImportFillsTheDraft()
{
    QVERIFY(backend_->enterWorkspace("participant"));
    backend_->forgetMyProfile();
    goToPage(0);
    QTRY_VERIFY(anyVisibleText("Start from your resume"));
    QVERIFY(anyVisibleText("Choose file"));

    // The file is analysed in the background; the review opens when it is ready.
    const QUrl file = QUrl::fromLocalFile(QStringLiteral(TEAMFORGE_SOURCE_DIR "/tests/data/resumes/priya-nair.pdf"));
    QVERIFY(backend_->analyseResumeFile(file));
    QTRY_VERIFY(anyVisibleText("Review before adding"));
    QObject *review = objectOfType("ResumeReviewDrawer");
    QVERIFY(review);
    QTRY_COMPARE(review->property("position").toReal(), 1.0);
    QVERIFY(anyVisibleText("PYTHON"));
    QVERIFY(anyVisibleText("+ REST APIs") || anyVisibleText("+ REST APIS"));
    QVERIFY(!visibleTextContaining("Choose a level for").isEmpty()); // listed skills with no stated experience

    // Untick everything without a level, then add: only reviewed values reach the draft.
    QVariant missing;
    QVERIFY(QMetaObject::invokeMethod(review, "missingLevels", Q_RETURN_ARG(QVariant, missing)));
    QVERIFY(missing.toInt() > 0);
    QObject *editor = objectOfType("ProfileEditor");
    QVERIFY(editor);
    QQuickItem *add = itemWithText("Add to profile");
    QVERIFY(add && !add->isEnabled());
    // Pick a level for every unrated skill, through the same model the level pips write to.
    QAbstractItemModel *rows = nullptr;
    for (QObject *child : review->findChildren<QObject *>())
        if (auto *model = qobject_cast<QAbstractItemModel *>(child))
            rows = model;
    QVERIFY(rows);
    const int levelRole = rows->roleNames().key("level");
    for (int i = 0; i < rows->rowCount(); ++i) {
        if (rows->data(rows->index(i, 0), levelRole).toInt() == 0)
            QVERIFY(QMetaObject::invokeMethod(rows, "setProperty", Q_ARG(int, i), Q_ARG(QString, "level"), Q_ARG(QVariant, 2)));
    }
    review->setProperty("revision", review->property("revision").toInt() + 1);
    QTRY_VERIFY(add->isEnabled());
    QTest::mouseClick(window_, Qt::LeftButton, {}, add->mapToScene(QPointF(add->width() / 2, add->height() / 2)).toPoint());
    QTRY_COMPARE(review->property("opened").toBool(), false);

    QCOMPARE(editor->property("draftName").toString(), QStringLiteral("Priya Nair"));
    QVariant skills;
    QVERIFY(QMetaObject::invokeMethod(editor, "skillList", Q_RETURN_ARG(QVariant, skills)));
    bool python = false;
    for (const QVariant& s : skills.toList())
        python = python || (s.toMap().value("skill") == QStringLiteral("python") && s.toMap().value("level").toInt() == 3);
    QVERIFY(python); // 4 years -> Intermediate
    const QVariantMap details = editor->property("draftDetails").toMap();
    QCOMPARE(details.value("projects").toList().size(), 2);
    QCOMPARE(details.value("skillYears").toMap().value("python").toDouble(), 4.0);
    QCOMPARE(backend_->resume().value("state").toString(), QStringLiteral("idle")); // the file is let go

    // Saved with the profile.
    QVERIFY(QMetaObject::invokeMethod(editor, "finish"));
    QTRY_VERIFY(!backend_->myProfileId().isEmpty());
    QCOMPARE(backend_->myProfile().value("details").toMap().value("certifications").toList().size(), 2);
}

void TestUi::interestNoteReachesTheHost()
{
    QVERIFY(backend_->enterWorkspace("participant"));
    QVERIFY(backend_->useDemoParticipant());
    goToPage(2);
    QString openId;
    for (const QVariant& entry : backend_->opportunities()) {
        if (entry.toMap().value("interestStatus").toString().isEmpty()) {
            openId = entry.toMap().value("id").toString();
            break;
        }
    }
    QVERIFY(!openId.isEmpty());
    QObject *drawer = objectOfType("OpportunityDrawer");
    QVERIFY(drawer);
    QVERIFY(QMetaObject::invokeMethod(drawer, "show", Q_ARG(QVariant, openId)));
    QTRY_COMPARE(drawer->property("position").toReal(), 1.0);
    auto *content = drawer->property("contentItem").value<QQuickItem *>();
    QVERIFY(content);
    // A button with this text in the drawer (labels can share the text, e.g. "How joining works").
    const auto inDrawer = [&](const QString& text) -> QQuickItem * {
        QQuickItem *label = nullptr;
        for (QQuickItem *item : itemsWithText(text)) {
            if (!content->isAncestorOf(item))
                continue;
            if (item->inherits("QQuickAbstractButton"))
                return item;
            if (!label)
                label = item;
        }
        return label;
    };
    const auto click = [&](QQuickItem *item) {
        QTest::mouseClick(window_, Qt::LeftButton, {}, item->mapToScene(QPointF(item->width() / 2, item->height() / 2)).toPoint());
    };

    // The note box opens on request; the note goes out with the interest.
    QQuickItem *addNote = inDrawer("+ Add a note for the host (optional)");
    QVERIFY(addNote);
    click(addNote);
    QTRY_VERIFY(drawer->property("writingNote").toBool());
    QQuickItem *box = nullptr;
    const std::function<void(QQuickItem *)> find = [&](QQuickItem *item) {
        if (!box && item->isVisible() && QByteArray(item->metaObject()->className()).startsWith("TfTextArea"))
            box = item;
        for (QQuickItem *child : item->childItems())
            find(child);
    };
    find(content);
    QVERIFY(box);
    box->setProperty("text", "Happy to take the frontend; I have two React projects.");
    QTRY_VERIFY(anyVisibleText(QStringLiteral("54 / 500")));
    QQuickItem *express = inDrawer("Express interest");
    QVERIFY(express);
    click(express);
    QString requestId;
    QTRY_VERIFY(!(requestId = [&] {
        for (const QVariant& e : backend_->myInterests())
            if (e.toMap().value("id").toString() == openId)
                return e.toMap().value("interestId").toString();
        return QString();
    }()).isEmpty());
    QTRY_VERIFY(inDrawer("Messages"));
    QVERIFY(inDrawer("Happy to take the frontend; I have two React projects."));
    QVERIFY(QMetaObject::invokeMethod(drawer, "close"));

    // The host sees it under the applicant on the Applicants page.
    QVERIFY(backend_->enterWorkspace("host"));
    backend_->setCurrentRequirementId(openId);
    goToPage(2);
    QTRY_VERIFY(anyVisibleText(QString::fromUtf8("\u201CHappy to take the frontend; I have two React projects.\u201D")));
    QVERIFY(!itemsWithText("Reply").isEmpty());
    QVERIFY(backend_->replyToInterest(requestId, "Welcome! Frontend is yours."));
    QTRY_VERIFY(anyVisibleText(QStringLiteral("You replied: Welcome! Frontend is yours.")));
}

void TestUi::noAvailabilityAnywhere()
{
    const auto check = [&](const char *where) {
        for (const char *word : {"availab", "session", "time slot"}) {
            const QString text = visibleTextContaining(QString::fromLatin1(word));
            QVERIFY2(text.isEmpty(), qPrintable(QStringLiteral("%1: \"%2\"").arg(QString::fromLatin1(where), text)));
        }
    };
    QVERIFY(backend_->enterWorkspace("participant"));
    QVERIFY(backend_->useDemoParticipant());
    for (int page = 0; page < 3; ++page) {
        goToPage(page);
        check("participant");
    }
    QVERIFY(backend_->enterWorkspace("host"));
    for (int page = 0; page < 5; ++page) {
        goToPage(page);
        check("host");
    }
}

int main(int argc, char *argv[])
{
    if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM"))
        qputenv("QT_QPA_PLATFORM", "offscreen");
    // The offscreen platform has no font directory of its own; use the system's.
    if (qEnvironmentVariableIsEmpty("QT_QPA_FONTDIR") && QFileInfo::exists(QStringLiteral("C:/Windows/Fonts")))
        qputenv("QT_QPA_FONTDIR", "C:/Windows/Fonts");
    QGuiApplication app(argc, argv);
    TestUi test;
    return QTest::qExec(&test, argc, argv);
}

#include "tst_ui.moc"
