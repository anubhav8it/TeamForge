#include "core/Errors.h"
#include "core/Hub.h"

#include <QTest>

#include <string>
#include <vector>

using namespace teamforge;
using namespace teamforge::hub;

// The Hub's rules (core/Hub.h). The same rules are enforced by the server and checked end to end
// in web/tests/test_hub.py; these tests pin the C++ side so the app explains them consistently.
class TestHub : public QObject
{
    Q_OBJECT

private slots:
    void textIsCleanedLikeTheServer();
    void textLimitsCountCharacters();
    void statusesRoundTrip();
    void teamApplicationRules();
    void eventApplicationRules();
    void capacityAndWaitlist();
    void eventWindow();
    void teamRoles();
    void teamApplyBlockers();
    void chatLogMergesPagesInOrder();
    void chatLogUnreadAndLayout();
    void notificationsReplaceById();
    void notificationLinks();
    void anonymousRules();
    void timeTexts();
};

void TestHub::textIsCleanedLikeTheServer()
{
    QCOMPARE(cleanText("  Hello\r\nteam\a!\t ", 100, "Message"), std::string("Hello\nteam!"));
    QCOMPARE(cleanText("a\n\n\n\n\nb", 100, "Message"), std::string("a\n\nb"));
    QCOMPARE(cleanText("a\rb", 100, "Message"), std::string("a\nb"));
    QCOMPARE(cleanText("", 100, "Note"), std::string());
    QVERIFY_THROWS_EXCEPTION(ValidationError, cleanText(" \n\t ", 100, "Message", true));
}

void TestHub::textLimitsCountCharacters()
{
    // "é" is two bytes but one character.
    std::string accents;
    for (int i = 0; i < 10; ++i)
        accents += "\xC3\xA9";
    QCOMPARE(characterCount(accents), std::size_t(10));
    QCOMPARE(cleanText(accents, 10, "Message"), accents);
    QVERIFY_THROWS_EXCEPTION(ValidationError, cleanText(accents + "x", 10, "Message"));
    try {
        cleanText(std::string(kMaxTeamMessage + 1, 'x'), kMaxTeamMessage, "Message");
        QFAIL("no exception");
    } catch (const ValidationError& error) {
        QCOMPARE(std::string(error.what()), std::string("Message can be at most 2000 characters"));
    }
}

void TestHub::statusesRoundTrip()
{
    for (auto s : {ApplicationStatus::Pending, ApplicationStatus::Approved, ApplicationStatus::Rejected,
                   ApplicationStatus::Withdrawn, ApplicationStatus::Waitlisted})
        QCOMPARE(applicationStatusFrom(applicationStatusName(s)), s);
    QVERIFY_THROWS_EXCEPTION(ValidationError, applicationStatusFrom("accepted"));
    for (auto r : {TeamRole::Leader, TeamRole::Admin, TeamRole::Member})
        QCOMPARE(teamRoleFrom(teamRoleName(r)), r);
    QVERIFY_THROWS_EXCEPTION(ValidationError, teamRoleFrom("owner"));
    for (auto a : {AnonymousStatus::New, AnonymousStatus::Reviewed, AnonymousStatus::Interested, AnonymousStatus::Declined})
        QCOMPARE(anonymousStatusFrom(anonymousStatusName(a)), a);
    QVERIFY_THROWS_EXCEPTION(ValidationError, anonymousStatusFrom("spam"));
}

