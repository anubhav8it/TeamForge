#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

// TeamForge Hub: the rules and client-side data structures behind teams with a private chat,
// applications to join a team or take part in an event, anonymous connection requests and
// notifications. No Qt, no I/O.
//
// The web server (web/hub.py) is the authority: it stores everything and checks every request
// against the signed-in account, because a program running in someone's browser can be changed.
// These classes hold the same rules so the app can explain them before a request is sent (which
// buttons to show, why "Apply" is unavailable, whether an approval would exceed capacity), and
// keep what the server sent in order (ChatLog, NotificationList). tst_hub checks them; the
// server's own tests (web/tests/test_hub.py) check the same rules end to end.
namespace teamforge::hub {

// ------------------------------------------------------------------------- text

// Longest texts, in characters (UTF-8 code points), as the server enforces them.
constexpr std::size_t kMaxTeamMessage = 2000;
constexpr std::size_t kMaxAnonymousMessage = 1000;
constexpr std::size_t kMaxApplicationText = 1500;
constexpr std::size_t kMaxResponse = 1000;

// Number of characters (code points) in UTF-8 text.
std::size_t characterCount(std::string_view text);

// Plain, bounded text, exactly as the server cleans it: CR LF / CR become LF, control characters
// other than tab and line feed are removed, runs of three or more line breaks become two, and
// the result is trimmed. Throws ValidationError naming `what` when it is empty but `required`,
// or longer than `limit` characters.
std::string cleanText(std::string_view text, std::size_t limit, const std::string& what, bool required = false);

// ------------------------------------------------------------------------- applications

enum class ApplicationKind { Team, Event };
enum class ApplicationStatus { Pending, Approved, Rejected, Withdrawn, Waitlisted };

// "pending", "approved", "rejected", "withdrawn", "waitlisted" (the stored spelling).
std::string applicationStatusName(ApplicationStatus status);
// Throws ValidationError on an unknown status.
ApplicationStatus applicationStatusFrom(std::string_view text);
// Waitlisted exists only for events.
bool statusAllowed(ApplicationKind kind, ApplicationStatus status);

// Still counts: blocks a second application by the same person (team: pending; event: pending,
// approved or waitlisted).
bool isActive(ApplicationKind kind, ApplicationStatus status);
// The applicant may withdraw: a pending team application; an event application that is pending,
// waitlisted or approved (an approved one only until the registration deadline).
bool canWithdraw(ApplicationKind kind, ApplicationStatus status, bool deadlinePassed = false);
// The reviewer may move `from` to `to`. Team: pending -> approved | rejected. Event:
// pending -> approved | rejected | waitlisted, waitlisted -> approved | rejected. Decided
// applications are final (the person can apply again after a rejection or withdrawal).
bool canDecide(ApplicationKind kind, ApplicationStatus from, ApplicationStatus to);
// The moves canDecide() allows from `status`, in button order (approve, waitlist, reject).
std::vector<ApplicationStatus> reviewerActions(ApplicationKind kind, ApplicationStatus status);

// ------------------------------------------------------------------------- event capacity

// How full an event is. `limit` empty = no limit.
struct Capacity
{
    std::optional<int> limit;
    int approved = 0;
    int waitlisted = 0;

    bool isFull() const { return limit.has_value() && approved >= *limit; }
    // Places left, or empty without a limit.
    std::optional<int> spotsLeft() const;
    // Approved / limit in [0, 1] (0 without a limit), for a meter.
    double fillRatio() const;
    // A new application joins the waitlist once the event is full, otherwise waits for review.
    ApplicationStatus statusForNewApplication() const;
    // Approving one more stays within the limit.
    bool canApproveOneMore() const { return !isFull(); }
};

// Why applying is not possible right now, or "" when it is. Checked in this order, the same
// order as the server's refusals.
struct EventWindow
{
    bool configured = false; // the organiser opened the project for applications
    bool accepting = false;
    std::optional<double> deadline; // seconds since the epoch
};
std::string eventClosedReason(const EventWindow& window, double now);

// ------------------------------------------------------------------------- teams

enum class TeamRole { Leader, Admin, Member };
std::string teamRoleName(TeamRole role);           // "leader", "admin", "member"
TeamRole teamRoleFrom(std::string_view text);      // ValidationError on anything else

// Leaders and team admins review applications and anonymous requests for the team.
bool canManageTeam(TeamRole role);
// Only the leader changes roles (making someone else leader hands leadership over).
bool canChangeRoles(TeamRole role);
// Leaders remove admins and members; admins remove members; nobody removes the leader.
bool canRemoveMember(TeamRole actor, TeamRole target);
// Everyone but the leader may leave (the leader hands over or deletes the team).
bool canLeave(TeamRole role);

// Why a person cannot apply to a team, or "" when they can.
struct TeamApplyState
{
    bool member = false;
    bool recruiting = true;
    int members = 0;
    int maxMembers = 0;
    bool hasPendingApplication = false;
};
std::string teamApplyBlocker(const TeamApplyState& state);

// ------------------------------------------------------------------------- chat

struct ChatMessage
{
    long long id = 0;
    long long authorId = 0;
    std::string authorName;
    std::string body;
    double createdAt = 0; // seconds since the epoch
    bool mine = false;
};

// One team's messages as the app holds them: ordered by id (the server's order, so also by
// time), each message once, whichever page it came from (newest page, older pages, live
// updates, the echo of one's own message). Insertion is a binary search into a sorted vector:
// pages arrive in order, so most inserts are at an end.
class ChatLog
{
public:
    // Messages from any page; returns how many were new. Messages with an id it already has are
    // ignored (a reply to a send can race the live update carrying the same message).
    std::size_t merge(const std::vector<ChatMessage>& page);
    void clear();

