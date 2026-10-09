#include "TestSupport.h"

#include "core/CoverageOnlyStrategy.h"
#include "core/Errors.h"
#include "core/JsonStorage.h"
#include "core/MatchingEngine.h"
#include "core/WeightedMatchingStrategy.h"

#include <QDebug>
#include <QFile>
#include <QTemporaryDir>

#include <algorithm>
#include <sstream>

using namespace teamforge;
using namespace testsupport;

class TestPersistence : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void studentsRoundTrip();
    void requirementsRoundTrip();
    void requirementTypeIsOptional();
    void studentSummaryIsOptional();
    void oldAvailabilityFieldsAreDropped();
    void interestsRoundTrip();
    void teamsRoundTrip();
    void missingFile();
    void malformedJson();
    void wrongVersion();
    void invalidRecordNamesFileAndIndex();
    void duplicateIdsRejected();
    void teamWithUnknownMember();
    void sampleDataLoadsAndRanks();

private:
    std::string path(const char *name) const;
    void writeFile(const std::string& file, const char *contents) const;

    QTemporaryDir dir_;
};

void TestPersistence::init()
{
    QVERIFY(dir_.isValid());
}

std::string TestPersistence::path(const char *name) const
{
    return dir_.filePath(QString::fromLatin1(name)).toStdString();
}

void TestPersistence::writeFile(const std::string& file, const char *contents) const
{
    QFile out(QString::fromStdString(file));
    QVERIFY(out.open(QIODevice::WriteOnly));
    out.write(contents);
}

void TestPersistence::studentsRoundTrip()
{
    const Student original("s1", "Wanted Skills", "DSA", {{"C++", 4}, {"Data  Analysis", 2}},
                           {"Qt", "qml"});
    storage::saveStudents(path("students.json"), {original, studentB()});

    const std::vector<Student> loaded = storage::loadStudents(path("students.json"));
    QCOMPARE(loaded.size(), std::size_t{2});
    const Student& s = loaded[0];
    QCOMPARE(s.id(), original.id());
    QCOMPARE(s.name(), original.name());
    QCOMPARE(s.role(), original.role());
    QVERIFY(s.skillsOffered() == original.skillsOffered());
    QVERIFY(s.skillsWanted() == original.skillsWanted());
}

void TestPersistence::requirementsRoundTrip()
{
    storage::saveRequirements(path("requirements.json"), {scenarioRequirement()});
    const auto loaded = storage::loadRequirements(path("requirements.json"));
    QCOMPARE(loaded.size(), std::size_t{1});
    const ProjectRequirement& r = loaded[0];
    const ProjectRequirement expected = scenarioRequirement();
    QCOMPARE(r.id(), expected.id());
    QVERIFY(r.requiredSkills() == expected.requiredSkills());
    QCOMPARE(r.minTeamSize(), expected.minTeamSize());
    QCOMPARE(r.maxTeamSize(), expected.maxTeamSize());
}

void TestPersistence::requirementTypeIsOptional()
{
    const ProjectRequirement typed("r1", "Typed", {{"c++", 3}}, 1, 3, "  Hackathon ", " Ship it ");
    QCOMPARE(typed.type(), std::string("Hackathon"));
    storage::saveRequirements(path("typed.json"), {typed, scenarioRequirement()});
    const auto loaded = storage::loadRequirements(path("typed.json"));
    QCOMPARE(loaded[0].type(), std::string("Hackathon"));
    QCOMPARE(loaded[0].summary(), std::string("Ship it"));
    QVERIFY(loaded[1].type().empty());
    QVERIFY(loaded[1].summary().empty());

    // Files written before the field existed still load.
    writeFile(path("untyped.json"), R"({"version": 1, "requirements": [
        {"id": "r", "name": "R", "requiredSkills": {"c++": 2}, "minTeamSize": 1, "maxTeamSize": 2}
    ]})");
    QVERIFY(storage::loadRequirements(path("untyped.json"))[0].type().empty());
    writeFile(path("badtype.json"), R"({"version": 1, "requirements": [
        {"id": "r", "name": "R", "type": 7, "requiredSkills": {"c++": 2}, "minTeamSize": 1, "maxTeamSize": 2}
    ]})");
    QVERIFY_THROWS_EXCEPTION(PersistenceError, storage::loadRequirements(path("badtype.json")));
}

