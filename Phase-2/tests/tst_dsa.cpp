#include "TestSupport.h"

#include "core/Errors.h"
#include "core/Ranker.h"
#include "core/Repository.h"
#include "core/SkillBST.h"
#include "core/SkillGraph.h"
#include "core/SkillIndex.h"
#include "core/Team.h"
#include "core/WeightedMatchingStrategy.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <random>
#include <set>

using namespace teamforge;
using namespace testsupport;

class TestDsa : public QObject
{
    Q_OBJECT

private slots:
    void repositoryCrud();
    void repositoryFilterAndOrder();

    void skillIndexLookup();
    void skillIndexRemove();

    void bstCountsAndOrder();
    void bstRemove();
    void bstStaysBalancedOnSortedInput();
    void bstPrefixSearch();

    void graphBuildsCoOccurrenceEdges();
    void graphBfsRelatedSkills();
    void graphShortestPath();
    void graphRemoveStudent();

    void topKOrdersAndExcludesMembers();
    void topKMatchesFullSort();
    void topKEdgeCases();
};

// --- Repository<T> -------------------------------------------------------------------------

void TestDsa::repositoryCrud()
{
    Repository<Student> repo("Student");
    repo.add(studentA());
    repo.add(studentB());
    QCOMPARE(repo.size(), std::size_t{2});
    QCOMPARE(repo.get("a").name(), std::string("Alpha"));
    QVERIFY(repo.find("zzz") == nullptr);

    QVERIFY_THROWS_EXCEPTION(DuplicateError, repo.add(studentA()));
    QVERIFY_THROWS_EXCEPTION(NotFoundError, repo.get("zzz"));

    repo.update(Student("a", "Alpha Updated", "C++/OOP", {{"c++", 5}}));
    QCOMPARE(repo.get("a").name(), std::string("Alpha Updated"));
    QVERIFY_THROWS_EXCEPTION(NotFoundError, repo.update(studentC()));

    repo.remove("a");
    QVERIFY(!repo.contains("a"));
    QVERIFY_THROWS_EXCEPTION(NotFoundError, repo.remove("a"));
}

void TestDsa::repositoryFilterAndOrder()
{
    Repository<Student> repo("Student");
    repo.add(studentC());
    repo.add(studentA());
    repo.add(studentB());

    std::vector<std::string> ids;
    for (const Student& s : repo.values())
        ids.push_back(s.id());
    QCOMPARE(ids, strings({"a", "b", "c"})); // id order, not insertion order

    const auto cpp = repo.filter([](const Student& s) { return s.offers("c++", 4); });
    QCOMPARE(cpp.size(), std::size_t{2});
}

// --- SkillIndex (unordered_map) ---------------------------------------------------------------

void TestDsa::skillIndexLookup()
{
    SkillIndex index;
    index.add(studentA());
    index.add(studentB());
    index.add(studentC());

    QCOMPARE(index.studentsWith("C++"), (std::set<std::string>{"a", "b"}));
    QCOMPARE(index.studentsWith("dsa"), (std::set<std::string>{"a", "c"})); // any level counts
    QVERIFY(index.studentsWith("rust").empty());
    QCOMPARE(index.studentsWithAny({"qt", "Testing"}), (std::set<std::string>{"b", "c"}));
    QCOMPARE(index.skillCount(), std::size_t{6}); // c++ dsa git qt testing python
}

void TestDsa::skillIndexRemove()
{
    SkillIndex index;
    index.add(studentA());
    index.add(studentB());
    index.remove(studentB());

    QCOMPARE(index.studentsWith("c++"), (std::set<std::string>{"a"}));
    QVERIFY(!index.hasSkill("qt")); // last holder removed, so the key is gone
}

// --- SkillBST (AVL) ----------------------------------------------------------------------------

void TestDsa::bstCountsAndOrder()
{
    SkillBST tree;
    for (const char *skill : {"qt", "c++", "dsa", "qt", "testing", "c++", "qt"})
        tree.insert(skill);

    QCOMPARE(tree.size(), std::size_t{4});
    QCOMPARE(tree.count("qt"), 3);
    QCOMPARE(tree.count("rust"), 0);
    const std::vector<std::pair<std::string, int>> expected{
        {"c++", 2}, {"dsa", 1}, {"qt", 3}, {"testing", 1}};
    QCOMPARE(tree.inOrder(), expected);
    QVERIFY(tree.isValidAvl());
}

