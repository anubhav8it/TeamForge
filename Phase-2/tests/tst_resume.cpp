#include "core/DocumentText.h"
#include "core/Errors.h"
#include "core/Inflate.h"
#include "core/JsonStorage.h"
#include "core/ResumeAnalyzer.h"
#include "core/Student.h"

#include <QFile>
#include <QtTest>

#include <algorithm>
#include <string>
#include <vector>

using namespace teamforge;

// Resume import in the core: DEFLATE, DOCX and PDF text, and the rule-based analysis.
class TestResume : public QObject
{
    Q_OBJECT

private slots:
    void inflateRoundTrips();
    void inflateRejectsDamageAndBombs();
    void detectsDocumentTypes();
    void docxFixtureText();
    void pdfFixtureText();
    void syntheticPdfLinesAndKerning();
    void encryptedAndImageOnlyPdfsAreReported();

    void levelBoundaries();
    void trieFindsLongestPhrase();
    void analysesPdfFixture();
    void analysesDocxFixture();
    void neverCopiesTotalExperienceOntoSkills();
    void listItemsKeepTheirOwnYears();
    void overlappingRolesCountOnce();
    void everydayWordsOnlyUnderSkillsHeadings();
    void aliasesNeedAKnownTarget();
    void ageOnlyWhenWritten();

    void studentDetailsAreValidatedAndStored();

private:
    static std::string fixture(const char *name);
    static std::vector<std::string> catalogue();
    static std::string pdf(const std::string& content, bool compress, const std::string& trailerExtra = {});
    static const ResumeSkill *find(const ResumeAnalysis& a, const std::string& skill);
};

std::string TestResume::fixture(const char *name)
{
    QFile file(QStringLiteral(TEAMFORGE_SOURCE_DIR "/tests/data/resumes/") + QString::fromLatin1(name));
    if (!file.open(QIODevice::ReadOnly))
        qFatal("missing fixture %s", name);
    return file.readAll().toStdString();
}

std::vector<std::string> TestResume::catalogue()
{
    std::vector<std::string> skills;
    for (const auto& [name, category] : storage::loadSkillCatalog(TEAMFORGE_SOURCE_DIR "/data/skills.json")) {
        (void)category;
        skills.push_back(name);
    }
    return skills;
}

// A one-page PDF with a Helvetica text stream; no xref table, which readers must tolerate.
std::string TestResume::pdf(const std::string& content, bool compress, const std::string& trailerExtra)
{
    std::string stream = content;
    std::string filter;
    if (compress) {
        const QByteArray z = qCompress(QByteArray::fromStdString(content), 9);
        stream = z.mid(4).toStdString(); // qCompress = 4-byte length + zlib stream
        filter = " /Filter /FlateDecode";
    }
    return "%PDF-1.4\n"
           "1 0 obj << /Type /Catalog /Pages 2 0 R >> endobj\n"
           "2 0 obj << /Type /Pages /Kids [3 0 R] /Count 1 >> endobj\n"
           "3 0 obj << /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] /Resources << /Font << /F1 4 0 R >> >> /Contents 5 0 R >> endobj\n"
           "4 0 obj << /Type /Font /Subtype /Type1 /BaseFont /Helvetica >> endobj\n"
           "5 0 obj << /Length " + std::to_string(stream.size()) + filter + " >> stream\n" + stream + "\nendstream endobj\n"
           "trailer << /Root 1 0 R" + trailerExtra + " >>\n%%EOF\n";
}

const ResumeSkill *TestResume::find(const ResumeAnalysis& a, const std::string& skill)
{
    const auto it = std::find_if(a.skills.begin(), a.skills.end(), [&](const ResumeSkill& s) { return s.skill == skill; });
    return it == a.skills.end() ? nullptr : &*it;
}

// --- documents --------------------------------------------------------------------------------