void TestHub::teamApplicationRules()
{
    const auto team = ApplicationKind::Team;
    QVERIFY(!statusAllowed(team, ApplicationStatus::Waitlisted));
    QVERIFY(isActive(team, ApplicationStatus::Pending));
    QVERIFY(!isActive(team, ApplicationStatus::Approved)); // approved = a member; membership blocks instead
    QVERIFY(canWithdraw(team, ApplicationStatus::Pending));
    QVERIFY(!canWithdraw(team, ApplicationStatus::Approved));
    QVERIFY(canDecide(team, ApplicationStatus::Pending, ApplicationStatus::Approved));
    QVERIFY(canDecide(team, ApplicationStatus::Pending, ApplicationStatus::Rejected));
    QVERIFY(!canDecide(team, ApplicationStatus::Pending, ApplicationStatus::Waitlisted));
    QVERIFY(!canDecide(team, ApplicationStatus::Rejected, ApplicationStatus::Approved));
    QVERIFY(!canDecide(team, ApplicationStatus::Withdrawn, ApplicationStatus::Approved));
    QCOMPARE(reviewerActions(team, ApplicationStatus::Pending),
             (std::vector<ApplicationStatus>{ApplicationStatus::Approved, ApplicationStatus::Rejected}));
    QVERIFY(reviewerActions(team, ApplicationStatus::Approved).empty());
}

void TestHub::eventApplicationRules()
{
    const auto event = ApplicationKind::Event;
    for (auto s : {ApplicationStatus::Pending, ApplicationStatus::Approved, ApplicationStatus::Waitlisted})
        QVERIFY(isActive(event, s));
    QVERIFY(!isActive(event, ApplicationStatus::Rejected));
    QVERIFY(!isActive(event, ApplicationStatus::Withdrawn));
    QVERIFY(canWithdraw(event, ApplicationStatus::Approved, false));
    QVERIFY(!canWithdraw(event, ApplicationStatus::Approved, true));
    QVERIFY(canWithdraw(event, ApplicationStatus::Waitlisted, true));
    QVERIFY(!canWithdraw(event, ApplicationStatus::Rejected));
    QCOMPARE(reviewerActions(event, ApplicationStatus::Pending),
             (std::vector<ApplicationStatus>{ApplicationStatus::Approved, ApplicationStatus::Waitlisted,
                                             ApplicationStatus::Rejected}));
    QCOMPARE(reviewerActions(event, ApplicationStatus::Waitlisted),
             (std::vector<ApplicationStatus>{ApplicationStatus::Approved, ApplicationStatus::Rejected}));
    QVERIFY(!canDecide(event, ApplicationStatus::Approved, ApplicationStatus::Rejected));
}

void TestHub::capacityAndWaitlist()
{
    Capacity open;
    QVERIFY(!open.isFull());
    QVERIFY(!open.spotsLeft().has_value());
    QCOMPARE(open.fillRatio(), 0.0);
    QCOMPARE(open.statusForNewApplication(), ApplicationStatus::Pending);

    Capacity small{3, 2, 0};
    QCOMPARE(*small.spotsLeft(), 1);
    QVERIFY(small.canApproveOneMore());
    small.approved = 3;
    QVERIFY(small.isFull());
    QCOMPARE(*small.spotsLeft(), 0);
    QCOMPARE(small.fillRatio(), 1.0);
    QCOMPARE(small.statusForNewApplication(), ApplicationStatus::Waitlisted);
    QVERIFY(!small.canApproveOneMore());
    small.approved = 5; // capacity lowered by an admin elsewhere: still reads as full, never negative
    QCOMPARE(*small.spotsLeft(), 0);
    QCOMPARE(small.fillRatio(), 1.0);
}

void TestHub::eventWindow()
{
    EventWindow window;
    QCOMPARE(eventClosedReason(window, 100), std::string("Not open for applications yet."));
    window.configured = true;
    QCOMPARE(eventClosedReason(window, 100), std::string("Applications are closed."));
    window.accepting = true;
    QCOMPARE(eventClosedReason(window, 100), std::string());
    window.deadline = 50;
    QCOMPARE(eventClosedReason(window, 100), std::string("The registration deadline has passed."));
    window.deadline = 200;
    QCOMPARE(eventClosedReason(window, 100), std::string());
}

