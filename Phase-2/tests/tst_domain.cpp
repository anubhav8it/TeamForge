#include "TestSupport.h"

#include "core/Errors.h"
#include "core/Interest.h"
#include "core/Skill.h"
#include "core/Team.h"

#include <sstream>

using namespace teamforge;
using namespace testsupport;

class TestDomain : public QObject
{
    Q_OBJECT

private slots:
    void interestStatusesRoundTrip();
    void interestTransitions();
    void interestBookIndexesRequests();
    void skillNamesAreNormalized();
    void studentValidation();
    void studentLookupAndEquality();
    void requirementValidation();
    void teamMembershipRules();
    void teamCoverageAndGaps();
    void teamFinalization();
    void exceptionHierarchy();
    void streamOperators();
};

void TestDomain::interestStatusesRoundTrip()
{
    for (InterestStatus status : {InterestStatus::Interested, InterestStatus::UnderReview, InterestStatus::Accepted,
                                  InterestStatus::Declined})
        QCOMPARE(interestStatusFrom(interestStatusName(status)), status);
    QCOMPARE(interestStatusName(InterestStatus::UnderReview), std::string("under_review"));
    QVERIFY_THROWS_EXCEPTION(ValidationError, interestStatusFrom("pending"));
    QVERIFY_THROWS_EXCEPTION(ValidationError, InterestRequest("", "a", "r"));
    QVERIFY_THROWS_EXCEPTION(ValidationError, InterestRequest("i", "a", " "));
}

void TestDomain::interestTransitions()
{
    using S = InterestStatus;
    QVERIFY(InterestRequest::canMove(S::Interested, S::UnderReview));
    QVERIFY(InterestRequest::canMove(S::Interested, S::Accepted));
    QVERIFY(InterestRequest::canMove(S::UnderReview, S::Declined));
    QVERIFY(InterestRequest::canMove(S::Accepted, S::Declined)); // the host changes their mind
    QVERIFY(InterestRequest::canMove(S::Declined, S::Accepted));
    QVERIFY(!InterestRequest::canMove(S::UnderReview, S::Interested));
    QVERIFY(!InterestRequest::canMove(S::Accepted, S::UnderReview));
    QVERIFY(!InterestRequest::canMove(S::Accepted, S::Accepted));

    InterestRequest request("i", "a", "r");
    request.moveTo(S::UnderReview);
    QCOMPARE(request.status(), S::UnderReview);
    QVERIFY_THROWS_EXCEPTION(ValidationError, request.moveTo(S::Interested));
}

void TestDomain::interestBookIndexesRequests()
{
    InterestBook book;
    const std::string first = book.express("a", "r1").id();
    QCOMPARE(first, std::string("int-0001"));
    book.express("a", "r2");
    book.express("b", "r1");
    QVERIFY_THROWS_EXCEPTION(DuplicateError, book.express("a", "r1")); // one request per project
    QCOMPARE(book.forRequirement("r1").size(), std::size_t{2});
    QCOMPARE(book.forStudent("a").size(), std::size_t{2});
    QCOMPARE(book.find("b", "r1")->status(), InterestStatus::Interested);
    QVERIFY(book.find("b", "r2") == nullptr);
    TF_COMPARE_NEAR(book.engagement("a", "r1"), kInterestedEngagement);
    TF_COMPARE_NEAR(book.engagement("b", "r2"), kNeutralEngagement);

    QCOMPARE(book.setStatus(first, InterestStatus::Accepted).status(), InterestStatus::Accepted);
    QVERIFY_THROWS_EXCEPTION(NotFoundError, book.setStatus("int-9999", InterestStatus::Accepted));

    book.removeStudent("a");
    QCOMPARE(book.size(), std::size_t{1});
    QVERIFY(book.find("a", "r1") == nullptr);
    book.express("a", "r1"); // allowed again after removal
    book.removeRequirement("r1");
    QCOMPARE(book.size(), std::size_t{0});
}
void TestDomain::skillNamesAreNormalized()
{
    QCOMPARE(normalizeSkill("  Data   Analysis "), std::string("data analysis"));
    QCOMPARE(normalizeSkill("C++"), std::string("c++"));
    QVERIFY_THROWS_EXCEPTION(ValidationError, normalizeSkill("   "));
}