void TestResume::inflateRoundTrips()
{
    QByteArray original;
    for (int i = 0; i < 4000; ++i)
        original += QByteArray::number(i * 7919 % 1000) + " skills, teams and projects; ";
    for (int level : {0, 1, 6, 9}) { // 0 = stored blocks, others fixed and dynamic Huffman codes
        const QByteArray z = qCompress(original, level).mid(4);
        QCOMPARE(QByteArray::fromStdString(inflateZlib(z.toStdString(), 1u << 24)), original);
        // Raw DEFLATE (as in ZIP files): without the 2-byte header and 4-byte checksum.
        const std::string raw = z.mid(2, z.size() - 6).toStdString();
        QCOMPARE(QByteArray::fromStdString(inflateRaw(raw, 1u << 24)), original);
    }
}

void TestResume::inflateRejectsDamageAndBombs()
{
    QVERIFY_THROWS_EXCEPTION(DocumentError, inflateZlib("not compressed at all", 1024));
    QByteArray z = qCompress(QByteArray(1000, 'a'), 9).mid(4);
    z[5] = static_cast<char>(z[5] ^ 0x5A);
    try {
        inflateZlib(z.toStdString(), 1u << 20); // damaged data: an error or a shorter result, never a crash
    } catch (const DocumentError&) {
    }
    const QByteArray bomb = qCompress(QByteArray(1 << 20, 'x'), 9).mid(4);
    QVERIFY_THROWS_EXCEPTION(DocumentError, inflateZlib(bomb.toStdString(), 1000));
}

void TestResume::detectsDocumentTypes()
{
    QVERIFY(detectDocumentType(fixture("priya-nair.pdf"), "cv.pdf") == DocumentType::Pdf);
    QVERIFY(detectDocumentType(fixture("rahul-verma.docx"), "anything") == DocumentType::Docx);
    QVERIFY_THROWS_EXCEPTION(DocumentError, detectDocumentType("", "cv.pdf"));
    QVERIFY_THROWS_EXCEPTION(DocumentError, detectDocumentType("plain text pretending", "cv.pdf"));
    try {
        detectDocumentType(std::string("\xD0\xCF\x11\xE0\xA1\xB1\x1A\xE1", 8), "old.doc");
        QFAIL("an old .doc must be refused");
    } catch (const DocumentError& error) {
        QVERIFY(std::string(error.what()).find("DOCX") != std::string::npos);
    }
}

void TestResume::docxFixtureText()
{
    const std::string text = docxText(fixture("rahul-verma.docx"));
    QVERIFY(text.find("Curriculum Vitae") != std::string::npos);   // header part
    QVERIFY(text.find("Data Analyst rahul@example.com") != std::string::npos); // tab -> space
    QVERIFY(text.find("Tech Fest 2021 & more") != std::string::npos);          // &amp; decoded
    // The text box appears once, although Word stores it twice.
    const std::size_t first = text.find("Rahul Verma");
    QVERIFY(first != std::string::npos);
    QCOMPARE(text.find("Rahul Verma", first + 1), std::string::npos);
}

void TestResume::pdfFixtureText()
{
    const std::string text = pdfText(fixture("priya-nair.pdf"));
    QVERIFY(text.find("PRIYA NAIR\n") == 0);
    QVERIFY(text.find("Languages: Python (4 years), JavaScript, TypeScript, C++, SQL") != std::string::npos);
    QVERIFY(text.find("Jan 2024 – Present") != std::string::npos);
    QVERIFY(text.find("spam classifier") != std::string::npos); // the "fi" ligature is plain letters
    QVERIFY(text.find('\x00') == std::string::npos);
}

void TestResume::syntheticPdfLinesAndKerning()
{
    const std::string content =
        "BT /F1 12 Tf 72 720 Td (Asha Rao) Tj 0 -16 Td (Skills: Python, SQL) Tj "
        "0 -16 Td [(Mach) -20 (ine) -400 (Learning)] TJ T* (ignored leading) Tj ET";
    for (bool compress : {false, true}) {
        const std::string text = pdfText(pdf(content, compress));
        QVERIFY2(text.find("Asha Rao\nSkills: Python, SQL\nMachine Learning") == 0, text.c_str());
    }
}

