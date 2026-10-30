#include "core/DocumentText.h"
#include "core/Errors.h"
#include "core/Inflate.h"
#include "core/JsonStorage.h"
#include "core/ResumeAnalyzer.h"
#include "core/ScanQuality.h"
#include "core/Student.h"

#include <QFile>
#include <QtTest>

#include <algorithm>
#include <cstdint>
#include <random>
#include <string>
#include <utility>
#include <vector>

using namespace teamforge;

// Resume import in the core: DEFLATE; DOCX, ODT, PDF, RTF, text and HTML text; checks on photos
// and on OCR text; and the rule-based analysis.
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
    void odtDocuments();
    void rtfDocuments();
    void htmlAndPlainText();
    void picturesNeedReading();
    void scanQualityChecks();
    void readTextIsCheckedForBlur();

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
    static std::string storedZip(const std::vector<std::pair<std::string, std::string>>& files);
    static GreyImage printedPage(int width, int height);
    static GreyImage blurred(const GreyImage& image, int passes);
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

// A ZIP with uncompressed (stored) entries, as an ODT test file.
std::string TestResume::storedZip(const std::vector<std::pair<std::string, std::string>>& files)
{
    const auto crc32 = [](const std::string& data) {
        std::uint32_t crc = 0xFFFFFFFFu;
        for (unsigned char c : data) {
            crc ^= c;
            for (int k = 0; k < 8; ++k)
                crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
        }
        return ~crc;
    };
    const auto le = [](std::string& out, std::uint32_t value, int bytes) {
        for (int i = 0; i < bytes; ++i)
            out += static_cast<char>((value >> (8 * i)) & 0xFF);
    };
    std::string zip;
    std::string directory;
    for (const auto& [name, data] : files) {
        const auto offset = static_cast<std::uint32_t>(zip.size());
        const std::uint32_t crc = crc32(data);
        const auto size = static_cast<std::uint32_t>(data.size());
        const auto nameLength = static_cast<std::uint32_t>(name.size());
        le(zip, 0x04034b50u, 4); le(zip, 20, 2); le(zip, 0, 2); le(zip, 0, 2); le(zip, 0, 4);
        le(zip, crc, 4); le(zip, size, 4); le(zip, size, 4); le(zip, nameLength, 2); le(zip, 0, 2);
        zip += name + data;
        le(directory, 0x02014b50u, 4); le(directory, 20, 2); le(directory, 20, 2); le(directory, 0, 2); le(directory, 0, 2);
        le(directory, 0, 4); le(directory, crc, 4); le(directory, size, 4); le(directory, size, 4);
        le(directory, nameLength, 2); le(directory, 0, 2); le(directory, 0, 2); le(directory, 0, 2); le(directory, 0, 2);
        le(directory, 0, 4); le(directory, offset, 4);
        directory += name;
    }
    const auto start = static_cast<std::uint32_t>(zip.size());
    zip += directory;
    le(zip, 0x06054b50u, 4); le(zip, 0, 2); le(zip, 0, 2);
    le(zip, static_cast<std::uint32_t>(files.size()), 2); le(zip, static_cast<std::uint32_t>(files.size()), 2);
    le(zip, static_cast<std::uint32_t>(directory.size()), 4); le(zip, start, 4); le(zip, 0, 2);
    return zip;
}

// Lines of dark "letters" (random 1-2 px strokes in 7 x 10 cells) on light paper, like printed
// text at the working scale. No fonts: the core tests stay Qt-free.
GreyImage TestResume::printedPage(int width, int height)
{
    GreyImage page{width, height, std::vector<std::uint8_t>(static_cast<std::size_t>(width) * static_cast<std::size_t>(height), 232)};
    std::mt19937 random(7);
    const auto ink = [&](int x, int y) { page.pixels[static_cast<std::size_t>(y) * static_cast<std::size_t>(width) + static_cast<std::size_t>(x)] = 25; };
    for (int line = height / 12; line + 12 < height * 11 / 12; line += 18)
        for (int cell = width / 12; cell + 8 < width * 11 / 12; cell += 9) {
            if (random() % 7 == 0)
                continue; // a space between words
            for (int stroke = 0; stroke < 3; ++stroke) {
                const bool vertical = random() % 2 == 0;
                const int at = static_cast<int>(random() % (vertical ? 6 : 9));
                for (int t = 0; t < (vertical ? 10 : 6); ++t)
                    for (int w = 0; w < 2; ++w)
                        vertical ? ink(cell + at + w, line + t) : ink(cell + t, line + at + w);
            }
        }
    return page;
}

