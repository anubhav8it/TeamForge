// Participant workspace and directory search. Part of TeamForgeController.
#include "TeamForgeController.h"

#include "BridgeSupport.h"

#include "core/Errors.h"
#include "core/Skill.h"

#include <QMetaType>
#include <QRegularExpression>

#include <algorithm>
#include <iterator>

using namespace teamforge;
using namespace bridge;

namespace {

// Skill names in a sentence: acronyms in capitals (SQL, NLP, ROS2, UI Design), other words in
// title case ("Machine Learning"), small joining words lower case.
QString proseSkill(const std::string& skill)
{
    static const QStringList joiners{QStringLiteral("and"), QStringLiteral("of"), QStringLiteral("for"),
                                     QStringLiteral("the"), QStringLiteral("in"), QStringLiteral("to"),
                                     QStringLiteral("with"), QStringLiteral("on")};
    QStringList words = qs(skill).split(QLatin1Char(' '));
    for (QString& word : words) {
        if (word.isEmpty() || joiners.contains(word))
            continue;
        const bool hasDigit = std::any_of(word.begin(), word.end(), [](QChar c) { return c.isDigit(); });
        if ((word.size() <= 3 && !word.contains(QLatin1Char('.'))) || (hasDigit && word.size() <= 6))
            word = word.toUpper();
        else
            word[0] = word[0].toUpper();
    }
    return words.join(QLatin1Char(' '));
}

QString proseList(const QStringList& names)
{
    if (names.size() <= 1)
        return names.value(0);
    return QStringList(names.mid(0, names.size() - 1)).join(QStringLiteral(", ")) + QStringLiteral(" and ") + names.last();
}

} // namespace

// --- my profile ------------------------------------------------------------------------------

QVariantMap TeamForgeController::myProfile() const
{
    const Student *me = engine_.students().find(ownProfileId());
    if (me == nullptr)
        return {};
    QVariantMap map = studentToMap(*me);
    const QVariantMap completion = profileCompletion(map);
    map.insert(QStringLiteral("completion"), completion.value(QStringLiteral("completion")));
    map.insert(QStringLiteral("checklist"), completion.value(QStringLiteral("checklist")));

    // Strongest skills first.
    const auto& skills = me->skillsOffered();
    std::vector<std::pair<std::string, int>> ranked(skills.begin(), skills.end());
    std::stable_sort(ranked.begin(), ranked.end(), [](const auto& a, const auto& b) { return a.second > b.second; });
    QVariantList top;
    for (const auto& [skill, level] : ranked)
        top.append(QVariantMap{{QStringLiteral("skill"), qs(skill)}, {QStringLiteral("level"), level}});
    map.insert(QStringLiteral("rankedSkills"), top);

    return map;
}

QVariantMap TeamForgeController::profileCompletion(const QVariantMap& profile) const
{
    // Five plain checks, shown as a checklist and a ratio. Works on a saved profile and on an
    // onboarding draft alike (same keys as `students` entries).
    const auto nonEmpty = [&](const char *key) {
        return !profile.value(QString::fromLatin1(key)).toString().trimmed().isEmpty();
    };
    const auto countOf = [&](const char *key) {
        const QVariant value = profile.value(QString::fromLatin1(key));
        return value.typeId() == QMetaType::QVariantMap ? value.toMap().size() : value.toList().size();
    };
    const struct { const char *key; const char *label; bool done; } items[] = {
        {"basics", "Name and academic details", nonEmpty("name") && nonEmpty("program")},
        {"skills", "At least three skills", countOf("skillsOffered") >= 3},
        {"experience", "One-line experience summary", nonEmpty("summary")},
        {"interests", "Learning interests", countOf("skillsWanted") > 0},
    };
    QVariantList checklist;
    int done = 0;
    for (const auto& item : items) {
        done += item.done ? 1 : 0;
        checklist.append(QVariantMap{{QStringLiteral("key"), QString::fromLatin1(item.key)},
                                     {QStringLiteral("label"), QString::fromLatin1(item.label)},
                                     {QStringLiteral("done"), item.done}});
    }
    return {{QStringLiteral("completion"), static_cast<double>(done) / static_cast<double>(std::size(items))},
            {QStringLiteral("checklist"), checklist}};
}