void TestResume::encryptedAndImageOnlyPdfsAreReported()
{
    try {
        pdfText(pdf("BT /F1 12 Tf 72 720 Td (Secret resume text here) Tj ET", false, " /Encrypt 9 0 R"));
        QFAIL("an encrypted PDF must be refused");
    } catch (const DocumentError& error) {
        QVERIFY(std::string(error.what()).find("protected") != std::string::npos);
    }
    try {
        pdfText(pdf("0 0 1 rg 72 72 200 200 re f", true)); // a drawing, no text: like a scan
        QFAIL("a PDF without text must be reported");
    } catch (const DocumentError& error) {
        QVERIFY(std::string(error.what()).find("No text") != std::string::npos);
    }
}

// --- analysis ---------------------------------------------------------------------------------

void TestResume::levelBoundaries()
{
    // The specified bands, with each boundary on the lower side.
    QCOMPARE(levelForYears(-1), 0);
    QCOMPARE(levelForYears(0), 1);
    QCOMPARE(levelForYears(0.5), 1);
    QCOMPARE(levelForYears(1), 1);   // exactly 1 year: Beginner
    QCOMPARE(levelForYears(1.1), 2);
    QCOMPARE(levelForYears(2), 2);   // exactly 2: Basic
    QCOMPARE(levelForYears(2.5), 3);
    QCOMPARE(levelForYears(4), 3);   // exactly 4: Intermediate
    QCOMPARE(levelForYears(4.2), 4);
    QCOMPARE(levelForYears(5), 4);   // exactly 5: Advanced
    QCOMPARE(levelForYears(5.1), 5);
    QCOMPARE(levelForYears(20), 5);
}

void TestResume::trieFindsLongestPhrase()
{
    PhraseTrie trie;
    trie.insert({"react"}, 1);
    trie.insert({"react", "native"}, 2);
    trie.insert({"machine", "learning"}, 3);
    QCOMPARE(trie.longestMatch({"react", "native", "apps"}, 0), std::make_pair(2, std::size_t{2}));
    QCOMPARE(trie.longestMatch({"react", "hooks"}, 0), std::make_pair(1, std::size_t{1}));
    QCOMPARE(trie.longestMatch({"machine", "shop"}, 0), std::make_pair(-1, std::size_t{0}));
    QCOMPARE(trie.longestMatch({"x", "machine", "learning"}, 1), std::make_pair(3, std::size_t{2}));
}

void TestResume::analysesPdfFixture()
{
    const RuleBasedResumeExtractor extractor(catalogue());
    const ResumeAnalysis a = extractor.analyse(pdfText(fixture("priya-nair.pdf")), {2026, 10});
    QCOMPARE(a.name, std::string("Priya Nair"));
    QCOMPARE(a.age, -1);

    const ResumeSkill *python = find(a, "python");
    QVERIFY(python);
    QCOMPARE(python->years, 4.0);           // "Python (4 years)"
    QCOMPARE(python->suggestedLevel, 3);    // exactly 4 years: Intermediate
    const ResumeSkill *javascript = find(a, "javascript");
    QVERIFY(javascript);
    QCOMPARE(javascript->years, -1.0);      // listed, no experience stated: nothing invented
    QCOMPARE(javascript->suggestedLevel, 0);
    const ResumeSkill *typescript = find(a, "typescript");
    QVERIFY(typescript);
    QCOMPARE(typescript->years, 1.0);       // the Jan-Dec 2023 role mentions it
    QCOMPARE(typescript->suggestedLevel, 1);
    QVERIFY(typescript->yearsEvidence.find("Jan 2023") != std::string::npos);
    const ResumeSkill *docker = find(a, "docker");
    QVERIFY(docker);
    QCOMPARE(docker->years, 2.8);           // Jan 2024 - Present (Oct 2026): 34 months
    QVERIFY(find(a, "react native"));       // longest match, not "react" twice
    QVERIFY(find(a, "machine learning"));
    QVERIFY(find(a, "scikit-learn"));

    QVERIFY(std::find(a.unmatchedSkills.begin(), a.unmatchedSkills.end(), "REST APIs") != a.unmatchedSkills.end());
    QVERIFY(std::find(a.unmatchedSkills.begin(), a.unmatchedSkills.end(), "Blockchainify") != a.unmatchedSkills.end());
    QCOMPARE(a.totalYears, 3.8);            // the two dated roles, merged
    QCOMPARE(a.projects.size(), std::size_t{2});
    QCOMPARE(a.education.size(), std::size_t{1});
    QCOMPARE(a.certifications.size(), std::size_t{2});
    QCOMPARE(a.achievements.size(), std::size_t{2});
    QVERIFY(a.achievements.front().find("Smart India Hackathon") != std::string::npos);
    for (const auto *list : {&a.projects, &a.education, &a.certifications, &a.achievements})
        for (const std::string& item : *list)
            QVERIFY2(item.find("Chess") == std::string::npos, "hobbies are not achievements");
}