void TestHub::teamRoles()
{
    QVERIFY(canManageTeam(TeamRole::Leader));
    QVERIFY(canManageTeam(TeamRole::Admin));
    QVERIFY(!canManageTeam(TeamRole::Member));
    QVERIFY(canChangeRoles(TeamRole::Leader));
    QVERIFY(!canChangeRoles(TeamRole::Admin));
    QVERIFY(canRemoveMember(TeamRole::Leader, TeamRole::Admin));
    QVERIFY(canRemoveMember(TeamRole::Admin, TeamRole::Member));
    QVERIFY(!canRemoveMember(TeamRole::Admin, TeamRole::Admin));
    QVERIFY(!canRemoveMember(TeamRole::Admin, TeamRole::Leader));
    QVERIFY(!canRemoveMember(TeamRole::Member, TeamRole::Member));
    QVERIFY(!canLeave(TeamRole::Leader));
    QVERIFY(canLeave(TeamRole::Admin));
}

void TestHub::teamApplyBlockers()
{
    TeamApplyState state{false, true, 2, 4, false};
    QCOMPARE(teamApplyBlocker(state), std::string());
    state.hasPendingApplication = true;
    QCOMPARE(teamApplyBlocker(state), std::string("Your application is waiting for the team's decision."));
    state.members = 4;
    QCOMPARE(teamApplyBlocker(state), std::string("This team is full."));
    state.recruiting = false;
    QCOMPARE(teamApplyBlocker(state), std::string("This team is not recruiting right now."));
    state.member = true;
    QCOMPARE(teamApplyBlocker(state), std::string("You are already a member of this team."));
}

namespace {

ChatMessage msg(long long id, long long author, double at, bool mine = false)
{
    return ChatMessage{id, author, "Author " + std::to_string(author), "message " + std::to_string(id), at, mine};
}

} // namespace

void TestHub::chatLogMergesPagesInOrder()
{
    ChatLog log;
    QVERIFY(log.empty());
    QCOMPARE(log.newestId(), 0LL);
    // newest page first, then an older page, then live updates incl. a duplicate echo
    QCOMPARE(log.merge({msg(10, 1, 1000), msg(11, 2, 1010), msg(12, 1, 1020)}), std::size_t(3));
    QCOMPARE(log.merge({msg(7, 2, 900), msg(8, 2, 910), msg(9, 1, 920)}), std::size_t(3));
    QCOMPARE(log.merge({msg(13, 2, 1030), msg(12, 1, 1020)}), std::size_t(1));
    QCOMPARE(log.merge({msg(13, 2, 1030)}), std::size_t(0));
    QCOMPARE(log.size(), std::size_t(7));
    QCOMPARE(log.oldestId(), 7LL);
    QCOMPARE(log.newestId(), 13LL);
    for (std::size_t i = 1; i < log.size(); ++i)
        QVERIFY(log.messages()[i - 1].id < log.messages()[i].id);
    QVERIFY(log.contains(9));
    QVERIFY(!log.contains(14));
    // a gap filled later lands in the middle
    log.merge({msg(1, 1, 100)});
    QCOMPARE(log.oldestId(), 1LL);
    log.setHasOlder(true);
    QVERIFY(log.hasOlder());
    log.clear();
    QVERIFY(log.empty());
    QVERIFY(!log.hasOlder());
}

void TestHub::chatLogUnreadAndLayout()
{
    ChatLog log;
    const double day = 86400;
    log.merge({msg(1, 1, 10 * day + 100), msg(2, 1, 10 * day + 160), msg(3, 9, 10 * day + 200, true),
               msg(4, 9, 10 * day + 900, true), msg(5, 1, 11 * day + 50)});
    QCOMPARE(log.unreadAfter(0), std::size_t(3)); // own messages never count
    QCOMPARE(log.unreadAfter(2), std::size_t(1));
    QCOMPARE(*log.firstUnread(2), std::size_t(4));
    QVERIFY(!log.firstUnread(5).has_value());
    // groups: new author, a gap over five minutes, a new day
    QVERIFY(log.startsGroup(0, 0));
    QVERIFY(!log.startsGroup(1, 0));
    QVERIFY(log.startsGroup(2, 0));
    QVERIFY(log.startsGroup(3, 0));  // same author, 700 s later
    QVERIFY(log.startsGroup(4, 0));
    QVERIFY(log.startsDay(4, 0));
    QVERIFY(!log.startsDay(3, 0));
    // the day boundary depends on the viewer's time zone: 50 s after midnight UTC is still the
    // previous day five hours west, and two hours east a late message moves to the next day
    QCOMPARE(dayNumber(11 * day + 50, 0), 11LL);
    QCOMPARE(dayNumber(11 * day + 50, -300), 10LL);
    QCOMPARE(dayNumber(10 * day + 86000, 120), 11LL);
    QVERIFY(!log.startsGroup(99, 0));
}

