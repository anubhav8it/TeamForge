// Interest workflow, host side: a project's requests, review / accept / decline, accepted
// participants for team building, and the local demo identities. Part of TeamForgeController.
// The participant side (expressInterest, myInterests) is in TeamForgeParticipant.cpp.
#include "TeamForgeController.h"

#include "BridgeSupport.h"

#include "core/Errors.h"

#include <algorithm>

using namespace teamforge;
using namespace bridge;

QVariantMap TeamForgeController::interestToMap(const InterestRequest& request) const
{
    const Student& student = engine_.students().get(request.studentId());
    const ProjectRequirement& requirement = requirements_.get(request.requirementId());
    // The participant's fit as the first member of a new team, as on their opportunity card.
    const Team empty("probe", requirement);
    const MatchResult result = weighted_.evaluate(student, empty);
    QVariantMap map = studentToMap(student);
    map.insert(QStringLiteral("studentId"), qs(student.id()));
    map.insert(QStringLiteral("id"), qs(request.id()));
    map.insert(QStringLiteral("requirementId"), qs(requirement.id()));
    map.insert(QStringLiteral("status"), qs(interestStatusName(request.status())));
    map.insert(QStringLiteral("score"), result.score);
    map.insert(QStringLiteral("coveredSkills"), toQStringList(result.coveredSkills));
    return map;
}

QVariantList TeamForgeController::projectInterests(const QString& requirementId) const
{
    QVariantList list;
    if (!allowed(Permission::BuildTeams) || !requirements_.contains(ss(requirementId)))
        return list;
    std::vector<QVariantMap> rows;
    for (const InterestRequest& request : interests_.forRequirement(ss(requirementId)))
        rows.push_back(interestToMap(request));
    // Review stage first (interested, under review, accepted, declined), then best fit.
    const auto stage = [](const QVariantMap& row) {
        return static_cast<int>(interestStatusFrom(ss(row.value(QStringLiteral("status")).toString())));
    };
    std::stable_sort(rows.begin(), rows.end(), [&](const QVariantMap& a, const QVariantMap& b) {
        if (stage(a) != stage(b))
            return stage(a) < stage(b);
        return a.value(QStringLiteral("score")).toDouble() > b.value(QStringLiteral("score")).toDouble();
    });
    for (const QVariantMap& row : rows)
        list.append(row);
    return list;
}

bool TeamForgeController::moveInterest(const char *operation, const QString& requestId, InterestStatus status)
{
    const bool ok = run(operation, requestId, [&] {
        require(Permission::BuildTeams);
        const InterestRequest& request = interests_.setStatus(ss(requestId), status); // NotFound / Validation
        setDirty(true);
        logEvent(AppLog::Level::Info, QStringLiteral("interest"),
                 QStringLiteral("%1: %2 for %3")
                     .arg(qs(interestStatusName(status)), qs(engine_.students().get(request.studentId()).name()),
                          qs(requirements_.get(request.requirementId()).name())));
    });
    if (!ok)
        return false;
    notifyDataChanged();
    refresh();
    emit interestsChanged();
    return save();
}

bool TeamForgeController::reviewInterest(const QString& requestId)
{
    // Opening a new request marks it under review; opening it again changes nothing.
    const InterestRequest *request = nullptr;
    run("reviewInterest", requestId, [&] {
        require(Permission::BuildTeams);
        request = &interests_.get(ss(requestId));
    }, false);
    if (request == nullptr)
        return false;
    if (request->status() != InterestStatus::Interested)
        return true;
    return moveInterest("reviewInterest", requestId, InterestStatus::UnderReview);
}

bool TeamForgeController::acceptInterest(const QString& requestId)
{
    return moveInterest("acceptInterest", requestId, InterestStatus::Accepted);
}

bool TeamForgeController::declineInterest(const QString& requestId)
{
    return moveInterest("declineInterest", requestId, InterestStatus::Declined);
}

QVariantList TeamForgeController::acceptedCandidates() const
{
    QVariantList list;
    if (!currentTeam_)
        return list;
    std::vector<MatchResult> results;
    for (const InterestRequest& request : interests_.forRequirement(currentRequirementId_)) {
        if (request.status() != InterestStatus::Accepted)
            continue;
        Team team = *currentTeam_;
        if (team.contains(request.studentId()))
            team.removeMember(request.studentId()); // a member is scored as if added last
        results.push_back(activeStrategy().evaluate(engine_.students().get(request.studentId()), team));
    }
    std::sort(results.begin(), results.end(), std::greater<MatchResult>());
    for (const MatchResult& result : results) {
        QVariantMap map = matchResultToMap(result, *currentTeam_, !useBaseline_);
        map.insert(QStringLiteral("isMember"), currentTeam_->contains(result.studentId));
        list.append(map);
    }
    return list;
}

QVariantList TeamForgeController::demoIdentities() const
{
    QVariantMap participant{{QStringLiteral("kind"), QStringLiteral("participant")},
                            {QStringLiteral("id"), qs(kDemoParticipant)},
                            {QStringLiteral("label"), QStringLiteral("Demo Participant")}};
    if (const Student *student = engine_.students().find(kDemoParticipant))
        participant.insert(QStringLiteral("name"), qs(student->name()));
    return {participant, hostIdentity()};
}

QVariantMap TeamForgeController::hostIdentity() const
{
    return {{QStringLiteral("kind"), QStringLiteral("host")},
            {QStringLiteral("id"), QString::fromLatin1(kDemoHostId)},
            {QStringLiteral("name"), QString::fromLatin1(kDemoHostName)},
            {QStringLiteral("label"), QStringLiteral("Demo Host")}};
}