void TestResume::analysesDocxFixture()
{
    const RuleBasedResumeExtractor extractor(catalogue());
    const ResumeAnalysis a = extractor.analyse(docxText(fixture("rahul-verma.docx")), {2026, 10});
    QCOMPARE(a.name, std::string("Rahul Verma"));
    QCOMPARE(a.age, 22);
    QCOMPARE(find(a, "sql")->years, 2.0);
    QCOMPARE(find(a, "sql")->suggestedLevel, 2);   // exactly 2 years: Basic
    QCOMPARE(find(a, "excel")->years, 2.0);        // explicit statement beats the roles
    QCOMPARE(find(a, "tableau")->years, 1.0);      // June 2022 - May 2023 only
    QCOMPARE(find(a, "python")->suggestedLevel, 4); // both roles: 4.4 years
    QVERIFY(find(a, "statistics"));                // an everyday word, but under a skills heading
    QVERIFY(find(a, "power bi"));
    QCOMPARE(a.certifications.size(), std::size_t{1});
    QCOMPARE(a.achievements.size(), std::size_t{1});
}

void TestResume::neverCopiesTotalExperienceOntoSkills()
{
    const RuleBasedResumeExtractor extractor({"python", "java"});
    const ResumeAnalysis a = extractor.analyse("Meera Iyer\nSummary\nSoftware engineer with 6 years of experience.\n"
                                               "Skills\nPython, Java\n", {2026, 10});
    QCOMPARE(a.totalYears, 6.0);
    QCOMPARE(find(a, "python")->years, -1.0);
    QCOMPARE(find(a, "java")->suggestedLevel, 0);
}

void TestResume::listItemsKeepTheirOwnYears()
{
    const RuleBasedResumeExtractor extractor({"python", "java", "sql", "excel"});
    const ResumeAnalysis a = extractor.analyse("Skills: Python (3 years), Java, SQL - 18 months\n"
                                               "2 years of experience with Excel\n", {2026, 10});
    QCOMPARE(find(a, "python")->years, 3.0);
    QCOMPARE(find(a, "java")->years, -1.0);
    QCOMPARE(find(a, "sql")->years, 1.5);
    QCOMPARE(find(a, "sql")->suggestedLevel, 2);
    QCOMPARE(find(a, "excel")->years, 2.0);
    QCOMPARE(a.totalYears, -1.0); // that "2 years of experience" belongs to Excel
}