void TestHub::notificationsReplaceById()
{
    NotificationList list;
    list.merge({{1, "team_application", "New application", "", "#/teams/1/applications", false, 100},
                {2, "team_message", "1 new message in Rocket", "", "#/messages/1", false, 200}});
    QCOMPARE(list.unreadCount(), std::size_t(2));
    QCOMPARE(list.items().front().id, 2LL); // newest first
    // the server bumped the chat notification in place
    list.merge({{2, "team_message", "3 new messages in Rocket", "", "#/messages/1", false, 300},
                {3, "invitation", "You're invited", "", "#/teams/2?apply=1", false, 250}});
    QCOMPARE(list.items().size(), std::size_t(3));
    QCOMPARE(list.items().front().title, std::string("3 new messages in Rocket"));
    QCOMPARE(list.items()[1].id, 3LL);
    QVERIFY(list.markRead(3));
    QVERIFY(!list.markRead(42));
    QCOMPARE(list.unreadCount(), std::size_t(2));
    list.markAllRead();
    QCOMPARE(list.unreadCount(), std::size_t(0));
}

void TestHub::notificationLinks()
{
    QCOMPARE(linkRoute("#/teams/5"), std::string("teams/5"));
    QCOMPARE(linkRoute("#/teams/5?apply=1"), std::string("teams/5?apply=1"));
    QCOMPARE(linkRoute("#/anonymous/Ab_c-9"), std::string("anonymous/Ab_c-9"));
    QCOMPARE(linkRoute("/admin"), std::string());
    QCOMPARE(linkRoute("https://evil.example/#/x"), std::string());
    QCOMPARE(linkRoute("#/teams/<script>"), std::string());
}

void TestHub::anonymousRules()
{
    QVERIFY(recipientCanSet(AnonymousStatus::New, AnonymousStatus::Interested));
    QVERIFY(recipientCanSet(AnonymousStatus::Declined, AnonymousStatus::Reviewed));
    QVERIFY(!recipientCanSet(AnonymousStatus::Reviewed, AnonymousStatus::New));
    QVERIFY(canReply(AnonymousStatus::Interested));
    QVERIFY(!canReply(AnonymousStatus::Declined));
    const std::string notice = anonymityNotice();
    QVERIFY(notice.find("not shown to the recipient") != std::string::npos);
    QVERIFY(notice.find("admins can see it only if the conversation is reported") != std::string::npos);
}

void TestHub::timeTexts()
{
    QCOMPARE(relativeTime(1000, 1030), std::string("just now"));
    QCOMPARE(relativeTime(1000, 1000 + 5 * 60), std::string("5 min ago"));
    QCOMPARE(relativeTime(0, 3 * 3600 + 5), std::string("3 h ago"));
    QCOMPARE(relativeTime(0, 86400 * 1.5), std::string("yesterday"));
    QCOMPARE(relativeTime(0, 86400 * 4), std::string("4 days ago"));
    QCOMPARE(relativeTime(0, 86400 * 30), std::string());
    QCOMPARE(deadlineText(100, 200), std::string("closed"));
    QCOMPARE(deadlineText(130, 100), std::string("closes in 1 min"));
    QCOMPARE(deadlineText(100 + 5 * 3600, 100), std::string("closes in 5 h"));
    QCOMPARE(deadlineText(100 + 86400, 100), std::string("closes in 1 day"));
    QCOMPARE(deadlineText(100 + 3 * 86400 + 10, 100), std::string("closes in 3 days"));
}

QTEST_APPLESS_MAIN(TestHub)
#include "tst_hub.moc"
