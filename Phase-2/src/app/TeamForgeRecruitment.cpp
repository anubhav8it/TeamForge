// Team recruitment in the Community: compatibility reports and team skill coverage
// (core/Compatibility.h) for Community teams. Part of TeamForgeController.
//
// The server decides who may be searched, invited or reviewed (web/recruitment.py) and only sends
// people who are open to team invitations, applicants and invitees. The skills and levels come
// from the shared participant profiles this app already holds, so the score is computed here with
// the same C++ matching engine as everywhere else in TeamForge.
#include "TeamForgeController.h"

#include "BridgeSupport.h"

#include "core/Compatibility.h"
#include "core/Errors.h"

using namespace teamforge;
using namespace bridge;

namespace {

SkillLevels levelsFrom(const QVariant& value)
{
    SkillLevels out;
    const QVariantMap map = value.toMap();
    for (auto it = map.constBegin(); it != map.constEnd(); ++it) {
        bool ok = false;
        const int level = it.value().toInt(&ok);
        if (ok && level >= kMinSkillLevel && level <= kMaxSkillLevel && !it.key().trimmed().isEmpty())
            out[ss(it.key())] = level;
    }
    return out;
}

std::vector<std::string> nonEmptyStrings(const QVariant& value)
{
    std::vector<std::string> out;
    for (const QVariant& item : value.toList())
        if (!item.toString().trimmed().isEmpty())
            out.push_back(ss(item.toString()));
    return out;
}

QVariantMap fitToMap(const SkillFit& fit)
{
    return {{QStringLiteral("skill"), qs(fit.skill)}, {QStringLiteral("required"), fit.required},
            {QStringLiteral("level"), fit.level}, {QStringLiteral("evidence"), qs(evidenceName(fit.evidence))},
            {QStringLiteral("fillsGap"), fit.fillsGap}};
}

QStringList toList(const std::vector<std::string>& items)
{
    QStringList out;
    for (const std::string& item : items)
        out << qs(item);
    return out;
}

} // namespace

TeamNeeds TeamForgeController::teamNeedsFrom(const QVariantMap& team) const
{
    TeamNeeds needs;
    needs.name = ss(team.value(QStringLiteral("name")).toString());
    needs.required = levelsFrom(team.value(QStringLiteral("requiredSkills")));
    // A team made for one of the app's projects and without its own skill list uses the project's.
    const QVariantMap event = team.value(QStringLiteral("event")).toMap();
    if (needs.required.empty() && !event.value(QStringLiteral("id")).toString().isEmpty())
        if (const ProjectRequirement *project = requirements_.find(ss(event.value(QStringLiteral("id")).toString())))
            needs.required = project->requiredSkills();
    needs.maxMembers = static_cast<std::size_t>(std::max(1, team.value(QStringLiteral("maxMembers"), 6).toInt()));
    needs.openRoles = nonEmptyStrings(team.value(QStringLiteral("openRoles")));
    // Members: by their participant profile ids (from the team's member list or the link claim).
    QStringList ids = team.value(QStringLiteral("memberStudentIds")).toStringList();
    for (const QVariant& member : team.value(QStringLiteral("members")).toList())
        ids << member.toMap().value(QStringLiteral("studentId")).toString();
    ids.removeAll(QString());
    ids.removeDuplicates();
    for (const QString& id : ids)
        if (const Student *student = engine_.students().find(ss(id)))
            needs.members.push_back(*student);
    return needs;
}