void TestResume::overlappingRolesCountOnce()
{
    const RuleBasedResumeExtractor extractor({"python", "django"});
    const ResumeAnalysis a = extractor.analyse("Experience\n"
                                               "Developer, Alpha Ltd, Jan 2020 - Dec 2021\nBuilt Python services.\n"
                                               "Freelance, Beta, Jan 2021 - Dec 2022\nPython and Django sites.\n",
                                               {2026, 10});
    QCOMPARE(find(a, "python")->years, 3.0); // 2020-2022 once, not 2 + 2 years
    QCOMPARE(find(a, "django")->years, 2.0);
    QCOMPARE(a.totalYears, 3.0);
}

void TestResume::everydayWordsOnlyUnderSkillsHeadings()
{
    const RuleBasedResumeExtractor extractor({"go", "r", "testing", "python"});
    const ResumeAnalysis prose = extractor.analyse("Projects\nA Python tool for testing ideas we go back to.\n", {2026, 10});
    QVERIFY(find(prose, "python"));
    QVERIFY(!find(prose, "go"));
    QVERIFY(!find(prose, "testing"));
    const ResumeAnalysis listed = extractor.analyse("Technical Skills\nGo, R, Testing\n", {2026, 10});
    QVERIFY(find(listed, "go") && find(listed, "r") && find(listed, "testing"));
}

void TestResume::aliasesNeedAKnownTarget()
{
    const RuleBasedResumeExtractor withTargets({"javascript", "kubernetes", "node.js"});
    const ResumeAnalysis a = withTargets.analyse("Skills\nJS, k8s, NodeJS\n", {2026, 10});
    QVERIFY(find(a, "javascript") && find(a, "kubernetes") && find(a, "node.js"));
    // Without "kubernetes" in TeamForge, "k8s" is not turned into it: it is offered for review.
    const RuleBasedResumeExtractor without({"javascript"});
    const ResumeAnalysis b = without.analyse("Skills\nJS, k8s\n", {2026, 10});
    QVERIFY(!find(b, "kubernetes"));
    QCOMPARE(b.unmatchedSkills, std::vector<std::string>{"k8s"});
}

void TestResume::ageOnlyWhenWritten()
{
    const RuleBasedResumeExtractor extractor({"python"});
    QCOMPARE(extractor.analyse("Page 22 of the average report\nSkills: Python\n", {2026, 10}).age, -1);
    QCOMPARE(extractor.analyse("Personal details\nAge: 19\n", {2026, 10}).age, 19);
}

// --- profile details --------------------------------------------------------------------------

void TestResume::studentDetailsAreValidatedAndStored()
{
    Student s("x1", "Asha Rao", "python", {{"python", 3}, {"sql", 2}});
    QVERIFY(s.details().empty());
    ProfileDetails d;
    d.projects = {"  CampusRide app  ", "", "SpamShield"};
    d.certifications = {"AWS Cloud Practitioner"};
    d.skillYears = {{"Python", 4.0}, {"rust", 2.0}}; // rust is not offered
    d.totalYears = 3.8;
    s.setDetails(d);
    QCOMPARE(s.details().projects, (std::vector<std::string>{"CampusRide app", "SpamShield"}));
    QCOMPARE(s.details().skillYears.size(), std::size_t{1});
    QCOMPARE(s.details().skillYears.at("python"), 4.0);

    ProfileDetails bad;
    bad.totalYears = 70;
    QVERIFY_THROWS_EXCEPTION(ValidationError, s.setDetails(bad));

    // Stored with the profile and read back unchanged; a profile without details has no new keys.
    const Student back = storage::studentFromJson(storage::toJson(s));
    QCOMPARE(back.details().projects, s.details().projects);
    QCOMPARE(back.details().certifications, s.details().certifications);
    QCOMPARE(back.details().skillYears, s.details().skillYears);
    QCOMPARE(back.details().totalYears, 3.8);
    const std::string plain = storage::toJson(Student("x2", "Ben", "sql", {{"sql", 2}}));
    QVERIFY(plain.find("projects") == std::string::npos && plain.find("skillYears") == std::string::npos);
}

QTEST_APPLESS_MAIN(TestResume)
#include "tst_resume.moc"
