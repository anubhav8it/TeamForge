#include "core/Hub.h"

#include "core/Errors.h"

#include <algorithm>
#include <cmath>

namespace teamforge::hub {

// ------------------------------------------------------------------------- text

std::size_t characterCount(std::string_view text)
{
    std::size_t count = 0;
    for (const char c : text)
        count += (static_cast<unsigned char>(c) & 0xC0) != 0x80 ? 1 : 0;
    return count;
}

namespace {

bool isSpace(char c)
{
    return c == ' ' || c == '\t' || c == '\n';
}

} // namespace

std::string cleanText(std::string_view text, std::size_t limit, const std::string& what, bool required)
{
    std::string out;
    out.reserve(text.size());
    int newlines = 0;
    for (std::size_t i = 0; i < text.size(); ++i) {
        char c = text[i];
        if (c == '\r') {
            if (i + 1 < text.size() && text[i + 1] == '\n')
                continue; // CR LF -> LF (the LF follows)
            c = '\n';
        }
        if (c == '\n') {
            if (++newlines <= 2)
                out += c;
            continue;
        }
        const auto u = static_cast<unsigned char>(c);
        if ((u < 0x20 && c != '\t') || u == 0x7F)
            continue;
        newlines = 0;
        out += c;
    }
    const auto first = std::find_if_not(out.begin(), out.end(), isSpace);
    const auto last = std::find_if_not(out.rbegin(), out.rend(), isSpace).base();
    out = first < last ? std::string(first, last) : std::string();
    if (required && out.empty())
        throw ValidationError(what + " is required");
    if (characterCount(out) > limit)
        throw ValidationError(what + " can be at most " + std::to_string(limit) + " characters");
    return out;
}

// ------------------------------------------------------------------------- applications

std::string applicationStatusName(ApplicationStatus status)
{
    switch (status) {
    case ApplicationStatus::Pending: return "pending";
    case ApplicationStatus::Approved: return "approved";
    case ApplicationStatus::Rejected: return "rejected";
    case ApplicationStatus::Withdrawn: return "withdrawn";
    case ApplicationStatus::Waitlisted: return "waitlisted";
    }
    return "pending";
}

ApplicationStatus applicationStatusFrom(std::string_view text)
{
    for (ApplicationStatus status : {ApplicationStatus::Pending, ApplicationStatus::Approved, ApplicationStatus::Rejected,
                                     ApplicationStatus::Withdrawn, ApplicationStatus::Waitlisted})
        if (applicationStatusName(status) == text)
            return status;
    throw ValidationError("Unknown application status '" + std::string(text) + "'");
}

bool statusAllowed(ApplicationKind kind, ApplicationStatus status)
{
    return kind == ApplicationKind::Event || status != ApplicationStatus::Waitlisted;
}

bool isActive(ApplicationKind kind, ApplicationStatus status)
{
    if (kind == ApplicationKind::Team)
        return status == ApplicationStatus::Pending;
    return status == ApplicationStatus::Pending || status == ApplicationStatus::Approved
        || status == ApplicationStatus::Waitlisted;
}

bool canWithdraw(ApplicationKind kind, ApplicationStatus status, bool deadlinePassed)
{
    if (kind == ApplicationKind::Team)
        return status == ApplicationStatus::Pending;
    if (status == ApplicationStatus::Approved)
        return !deadlinePassed;
    return status == ApplicationStatus::Pending || status == ApplicationStatus::Waitlisted;
}

bool canDecide(ApplicationKind kind, ApplicationStatus from, ApplicationStatus to)
{
    if (!statusAllowed(kind, to))
        return false;
    if (from == ApplicationStatus::Pending)
        return to == ApplicationStatus::Approved || to == ApplicationStatus::Rejected
            || (kind == ApplicationKind::Event && to == ApplicationStatus::Waitlisted);
    if (from == ApplicationStatus::Waitlisted && kind == ApplicationKind::Event)
        return to == ApplicationStatus::Approved || to == ApplicationStatus::Rejected;
    return false;
}

std::vector<ApplicationStatus> reviewerActions(ApplicationKind kind, ApplicationStatus status)
{
    std::vector<ApplicationStatus> actions;
    for (ApplicationStatus to : {ApplicationStatus::Approved, ApplicationStatus::Waitlisted, ApplicationStatus::Rejected})
        if (canDecide(kind, status, to))
            actions.push_back(to);
    return actions;
}

// ------------------------------------------------------------------------- event capacity

std::optional<int> Capacity::spotsLeft() const
{
    if (!limit)
        return std::nullopt;
    return std::max(0, *limit - approved);
}

double Capacity::fillRatio() const
{
    if (!limit || *limit <= 0)
        return 0.0;
    return std::clamp(static_cast<double>(approved) / *limit, 0.0, 1.0);
}

ApplicationStatus Capacity::statusForNewApplication() const
{
    return isFull() ? ApplicationStatus::Waitlisted : ApplicationStatus::Pending;
}

std::string eventClosedReason(const EventWindow& window, double now)
{
    if (!window.configured)
        return "Not open for applications yet.";
    if (!window.accepting)
        return "Applications are closed.";
    if (window.deadline && now > *window.deadline)
        return "The registration deadline has passed.";
    return {};
}

// ------------------------------------------------------------------------- teams

std::string teamRoleName(TeamRole role)
{
    switch (role) {
    case TeamRole::Leader: return "leader";
    case TeamRole::Admin: return "admin";
    case TeamRole::Member: return "member";
    }
    return "member";
}

TeamRole teamRoleFrom(std::string_view text)
{
    for (TeamRole role : {TeamRole::Leader, TeamRole::Admin, TeamRole::Member})
        if (teamRoleName(role) == text)
            return role;
    throw ValidationError("Unknown team role '" + std::string(text) + "'");
}

bool canManageTeam(TeamRole role)
{
    return role == TeamRole::Leader || role == TeamRole::Admin;
}

bool canChangeRoles(TeamRole role)
{
    return role == TeamRole::Leader;
}

bool canRemoveMember(TeamRole actor, TeamRole target)
{
    if (target == TeamRole::Leader)
        return false;
    if (actor == TeamRole::Leader)
        return true;
    return actor == TeamRole::Admin && target == TeamRole::Member;
}

bool canLeave(TeamRole role)
{
    return role != TeamRole::Leader;
}

std::string teamApplyBlocker(const TeamApplyState& state)
{
    if (state.member)
        return "You are already a member of this team.";
    if (!state.recruiting)
        return "This team is not recruiting right now.";
    if (state.maxMembers > 0 && state.members >= state.maxMembers)
        return "This team is full.";
    if (state.hasPendingApplication)
        return "Your application is waiting for the team's decision.";
    return {};
}

// ------------------------------------------------------------------------- chat

long long dayNumber(double epochSeconds, int utcOffsetMinutes)
{
    return static_cast<long long>(std::floor((epochSeconds + utcOffsetMinutes * 60.0) / 86400.0));
}

std::size_t ChatLog::merge(const std::vector<ChatMessage>& page)
{
    std::size_t added = 0;
    for (const ChatMessage& message : page) {
        const auto at = std::lower_bound(messages_.begin(), messages_.end(), message.id,
                                         [](const ChatMessage& m, long long id) { return m.id < id; });
        if (at != messages_.end() && at->id == message.id)
            continue;
        messages_.insert(at, message);
        ++added;
    }
    return added;
}

void ChatLog::clear()
{
    messages_.clear();
    hasOlder_ = false;
}

bool ChatLog::contains(long long id) const
{
    const auto at = std::lower_bound(messages_.begin(), messages_.end(), id,
                                     [](const ChatMessage& m, long long key) { return m.id < key; });
    return at != messages_.end() && at->id == id;
}

std::size_t ChatLog::unreadAfter(long long lastReadId) const
{
    return static_cast<std::size_t>(std::count_if(messages_.begin(), messages_.end(), [lastReadId](const ChatMessage& m) {
        return m.id > lastReadId && !m.mine;
    }));
}

std::optional<std::size_t> ChatLog::firstUnread(long long lastReadId) const
{
    for (std::size_t i = 0; i < messages_.size(); ++i)
        if (messages_[i].id > lastReadId && !messages_[i].mine)
            return i;
    return std::nullopt;
}

bool ChatLog::startsDay(std::size_t index, int utcOffsetMinutes) const
{
    if (index >= messages_.size())
        return false;
    if (index == 0)
        return true;
    return dayNumber(messages_[index].createdAt, utcOffsetMinutes)
        != dayNumber(messages_[index - 1].createdAt, utcOffsetMinutes);
}

bool ChatLog::startsGroup(std::size_t index, int utcOffsetMinutes, double gapSeconds) const
{
    if (index >= messages_.size())
        return false;
    if (index == 0 || startsDay(index, utcOffsetMinutes))
        return true;
    const ChatMessage& previous = messages_[index - 1];
    const ChatMessage& current = messages_[index];
    return previous.authorId != current.authorId || current.createdAt - previous.createdAt > gapSeconds;
}

// ------------------------------------------------------------------------- notifications

void NotificationList::merge(const std::vector<Notification>& items)
{
    for (const Notification& item : items) {
        const auto found = std::find_if(items_.begin(), items_.end(), [&](const Notification& n) { return n.id == item.id; });
        if (found != items_.end())
            *found = item;
        else
            items_.push_back(item);
    }
    std::stable_sort(items_.begin(), items_.end(), [](const Notification& a, const Notification& b) {
        return a.createdAt != b.createdAt ? a.createdAt > b.createdAt : a.id > b.id;
    });
}

bool NotificationList::markRead(long long id)
{
    for (Notification& n : items_)
        if (n.id == id) {
            n.read = true;
            return true;
        }
    return false;
}

void NotificationList::markAllRead()
{
    for (Notification& n : items_)
        n.read = true;
}

std::size_t NotificationList::unreadCount() const
{
    return static_cast<std::size_t>(std::count_if(items_.begin(), items_.end(), [](const Notification& n) { return !n.read; }));
}

std::string linkRoute(std::string_view link)
{
    if (link.substr(0, 2) != "#/")
        return {};
    std::string route(link.substr(2));
    for (const char c : route) {
        const bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '/' || c == '-'
                     || c == '_' || c == '.' || c == '?' || c == '=' || c == '&';
        if (!ok)
            return {};
    }
    return route;
}

// ------------------------------------------------------------------------- anonymous requests

std::string anonymousStatusName(AnonymousStatus status)
{
    switch (status) {
    case AnonymousStatus::New: return "new";
    case AnonymousStatus::Reviewed: return "reviewed";
    case AnonymousStatus::Interested: return "interested";
    case AnonymousStatus::Declined: return "declined";
    }
    return "new";
}

AnonymousStatus anonymousStatusFrom(std::string_view text)
{
    for (AnonymousStatus status : {AnonymousStatus::New, AnonymousStatus::Reviewed, AnonymousStatus::Interested,
                                   AnonymousStatus::Declined})
        if (anonymousStatusName(status) == text)
            return status;
    throw ValidationError("Unknown request status '" + std::string(text) + "'");
}

bool recipientCanSet(AnonymousStatus from, AnonymousStatus to)
{
    (void)from;
    return to != AnonymousStatus::New;
}

bool canReply(AnonymousStatus status)
{
    return status != AnonymousStatus::Declined;
}

std::string anonymityNotice()
{
    return "Your name, email and profile are not shown to the recipient, and nothing about you is added to "
           "your message, so leave out details that identify you if you want to stay anonymous. TeamForge "
           "still records which account sent it, to stop spam and abuse: admins can see it only if the "
           "conversation is reported. The recipient can reply here, invite you to apply (applying shows your "
           "profile) or block further messages.";
}

// ------------------------------------------------------------------------- time

std::string relativeTime(double then, double now)
{
    const double seconds = now - then;
    if (seconds < 60)
        return "just now";
    if (seconds < 3600)
        return std::to_string(static_cast<long long>(seconds / 60)) + " min ago";
    if (seconds < 86400)
        return std::to_string(static_cast<long long>(seconds / 3600)) + " h ago";
    const auto days = static_cast<long long>(seconds / 86400);
    if (days == 1)
        return "yesterday";
    if (days < 7)
        return std::to_string(days) + " days ago";
    return {};
}

std::string deadlineText(double deadline, double now)
{
    const double seconds = deadline - now;
    if (seconds <= 0)
        return "closed";
    if (seconds < 3600)
        return "closes in " + std::to_string(std::max(1LL, static_cast<long long>(seconds / 60))) + " min";
    if (seconds < 86400)
        return "closes in " + std::to_string(static_cast<long long>(seconds / 3600)) + " h";
    const auto days = static_cast<long long>(seconds / 86400);
    return "closes in " + std::to_string(days) + (days == 1 ? " day" : " days");
}

} // namespace teamforge::hub