// A 3 x 3 box blur repeated `passes` times (close to a Gaussian of sigma 0.8 x sqrt(passes)).
GreyImage TestResume::blurred(const GreyImage& image, int passes)
{
    GreyImage out = image;
    for (int pass = 0; pass < passes; ++pass) {
        const GreyImage in = out;
        for (int y = 1; y + 1 < in.height; ++y)
            for (int x = 1; x + 1 < in.width; ++x) {
                int sum = 0;
                for (int dy = -1; dy <= 1; ++dy)
                    for (int dx = -1; dx <= 1; ++dx)
                        sum += in.pixels[static_cast<std::size_t>(y + dy) * static_cast<std::size_t>(in.width) + static_cast<std::size_t>(x + dx)];
                out.pixels[static_cast<std::size_t>(y) * static_cast<std::size_t>(in.width) + static_cast<std::size_t>(x)] = static_cast<std::uint8_t>(sum / 9);
            }
    }
    return out;
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
    QVERIFY(detectDocumentType(fixture("photo-sharp.jpg"), "IMG_2041.JPG") == DocumentType::Image);
    QVERIFY(detectDocumentType(std::string("\x89PNG\r\n\x1A\n rest", 13), "") == DocumentType::Image);
    QVERIFY(detectDocumentType(std::string("\0\0\0\x18" "ftypheic", 12), "") == DocumentType::Image);
    QVERIFY(detectDocumentType("{\\rtf1\\ansi Hello}", "") == DocumentType::Rtf);
    QVERIFY(detectDocumentType(fixture("priya-nair.html"), "cv.htm") == DocumentType::Html);
    QVERIFY(detectDocumentType("  <!DOCTYPE html><p>x</p>", "cv") == DocumentType::Html);
    QVERIFY(detectDocumentType("# Asha Rao\n\nSkills: Python", "cv.md") == DocumentType::Text);
    QVERIFY(detectDocumentType("\xFF\xFEh\0i\0", "cv.txt") == DocumentType::Text); // UTF-16, NULs and all
    QVERIFY(detectDocumentType(storedZip({{"mimetype", "application/vnd.oasis.opendocument.text"}, {"content.xml", "<x/>"}}), "cv")
            == DocumentType::Odt);
    QVERIFY_THROWS_EXCEPTION(DocumentError, detectDocumentType("", "cv.pdf"));
    QVERIFY_THROWS_EXCEPTION(DocumentError, detectDocumentType("plain text pretending", "cv.pdf"));
    QVERIFY_THROWS_EXCEPTION(DocumentError, detectDocumentType("plain text pretending", "photo.jpg"));
    QVERIFY_THROWS_EXCEPTION(DocumentError, detectDocumentType(std::string("\x7F" "ELF\x02\x01\0\0\0\0", 10), "cv"));
    try {
        detectDocumentType(storedZip({{"ppt/presentation.xml", "<p/>"}}), "cv.pptx");
        QFAIL("slides must be refused");
    } catch (const DocumentError& error) {
        QVERIFY(std::string(error.what()).find("PowerPoint") != std::string::npos);
    }
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
    } catch (const NeedsReadingError& error) { // its text has to be read from the picture
        QVERIFY(std::string(error.what()).find("scan") != std::string::npos);
    }
}