void TestDomain::studentValidation()
{
    QVERIFY_THROWS_EXCEPTION(ValidationError, Student("", "Name", "Role", {{"c++", 3}}));
    QVERIFY_THROWS_EXCEPTION(ValidationError, Student("id", "  ", "Role", {{"c++", 3}}));
    QVERIFY_THROWS_EXCEPTION(ValidationError, Student("id", "Name", "", {{"c++", 3}}));
    QVERIFY_THROWS_EXCEPTION(ValidationError, Student("id", "Name", "Role", {}));
    QVERIFY_THROWS_EXCEPTION(ValidationError, Student("id", "Name", "Role", {{"c++", 0}}));
    QVERIFY_THROWS_EXCEPTION(ValidationError, Student("id", "Name", "Role", {{"c++", 6}}));
    // Two spellings of the same skill.
    QVERIFY_THROWS_EXCEPTION(ValidationError, Student("id", "Name", "Role", {{"C++", 3}, {"c++", 4}}));
}

void TestDomain::studentLookupAndEquality()
{
    const Student a = studentA();
    QCOMPARE(a.levelIn("C++"), 4);
    QCOMPARE(a.levelIn(" c++ "), 4);
    QCOMPARE(a.levelIn("qt"), 0);
    QVERIFY(a.offers("dsa"));
    QVERIFY(!a.offers("dsa", 3));

    const Student renamed("a", "Alpha Renamed", "Other", {{"java", 1}});
    QVERIFY(a == renamed); // identity is the id
    QVERIFY(a != studentB());
}

void TestDomain::requirementValidation()
{
    QVERIFY_THROWS_EXCEPTION(ValidationError, ProjectRequirement("r", "Name", {{"c++", 3}}, 0, 3));
    QVERIFY_THROWS_EXCEPTION(ValidationError, ProjectRequirement("r", "Name", {{"c++", 3}}, 3, 2));
    QVERIFY_THROWS_EXCEPTION(ValidationError, ProjectRequirement("r", "Name", {}, 1, 3));
    QVERIFY_THROWS_EXCEPTION(ValidationError, ProjectRequirement("r", "", {{"c++", 3}}, 1, 3));

    const ProjectRequirement r = scenarioRequirement();
    QCOMPARE(r.minLevelFor("C++"), 3);
    QCOMPARE(r.minLevelFor("java"), 0);
    QVERIFY(r.needsSkill("testing"));
    QCOMPARE(r.skillNames(), strings({"c++", "dsa", "qt", "testing"}));
}

void TestDomain::teamMembershipRules()
{
    Team team("t", scenarioRequirement()); // max 4
    team.addMember(studentA());
    team += studentB();
    QCOMPARE(team.size(), std::size_t{2});
    QVERIFY(team.contains("a"));

    QVERIFY_THROWS_EXCEPTION(DuplicateError, team.addMember(studentA()));

    team += studentC();
    team += studentD();
    QVERIFY(team.isFull());
    QVERIFY_THROWS_EXCEPTION(TeamConstraintError, team.addMember(studentE()));

    team.removeMember("b");
    QVERIFY(!team.contains("b"));
    QVERIFY_THROWS_EXCEPTION(NotFoundError, team.removeMember("b"));
}

void TestDomain::teamCoverageAndGaps()
{
    Team team("t", scenarioRequirement());
    team += studentA(); // c++ only; dsa 2 < 3
    QCOMPARE(team.coveredSkills(), strings({"c++"}));
    QCOMPARE(team.missingSkills(), strings({"dsa", "qt", "testing"}));

    team += studentC();
    QCOMPARE(team.missingSkills(), strings({"qt"}));
    TF_COMPARE_NEAR(team.coverageRatio(), 0.75);
    QVERIFY(!team.isComplete());

    team += studentB();
    QVERIFY(team.missingSkills().empty());
    QVERIFY(team.isComplete());
    QCOMPARE(team.countRole("C++/OOP"), std::size_t{2});
}

void TestDomain::teamFinalization()
{
    Team team("t", scenarioRequirement()); // min 2
    team += studentA();
    QVERIFY_THROWS_EXCEPTION(TeamConstraintError, team.validateForFinalization());
    team += studentE(); // size is enough even though skills are missing
    team.validateForFinalization();
    QVERIFY(!team.isComplete());
}

void TestDomain::exceptionHierarchy()
{
    try {
        Student("", "Name", "Role", {{"c++", 3}});
        QFAIL("expected ValidationError");
    } catch (const TeamForgeError& e) {
        QVERIFY(dynamic_cast<const ValidationError *>(&e) != nullptr);
        QVERIFY(std::string(e.what()).find("Student id") != std::string::npos);
    }
    QVERIFY((std::is_base_of_v<std::runtime_error, TeamForgeError>));
    QVERIFY((std::is_base_of_v<TeamForgeError, PersistenceError>));
}

void TestDomain::streamOperators()
{
    std::ostringstream out;
    out << studentA() << " | " << scenarioRequirement();
    QCOMPARE(out.str(), std::string("Alpha (a, C++/OOP) | Scenario (req-t, 4 skills, team 2-4)"));
}

QTEST_APPLESS_MAIN(TestDomain)
#include "tst_domain.moc"