void TestPersistence::studentSummaryIsOptional()
{
    const Student described("s1", "Described", "Web", {{"react", 3}}, {}, " Builds web apps. ",
                            "B.Tech CSE · 2nd year");
    QCOMPARE(described.summary(), std::string("Builds web apps."));
    storage::saveStudents(path("summaries.json"), {described, studentA()});
    const auto loaded = storage::loadStudents(path("summaries.json"));
    QCOMPARE(loaded[0].summary(), std::string("Builds web apps."));
    QCOMPARE(loaded[0].program(), std::string("B.Tech CSE · 2nd year")); // UTF-8 round trip
    QVERIFY(loaded[1].summary().empty());
    QVERIFY(loaded[1].program().empty());

    writeFile(path("badsummary.json"), R"({"version": 1, "students": [
        {"id": "s", "name": "S", "role": "Web", "summary": 3, "skillsOffered": {"react": 2}}
    ]})");
    QVERIFY_THROWS_EXCEPTION(PersistenceError, storage::loadStudents(path("badsummary.json")));
}

void TestPersistence::oldAvailabilityFieldsAreDropped()
{
    // Files from before availability was removed still load; saving drops the old fields.
    writeFile(path("old-students.json"), R"({"version": 1, "students": [
        {"id": "s", "name": "S", "role": "Web", "skillsOffered": {"react": 2}, "availability": ["Mon:Evening"]}
    ]})");
    writeFile(path("old-requirements.json"), R"({"version": 1, "requirements": [
        {"id": "r", "name": "R", "requiredSkills": {"c++": 2}, "minTeamSize": 1, "maxTeamSize": 2,
         "timeSlots": ["Sat:Morning"]}
    ]})");
    const auto students = storage::loadStudents(path("old-students.json"));
    const auto requirements = storage::loadRequirements(path("old-requirements.json"));
    storage::saveStudents(path("old-students.json"), students);
    storage::saveRequirements(path("old-requirements.json"), requirements);
    for (const char *file : {"old-students.json", "old-requirements.json"}) {
        QFile saved(QString::fromStdString(path(file)));
        QVERIFY(saved.open(QIODevice::ReadOnly));
        const QByteArray bytes = saved.readAll();
        QVERIFY2(!bytes.contains("availability") && !bytes.contains("timeSlots"), file);
    }
}

void TestPersistence::interestsRoundTrip()
{
    InterestBook book;
    book.express("a", "req-t");
    const std::string second = book.express("c", "req-t").id();
    book.setStatus(second, InterestStatus::Accepted);
    storage::saveInterests(path("interest_requests.json"), book.values());

    InterestBook loaded;
    for (const InterestRequest& request : storage::loadInterests(path("interest_requests.json")))
        loaded.add(request);
    QCOMPARE(loaded.size(), std::size_t{2});
    QCOMPARE(loaded.get(second).status(), InterestStatus::Accepted);
    QVERIFY(loaded.find("a", "req-t") != nullptr);

    writeFile(path("bad-interest.json"), R"({"version": 1, "requests": [
        {"id": "int-0001", "studentId": "a", "requirementId": "r", "status": "maybe"}
    ]})");
    QVERIFY_THROWS_EXCEPTION(PersistenceError, storage::loadInterests(path("bad-interest.json")));
}

void TestPersistence::teamsRoundTrip()
{
    Repository<Student> students("Student");
    for (const Student& s : {studentA(), studentB(), studentC()})
        students.add(s);
    Repository<ProjectRequirement> requirements("Requirement");
    requirements.add(scenarioRequirement());

    Team team("team-1", scenarioRequirement());
    team += studentC();
    team += studentB();
    storage::saveTeams(path("teams.json"), {team});

    const auto loaded = storage::loadTeams(path("teams.json"), students, requirements);
    QCOMPARE(loaded.size(), std::size_t{1});
    QCOMPARE(loaded[0].id(), std::string("team-1"));
    QCOMPARE(loaded[0].requirement().id(), std::string("req-t"));
    QCOMPARE(loaded[0].size(), std::size_t{2});
    QVERIFY(loaded[0].contains("c") && loaded[0].contains("b"));
}

void TestPersistence::missingFile()
{
    QVERIFY_THROWS_EXCEPTION(PersistenceError, storage::loadStudents(path("does-not-exist.json")));
}

void TestPersistence::malformedJson()
{
    writeFile(path("bad.json"), "{ \"version\": 1, \"students\": [ ");
    QVERIFY_THROWS_EXCEPTION(PersistenceError, storage::loadStudents(path("bad.json")));
    writeFile(path("array.json"), "[]");
    QVERIFY_THROWS_EXCEPTION(PersistenceError, storage::loadStudents(path("array.json")));
}

void TestPersistence::wrongVersion()
{
    writeFile(path("v2.json"), R"({"version": 2, "students": []})");
    QVERIFY_THROWS_EXCEPTION(PersistenceError, storage::loadStudents(path("v2.json")));
}