QVariantMap TeamForgeController::teamCompatibility(const QVariantMap& team, const QVariantMap& person) const
{
    QVariantMap map;
    try {
        const TeamNeeds needs = teamNeedsFrom(team);
        CandidateProfile candidate;
        const QString studentId = person.value(QStringLiteral("studentId")).toString();
        const Student *student = studentId.isEmpty() ? nullptr : engine_.students().find(ss(studentId));
        candidate.id = student ? student->id() : "account-" + ss(person.value(QStringLiteral("id")).toString());
        candidate.name = student ? student->name() : ss(person.value(QStringLiteral("name")).toString());
        candidate.role = student ? student->role() : ss(person.value(QStringLiteral("headline")).toString());
        if (student) {
            candidate.levels = student->skillsOffered();
            candidate.details = student->details();
        }
        // Skills named on the Community profile (and in an application) without a level.
        candidate.unratedSkills = nonEmptyStrings(person.value(QStringLiteral("skills")));
        for (const std::string& extra : nonEmptyStrings(person.value(QStringLiteral("applicationSkills"))))
            candidate.unratedSkills.push_back(extra);
        candidate.links = nonEmptyStrings(person.value(QStringLiteral("links")));
        candidate.expressedInterest = person.value(QStringLiteral("expressedInterest")).toBool();

        const CompatibilityReport report = evaluateCompatibility(candidate, needs);
        QVariantList matching, below;
        for (const SkillFit& fit : report.matching)
            matching << fitToMap(fit);
        for (const SkillFit& fit : report.belowLevel)
            below << fitToMap(fit);
        map = {
            {QStringLiteral("score"), report.score},
            {QStringLiteral("percent"), report.percent},
            {QStringLiteral("band"), qs(bandName(report.band))},
            {QStringLiteral("bandLabel"), qs(bandLabel(report.band))},
            {QStringLiteral("enoughInformation"), report.enoughInformation},
            {QStringLiteral("teamHasSkills"), !needs.required.empty()},
            {QStringLiteral("hasParticipantProfile"), student != nullptr},
            {QStringLiteral("factors"), QVariantMap{{QStringLiteral("coverage"), report.factors.coverage},
                                                    {QStringLiteral("complementarity"), report.factors.complementarity},
                                                    {QStringLiteral("experience"), report.factors.experience},
                                                    {QStringLiteral("engagement"), report.factors.engagement},
                                                    {QStringLiteral("diversity"), report.factors.diversity}}},
            {QStringLiteral("matching"), matching},
            {QStringLiteral("belowLevel"), below},
            {QStringLiteral("notShown"), toList(report.notShown)},
            {QStringLiteral("strengths"), toList(report.strengths)},
            {QStringLiteral("suggestedRole"), qs(report.suggestedRole)},
            {QStringLiteral("roleReason"), qs(report.roleReason)},
            {QStringLiteral("improvements"), toList(report.improvements)},
            {QStringLiteral("explanation"), toList(report.explanation)},
        };
    } catch (const TeamForgeError& error) { // never expected (bad data is skipped); kept defensive
        map = {{QStringLiteral("enoughInformation"), false}, {QStringLiteral("band"), QStringLiteral("unknown")},
               {QStringLiteral("bandLabel"), QStringLiteral("Not enough information")},
               {QStringLiteral("explanation"), QStringList{QString::fromUtf8(error.what())}}};
    }
    return map;
}

QVariantList TeamForgeController::teamSkillCoverage(const QVariantMap& team) const
{
    QVariantList out;
    for (const SkillCoverage& entry : teamCoverage(teamNeedsFrom(team)))
        out << QVariantMap{{QStringLiteral("skill"), qs(entry.skill)}, {QStringLiteral("required"), entry.required},
                           {QStringLiteral("bestLevel"), entry.bestLevel},
                           {QStringLiteral("state"), qs(coverageName(entry.state))},
                           {QStringLiteral("coveredBy"), toList(entry.coveredBy)}};
    return out;
}

QVariantMap TeamForgeController::participantCard(const QString& studentId) const
{
    // What a team leader sees about a candidate or applicant: the participant profile's skills
    // with levels and its details. Never contact details (the records have none).
    const Student *student = studentId.isEmpty() ? nullptr : engine_.students().find(ss(studentId));
    if (student == nullptr)
        return {};
    QVariantMap map{{QStringLiteral("id"), qs(student->id())}, {QStringLiteral("name"), qs(student->name())},
                    {QStringLiteral("role"), qs(student->role())}, {QStringLiteral("summary"), qs(student->summary())},
                    {QStringLiteral("program"), qs(student->program())},
                    {QStringLiteral("skills"), skillLevelList(student->skillsOffered(), "level")},
                    {QStringLiteral("details"), detailsToMap(student->details())}};
    return map;
}
