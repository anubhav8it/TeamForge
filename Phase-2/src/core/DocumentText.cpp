#include "core/DocumentText.h"

#include "core/Errors.h"
#include "core/Inflate.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <unordered_map>
#include <utility>
#include <vector>

namespace teamforge {

namespace {

constexpr std::size_t kMaxPart = 64u * 1024u * 1024u;   // one decompressed part or stream
constexpr std::size_t kMaxText = 8u * 1024u * 1024u;    // extracted text
constexpr int kMinLetters = 20;                          // below this the file has no real text

bool startsWith(std::string_view text, std::string_view prefix)
{
    return text.size() >= prefix.size() && text.compare(0, prefix.size(), prefix) == 0;
}

std::string lowerExtension(std::string_view fileName)
{
    const std::size_t dot = fileName.rfind('.');
    std::string ext = dot == std::string_view::npos ? std::string() : std::string(fileName.substr(dot + 1));
    for (char& c : ext)
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return ext;
}

void appendUtf8(std::string& out, std::uint32_t cp)
{
    if (cp == 0 || (cp >= 0xD800 && cp <= 0xDFFF) || cp > 0x10FFFF)
        return;
    if (cp < 0x80) {
        out.push_back(static_cast<char>(cp));
    } else if (cp < 0x800) {
        out.push_back(static_cast<char>(0xC0 | (cp >> 6)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else if (cp < 0x10000) {
        out.push_back(static_cast<char>(0xE0 | (cp >> 12)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else {
        out.push_back(static_cast<char>(0xF0 | (cp >> 18)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    }
}

// Typographic ligatures (PDF fonts often draw "fi" as one glyph) and no-break spaces become
// plain text, so a word drawn with the "fi" glyph still matches "classifier".
std::string plainLetters(const std::string& raw)
{
    static const std::pair<const char *, const char *> replacements[] = {
        {"\xEF\xAC\x80", "ff"}, {"\xEF\xAC\x81", "fi"}, {"\xEF\xAC\x82", "fl"}, {"\xEF\xAC\x83", "ffi"},
        {"\xEF\xAC\x84", "ffl"}, {"\xEF\xAC\x86", "st"}, {"\xC2\xA0", " "}, {"\xE2\x80\x8B", ""}};
    std::string out = raw;
    for (const auto& [from, to] : replacements) {
        const std::size_t length = std::strlen(from);
        for (std::size_t at = out.find(from); at != std::string::npos; at = out.find(from, at + std::strlen(to)))
            out.replace(at, length, to);
    }
    return out;
}

// Tidies extracted text: no control characters, single spaces, no blank-line runs.
std::string tidy(const std::string& input)
{
    const std::string raw = plainLetters(input);
    std::string out;
    out.reserve(raw.size());
    int newlines = 0;
    for (char c : raw) {
        if (c == '\r')
            c = '\n';
        if (c == '\t' || c == '\f' || c == '\v')
            c = ' ';
        if (c == '\n') {
            while (!out.empty() && out.back() == ' ')
                out.pop_back();
            if (++newlines <= 2 && !out.empty())
                out.push_back('\n');
            continue;
        }
        if (static_cast<unsigned char>(c) < 0x20)
            continue;
        if (c == ' ' && (out.empty() || out.back() == ' ' || out.back() == '\n'))
            continue;
        newlines = 0;
        out.push_back(c);
    }
    while (!out.empty() && (out.back() == ' ' || out.back() == '\n'))
        out.pop_back();
    return out;
}

void requireText(const std::string& text)
{
    int letters = 0;
    for (char c : text)
        letters += std::isalpha(static_cast<unsigned char>(c)) != 0 ? 1 : 0;
    if (letters < kMinLetters)
        throw DocumentError("No text could be read from this file. If it is a scanned image, "
                            "upload a text-based PDF or a DOCX, or add your details by hand.");
}

// --- DOCX ---------------------------------------------------------------------------------------

std::uint32_t le16(std::string_view d, std::size_t at)
{
    if (at + 2 > d.size())
        throw DocumentError("The DOCX file is damaged (it ends early).");
    return static_cast<std::uint32_t>(static_cast<unsigned char>(d[at]))
           | static_cast<std::uint32_t>(static_cast<unsigned char>(d[at + 1])) << 8u;
}
std::uint32_t le32(std::string_view d, std::size_t at)
{
    return le16(d, at) | le16(d, at + 2) << 16u;
}

struct ZipEntry
{
    std::string name;
    std::uint32_t method;
    std::uint32_t compressedSize;
    std::uint32_t localOffset;
};

std::vector<ZipEntry> zipDirectory(std::string_view d)
{
    // End of central directory: the last 22+ bytes (a comment may follow it).
    const std::size_t lowest = d.size() > 65557 ? d.size() - 65557 : 0;
    std::size_t eocd = std::string_view::npos;
    for (std::size_t i = d.size() >= 22 ? d.size() - 22 : 0; i + 1 > lowest; --i) {
        if (le32(d, i) == 0x06054b50u) {
            eocd = i;
            break;
        }
        if (i == 0)
            break;
    }
    if (eocd == std::string_view::npos)
        throw DocumentError("The DOCX file is damaged (no ZIP directory).");
    const std::uint32_t count = le16(d, eocd + 10);
    std::size_t at = le32(d, eocd + 16);
    std::vector<ZipEntry> entries;
    for (std::uint32_t i = 0; i < count; ++i) {
        if (le32(d, at) != 0x02014b50u)
            throw DocumentError("The DOCX file is damaged (bad ZIP directory).");
        const std::uint32_t nameLength = le16(d, at + 28);
        const std::uint32_t extra = le16(d, at + 30);
        const std::uint32_t comment = le16(d, at + 32);
        if (at + 46 + nameLength > d.size())
            throw DocumentError("The DOCX file is damaged (bad ZIP directory).");
        entries.push_back({std::string(d.substr(at + 46, nameLength)), le16(d, at + 10), le32(d, at + 20),
                           le32(d, at + 42)});
        at += 46 + nameLength + extra + comment;
    }
    return entries;
}

std::string zipRead(std::string_view d, const ZipEntry& entry)
{
    const std::size_t local = entry.localOffset;
    if (le32(d, local) != 0x04034b50u)
        throw DocumentError("The DOCX file is damaged (bad ZIP entry).");
    if (entry.compressedSize == 0xFFFFFFFFu)
        throw DocumentError("This DOCX file uses a ZIP format TeamForge cannot read.");
    const std::size_t start = local + 30 + le16(d, local + 26) + le16(d, local + 28);
    if (start + entry.compressedSize > d.size())
        throw DocumentError("The DOCX file is damaged (it ends early).");
    const std::string_view data = d.substr(start, entry.compressedSize);
    if (entry.method == 0)
        return std::string(data);
    if (entry.method == 8)
        return inflateRaw(data, kMaxPart);
    throw DocumentError("This DOCX file uses a compression TeamForge cannot read.");
}

void appendXmlEntity(std::string& out, std::string_view entity)
{
    if (entity == "amp") out += '&';
    else if (entity == "lt") out += '<';
    else if (entity == "gt") out += '>';
    else if (entity == "quot") out += '"';
    else if (entity == "apos") out += '\'';
    else if (entity.size() > 1 && entity[0] == '#') {
        const bool hex = entity[1] == 'x' || entity[1] == 'X';
        const std::string digits(entity.substr(hex ? 2 : 1));
        char *end = nullptr;
        const unsigned long cp = std::strtoul(digits.c_str(), &end, hex ? 16 : 10);
        if (end != nullptr && *end == '\0')
            appendUtf8(out, static_cast<std::uint32_t>(cp));
    }
}

// Word XML -> text: <w:t> runs, a line per paragraph, tabs and breaks. The fallback copy of
// drawing text (mc:Fallback) repeats mc:Choice, so it is skipped.
std::string wordXmlText(std::string_view xml)
{
    std::string out;
    bool inText = false;
    int fallbackDepth = 0;
    std::size_t i = 0;
    while (i < xml.size()) {
        if (xml[i] == '<') {
            const std::size_t close = xml.find('>', i);
            if (close == std::string_view::npos)
                break;
            std::string_view tag = xml.substr(i + 1, close - i - 1);
            const bool closing = !tag.empty() && tag[0] == '/';
            const bool selfClosing = !tag.empty() && tag.back() == '/';
            if (closing)
                tag.remove_prefix(1);
            const std::string_view name = tag.substr(0, tag.find_first_of(" /\t\r\n"));
            if (name == "mc:Fallback")
                fallbackDepth += closing ? -1 : selfClosing ? 0 : 1;
            else if (fallbackDepth == 0) {
                if (name == "w:t")
                    inText = !closing && !selfClosing;
                else if (name == "w:p" && closing)
                    out += '\n';
                else if ((name == "w:tab" || name == "w:ptab") && !closing)
                    out += '\t';
                else if ((name == "w:br" || name == "w:cr") && !closing)
                    out += '\n';
                else if (name == "w:noBreakHyphen")
                    out += '-';
                else if (name == "w:tc" && closing)
                    out += '\t';
            }
            i = close + 1;
            continue;
        }
        const std::size_t next = xml.find('<', i);
        const std::string_view chunk = xml.substr(i, next == std::string_view::npos ? std::string_view::npos : next - i);
        if (inText && fallbackDepth == 0) {
            for (std::size_t k = 0; k < chunk.size(); ++k) {
                if (chunk[k] == '&') {
                    const std::size_t semi = chunk.find(';', k);
                    if (semi != std::string_view::npos && semi - k <= 10) {
                        appendXmlEntity(out, chunk.substr(k + 1, semi - k - 1));
                        k = semi;
                        continue;
                    }
                }
                out += chunk[k];
            }
        }
        if (out.size() > kMaxText)
            break;
        i = next == std::string_view::npos ? xml.size() : next;
    }
    return out;
}

// --- PDF objects --------------------------------------------------------------------------------

struct PdfObject;
using PdfArray = std::vector<PdfObject>;
using PdfDict = std::map<std::string, PdfObject, std::less<>>;

struct PdfObject
{
    enum class Kind { Null, Bool, Number, Name, String, Array, Dict, Ref };
    Kind kind = Kind::Null;
    double number = 0;
    std::string text; // name (without '/') or string bytes
    std::shared_ptr<PdfArray> array;
    std::shared_ptr<PdfDict> dict;
    int ref = 0;

    bool is(Kind k) const { return kind == k; }
    const PdfObject *get(std::string_view key) const
    {
        if (kind != Kind::Dict)
            return nullptr;
        const auto it = dict->find(key);
        return it == dict->end() ? nullptr : &it->second;
    }
    bool isName(std::string_view name) const { return kind == Kind::Name && text == name; }
};

bool isPdfWhite(char c)
{
    return c == ' ' || c == '\n' || c == '\r' || c == '\t' || c == '\f' || c == '\0';
}
bool isPdfDelimiter(char c)
{
    return std::strchr("()<>[]{}/%", c) != nullptr && c != '\0';
}

// Tokens and values in PDF syntax, shared by the object parser and the content streams.
class PdfLexer
{
public:
    explicit PdfLexer(std::string_view data, std::size_t pos = 0) : d_(data), pos_(pos) {}

    std::size_t pos() const { return pos_; }
    void seek(std::size_t pos) { pos_ = pos; }
    bool atEnd()
    {
        skipWhite();
        return pos_ >= d_.size();
    }

    void skipWhite()
    {
        while (pos_ < d_.size()) {
            if (isPdfWhite(d_[pos_])) {
                ++pos_;
            } else if (d_[pos_] == '%') {
                while (pos_ < d_.size() && d_[pos_] != '\n' && d_[pos_] != '\r')
                    ++pos_;
            } else {
                break;
            }
        }
    }

    // A bare word (keyword or number) at the current position, without consuming it.
    std::string_view peekWord()
    {
        skipWhite();
        std::size_t end = pos_;
        while (end < d_.size() && !isPdfWhite(d_[end]) && !isPdfDelimiter(d_[end]))
            ++end;
        return d_.substr(pos_, end - pos_);
    }

    // One value. Numbers followed by "G R" become references. Operators (content streams) are
    // returned as names with `isOperator` set.
    PdfObject value(bool& isOperator, int depth = 0)
    {
        isOperator = false;
        skipWhite();
        if (pos_ >= d_.size() || depth > 64)
            return {};
        const char c = d_[pos_];
        if (c == '/') {
            ++pos_;
            PdfObject o;
            o.kind = PdfObject::Kind::Name;
            while (pos_ < d_.size() && !isPdfWhite(d_[pos_]) && !isPdfDelimiter(d_[pos_])) {
                const int high = pos_ + 2 < d_.size() ? hexDigit(d_[pos_ + 1]) : -1;
                const int low = pos_ + 2 < d_.size() ? hexDigit(d_[pos_ + 2]) : -1;
                if (d_[pos_] == '#' && high >= 0 && low >= 0) {
                    o.text += static_cast<char>(high * 16 + low);
                    pos_ += 3;
                } else {
                    o.text += d_[pos_++];
                }
            }
            return o;
        }
        if (c == '(')
            return literalString();
        if (c == '<' && pos_ + 1 < d_.size() && d_[pos_ + 1] == '<') {
            pos_ += 2;
            PdfObject o;
            o.kind = PdfObject::Kind::Dict;
            o.dict = std::make_shared<PdfDict>();
            for (;;) {
                skipWhite();
                if (pos_ + 1 >= d_.size())
                    break;
                if (d_[pos_] == '>' && d_[pos_ + 1] == '>') {
                    pos_ += 2;
                    break;
                }
                bool op = false;
                PdfObject key = value(op, depth + 1);
                if (!key.is(PdfObject::Kind::Name)) {
                    if (op && key.text.empty())
                        ++pos_; // skip a stray character
                    continue;
                }
                PdfObject val = value(op, depth + 1);
                (*o.dict)[key.text] = std::move(val);
            }
            return o;
        }
        if (c == '<')
            return hexString();
        if (c == '[') {
            ++pos_;
            PdfObject o;
            o.kind = PdfObject::Kind::Array;
            o.array = std::make_shared<PdfArray>();
            for (;;) {
                skipWhite();
                if (pos_ >= d_.size())
                    break;
                if (d_[pos_] == ']') {
                    ++pos_;
                    break;
                }
                bool op = false;
                const std::size_t before = pos_;
                PdfObject item = value(op, depth + 1);
                if (pos_ == before)
                    ++pos_;
                if (!op)
                    o.array->push_back(std::move(item));
            }
            return o;
        }
        if (c == ']' || c == '>' || c == ')' || c == '{' || c == '}') {
            ++pos_;
            isOperator = true;
            return {};
        }
        const std::string_view word = peekWord();
        if (word.empty()) {
            ++pos_;
            isOperator = true;
            return {};
        }
        pos_ += word.size();
        if (isNumber(word)) {
            PdfObject o;
            o.kind = PdfObject::Kind::Number;
            o.number = std::strtod(std::string(word).c_str(), nullptr);
            // "12 0 R": a reference to object 12.
            const std::size_t save = pos_;
            const std::string_view generation = peekWord();
            if (!generation.empty() && isInteger(word) && isInteger(generation)) {
                pos_ += generation.size();
                if (peekWord() == "R") {
                    pos_ += 1;
                    PdfObject r;
                    r.kind = PdfObject::Kind::Ref;
                    r.ref = static_cast<int>(o.number);
                    return r;
                }
            }
            pos_ = save;
            return o;
        }
        if (word == "true" || word == "false") {
            PdfObject o;
            o.kind = PdfObject::Kind::Bool;
            o.number = word == "true" ? 1 : 0;
            return o;
        }
        if (word == "null")
            return {};
        PdfObject o;
        o.kind = PdfObject::Kind::Name;
        o.text = std::string(word);
        isOperator = true;
        return o;
    }

private:
    static int hexDigit(char ch)
    {
        if (ch >= '0' && ch <= '9') return ch - '0';
        if (ch >= 'a' && ch <= 'f') return ch - 'a' + 10;
        if (ch >= 'A' && ch <= 'F') return ch - 'A' + 10;
        return -1;
    }
    static bool isInteger(std::string_view w)
    {
        return !w.empty() && std::all_of(w.begin(), w.end(), [](char ch) { return std::isdigit(static_cast<unsigned char>(ch)) != 0; });
    }
    static bool isNumber(std::string_view w)
    {
        bool digit = false;
        for (std::size_t i = 0; i < w.size(); ++i) {
            const char ch = w[i];
            if (std::isdigit(static_cast<unsigned char>(ch)) != 0)
                digit = true;
            else if (!(ch == '.' || ((ch == '-' || ch == '+') && i == 0)))
                return false;
        }
        return digit;
    }

    PdfObject literalString()
    {
        ++pos_; // (
        PdfObject o;
        o.kind = PdfObject::Kind::String;
        int nesting = 1;
        while (pos_ < d_.size()) {
            const char ch = d_[pos_++];
            if (ch == '\\' && pos_ < d_.size()) {
                const char e = d_[pos_++];
                switch (e) {
                case 'n': o.text += '\n'; break;
                case 'r': o.text += '\r'; break;
                case 't': o.text += '\t'; break;
                case 'b': o.text += '\b'; break;
                case 'f': o.text += '\f'; break;
                case '\r':
                    if (pos_ < d_.size() && d_[pos_] == '\n')
                        ++pos_;
                    break;
                case '\n': break;
                default:
                    if (e >= '0' && e <= '7') {
                        int v = e - '0';
                        for (int k = 0; k < 2 && pos_ < d_.size() && d_[pos_] >= '0' && d_[pos_] <= '7'; ++k)
                            v = v * 8 + (d_[pos_++] - '0');
                        o.text += static_cast<char>(v & 0xFF);
                    } else {
                        o.text += e;
                    }
                }
            } else if (ch == '(') {
                ++nesting;
                o.text += ch;
            } else if (ch == ')') {
                if (--nesting == 0)
                    break;
                o.text += ch;
            } else {
                o.text += ch;
            }
        }
        return o;
    }

    PdfObject hexString()
    {
        ++pos_; // <
        PdfObject o;
        o.kind = PdfObject::Kind::String;
        int high = -1;
        while (pos_ < d_.size() && d_[pos_] != '>') {
            const int v = hexDigit(d_[pos_++]);
            if (v < 0)
                continue;
            if (high < 0) {
                high = v;
            } else {
                o.text += static_cast<char>(high * 16 + v);
                high = -1;
            }
        }
        if (high >= 0)
            o.text += static_cast<char>(high * 16);
        ++pos_; // >
        return o;
    }

    std::string_view d_;
    std::size_t pos_;
};

struct StoredObject
{
    PdfObject value;
    std::string_view rawStream; // encoded stream bytes, if the object is a stream
    bool hasStream = false;
};

class PdfFile
{
public:
    explicit PdfFile(std::string_view data) : d_(data)
    {
        scanObjects();
        expandObjectStreams();
    }

    const PdfObject *resolve(const PdfObject *o, int depth = 0) const
    {
        while (o != nullptr && o->is(PdfObject::Kind::Ref) && depth++ < 16) {
            const auto it = objects_.find(o->ref);
            o = it == objects_.end() ? nullptr : &it->second.value;
        }
        return o;
    }
    const StoredObject *stored(const PdfObject *o) const
    {
        if (o == nullptr || !o->is(PdfObject::Kind::Ref))
            return nullptr;
        const auto it = objects_.find(o->ref);
        return it == objects_.end() ? nullptr : &it->second;
    }
    // Decoded bytes of a stream object (by reference). Unknown filters give an empty string.
    std::string streamData(const StoredObject& s) const
    {
        const PdfObject *filter = resolve(s.value.get("Filter"));
        std::vector<std::string> filters;
        if (filter != nullptr && filter->is(PdfObject::Kind::Name))
            filters.push_back(filter->text);
        else if (filter != nullptr && filter->is(PdfObject::Kind::Array))
            for (const PdfObject& f : *filter->array)
                if (f.is(PdfObject::Kind::Name))
                    filters.push_back(f.text);
        std::string data(s.rawStream);
        for (const std::string& f : filters) {
            if (f == "FlateDecode" || f == "Fl")
                data = inflateZlib(data, kMaxPart);
            else
                return {};
        }
        const PdfObject *parms = resolve(s.value.get("DecodeParms"));
        if (parms != nullptr && parms->is(PdfObject::Kind::Dict)) {
            const PdfObject *predictor = parms->get("Predictor");
            if (predictor != nullptr && predictor->number >= 10)
                return {}; // PNG predictors are only used for xref streams and images
        }
        return data;
    }

    bool encrypted() const { return encrypted_; }
    const std::map<int, StoredObject>& objects() const { return objects_; }

    // Pages in document order, each with the resources it inherits.
    std::vector<std::pair<const PdfObject *, const PdfObject *>> pages() const
    {
        std::vector<std::pair<const PdfObject *, const PdfObject *>> result;
        const PdfObject *root = nullptr;
        for (const auto& [number, object] : objects_) {
            (void)number;
            if (const PdfObject *type = object.value.get("Type"); type != nullptr && type->isName("Catalog"))
                root = resolve(object.value.get("Pages"));
        }
        std::set<const PdfObject *> seen;
        if (root != nullptr)
            walkPages(root, nullptr, result, seen, 0);
        if (result.empty()) { // no usable page tree: every page object, in file order
            for (const auto& [number, object] : objects_) {
                (void)number;
                if (const PdfObject *type = object.value.get("Type"); type != nullptr && type->isName("Page"))
                    result.emplace_back(&object.value, resolve(object.value.get("Resources")));
            }
        }
        return result;
    }

private:
    void walkPages(const PdfObject *node, const PdfObject *resources,
                   std::vector<std::pair<const PdfObject *, const PdfObject *>>& out,
                   std::set<const PdfObject *>& seen, int depth) const
    {
        if (node == nullptr || !node->is(PdfObject::Kind::Dict) || depth > 32 || !seen.insert(node).second)
            return;
        if (const PdfObject *own = resolve(node->get("Resources")); own != nullptr)
            resources = own;
        const PdfObject *kids = resolve(node->get("Kids"));
        if (kids != nullptr && kids->is(PdfObject::Kind::Array)) {
            for (const PdfObject& kid : *kids->array)
                walkPages(resolve(&kid), resources, out, seen, depth + 1);
        } else {
            out.emplace_back(node, resources);
        }
    }

    void scanObjects()
    {
        std::size_t pos = 0;
        while ((pos = d_.find("obj", pos)) != std::string_view::npos) {
            const std::size_t keyword = pos;
            pos += 3;
            if (pos < d_.size() && !isPdfWhite(d_[pos]) && !isPdfDelimiter(d_[pos]))
                continue;
            // "<number> <generation> obj"
            std::size_t back = keyword;
            auto skipBackWhite = [&] { while (back > 0 && isPdfWhite(d_[back - 1])) --back; };
            auto backDigits = [&] {
                const std::size_t end = back;
                while (back > 0 && std::isdigit(static_cast<unsigned char>(d_[back - 1])) != 0)
                    --back;
                return d_.substr(back, end - back);
            };
            skipBackWhite();
            if (backDigits().empty())
                continue;
            skipBackWhite();
            const std::string_view number = backDigits();
            if (number.empty() || number.size() > 9)
                continue;
            PdfLexer lexer(d_, pos);
            bool op = false;
            StoredObject object;
            object.value = lexer.value(op);
            if (lexer.peekWord() == "stream") {
                std::size_t start = lexer.pos() + 6;
                if (start < d_.size() && d_[start] == '\r') ++start;
                if (start < d_.size() && d_[start] == '\n') ++start;
                std::size_t end = std::string_view::npos;
                const PdfObject *length = object.value.get("Length");
                if (length != nullptr && length->is(PdfObject::Kind::Number) && length->number >= 0) {
                    const std::size_t guess = start + static_cast<std::size_t>(length->number);
                    if (guess <= d_.size() && d_.substr(guess, 40).find("endstream") != std::string_view::npos)
                        end = guess;
                }
                if (end == std::string_view::npos) {
                    end = d_.find("endstream", start);
                    if (end == std::string_view::npos)
                        break;
                    while (end > start && (d_[end - 1] == '\n' || d_[end - 1] == '\r'))
                        --end;
                }
                object.rawStream = d_.substr(start, end - start);
                object.hasStream = true;
                pos = end;
            } else {
                pos = lexer.pos();
            }
            const PdfObject *type = object.value.get("Type");
            if (type != nullptr && type->isName("XRef") && object.value.get("Encrypt") != nullptr)
                encrypted_ = true;
            objects_[std::stoi(std::string(number))] = std::move(object); // later updates win
        }
        // Classic trailers.
        std::size_t t = 0;
        while ((t = d_.find("trailer", t)) != std::string_view::npos) {
            t += 7;
            PdfLexer lexer(d_, t);
            bool op = false;
            const PdfObject trailer = lexer.value(op);
            if (trailer.get("Encrypt") != nullptr)
                encrypted_ = true;
        }
    }

    // Compressed object streams (PDF 1.5+) hold many small objects, fonts among them.
    void expandObjectStreams()
    {
        std::vector<int> streams;
        for (const auto& [number, object] : objects_) {
            const PdfObject *type = object.value.get("Type");
            if (object.hasStream && type != nullptr && type->isName("ObjStm"))
                streams.push_back(number);
        }
        for (int number : streams) {
            const StoredObject& holder = objects_.at(number);
            const PdfObject *first = holder.value.get("First");
            const PdfObject *count = holder.value.get("N");
            if (first == nullptr || count == nullptr)
                continue;
            std::string data;
            try {
                data = streamData(holder);
            } catch (const DocumentError&) {
                continue;
            }
            objectStreamData_.push_back(std::make_unique<std::string>(std::move(data)));
            const std::string_view bytes = *objectStreamData_.back();
            PdfLexer header(bytes);
            const auto n = static_cast<int>(count->number);
            std::vector<std::pair<int, std::size_t>> entries;
            for (int i = 0; i < n && i < 100000; ++i) {
                bool op = false;
                const PdfObject num = header.value(op);
                const PdfObject off = header.value(op);
                if (!num.is(PdfObject::Kind::Number) || !off.is(PdfObject::Kind::Number))
                    break;
                entries.emplace_back(static_cast<int>(num.number), static_cast<std::size_t>(off.number));
            }
            for (const auto& [objectNumber, offset] : entries) {
                if (objects_.count(objectNumber) != 0)
                    continue;
                PdfLexer lexer(bytes, static_cast<std::size_t>(first->number) + offset);
                bool op = false;
                StoredObject object;
                object.value = lexer.value(op);
                objects_[objectNumber] = std::move(object);
            }
        }
    }

    std::string_view d_;
    std::map<int, StoredObject> objects_;
    std::vector<std::unique_ptr<std::string>> objectStreamData_; // keeps decoded object streams alive
    bool encrypted_ = false;
};

// --- PDF fonts ----------------------------------------------------------------------------------

std::uint32_t glyphNameToUnicode(const std::string& name)
{
    static const std::unordered_map<std::string, std::uint32_t> names = {
        {"space", 0x20}, {"bullet", 0x2022}, {"endash", 0x2013}, {"emdash", 0x2014}, {"quoteright", 0x2019},
        {"quoteleft", 0x2018}, {"quotedblleft", 0x201C}, {"quotedblright", 0x201D}, {"fi", 0xFB01},
        {"fl", 0xFB02}, {"ff", 0xFB00}, {"ffi", 0xFB03}, {"ffl", 0xFB04}, {"hyphen", 0x2D}, {"period", 0x2E},
        {"comma", 0x2C}, {"colon", 0x3A}, {"semicolon", 0x3B}, {"slash", 0x2F}, {"parenleft", 0x28},
        {"parenright", 0x29}, {"plus", 0x2B}, {"numbersign", 0x23}, {"at", 0x40}, {"ampersand", 0x26},
        {"bar", 0x7C}, {"underscore", 0x5F}, {"periodcentered", 0xB7}, {"minus", 0x2212}, {"percent", 0x25},
        {"zero", 0x30}, {"one", 0x31}, {"two", 0x32}, {"three", 0x33}, {"four", 0x34}, {"five", 0x35},
        {"six", 0x36}, {"seven", 0x37}, {"eight", 0x38}, {"nine", 0x39}, {"quotesingle", 0x27},
        {"quotedbl", 0x22}, {"exclam", 0x21}, {"question", 0x3F}, {"bracketleft", 0x5B}, {"bracketright", 0x5D},
        {"greater", 0x3E}, {"less", 0x3C}, {"equal", 0x3D}, {"dollar", 0x24}, {"asterisk", 0x2A},
    };
    if (name.size() == 1 && std::isalpha(static_cast<unsigned char>(name[0])) != 0)
        return static_cast<unsigned char>(name[0]);
    if (const auto it = names.find(name); it != names.end())
        return it->second;
    if (startsWith(name, "uni") && name.size() == 7)
        return static_cast<std::uint32_t>(std::strtoul(name.c_str() + 3, nullptr, 16));
    if (name.size() > 1 && name[0] == 'u' && name.size() <= 7)
        return static_cast<std::uint32_t>(std::strtoul(name.c_str() + 1, nullptr, 16));
    return 0;
}

std::uint32_t winAnsi(unsigned char c)
{
    static const std::uint16_t high[32] = {0x20AC, 0, 0x201A, 0x0192, 0x201E, 0x2026, 0x2020, 0x2021, 0x02C6, 0x2030,
                                           0x0160, 0x2039, 0x0152, 0, 0x017D, 0, 0, 0x2018, 0x2019, 0x201C, 0x201D,
                                           0x2022, 0x2013, 0x2014, 0x02DC, 0x2122, 0x0161, 0x203A, 0x0153, 0, 0x017E,
                                           0x0178};
    if (c >= 0x80 && c < 0xA0)
        return high[c - 0x80];
    return c;
}

struct Font
{
    bool twoByte = false;
    std::vector<std::pair<int, std::pair<std::uint32_t, std::uint32_t>>> codespace; // bytes, [lo, hi]
    std::unordered_map<std::uint32_t, std::string> toUnicode;
    std::unordered_map<std::uint32_t, std::uint32_t> differences;
    std::unordered_map<std::uint32_t, double> widths;
    double defaultWidth = 500;

    // Splits a shown string into character codes.
    std::vector<std::uint32_t> codes(const std::string& bytes) const
    {
        std::vector<std::uint32_t> result;
        std::size_t i = 0;
        while (i < bytes.size()) {
            int length = twoByte ? 2 : 1;
            for (int len = 1; len <= 4 && !codespace.empty(); ++len) {
                if (i + static_cast<std::size_t>(len) > bytes.size())
                    break;
                std::uint32_t code = 0;
                for (int k = 0; k < len; ++k)
                    code = code << 8u | static_cast<unsigned char>(bytes[i + static_cast<std::size_t>(k)]);
                const bool fits = std::any_of(codespace.begin(), codespace.end(), [&](const auto& range) {
                    return range.first == len && code >= range.second.first && code <= range.second.second;
                });
                if (fits) {
                    length = len;
                    break;
                }
            }
            std::uint32_t code = 0;
            for (int k = 0; k < length && i < bytes.size(); ++k)
                code = code << 8u | static_cast<unsigned char>(bytes[i++]);
            result.push_back(code);
        }
        return result;
    }

    void appendText(std::string& out, std::uint32_t code) const
    {
        if (const auto it = toUnicode.find(code); it != toUnicode.end()) {
            out += it->second;
            return;
        }
        if (twoByte)
            return; // a CID with no Unicode mapping: nothing reliable to show
        if (const auto it = differences.find(code); it != differences.end()) {
            appendUtf8(out, it->second);
            return;
        }
        appendUtf8(out, winAnsi(static_cast<unsigned char>(code)));
    }

    double width(std::uint32_t code) const
    {
        const auto it = widths.find(code);
        return it == widths.end() ? defaultWidth : it->second;
    }
};

std::uint32_t bytesToCode(const std::string& bytes)
{
    std::uint32_t code = 0;
    for (char c : bytes)
        code = code << 8u | static_cast<unsigned char>(c);
    return code;
}

std::string utf16beToUtf8(const std::string& bytes)
{
    std::string out;
    for (std::size_t i = 0; i + 1 < bytes.size(); i += 2) {
        std::uint32_t unit = static_cast<unsigned char>(bytes[i]) << 8u | static_cast<unsigned char>(bytes[i + 1]);
        if (unit >= 0xD800 && unit <= 0xDBFF && i + 3 < bytes.size()) {
            const std::uint32_t low = static_cast<unsigned char>(bytes[i + 2]) << 8u | static_cast<unsigned char>(bytes[i + 3]);
            unit = 0x10000 + ((unit - 0xD800) << 10u) + (low - 0xDC00);
            i += 2;
        }
        appendUtf8(out, unit);
    }
    if (bytes.size() == 1)
        appendUtf8(out, static_cast<unsigned char>(bytes[0]));
    return out;
}

void parseToUnicode(const std::string& cmap, Font& font)
{
    PdfLexer lexer(cmap);
    std::vector<PdfObject> operands;
    while (!lexer.atEnd()) {
        bool op = false;
        const std::size_t before = lexer.pos();
        PdfObject value = lexer.value(op);
        if (lexer.pos() == before)
            lexer.seek(before + 1);
        if (!op) {
            operands.push_back(std::move(value));
            continue;
        }
        if (value.text == "endcodespacerange") {
            for (std::size_t i = 0; i + 1 < operands.size(); i += 2)
                if (operands[i].is(PdfObject::Kind::String))
                    font.codespace.push_back({static_cast<int>(operands[i].text.size()),
                                              {bytesToCode(operands[i].text), bytesToCode(operands[i + 1].text)}});
        } else if (value.text == "endbfchar") {
            for (std::size_t i = 0; i + 1 < operands.size(); i += 2)
                if (operands[i].is(PdfObject::Kind::String) && operands[i + 1].is(PdfObject::Kind::String))
                    font.toUnicode[bytesToCode(operands[i].text)] = utf16beToUtf8(operands[i + 1].text);
        } else if (value.text == "endbfrange") {
            for (std::size_t i = 0; i + 2 < operands.size(); i += 3) {
                const std::uint32_t lo = bytesToCode(operands[i].text);
                const std::uint32_t hi = bytesToCode(operands[i + 1].text);
                if (hi < lo || hi - lo > 0xFFFF)
                    continue;
                const PdfObject& dst = operands[i + 2];
                if (dst.is(PdfObject::Kind::Array)) {
                    for (std::uint32_t c = lo; c <= hi && c - lo < dst.array->size(); ++c)
                        font.toUnicode[c] = utf16beToUtf8((*dst.array)[c - lo].text);
                } else if (dst.is(PdfObject::Kind::String) && dst.text.size() >= 2) {
                    std::string start = dst.text;
                    for (std::uint32_t c = lo; c <= hi; ++c) {
                        font.toUnicode[c] = utf16beToUtf8(start);
                        // Increment the last code unit.
                        auto last = static_cast<unsigned char>(start.back());
                        start.back() = static_cast<char>(last + 1);
                        if (last == 0xFF && start.size() >= 2)
                            start[start.size() - 2] = static_cast<char>(static_cast<unsigned char>(start[start.size() - 2]) + 1);
                    }
                }
            }
        }
        operands.clear();
    }
    if (!font.codespace.empty())
        font.twoByte = std::all_of(font.codespace.begin(), font.codespace.end(), [](const auto& r) { return r.first == 2; });
}

Font loadFont(const PdfFile& pdf, const PdfObject *ref)
{
    Font font;
    const PdfObject *dict = pdf.resolve(ref);
    if (dict == nullptr || !dict->is(PdfObject::Kind::Dict))
        return font;
    const PdfObject *subtype = pdf.resolve(dict->get("Subtype"));
    const bool type0 = subtype != nullptr && subtype->isName("Type0");
    font.twoByte = type0;

    if (type0) {
        const PdfObject *descendants = pdf.resolve(dict->get("DescendantFonts"));
        if (descendants != nullptr && descendants->is(PdfObject::Kind::Array) && !descendants->array->empty()) {
            const PdfObject *cid = pdf.resolve(&descendants->array->front());
            if (cid != nullptr) {
                if (const PdfObject *dw = pdf.resolve(cid->get("DW")); dw != nullptr && dw->is(PdfObject::Kind::Number))
                    font.defaultWidth = dw->number;
                else
                    font.defaultWidth = 1000;
                const PdfObject *w = pdf.resolve(cid->get("W"));
                if (w != nullptr && w->is(PdfObject::Kind::Array)) {
                    const PdfArray& a = *w->array;
                    for (std::size_t i = 0; i + 1 < a.size();) {
                        const auto first = static_cast<std::uint32_t>(a[i].number);
                        const PdfObject *next = pdf.resolve(&a[i + 1]);
                        if (next != nullptr && next->is(PdfObject::Kind::Array)) {
                            for (std::size_t k = 0; k < next->array->size(); ++k)
                                font.widths[first + static_cast<std::uint32_t>(k)] = (*next->array)[k].number;
                            i += 2;
                        } else if (i + 2 < a.size()) {
                            const auto last = static_cast<std::uint32_t>(a[i + 1].number);
                            for (std::uint32_t c = first; c <= last && c - first < 65536; ++c)
                                font.widths[c] = a[i + 2].number;
                            i += 3;
                        } else {
                            break;
                        }
                    }
                }
            }
        }
    } else {
        const PdfObject *firstChar = pdf.resolve(dict->get("FirstChar"));
        const PdfObject *widths = pdf.resolve(dict->get("Widths"));
        if (firstChar != nullptr && widths != nullptr && widths->is(PdfObject::Kind::Array))
            for (std::size_t k = 0; k < widths->array->size(); ++k)
                font.widths[static_cast<std::uint32_t>(firstChar->number) + static_cast<std::uint32_t>(k)] =
                    pdf.resolve(&(*widths->array)[k])->number;
        const PdfObject *encoding = pdf.resolve(dict->get("Encoding"));
        if (encoding != nullptr && encoding->is(PdfObject::Kind::Dict)) {
            const PdfObject *differences = pdf.resolve(encoding->get("Differences"));
            if (differences != nullptr && differences->is(PdfObject::Kind::Array)) {
                std::uint32_t code = 0;
                for (const PdfObject& item : *differences->array) {
                    if (item.is(PdfObject::Kind::Number)) {
                        code = static_cast<std::uint32_t>(item.number);
                    } else if (item.is(PdfObject::Kind::Name)) {
                        if (const std::uint32_t cp = glyphNameToUnicode(item.text); cp != 0)
                            font.differences[code] = cp;
                        ++code;
                    }
                }
            }
        }
    }

    if (const StoredObject *cmap = pdf.stored(dict->get("ToUnicode")); cmap != nullptr && cmap->hasStream) {
        try {
            parseToUnicode(pdf.streamData(*cmap), font);
        } catch (const DocumentError&) {
        }
        if (type0)
            font.twoByte = font.codespace.empty() || font.twoByte;
    }
    return font;
}

// --- PDF content streams ------------------------------------------------------------------------

class TextCollector
{
public:
    TextCollector(const PdfFile& pdf, std::string& out) : pdf_(pdf), out_(out) {}

    void page(const PdfObject *page, const PdfObject *resources)
    {
        std::string content;
        const PdfObject *contents = page->get("Contents");
        const PdfObject *resolved = pdf_.resolve(contents);
        std::vector<const PdfObject *> parts;
        if (resolved != nullptr && resolved->is(PdfObject::Kind::Array)) {
            for (const PdfObject& part : *resolved->array)
                parts.push_back(&part);
        } else if (contents != nullptr) {
            parts.push_back(contents);
        }
        for (const PdfObject *part : parts) {
            if (const StoredObject *s = pdf_.stored(part); s != nullptr && s->hasStream) {
                try {
                    content += pdf_.streamData(*s);
                    content += '\n';
                } catch (const DocumentError&) {
                }
            }
        }
        lastY_.reset();
        run(content, resources, 0);
        out_ += '\n';
    }

private:
    struct State
    {
        double a = 1, b = 0, c = 0, d = 1, e = 0, f = 0;           // text matrix
        double la = 1, lb = 0, lc = 0, ld = 1, le = 0, lf = 0;     // text line matrix
        double fontSize = 12, charSpacing = 0, wordSpacing = 0, scale = 1, leading = 0;
        const Font *font = nullptr;
    };

    const Font& fontFor(const PdfObject *resources, const std::string& name)
    {
        static const Font fallback;
        const PdfObject *fonts = pdf_.resolve(resources != nullptr ? resources->get("Font") : nullptr);
        const PdfObject *ref = fonts != nullptr ? fonts->get(name) : nullptr;
        if (ref == nullptr)
            return fallback;
        const void *key = ref->is(PdfObject::Kind::Ref) ? static_cast<const void *>(pdf_.resolve(ref)) : ref;
        auto it = fonts_.find(key);
        if (it == fonts_.end())
            it = fonts_.emplace(key, loadFont(pdf_, ref)).first;
        return it->second;
    }

    void moveLine(State& s, double tx, double ty)
    {
        s.le = tx * s.la + ty * s.lc + s.le;
        s.lf = tx * s.lb + ty * s.ld + s.lf;
        s.a = s.la; s.b = s.lb; s.c = s.lc; s.d = s.ld; s.e = s.le; s.f = s.lf;
    }

    void show(State& s, const std::string& bytes)
    {
        if (s.font == nullptr)
            return;
        const double size = std::abs(s.fontSize * (std::abs(s.d) > 0 ? s.d : s.a));
        const double x = s.e;
        const double y = s.f;
        if (lastY_) {
            if (std::abs(y - *lastY_) > std::max(1.0, size * 0.45))
                out_ += '\n';
            else if (x < lastEndX_ - size * 3)
                out_ += '\n'; // back on the same baseline: another column
            else if (x - lastEndX_ > size * 0.18 && !out_.empty() && out_.back() != ' ' && out_.back() != '\n')
                out_ += ' ';
        }
        double advance = 0;
        for (const std::uint32_t code : s.font->codes(bytes)) {
            s.font->appendText(out_, code);
            advance += (s.font->width(code) / 1000.0 * s.fontSize + s.charSpacing
                        + (code == 32 && !s.font->twoByte ? s.wordSpacing : 0)) * s.scale;
        }
        s.e += advance * s.a;
        s.f += advance * s.b;
        lastEndX_ = s.e;
        lastY_ = y;
    }

    void run(const std::string& content, const PdfObject *resources, int depth)
    {
        if (depth > 4 || out_.size() > kMaxText)
            return;
        PdfLexer lexer(content);
        std::vector<PdfObject> ops;
        State s;
        std::vector<State> saved;
        while (!lexer.atEnd()) {
            bool isOp = false;
            const std::size_t before = lexer.pos();
            PdfObject value = lexer.value(isOp);
            if (lexer.pos() == before) {
                lexer.seek(before + 1);
                continue;
            }
            if (!isOp) {
                ops.push_back(std::move(value));
                if (ops.size() > 64)
                    ops.erase(ops.begin());
                continue;
            }
            const std::string& op = value.text;
            auto num = [&](std::size_t fromEnd) {
                return ops.size() >= fromEnd ? ops[ops.size() - fromEnd].number : 0.0;
            };
            if (op == "BT") {
                s.a = s.la = 1; s.b = s.lb = 0; s.c = s.lc = 0; s.d = s.ld = 1; s.e = s.le = 0; s.f = s.lf = 0;
            } else if (op == "Tf" && ops.size() >= 2) {
                s.fontSize = num(1);
                if (ops[ops.size() - 2].is(PdfObject::Kind::Name))
                    s.font = &fontFor(resources, ops[ops.size() - 2].text);
            } else if (op == "Td") {
                moveLine(s, num(2), num(1));
            } else if (op == "TD") {
                s.leading = -num(1);
                moveLine(s, num(2), num(1));
            } else if (op == "Tm" && ops.size() >= 6) {
                s.a = s.la = num(6); s.b = s.lb = num(5); s.c = s.lc = num(4);
                s.d = s.ld = num(3); s.e = s.le = num(2); s.f = s.lf = num(1);
            } else if (op == "T*") {
                moveLine(s, 0, -s.leading);
            } else if (op == "TL") {
                s.leading = num(1);
            } else if (op == "Tc") {
                s.charSpacing = num(1);
            } else if (op == "Tw") {
                s.wordSpacing = num(1);
            } else if (op == "Tz") {
                s.scale = num(1) / 100.0;
            } else if (op == "Tj" && !ops.empty()) {
                show(s, ops.back().text);
            } else if ((op == "'" || op == "\"") && !ops.empty()) {
                if (op == "\"" && ops.size() >= 3) {
                    s.wordSpacing = num(3);
                    s.charSpacing = num(2);
                }
                moveLine(s, 0, -s.leading);
                show(s, ops.back().text);
            } else if (op == "TJ" && !ops.empty() && ops.back().is(PdfObject::Kind::Array)) {
                for (const PdfObject& item : *ops.back().array) {
                    if (item.is(PdfObject::Kind::String)) {
                        show(s, item.text);
                    } else if (item.is(PdfObject::Kind::Number)) {
                        const double shift = -item.number / 1000.0 * s.fontSize * s.scale;
                        s.e += shift * s.a;
                        s.f += shift * s.b;
                        if (item.number < -250 && !out_.empty() && out_.back() != ' ' && out_.back() != '\n')
                            out_ += ' ';
                        lastEndX_ = s.e;
                    }
                }
            } else if (op == "q") {
                saved.push_back(s);
            } else if (op == "Q") {
                if (!saved.empty()) {
                    s = saved.back();
                    saved.pop_back();
                }
            } else if (op == "Do" && !ops.empty() && ops.back().is(PdfObject::Kind::Name)) {
                form(resources, ops.back().text, depth);
            } else if (op == "BI") {
                // Inline image: skip its data up to "EI".
                const std::size_t id = content.find("ID", lexer.pos());
                const std::size_t ei = id == std::string::npos ? std::string::npos : content.find("EI", id + 2);
                lexer.seek(ei == std::string::npos ? content.size() : ei + 2);
            }
            ops.clear();
        }
    }

    void form(const PdfObject *resources, const std::string& name, int depth)
    {
        const PdfObject *xobjects = pdf_.resolve(resources != nullptr ? resources->get("XObject") : nullptr);
        const PdfObject *ref = xobjects != nullptr ? xobjects->get(name) : nullptr;
        const StoredObject *s = pdf_.stored(ref);
        if (s == nullptr || !s->hasStream)
            return;
        const PdfObject *subtype = s->value.get("Subtype");
        if (subtype == nullptr || !subtype->isName("Form"))
            return;
        const PdfObject *own = pdf_.resolve(s->value.get("Resources"));
        try {
            run(pdf_.streamData(*s), own != nullptr ? own : resources, depth + 1);
        } catch (const DocumentError&) {
        }
    }

    const PdfFile& pdf_;
    std::string& out_;
    std::map<const void *, Font> fonts_;
    std::optional<double> lastY_;
    double lastEndX_ = 0;
};

} // namespace

DocumentType detectDocumentType(std::string_view bytes, std::string_view fileName)
{
    if (bytes.empty())
        throw DocumentError("The file is empty.");
    if (bytes.substr(0, 1024).find("%PDF-") != std::string_view::npos)
        return DocumentType::Pdf;
    if (startsWith(bytes, "PK\x03\x04"))
        return DocumentType::Docx;
    if (startsWith(bytes, "\xD0\xCF\x11\xE0"))
        throw DocumentError("This looks like an old Word .doc file, or a password-protected document. "
                            "Save it as a DOCX or PDF and upload it again.");
    const std::string ext = lowerExtension(fileName);
    if (ext == "pdf" || ext == "docx")
        throw DocumentError("This file is named ." + ext + ", but its content is not a "
                            + std::string(ext == "pdf" ? "PDF" : "Word document") + "; it may be damaged.");
    throw DocumentError("Only PDF and DOCX resumes are supported.");
}

std::string docxText(std::string_view bytes)
{
    const std::vector<ZipEntry> entries = zipDirectory(bytes);
    const auto body = std::find_if(entries.begin(), entries.end(), [](const ZipEntry& e) { return e.name == "word/document.xml"; });
    if (body == entries.end())
        throw DocumentError("This file is a ZIP archive but not a Word document.");
    std::string text;
    // Headers first: some templates put the name and contact line there.
    for (const ZipEntry& entry : entries)
        if (startsWith(entry.name, "word/header") && entry.name.size() > 4 && entry.name.substr(entry.name.size() - 4) == ".xml")
            text += wordXmlText(zipRead(bytes, entry)) + "\n";
    text += wordXmlText(zipRead(bytes, *body));
    text = tidy(text);
    requireText(text);
    return text;
}

std::string pdfText(std::string_view bytes)
{
    const PdfFile pdf(bytes);
    if (pdf.encrypted())
        throw DocumentError("This PDF is protected (encrypted). Save or print it again without a password and "
                            "upload that copy, or upload a DOCX.");
    std::string raw;
    TextCollector collector(pdf, raw);
    for (const auto& [page, resources] : pdf.pages()) {
        collector.page(page, resources);
        if (raw.size() > kMaxText)
            break;
    }
    std::string text = tidy(raw);
    requireText(text);
    return text;
}

std::string documentText(std::string_view bytes, std::string_view fileName)
{
    return detectDocumentType(bytes, fileName) == DocumentType::Pdf ? pdfText(bytes) : docxText(bytes);
}

} // namespace teamforge