void TestResume::odtDocuments()
{
    const std::string content =
        "<?xml version=\"1.0\"?><office:document-content><office:meta><dc:title>not this</dc:title></office:meta>"
        "<office:body><office:text><text:h text:outline-level=\"1\">Asha Rao</text:h>"
        "<text:p>Skills:<text:s/>Python,<text:s text:c=\"3\"/>SQL<text:tab/>R&amp;D</text:p>"
        "<text:tracked-changes><text:changed-region><text:p>Deleted words</text:p></text:changed-region></text:tracked-changes>"
        "<text:p>Line one<text:line-break/>Line two</text:p>"
        "<office:annotation><text:p>A reviewer's comment</text:p></office:annotation>"
        "<text:p>Caf&#233; volunteer, 2023</text:p></office:text></office:body></office:document-content>";
    const std::string odt = storedZip({{"mimetype", "application/vnd.oasis.opendocument.text"}, {"content.xml", content}});
    const std::string text = documentText(odt, "cv.odt");
    QCOMPARE(text, std::string("Asha Rao\nSkills: Python, SQL R&D\nLine one\nLine two\nCaf\xC3\xA9 volunteer, 2023"));
    QVERIFY_THROWS_EXCEPTION(DocumentError, odtText(storedZip({{"mimetype", "application/vnd.oasis.opendocument.text"}})));
}

void TestResume::rtfDocuments()
{
    const std::string rtf =
        "{\\rtf1\\ansi\\ansicpg1252\\deff0{\\fonttbl{\\f0\\fswiss Arial;}}{\\colortbl;\\red0\\green0\\blue0;}"
        "{\\*\\generator Riched20 10.0;}{\\info{\\author Someone Else}}\\viewkind4\\uc1\\pard\\f0\\fs22 "
        "{\\b Asha Rao}\\par\r\nSkills:\\tab Python, SQL\\par "
        "Caf\\'e9 \\u8212? Ren\\u233\\'e9e \\{braces\\}\\line "
        "{\\field{\\*\\fldinst HYPERLINK \"https://example.com\"}{\\fldrslt asha.dev}}"
        "{\\pict\\pngblip 89504e470d0a}\\par}";
    const std::string text = documentText(rtf, "cv.rtf");
    QCOMPARE(text, std::string("Asha Rao\nSkills: Python, SQL\nCaf\xC3\xA9 \xE2\x80\x94 Ren\xC3\xA9" "e {braces}\nasha.dev"));
}

void TestResume::htmlAndPlainText()
{
    const std::string html = documentText(fixture("priya-nair.html"), "priya.html");
    QVERIFY(html.find("PRIYA NAIR\npriya.nair@example.com \xC2\xB7 +91") == 0); // &middot; style text, head/style dropped
    QVERIFY(html.find("Languages: Python (4 years), JavaScript, TypeScript, C++, SQL\nFrameworks:") != std::string::npos);
    QVERIFY(html.find("font-family") == std::string::npos);
    const std::string scripted = htmlText("<html><head><title>T</title><script>var x = '<p>no</p>';</script></head>"
                                          "<body><!-- hidden --><h1>Asha&nbsp;Rao</h1><ul><li>Python &amp; SQL</li>"
                                          "<li>Data   analysis\n with R</li></ul><table><tr><td>2023</td><td>Intern</td></tr></table></body></html>");
    QCOMPARE(scripted, std::string("Asha Rao\nPython & SQL\nData analysis with R\n2023 Intern"));

    QCOMPARE(documentText("# Asha Rao\n\n## Skills\n- **Python** and `SQL`\n> Team lead, 2023", "cv.md"),
             std::string("Asha Rao\n\nSkills\n- Python and SQL\nTeam lead, 2023"));
    std::string utf16 = "\xFF\xFE";
    for (char c : std::string("Asha Rao, Python developer"))
        utf16 += std::string{c, '\0'};
    QCOMPARE(documentText(utf16, "cv.txt"), std::string("Asha Rao, Python developer"));
    QCOMPARE(plainText("Caf\xE9 owner, Python developer\r\n"), std::string("Caf\xC3\xA9 owner, Python developer")); // Windows-1252
    QVERIFY_THROWS_EXCEPTION(DocumentError, documentText("just some notes", "notes.txt")); // too little to be a resume
}