void TestPersistence::invalidRecordNamesFileAndIndex()
{
    writeFile(path("level.json"), R"({"version": 1, "students": [
        {"id": "ok", "name": "Ok", "role": "R", "skillsOffered": {"c++": 3}},
        {"id": "x", "name": "X", "role": "R", "skillsOffered": {"c++": 7}}
    ]})");
    try {
        storage::loadStudents(path("level.json"));
        QFAIL("expected PersistenceError");
    } catch (const PersistenceError& e) {
        const std::string message = e.what();
        QVERIFY2(message.find("student #2") != std::string::npos, e.what());
        QVERIFY2(message.find("between 1 and 5") != std::string::npos, e.what());
    }

    writeFile(path("type.json"), R"({"version": 1, "students": [
        {"id": "x", "name": "X", "role": "R", "skillsOffered": {"c++": 3.5}}
    ]})");
    QVERIFY_THROWS_EXCEPTION(PersistenceError, storage::loadStudents(path("type.json")));

    writeFile(path("missing.json"), R"({"version": 1, "students": [{"id": "x", "role": "R"}]})");
    QVERIFY_THROWS_EXCEPTION(PersistenceError, storage::loadStudents(path("missing.json")));
}

void TestPersistence::duplicateIdsRejected()
{
    writeFile(path("dup.json"), R"({"version": 1, "students": [
        {"id": "x", "name": "X", "role": "R", "skillsOffered": {"c++": 3}},
        {"id": "x", "name": "Y", "role": "R", "skillsOffered": {"qt": 3}}
    ]})");
    QVERIFY_THROWS_EXCEPTION(PersistenceError, storage::loadStudents(path("dup.json")));
}

void TestPersistence::teamWithUnknownMember()
{
    Repository<Student> students("Student");
    students.add(studentA());
    Repository<ProjectRequirement> requirements("Requirement");
    requirements.add(scenarioRequirement());
    writeFile(path("teams.json"),
              R"({"version": 1, "teams": [{"id": "t", "requirementId": "req-t", "memberIds": ["a", "ghost"]}]})");
    QVERIFY_THROWS_EXCEPTION(PersistenceError, storage::loadTeams(path("teams.json"), students, requirements));
}

void TestPersistence::sampleDataLoadsAndRanks()
{
    const std::string dataDir = std::string(TEAMFORGE_SOURCE_DIR) + "/data/";
    const auto students = storage::loadStudents(dataDir + "students.json");
    const auto requirements = storage::loadRequirements(dataDir + "requirements.json");
    QVERIFY(!requirements.empty());

    MatchingEngine engine;
    for (const Student& s : students)
        engine.addStudent(s);
    for (const char *member : {"Mukul Fartiyal", "Anubhav Bisht", "Siddhant Singh", "Mehul"}) {
        const auto found = engine.students().filter([&](const Student& s) { return s.name() == member; });
        QVERIFY2(found.size() == 1, member);
    }

    const WeightedMatchingStrategy weighted;
    const CoverageOnlyStrategy baseline;
    for (const ProjectRequirement& requirement : requirements) {
        const Team empty("probe", requirement);
        const auto ranked = engine.rank(empty, weighted, 5);
        QVERIFY(!ranked.empty());
        QVERIFY(std::is_sorted(ranked.begin(), ranked.end(), std::greater<MatchResult>()));

        std::ostringstream report;
        report << requirement << "\n  Top candidates (" << weighted.name() << "):";
        for (const MatchResult& r : ranked) {
            QVERIFY(r.score >= 0.0 && r.score <= 1.0);
            report << "\n    " << r;
        }

        for (const MatchingStrategy *strategy : {static_cast<const MatchingStrategy *>(&weighted),
                                                 static_cast<const MatchingStrategy *>(&baseline)}) {
            const Team team = engine.suggestTeam("suggested", requirement, *strategy);
            QVERIFY(team.size() >= 1 && team.size() <= requirement.maxTeamSize());
            report << "\n  Suggested team (" << strategy->name() << "):";
            for (const Student& m : team.members())
                report << " " << m.name() << ";";
            report << " coverage " << team.coverageRatio() << ", missing:";
            for (const std::string& skill : team.missingSkills())
                report << " " << skill;
        }
        qInfo().noquote() << QString::fromStdString(report.str());
    }

    // Saving what was loaded and reloading it is lossless.
    storage::saveStudents(path("resaved.json"), students);
    QCOMPARE(storage::loadStudents(path("resaved.json")).size(), students.size());
}

QTEST_APPLESS_MAIN(TestPersistence)
#include "tst_persistence.moc"