void TestDsa::bstRemove()
{
    SkillBST tree;
    for (const char *skill : {"m", "d", "t", "b", "f", "p", "w", "f"})
        tree.insert(skill);

    QVERIFY(tree.remove("f")); // count 2 -> 1, node stays
    QVERIFY(tree.contains("f"));
    QVERIFY(tree.remove("f")); // count 1 -> node deleted
    QVERIFY(!tree.contains("f"));
    QVERIFY(tree.remove("m")); // root with two children
    QVERIFY(!tree.contains("m"));
    QVERIFY(!tree.remove("zzz"));
    QCOMPARE(tree.size(), std::size_t{5});
    QVERIFY(tree.isValidAvl());

    std::vector<std::string> keys;
    for (const auto& entry : tree.inOrder())
        keys.push_back(entry.first);
    QCOMPARE(keys, strings({"b", "d", "p", "t", "w"}));
}

void TestDsa::bstStaysBalancedOnSortedInput()
{
    // Sorted inserts are the worst case for a plain BST (height n). AVL height is bounded by
    // about 1.44 * log2(n + 2).
    SkillBST tree;
    const int n = 1000;
    char name[16];
    for (int i = 0; i < n; ++i) {
        std::snprintf(name, sizeof name, "skill%04d", i);
        tree.insert(name);
    }
    QCOMPARE(tree.size(), std::size_t{n});
    QVERIFY(tree.isValidAvl());
    const double bound = 1.4405 * std::log2(n + 2.0);
    QVERIFY2(tree.height() <= bound, qPrintable(QString("height %1").arg(tree.height())));

    for (int i = 0; i < n; i += 2) {
        std::snprintf(name, sizeof name, "skill%04d", i);
        QVERIFY(tree.remove(name));
    }
    QCOMPARE(tree.size(), std::size_t{n / 2});
    QVERIFY(tree.isValidAvl());
}

void TestDsa::bstPrefixSearch()
{
    SkillBST tree;
    for (const char *skill : {"data analysis", "databases", "dsa", "design patterns", "c++", "data"})
        tree.insert(skill);

    QCOMPARE(tree.withPrefix("da"), strings({"data", "data analysis", "databases"}));
    QCOMPARE(tree.withPrefix("d").size(), std::size_t{5});
    QCOMPARE(tree.withPrefix("data "), strings({"data analysis"}));
    QVERIFY(tree.withPrefix("x").empty());
    QCOMPARE(tree.withPrefix("").size(), std::size_t{6});
}

// --- SkillGraph (BFS) --------------------------------------------------------------------------

namespace {

// Edges: c++-qt, qt-qml, c++-dsa, python-ml. Two components.
SkillGraph sampleGraph()
{
    SkillGraph graph;
    graph.addStudent(Student("s1", "S1", "R", {{"c++", 3}, {"qt", 3}}));
    graph.addStudent(Student("s2", "S2", "R", {{"qt", 3}, {"qml", 3}}));
    graph.addStudent(Student("s3", "S3", "R", {{"python", 3}, {"ml", 3}}));
    graph.addStudent(Student("s4", "S4", "R", {{"c++", 3}, {"dsa", 3}}));
    return graph;
}

} // namespace

void TestDsa::graphBuildsCoOccurrenceEdges()
{
    SkillGraph graph = sampleGraph();
    QCOMPARE(graph.skillCount(), std::size_t{6});
    QCOMPARE(graph.edgeCount(), std::size_t{4});
    QCOMPARE(graph.edgeWeight("C++", "qt"), 1);
    QCOMPARE(graph.edgeWeight("qt", "c++"), 1); // undirected
    QCOMPARE(graph.edgeWeight("c++", "qml"), 0);

    graph.addStudent(Student("s5", "S5", "R", {{"c++", 2}, {"qt", 2}}));
    QCOMPARE(graph.edgeWeight("c++", "qt"), 2);
}

void TestDsa::graphBfsRelatedSkills()
{
    const SkillGraph graph = sampleGraph();
    const std::vector<SkillDistance> all{{"qt", 1}, {"c++", 2}, {"dsa", 3}};
    QCOMPARE(graph.relatedSkills("qml", 5), all);
    QCOMPARE(graph.relatedSkills("qml", 1), (std::vector<SkillDistance>{{"qt", 1}}));
    // Same distance: alphabetical.
    QCOMPARE(graph.relatedSkills("c++", 1), (std::vector<SkillDistance>{{"dsa", 1}, {"qt", 1}}));
    QVERIFY(graph.relatedSkills("qml", 0).empty());
    QVERIFY(graph.relatedSkills("rust", 3).empty());
}

