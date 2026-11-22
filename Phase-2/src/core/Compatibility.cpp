#include "core/Compatibility.h"

#include "core/Errors.h"
#include "core/Interest.h"
#include "core/ProjectRequirement.h"
#include "core/Team.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <set>

namespace teamforge {

namespace {

std::string lower(std::string text)
{
    for (char& c : text)
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return text;
}

std::string safeNormalize(const std::string& name)
{
    try {
        return normalizeSkill(name);
    } catch (const ValidationError&) {
        return {};
    }
}

// True when `skill` appears as a whole word (or phrase) in `text`: "c++" in "Built it in C++",
// not "go" in "Google".
bool mentions(const std::string& text, const std::string& skill)
{
    const std::string haystack = lower(text);
    std::size_t at = haystack.find(skill);
    while (at != std::string::npos) {
        const bool startOk = at == 0 || !std::isalnum(static_cast<unsigned char>(haystack[at - 1]));
        const std::size_t end = at + skill.size();
        const bool endOk = end >= haystack.size() || !std::isalnum(static_cast<unsigned char>(haystack[end]));
        if (startOk && endOk)
            return true;
        at = haystack.find(skill, at + 1);
    }
    return false;
}

bool backedByEvidence(const CandidateProfile& candidate, const std::string& skill)
{
    for (const auto& [name, years] : candidate.details.skillYears)
        if (safeNormalize(name) == skill && years > 0)
            return true;
    for (const auto *list : {&candidate.details.projects, &candidate.details.certifications,
                             &candidate.details.achievements})
        for (const std::string& line : *list)
            if (mentions(line, skill))
                return true;
    return false;
}

std::string joinNames(const std::vector<std::string>& names, std::size_t most = 3)
{
    std::string out;
    const std::size_t shown = std::min(most, names.size());
    for (std::size_t i = 0; i < shown; ++i) {
        if (i > 0)
            out += i + 1 == shown ? " and " : ", ";
        out += names[i];
    }
    if (names.size() > shown)
        out += " (+" + std::to_string(names.size() - shown) + " more)";
    return out;
}

std::string percentText(double value)
{
    return std::to_string(static_cast<int>(std::lround(value * 100.0))) + "%";
}

// Engagement for this one evaluation: 1 when the person has expressed interest (applied or came
// through a link), otherwise neutral (0.5), exactly like the app's project matching.
class OneCandidateEngagement final : public EngagementSource
{
public:
    explicit OneCandidateEngagement(bool interested) : interested_(interested) {}
    double engagement(const std::string&, const std::string&) const override { return interested_ ? 1.0 : 0.5; }

private:
    bool interested_;
};

} // namespace

std::string evidenceName(Evidence evidence)
{
    switch (evidence) {
    case Evidence::Projects: return "projects";
    case Evidence::Declared: return "declared";
    case Evidence::LevelNotStated: return "unrated";
    }
    return "declared";
}

std::string bandName(FitBand band)
{
    switch (band) {
    case FitBand::Strong: return "strong";
    case FitBand::Good: return "good";
    case FitBand::Partial: return "partial";
    case FitBand::Low: return "low";
    case FitBand::NotEnoughInformation: return "unknown";
    }
    return "unknown";
}

std::string bandLabel(FitBand band)
{
    switch (band) {
    case FitBand::Strong: return "Strong match";
    case FitBand::Good: return "Good match";
    case FitBand::Partial: return "Partial match";
    case FitBand::Low: return "Low match";
    case FitBand::NotEnoughInformation: return "Not enough information";
    }
    return "Not enough information";
}

std::string coverageName(CoverageState state)
{
    switch (state) {
    case CoverageState::Covered: return "covered";
    case CoverageState::Partial: return "partial";
    case CoverageState::Missing: return "missing";
    }
    return "missing";
}

FitBand bandFor(double score)
{
    if (score >= 0.70)
        return FitBand::Strong;
    if (score >= 0.50)
        return FitBand::Good;
    if (score >= 0.30)
        return FitBand::Partial;
    return FitBand::Low;
}

std::vector<SkillCoverage> teamCoverage(const TeamNeeds& team)
{
    std::vector<SkillCoverage> out;
    for (const auto& [rawSkill, minimum] : team.required) {
        SkillCoverage entry;
        entry.skill = safeNormalize(rawSkill);
        if (entry.skill.empty())
            continue;
        entry.required = minimum;
        for (const Student& member : team.members) {
            const int level = member.levelIn(entry.skill);
            entry.bestLevel = std::max(entry.bestLevel, level);
            if (level >= minimum)
                entry.coveredBy.push_back(member.name());
        }
        entry.state = !entry.coveredBy.empty() ? CoverageState::Covered
                    : entry.bestLevel > 0      ? CoverageState::Partial
                                               : CoverageState::Missing;
        out.push_back(std::move(entry));
    }
    return out;
}

CompatibilityReport evaluateCompatibility(const CandidateProfile& candidate, const TeamNeeds& team,
                                          const MatchWeights& weights)
{
    CompatibilityReport report;

    // The team's needs, normalised (bad entries are skipped rather than failing the report).
    SkillLevels required;
    for (const auto& [name, level] : team.required) {
        const std::string key = safeNormalize(name);
        if (!key.empty())
            required[key] = std::clamp(level, kMinSkillLevel, kMaxSkillLevel);
    }
    if (required.empty()) {
        report.explanation.push_back("The team has not listed the skills it needs yet, so there is nothing to "
                                     "compare with. The team leader can add them under Edit team.");
        return report;
    }

    // The person's skills: rated ones as given; unrated ones count at the team's minimum level
    // when the team needs them (level 1 otherwise), and are flagged.
    SkillLevels levels;
    std::set<std::string> unrated;
    for (const auto& [name, level] : candidate.levels) {
        const std::string key = safeNormalize(name);
        if (!key.empty())
            levels[key] = std::clamp(level, kMinSkillLevel, kMaxSkillLevel);
    }
    for (const std::string& name : candidate.unratedSkills) {
        const std::string key = safeNormalize(name);
        if (key.empty() || levels.count(key))
            continue;
        const auto need = required.find(key);
        levels[key] = need != required.end() ? need->second : kMinSkillLevel;
        unrated.insert(key);
    }
    report.enoughInformation = !levels.empty();
    if (!report.enoughInformation) {
        // Nothing to score (the engine needs at least one skill). Say so, without a number.
        for (const auto& [skill, minimum] : required)
            report.notShown.push_back(skill);
        report.suggestedRole = candidate.role.empty() ? std::string("Contributor") : candidate.role;
        report.roleReason = "Their profile does not list skills yet.";
        report.explanation.push_back("Their profile lists no skills yet, so no score is shown. That says nothing "
                                     "about what they can do: ask them, or look at their profile and links.");
        report.explanation.push_back("TeamForge does not verify skills. The team leader decides.");
        return report;
    }

    // Score with the project's matching engine against the team as it is now.
    const std::size_t size = std::max<std::size_t>({team.maxMembers, team.members.size() + 1, 1});
    const ProjectRequirement requirement("community-team", team.name.empty() ? std::string("Team") : team.name,
                                         required, 1, size);
    Team current("community-team", requirement);
    for (const Student& member : team.members) {
        if (member.id() == candidate.id || current.contains(member.id()) || current.isFull())
            continue;
        current.addMember(member);
    }
    // (The engine's Student needs a role; "no role stated" matches no member, like an unknown role.)
    const Student student(candidate.id.empty() ? std::string("candidate") : candidate.id,
                          candidate.name.empty() ? std::string("Candidate") : candidate.name,
                          candidate.role.empty() ? std::string("(no role stated)") : candidate.role, levels);
    WeightedMatchingStrategy strategy(weights);
    const OneCandidateEngagement engagement(candidate.expressedInterest);
    strategy.setEngagementSource(&engagement);
    const MatchResult result = strategy.evaluate(student, current);
    report.score = std::clamp(result.score, 0.0, 1.0);
    report.percent = static_cast<int>(std::lround(report.score * 100.0));
    report.factors = result.factors;
    report.band = report.enoughInformation ? bandFor(report.score) : FitBand::NotEnoughInformation;

    // Skill by skill.
    std::vector<std::string> gapSkills;
    for (const auto& [skill, minimum] : required) {
        const auto have = levels.find(skill);
        if (have == levels.end()) {
            report.notShown.push_back(skill);
            continue;
        }
        SkillFit fit;
        fit.skill = skill;
        fit.required = minimum;
        fit.level = have->second;
        fit.evidence = unrated.count(skill)               ? Evidence::LevelNotStated
                     : backedByEvidence(candidate, skill) ? Evidence::Projects
                                                          : Evidence::Declared;
        fit.fillsGap = !current.covers(skill);
        if (fit.level >= minimum) {
            report.matching.push_back(fit);
            if (fit.fillsGap)
                gapSkills.push_back(skill);
        } else {
            report.belowLevel.push_back(fit);
        }
    }

    // Strengths: what the person adds to this team.
    if (!gapSkills.empty())
        report.strengths.push_back("Covers " + joinNames(gapSkills) + ", which nobody in the team covers yet.");
    std::vector<std::string> depth, backed;
    for (const SkillFit& fit : report.matching) {
        if (!fit.fillsGap)
            depth.push_back(fit.skill);
        if (fit.evidence == Evidence::Projects)
            backed.push_back(fit.skill);
    }
    if (!depth.empty())
        report.strengths.push_back("Adds depth in " + joinNames(depth) + ".");
    if (!backed.empty())
        report.strengths.push_back(joinNames(backed) + (backed.size() == 1 ? " is" : " are")
                                   + " backed by their projects or resume.");
    if (candidate.details.totalYears > 0)
        report.strengths.push_back("About " + std::to_string(static_cast<int>(std::lround(candidate.details.totalYears)))
                                   + " years of experience (from their resume).");
    if (!candidate.details.projects.empty())
        report.strengths.push_back(std::to_string(candidate.details.projects.size())
                                   + (candidate.details.projects.size() == 1 ? " project" : " projects")
                                   + " in their profile.");
    if (candidate.expressedInterest)
        report.strengths.push_back("Has shown interest in this team.");

    // Suggested role: an open role that matches what they say they do, else the gaps they fill.
    const std::string role = lower(candidate.role);
    for (const std::string& open : team.openRoles) {
        const std::string want = lower(open);
        if (!role.empty() && !want.empty() && (role.find(want) != std::string::npos || want.find(role) != std::string::npos)) {
            report.suggestedRole = open;
            report.roleReason = "The team is looking for this role and it is their stated role.";
            break;
        }
    }
    if (report.suggestedRole.empty()) {
        if (!gapSkills.empty()) {
            report.suggestedRole = "Lead for " + joinNames(gapSkills, 2);
            report.roleReason = "Nobody in the team covers these yet, and they meet the level the team asks for.";
        } else if (!report.matching.empty()) {
            std::vector<std::string> names;
            for (const SkillFit& fit : report.matching)
                names.push_back(fit.skill);
            report.suggestedRole = "Support for " + joinNames(names, 2);
            report.roleReason = "The team already covers these; a second person shares the work.";
        } else if (!candidate.role.empty()) {
            report.suggestedRole = candidate.role;
            report.roleReason = "Their stated role; it does not overlap the team's listed skills.";
        } else {
            report.suggestedRole = "Contributor";
            report.roleReason = "Their profile does not show the team's skills yet.";
        }
    }

    // Improvement opportunities and open questions.
    for (const SkillFit& fit : report.belowLevel)
        report.improvements.push_back("Grow " + fit.skill + " from level " + std::to_string(fit.level) + " to "
                                      + std::to_string(fit.required) + ".");
    std::vector<std::string> uncovered;
    for (const std::string& skill : report.notShown)
        if (!current.covers(skill))
            uncovered.push_back(skill);
    if (!uncovered.empty())
        report.improvements.push_back(joinNames(uncovered) + (uncovered.size() == 1 ? " is" : " are")
                                      + " not shown in their profile and the team lacks "
                                      + (uncovered.size() == 1 ? "it" : "them")
                                      + ". Ask about it: it may simply be missing from the profile.");
    if (!unrated.empty()) {
        std::vector<std::string> names(unrated.begin(), unrated.end());
        report.improvements.push_back("No level given for " + joinNames(names) + "; ask how experienced they are.");
    }

    // How the score was made.
    const FactorScores& f = report.factors;
    report.explanation.push_back("Skill coverage " + percentText(f.coverage) + " (weight " + percentText(weights.coverage) + "): meets "
                                 + std::to_string(report.matching.size()) + " of " + std::to_string(required.size())
                                 + " required skills at the requested level.");
    report.explanation.push_back("Fills gaps " + percentText(f.complementarity)
                                 + " (weight " + percentText(weights.complementarity) + "): share of their matching skills the team does not cover yet.");
    report.explanation.push_back("Experience " + percentText(f.experience)
                                 + " (weight " + percentText(weights.experience) + "): their average level in the required skills they have.");
    report.explanation.push_back("Interest " + percentText(f.engagement) + " (weight " + percentText(weights.engagement) + "): "
                                 + (candidate.expressedInterest ? "they applied or opened your invitation link."
                                                                : "neutral until they apply."));
    report.explanation.push_back("Role variety " + percentText(f.diversity)
                                 + " (weight " + percentText(weights.diversity) + "): fewer members with the same main role scores higher.");
    report.explanation.push_back("Levels are self-assessed unless marked as backed by projects or a resume. "
                                 "TeamForge does not verify skills. The team leader decides.");
    return report;
}

} // namespace teamforge
