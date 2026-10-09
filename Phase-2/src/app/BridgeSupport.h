#pragma once

#include "core/Errors.h"
#include "core/ProjectRequirement.h"
#include "core/Student.h"

#include <QString>
#include <QStringList>
#include <QVariant>
#include <QVariantList>
#include <QVariantMap>

#include <string>
#include <vector>

// Conversions between core objects and the QVariant maps QML sees. Keys match the JSON files.
// Shared by the TeamForgeController translation units; no rules live here.
namespace bridge {

// The active workspace does not allow the action. An app-level rule, so it lives here rather
// than in the core, but it joins the core hierarchy so run() handles it like any other error.
class PermissionError : public teamforge::TeamForgeError
{
public:
    using TeamForgeError::TeamForgeError;
};

// Local demonstration identities (no accounts or passwords): the demo participant is a real
// record in the sample data; the demo host is the one local host.
inline const std::string kDemoParticipant = "TF-P001";
inline const char *const kDemoHostId = "TF-H001";
inline const char *const kDemoHostName = "TeamForge Demo Host";

inline QString qs(const std::string& text)
{
    return QString::fromStdString(text);
}

inline std::string ss(const QString& text)
{
    return text.toStdString();
}

template <typename Container>
QStringList toQStringList(const Container& items)
{
    QStringList list;
    for (const std::string& item : items)
        list.append(qs(item));
    return list;
}

QVariantList skillLevelList(const teamforge::SkillLevels& levels, const char *levelKey);
QVariantMap studentToMap(const teamforge::Student& student);

// Plain-language band for an average skill level (1-5): Beginner .. Advanced.
QString experienceBand(double averageLevel);
// True when `query` (normalised) starts the skill, starts one of its words, or equals its
// initials. So "ml" finds "machine learning" and "ml ops" but not "html/css".
bool skillMatchesQuery(const std::string& skill, const std::string& query);

// QML numbers arrive as doubles; levels and team sizes must be whole numbers.
int wholeNumber(const QVariant& value, const std::string& what);
// Accepts {"skill": level} or [{"skill": ..., <levelKey>: ...}].
teamforge::SkillLevels skillLevelsFrom(const QVariant& value, const char *levelKey);
std::vector<std::string> stringsFrom(const QVariant& value);

// Core constructors validate; these only unpack the maps.
teamforge::Student studentFrom(const QVariantMap& map);
teamforge::ProjectRequirement requirementFrom(const QVariantMap& map);

} // namespace bridge