void TestDsa::graphShortestPath()
{
    const SkillGraph graph = sampleGraph();
    QCOMPARE(graph.shortestPath("qml", "dsa"), strings({"qml", "qt", "c++", "dsa"}));
    QCOMPARE(graph.shortestPath("qt", "qt"), strings({"qt"}));
    QVERIFY(graph.shortestPath("qml", "python").empty()); // different component
    QVERIFY(graph.shortestPath("qml", "rust").empty());
}

void TestDsa::graphRemoveStudent()
{
    SkillGraph graph = sampleGraph();
    graph.removeStudent(Student("s1", "S1", "R", {{"c++", 3}, {"qt", 3}}));
    QCOMPARE(graph.edgeWeight("c++", "qt"), 0);
    QVERIFY(graph.shortestPath("qml", "dsa").empty()); // the bridge is gone
    QVERIFY(graph.hasSkill("c++"));                     // s4 still offers it

    graph.removeStudent(Student("s3", "S3", "R", {{"python", 3}, {"ml", 3}}));
    QVERIFY(!graph.hasSkill("python"));
    QCOMPARE(graph.edgeCount(), std::size_t{2});
}

// --- priority_queue ranking --------------------------------------------------------------------

void TestDsa::topKOrdersAndExcludesMembers()
{
    const Student a = studentA();
    const Student b = studentB();
    const Student c = studentC();
    Team team("t", scenarioRequirement());
    team += b;

    // Scores with Beta in the team: Gamma 0.74, Alpha 0.29 (see tst_scoring).
    const auto ranked = rankTopK({&a, &b, &c, nullptr}, team, scenarioStrategy(), 10);
    QCOMPARE(ranked.size(), std::size_t{2});
    QCOMPARE(ranked[0].studentId, std::string("c"));
    QCOMPARE(ranked[1].studentId, std::string("a"));
    TF_COMPARE_NEAR(ranked[0].score, 0.74);
    TF_COMPARE_NEAR(ranked[1].score, 0.29);
}

void TestDsa::topKMatchesFullSort()
{
    // Random profiles with a fixed seed: the heap result must equal the first k of a full sort.
    std::mt19937 rng(20260831);
    const std::vector<std::string> pool{"c++", "qt", "dsa", "testing", "python", "java", "git", "ui design"};
    const std::vector<std::string> roles{"C++/OOP", "DSA", "UI/Testing", "Backend"};
    std::uniform_int_distribution<int> level(1, 5);
    std::uniform_int_distribution<int> coin(0, 1);

    std::vector<Student> students;
    for (int i = 0; i < 300; ++i) {
        SkillLevels skills;
        for (const std::string& skill : pool) {
            if (coin(rng) == 1)
                skills[skill] = level(rng);
        }
        if (skills.empty())
            skills["git"] = 1;
        char id[8];
        std::snprintf(id, sizeof id, "r%03d", i);
        students.emplace_back(id, id, roles[static_cast<std::size_t>(i) % roles.size()], skills);
    }
    std::vector<const Student *> candidates;
    for (const Student& s : students)
        candidates.push_back(&s);

    Team team("t", scenarioRequirement());
    team += studentB();
    const WeightedMatchingStrategy strategy;

    std::vector<MatchResult> sorted;
    for (const Student *s : candidates)
        sorted.push_back(strategy.evaluate(*s, team));
    std::sort(sorted.begin(), sorted.end(), std::greater<MatchResult>());

    for (std::size_t k : {std::size_t{1}, std::size_t{5}, std::size_t{25}, std::size_t{300}}) {
        const auto top = rankTopK(candidates, team, strategy, k);
        QCOMPARE(top.size(), k);
        for (std::size_t i = 0; i < k; ++i)
            QCOMPARE(top[i].studentId, sorted[i].studentId);
    }
}

void TestDsa::topKEdgeCases()
{
    const Student a = studentA();
    const Team team("t", scenarioRequirement());
    const WeightedMatchingStrategy strategy;
    QVERIFY(rankTopK({&a}, team, strategy, 0).empty());
    QVERIFY(rankTopK({}, team, strategy, 5).empty());
    QCOMPARE(rankTopK({&a}, team, strategy, 5).size(), std::size_t{1}); // k larger than n
}

QTEST_APPLESS_MAIN(TestDsa)
#include "tst_dsa.moc"
