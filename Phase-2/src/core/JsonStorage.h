#pragma once

#include "core/Interest.h"
#include "core/ProjectRequirement.h"
#include "core/Repository.h"
#include "core/Student.h"
#include "core/Team.h"

#include <map>
#include <string>
#include <vector>

// Local JSON persistence. Each file is {"version": 1, "<collection>": [...]}; unknown fields are
// ignored, so data files may carry notes. Saves go through QSaveFile (write to a temp file, then
// rename), so a crash mid-save never leaves a half-written file. Every failure, including an
// invalid record, is reported as PersistenceError naming the file and record number.
namespace teamforge::storage {

constexpr int kFormatVersion = 1;

std::vector<Student> loadStudents(const std::string& path);
void saveStudents(const std::string& path, const std::vector<Student>& students);

std::vector<ProjectRequirement> loadRequirements(const std::string& path);
void saveRequirements(const std::string& path, const std::vector<ProjectRequirement>& requirements);

// Teams are stored as a requirement id plus member ids and resolved against the repositories,
// so a team always reflects the current profiles.
std::vector<Team> loadTeams(const std::string& path, const Repository<Student>& students,
                            const Repository<ProjectRequirement>& requirements);
void saveTeams(const std::string& path, const std::vector<Team>& teams);

// Interest requests (interest_requests.json): who expressed interest in which project and
// where the host's review stands. References are checked by the caller.
std::vector<InterestRequest> loadInterests(const std::string& path);
void saveInterests(const std::string& path, const std::vector<InterestRequest>& requests);

// The skill catalogue (skills.json): normalised skill name -> category. Display metadata only;
// skills stay open-ended, so a skill without an entry is just uncategorised.
using SkillCatalog = std::map<std::string, std::string>;
SkillCatalog loadSkillCatalog(const std::string& path);
void saveSkillCatalog(const std::string& path, const SkillCatalog& catalog);

// Single records in the stored format (pretty-printed JSON), for inspecting and editing one
// record. The parsers apply the same field rules as the loaders and throw ValidationError.
std::string toJson(const Student& student);
std::string toJson(const ProjectRequirement& requirement);
std::string toJson(const Team& team);
Student studentFromJson(const std::string& json);
ProjectRequirement requirementFromJson(const std::string& json);
// The record whose `idKey` field is `id` in a collection file ("students", "requirements",
// "teams", "skills" with idKey "name"), exactly as stored; empty if there is none.
// PersistenceError if the file cannot be read.
std::string storedRecord(const std::string& path, const char *collection, const std::string& id,
                         const char *idKey = "id");

} // namespace teamforge::storage