QVariantMap TeamForgeController::opportunityFor(const Student& student, const ProjectRequirement& requirement) const
{
    // Same engine, same strategy: the participant scored as the first member of a new team.
    const Team empty("probe", requirement);
    const MatchResult result = weighted_.evaluate(student, empty);
    QVariantMap map = cachedRequirement(requirement.id());

    // Key skills: the ones the participant brings first, then the highest minimums.
    struct Key { std::string skill; int minLevel; int level; };
    std::vector<Key> keys;
    int covered = 0;
    for (const auto& [skill, minLevel] : requirement.requiredSkills()) {
        const int level = student.levelIn(skill);
        covered += level >= minLevel ? 1 : 0;
        keys.push_back({skill, minLevel, level});
    }
    std::stable_sort(keys.begin(), keys.end(), [](const Key& a, const Key& b) {
        const bool aMet = a.level >= a.minLevel;
        const bool bMet = b.level >= b.minLevel;
        return aMet != bMet ? aMet : a.minLevel > b.minLevel;
    });
    QVariantList keySkills;
    for (const Key& key : keys)
        keySkills.append(QVariantMap{{QStringLiteral("skill"), qs(key.skill)},
                                     {QStringLiteral("minLevel"), key.minLevel},
                                     {QStringLiteral("level"), key.level},
                                     {QStringLiteral("covered"), key.level >= key.minLevel}});

    // Where this participant's interest in the project stands, if they expressed it.
    const InterestRequest *interest = interests_.find(student.id(), requirement.id());

    map.insert(QStringLiteral("score"), result.score);
    map.insert(QStringLiteral("coveredCount"), covered);
    map.insert(QStringLiteral("requiredCount"), static_cast<int>(requirement.requiredSkills().size()));
    map.insert(QStringLiteral("interestStatus"), interest != nullptr ? qs(interestStatusName(interest->status())) : QString());
    map.insert(QStringLiteral("interestId"), interest != nullptr ? qs(interest->id()) : QString());
    map.insert(QStringLiteral("keySkills"), keySkills);
    map.insert(QStringLiteral("match"), matchResultToMap(result, empty, true));
    return map;
}

QVariantList TeamForgeController::opportunities() const
{
    if (opportunitiesCache_)
        return *opportunitiesCache_;
    QVariantList list;
    if (const Student *me = engine_.students().find(ownProfileId())) {
        std::vector<QVariantMap> maps;
        for (const ProjectRequirement& requirement : requirements_.values())
            maps.push_back(opportunityFor(*me, requirement));
        std::stable_sort(maps.begin(), maps.end(), [](const QVariantMap& a, const QVariantMap& b) {
            return a.value(QStringLiteral("score")).toDouble() > b.value(QStringLiteral("score")).toDouble();
        });
        for (const QVariantMap& map : maps)
            list.append(map);
    }
    opportunitiesCache_ = list;
    return list;
}

QVariantMap TeamForgeController::evaluateFit(const QString& studentId, const QString& requirementId)
{
    QVariantMap map;
    run("evaluateFit", studentId, [&] {
        require(Permission::Evaluate);
        // A participant may only look at their own fit.
        if (workspace_ == QLatin1String("participant") && ss(studentId) != ownProfileId())
            throw PermissionError("Participants can only view their own project fit");
        const Student& student = engine_.students().get(ss(studentId));
        const ProjectRequirement& requirement = requirements_.get(ss(requirementId));
        const Team empty("probe", requirement);
        map = matchResultToMap(weighted_.evaluate(student, empty), empty, true);
        map.insert(QStringLiteral("isMember"), false);
        map.insert(QStringLiteral("requirementName"), qs(requirement.name()));
    }, false);
    return map;
}

bool TeamForgeController::saveMyProfile(const QVariantMap& profile)
{
    QString id = qs(ownProfileId());
    const bool ok = run("saveMyProfile", id, [&] {
        require(Permission::EditOwnProfile);
        QVariantMap map = profile;
        const Student *existing = engine_.students().find(ownProfileId());
        if (existing == nullptr) {
            for (int n = static_cast<int>(engine_.students().size()) + 1;; ++n) {
                id = QStringLiteral("tf-p%1").arg(n, 4, 10, QLatin1Char('0'));
                if (!engine_.students().contains(ss(id)))
                    break;
            }
        }
        map.insert(QStringLiteral("id"), id);
        // Diversity "role": kept for an existing profile; a new one specialises in its strongest
        // skill. Never shown to users.
        QString role = existing != nullptr ? qs(existing->role()) : QString();
        if (role.isEmpty()) {
            const SkillLevels skills = skillLevelsFrom(map.value(QStringLiteral("skillsOffered")), "level");
            const auto strongest = std::max_element(skills.begin(), skills.end(),
                                                    [](const auto& a, const auto& b) { return a.second < b.second; });
            role = strongest == skills.end() ? QStringLiteral("Generalist") : qs(normalizeSkill(strongest->first));
        }
        map.insert(QStringLiteral("role"), role);
        const Student parsed = studentFrom(map); // core validation
        if (existing != nullptr)
            engine_.updateStudent(parsed);
        else
            engine_.addStudent(parsed);
        ownProfileSlot() = parsed.id();
        setDirty(true);
        logEvent(AppLog::Level::Info, QStringLiteral("profile"),
                 QStringLiteral("%1 profile %2 (%3 skills)")
                     .arg(existing != nullptr ? QStringLiteral("Updated") : QStringLiteral("Created"), id)
                     .arg(parsed.skillsOffered().size()));
    });
    if (ok) {
        notifyDataChanged();
        refresh();
        saveSession();
        return save(); // a participant's edits are written immediately
    }
    return false;
}

bool TeamForgeController::claimProfile(const QString& studentId)
{
    const bool ok = run("claimProfile", studentId, [&] {
        require(Permission::EditOwnProfile);
        engine_.students().get(ss(studentId)); // NotFoundError
        ownProfileSlot() = ss(studentId);
        logEvent(AppLog::Level::Info, QStringLiteral("profile"), QStringLiteral("Using existing profile %1").arg(studentId));
    });
    if (ok) {
        opportunitiesCache_.reset();
        saveSession();
        emit profileChanged();
    }
    return ok;
}