    const std::vector<ChatMessage>& messages() const { return messages_; }
    std::size_t size() const { return messages_.size(); }
    bool empty() const { return messages_.empty(); }
    bool contains(long long id) const;
    long long newestId() const { return messages_.empty() ? 0 : messages_.back().id; }
    long long oldestId() const { return messages_.empty() ? 0 : messages_.front().id; }

    // Whether the server has older messages than the oldest held (from the last older page).
    bool hasOlder() const { return hasOlder_; }
    void setHasOlder(bool value) { hasOlder_ = value; }

    // Messages from others after `lastReadId`.
    std::size_t unreadAfter(long long lastReadId) const;
    // Index of the first message from others after `lastReadId`, for a "New messages" divider,
    // or empty when there is none.
    std::optional<std::size_t> firstUnread(long long lastReadId) const;

    // Layout: a message starts a new visual group when it begins a new day, comes from another
    // author than the one before, or follows it by more than `gapSeconds`.
    bool startsGroup(std::size_t index, int utcOffsetMinutes, double gapSeconds = 300) const;
    // A message is the first of its calendar day (in the viewer's time zone).
    bool startsDay(std::size_t index, int utcOffsetMinutes) const;

private:
    std::vector<ChatMessage> messages_;
    bool hasOlder_ = false;
};

// Day number (days since 1970-01-01) of a time in a time zone `utcOffsetMinutes` east of UTC.
long long dayNumber(double epochSeconds, int utcOffsetMinutes);

// ------------------------------------------------------------------------- notifications

struct Notification
{
    long long id = 0;
    std::string kind;
    std::string title;
    std::string body;
    std::string link; // "#/teams/5", "#/messages/5", "#/anonymous/<id>" ... (see linkRoute)
    bool read = false;
    double createdAt = 0;
};

// The notifications the app has seen, newest first. merge() replaces by id: the server updates a
// notification in place (a busy chat bumps one "new messages" notification).
class NotificationList
{
public:
    void merge(const std::vector<Notification>& items);
    void clear() { items_.clear(); }
    // Returns false for an unknown id.
    bool markRead(long long id);
    void markAllRead();

    const std::vector<Notification>& items() const { return items_; }
    std::size_t unreadCount() const;

private:
    std::vector<Notification> items_;
};

// The app's route for a notification link: "#/teams/5" -> "teams/5". Links outside the hub
// (such as "/admin") and anything unexpected give "".
std::string linkRoute(std::string_view link);

// ------------------------------------------------------------------------- anonymous requests

enum class AnonymousStatus { New, Reviewed, Interested, Declined };
std::string anonymousStatusName(AnonymousStatus status); // "new", "reviewed", "interested", "declined"
AnonymousStatus anonymousStatusFrom(std::string_view text);
// Recipients mark a request reviewed, interested or declined (never back to new).
bool recipientCanSet(AnonymousStatus from, AnonymousStatus to);
// Either side may write while the request is not declined.
bool canReply(AnonymousStatus status);

// What anonymity means, shown next to every anonymous form and inbox.
std::string anonymityNotice();

// ------------------------------------------------------------------------- time

// "just now", "5 min ago", "3 h ago", "yesterday", "4 days ago"; older: "" (show the date).
std::string relativeTime(double then, double now);
// "closes in 3 days", "closes in 5 h", "closes in 20 min", "closed".
std::string deadlineText(double deadline, double now);

} // namespace teamforge::hub