void TestResume::picturesNeedReading()
{
    QVERIFY_THROWS_EXCEPTION(NeedsReadingError, documentText(fixture("photo-sharp.jpg"), "IMG_2041.jpg"));
    QVERIFY_THROWS_EXCEPTION(NeedsReadingError, documentText(pdf("0 0 1 rg 72 72 200 200 re f", false), "scan.pdf"));
    // Ordinary document errors are not "needs reading".
    try {
        documentText("just some notes", "notes.txt");
    } catch (const NeedsReadingError&) {
        QFAIL("a short text file is not a picture");
    } catch (const DocumentError&) {
    }
}

void TestResume::scanQualityChecks()
{
    const GreyImage page = printedPage(1000, 1400);
    const ScanQuality sharp = measureScan(page);
    QVERIFY2(sharp.sharpness > 1.0 && sharp.detail > 0.3 && sharp.contrast > 150, qPrintable(QString::number(sharp.sharpness)));
    requireReadableScan(1000, 1400, sharp); // passes

    const auto refusal = [](int width, int height, const ScanQuality& q) -> std::string {
        try {
            requireReadableScan(width, height, q);
        } catch (const DocumentError& error) {
            return error.what();
        }
        return {};
    };
    const ScanQuality soft = measureScan(blurred(page, 2)); // sigma ~1.1: still readable
    QVERIFY2(refusal(1000, 1400, soft).empty(), qPrintable(QString::number(soft.sharpness)));
    const ScanQuality blurry = measureScan(blurred(page, 6)); // sigma ~2
    QVERIFY2(refusal(1000, 1400, blurry).find("too blurry") != std::string::npos, qPrintable(QString::number(blurry.sharpness)));
    QVERIFY(refusal(400, 560, sharp).find("too small") != std::string::npos);

    GreyImage dark = page;
    for (std::uint8_t& p : dark.pixels)
        p = static_cast<std::uint8_t>(p / 8);
    QVERIFY(refusal(1000, 1400, measureScan(dark)).find("too dark") != std::string::npos);
    GreyImage faint = page;
    for (std::uint8_t& p : faint.pixels)
        p = static_cast<std::uint8_t>(232 - (232 - p) / 9); // 25 on 232 becomes 209 on 232
    QVERIFY2(refusal(1000, 1400, measureScan(faint)).find("too faint") != std::string::npos, refusal(1000, 1400, measureScan(faint)).c_str());
    const GreyImage blank{1000, 1400, std::vector<std::uint8_t>(1000u * 1400u, 240)};
    QVERIFY(refusal(1000, 1400, measureScan(blank)).find("No text can be seen") != std::string::npos);
    QCOMPARE(measureScan(GreyImage{10, 10, {}}).sharpness, 0.0); // malformed: measured as nothing
}

void TestResume::readTextIsCheckedForBlur()
{
    ReadText clear{"Asha  Rao\r\nSkills: Python, SQL, React", std::vector<double>(7, 0.99), 1};
    QCOMPARE(acceptReadText(clear), std::string("Asha Rao\nSkills: Python, SQL, React"));
    ReadText smudged = clear;
    smudged.wordConfidence = {0.99, 0.95, 0.4, 0.3, 0.97, 0.5, 0.92}; // 3 of 7 uncertain
    try {
        acceptReadText(smudged);
        QFAIL("a picture with many uncertain words must be refused");
    } catch (const DocumentError& error) {
        QVERIFY(std::string(error.what()).find("too blurry or unclear") != std::string::npos);
        QVERIFY(std::string(error.what()).find("43%") != std::string::npos);
    }
    ReadText oneDoubt = clear;
    oneDoubt.wordConfidence[2] = 0.5; // 1 of 7: fine
    QVERIFY(!acceptReadText(oneDoubt).empty());
    QVERIFY_THROWS_EXCEPTION(DocumentError, acceptReadText(ReadText{"Hi", {0.99}, 1})); // a picture of almost nothing
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