void TeamForgeController::forgetMyProfile()
{
    if (ownProfileId().empty())
        return;
    ownProfileSlot().clear();
    opportunitiesCache_.reset();
    saveSession();
    emit profileChanged();
}

QString TeamForgeController::composeSummary(const QVariantList& skills) const
{
    std::vector<std::pair<std::string, int>> ranked;
    double total = 0.0;
    for (const QVariant& entry : skills) {
        const QVariantMap map = entry.toMap();
        const QString name = map.value(QStringLiteral("skill")).toString().trimmed();
        if (name.isEmpty())
            continue;
        const int level = map.value(QStringLiteral("level")).toInt();
        ranked.emplace_back(normalizeSkill(ss(name)), level);
        total += level;
    }
    if (ranked.empty())
        return {};
    std::stable_sort(ranked.begin(), ranked.end(), [](const auto& a, const auto& b) { return a.second > b.second; });
    QStringList names;
    for (std::size_t i = 0; i < ranked.size() && i < 3; ++i)
        names.append(proseSkill(ranked[i].first));
    const double average = total / static_cast<double>(ranked.size());
    if (ranked.front().second >= 4)
        return QStringLiteral("Experienced with %1.").arg(proseList(names));
    if (average < 2.5)
        return QStringLiteral("Learning %1 through hands-on projects.").arg(proseList(names));
    return QStringLiteral("Builds projects with %1.").arg(proseList(names));
}

bool TeamForgeController::useDemoParticipant()
{
    return claimProfile(qs(kDemoParticipant));
}

// --- interest workflow -----------------------------------------------------------------------

QVariantList TeamForgeController::myInterests() const
{
    QVariantList list;
    const Student *me = engine_.students().find(ownProfileId());
    if (me == nullptr)
        return list;
    std::vector<InterestRequest> mine = interests_.forStudent(me->id());
    std::sort(mine.begin(), mine.end(), [](const InterestRequest& a, const InterestRequest& b) { return a.id() > b.id(); });
    for (const InterestRequest& request : mine) {
        const ProjectRequirement *requirement = requirements_.find(request.requirementId());
        if (requirement == nullptr)
            continue;
        QVariantMap entry = opportunityFor(*me, *requirement);
        entry.insert(QStringLiteral("requestId"), qs(request.id()));
        list.append(entry);
    }
    return list;
}

bool TeamForgeController::expressInterest(const QString& requirementId)
{
    const bool ok = run("expressInterest", requirementId, [&] {
        require(Permission::EditOwnProfile);
        const Student *me = engine_.students().find(ownProfileId());
        if (me == nullptr)
            throw ValidationError("Create or choose your profile before expressing interest");
        const ProjectRequirement& requirement = requirements_.get(ss(requirementId)); // NotFoundError
        interests_.express(me->id(), requirement.id()); // DuplicateError if already expressed
        setDirty(true);
        logEvent(AppLog::Level::Info, QStringLiteral("interest"),
                 QStringLiteral("%1 expressed interest in %2").arg(qs(me->name()), qs(requirement.name())));
    });
    if (!ok)
        return false;
    notifyDataChanged();
    refresh(); // the engagement factor changed
    emit interestsChanged();
    return save(); // like a saved team, written immediately
}

// --- directory search ------------------------------------------------------------------------

QVariantList TeamForgeController::searchStudents(const QString& query, const QString& skill, const QString& sort) const
{
    const QVariantList all = students(); // also builds searchCache_
    const QString q = query.trimmed().toLower().replace(QRegularExpression(QStringLiteral("\\s+")), QStringLiteral(" "));
    const std::string skillQuery = ss(q);
    const std::string skillFilter = skill.trimmed().isEmpty() ? std::string() : normalizeSkill(ss(skill));

    std::vector<int> hits;
    hits.reserve(searchCache_.size());
    for (std::size_t i = 0; i < searchCache_.size(); ++i) {
        const SearchEntry& entry = searchCache_[i];
        if (!skillFilter.empty()
            && std::find(entry.skills.begin(), entry.skills.end(), skillFilter) == entry.skills.end())
            continue;
        bool match = q.isEmpty() || entry.name.contains(q);
        for (std::size_t s = 0; !match && s < entry.skills.size(); ++s)
            match = skillMatchesQuery(entry.skills[s], skillQuery);
        if (match)
            hits.push_back(static_cast<int>(i));
    }
    // The cache is already in name order; "level" re-sorts by average level, stable.
    if (sort == QLatin1String("level")) {
        std::stable_sort(hits.begin(), hits.end(), [this](int a, int b) {
            return searchCache_[static_cast<std::size_t>(a)].averageLevel
                   > searchCache_[static_cast<std::size_t>(b)].averageLevel;
        });
    }
    QVariantList result;
    result.reserve(static_cast<qsizetype>(hits.size()));
    for (int i : hits)
        result.append(all.at(i));
    return result;
}
