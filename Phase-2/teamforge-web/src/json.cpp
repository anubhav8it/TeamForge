#include "json.hpp"

#include <cerrno>
#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace tf {

bool Json::has(const std::string& k) const {
    if (t_ != Type::Object) return false;
    for (const auto& key : keys_) if (key == k) return true;
    return false;
}

Json Json::operator[](const std::string& k) && {
    const Json& self = *this;
    return self[k];
}

const Json& Json::operator[](const std::string& k) const& {
    static const Json null_value;
    if (t_ != Type::Object) return null_value;
    for (size_t i = 0; i < keys_.size(); ++i)
        if (keys_[i] == k) return arr_[i];
    return null_value;
}

Json& Json::set(const std::string& k, Json v) {
    if (t_ == Type::Null) t_ = Type::Object;
    for (size_t i = 0; i < keys_.size(); ++i)
        if (keys_[i] == k) { arr_[i] = std::move(v); return *this; }
    keys_.push_back(k);
    arr_.push_back(std::move(v));
    return *this;
}

void json_escape(const std::string& s, std::string& out) {
    out += '"';
    for (unsigned char c : s) {
        switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            case '\b': out += "\\b"; break;
            case '\f': out += "\\f"; break;
            default:
                if (c < 0x20) {
                    char buf[8];
                    std::snprintf(buf, sizeof buf, "\\u%04x", c);
                    out += buf;
                } else {
                    out += static_cast<char>(c);   // UTF-8 passes through unchanged
                }
        }
    }
    out += '"';
}

void Json::dump_to(std::string& out) const {
    switch (t_) {
        case Type::Null: out += "null"; break;
        case Type::Bool: out += b_ ? "true" : "false"; break;
        case Type::Int: out += std::to_string(i_); break;
        case Type::Double: {
            if (!std::isfinite(d_)) { out += "null"; break; }
            char buf[32];
            std::snprintf(buf, sizeof buf, "%.17g", d_);
            out += buf;
            break;
        }
        case Type::String: json_escape(s_, out); break;
        case Type::Array:
            out += '[';
            for (size_t i = 0; i < arr_.size(); ++i) {
                if (i) out += ',';
                arr_[i].dump_to(out);
            }
            out += ']';
            break;
        case Type::Object:
            out += '{';
            for (size_t i = 0; i < keys_.size(); ++i) {
                if (i) out += ',';
                json_escape(keys_[i], out);
                out += ':';
                arr_[i].dump_to(out);
            }
            out += '}';
            break;
    }
}

std::string Json::dump() const {
    std::string out;
    dump_to(out);
    return out;
}

// ------------------------------------------------------------------ parser

class JsonParser {
public:
    explicit JsonParser(const std::string& s) : s_(s) {}

    Json run() {
        Json v = value(0);
        ws();
        if (p_ != s_.size()) fail("unexpected text after the JSON value");
        return v;
    }

private:
    const std::string& s_;
    size_t p_ = 0;
    static constexpr int kMaxDepth = 64;   // stops stack overflow on hostile input

    [[noreturn]] void fail(const std::string& why) {
        throw JsonError("Invalid JSON at position " + std::to_string(p_) + ": " + why);
    }
    void ws() { while (p_ < s_.size() && (s_[p_] == ' ' || s_[p_] == '\n' || s_[p_] == '\r' || s_[p_] == '\t')) ++p_; }
    bool lit(const char* w) {
        size_t n = std::char_traits<char>::length(w);
        if (s_.compare(p_, n, w) == 0) { p_ += n; return true; }
        return false;
    }

    Json value(int depth) {
        if (depth > kMaxDepth) fail("nested too deeply");
        ws();
        if (p_ >= s_.size()) fail("unexpected end");
        char c = s_[p_];
        if (c == '{') return object(depth);
        if (c == '[') return array(depth);
        if (c == '"') return Json(string());
        if (lit("true")) return Json(true);
        if (lit("false")) return Json(false);
        if (lit("null")) return Json();
        if (c == '-' || (c >= '0' && c <= '9')) return number();
        fail("unexpected character");
    }

    Json object(int depth) {
        Json o = Json::object();
        ++p_;
        ws();
        if (p_ < s_.size() && s_[p_] == '}') { ++p_; return o; }
        for (;;) {
            ws();
            if (p_ >= s_.size() || s_[p_] != '"') fail("expected a key");
            std::string k = string();
            ws();
            if (p_ >= s_.size() || s_[p_] != ':') fail("expected ':'");
            ++p_;
            o.set(k, value(depth + 1));
            ws();
            if (p_ < s_.size() && s_[p_] == ',') { ++p_; continue; }
            if (p_ < s_.size() && s_[p_] == '}') { ++p_; return o; }
            fail("expected ',' or '}'");
        }
    }

    Json array(int depth) {
        Json a = Json::array();
        ++p_;
        ws();
        if (p_ < s_.size() && s_[p_] == ']') { ++p_; return a; }
        for (;;) {
            a.push(value(depth + 1));
            ws();
            if (p_ < s_.size() && s_[p_] == ',') { ++p_; continue; }
            if (p_ < s_.size() && s_[p_] == ']') { ++p_; return a; }
            fail("expected ',' or ']'");
        }
    }

    static void utf8(unsigned cp, std::string& out) {
        if (cp < 0x80) out += static_cast<char>(cp);
        else if (cp < 0x800) { out += static_cast<char>(0xC0 | (cp >> 6)); out += static_cast<char>(0x80 | (cp & 0x3F)); }
        else if (cp < 0x10000) {
            out += static_cast<char>(0xE0 | (cp >> 12));
            out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
            out += static_cast<char>(0x80 | (cp & 0x3F));
        } else {
            out += static_cast<char>(0xF0 | (cp >> 18));
            out += static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
            out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
            out += static_cast<char>(0x80 | (cp & 0x3F));
        }
    }

    unsigned hex4() {
        if (p_ + 4 > s_.size()) fail("bad \\u escape");
        unsigned v = 0;
        for (int i = 0; i < 4; ++i) {
            char c = s_[p_++];
            v <<= 4;
            if (c >= '0' && c <= '9') v |= c - '0';
            else if (c >= 'a' && c <= 'f') v |= c - 'a' + 10;
            else if (c >= 'A' && c <= 'F') v |= c - 'A' + 10;
            else fail("bad \\u escape");
        }
        return v;
    }

    std::string string() {
        ++p_;  // opening quote
        std::string out;
        while (p_ < s_.size()) {
            char c = s_[p_++];
            if (c == '"') return out;
            if (static_cast<unsigned char>(c) < 0x20) fail("control character in string");
            if (c != '\\') { out += c; continue; }
            if (p_ >= s_.size()) break;
            char e = s_[p_++];
            switch (e) {
                case '"': out += '"'; break;
                case '\\': out += '\\'; break;
                case '/': out += '/'; break;
                case 'b': out += '\b'; break;
                case 'f': out += '\f'; break;
                case 'n': out += '\n'; break;
                case 'r': out += '\r'; break;
                case 't': out += '\t'; break;
                case 'u': {
                    unsigned cp = hex4();
                    if (cp >= 0xD800 && cp <= 0xDBFF) {   // surrogate pair
                        if (!lit("\\u")) fail("lone surrogate");
                        unsigned lo = hex4();
                        if (lo < 0xDC00 || lo > 0xDFFF) fail("bad surrogate pair");
                        cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
                    } else if (cp >= 0xDC00 && cp <= 0xDFFF) {
                        fail("lone surrogate");
                    }
                    utf8(cp, out);
                    break;
                }
                default: fail("bad escape");
            }
        }
        fail("unterminated string");
    }

    Json number() {
        size_t start = p_;
        bool decimal = false;
        if (s_[p_] == '-') ++p_;
        if (p_ >= s_.size() || !(s_[p_] >= '0' && s_[p_] <= '9')) fail("bad number");
        while (p_ < s_.size() && s_[p_] >= '0' && s_[p_] <= '9') ++p_;
        if (p_ < s_.size() && s_[p_] == '.') {
            decimal = true;
            ++p_;
            if (p_ >= s_.size() || !(s_[p_] >= '0' && s_[p_] <= '9')) fail("bad number");
            while (p_ < s_.size() && s_[p_] >= '0' && s_[p_] <= '9') ++p_;
        }
        if (p_ < s_.size() && (s_[p_] == 'e' || s_[p_] == 'E')) {
            decimal = true;
            ++p_;
            if (p_ < s_.size() && (s_[p_] == '+' || s_[p_] == '-')) ++p_;
            if (p_ >= s_.size() || !(s_[p_] >= '0' && s_[p_] <= '9')) fail("bad number");
            while (p_ < s_.size() && s_[p_] >= '0' && s_[p_] <= '9') ++p_;
        }
        std::string num = s_.substr(start, p_ - start);
        if (!decimal) {
            errno = 0;
            char* end = nullptr;
            long long v = std::strtoll(num.c_str(), &end, 10);
            if (errno != ERANGE) return Json(v);
        }
        return Json(std::strtod(num.c_str(), nullptr));
    }
};

Json Json::parse(const std::string& text) {
    // Skip a UTF-8 byte-order mark (the desktop data files have one).
    if (text.size() >= 3 && static_cast<unsigned char>(text[0]) == 0xEF &&
        static_cast<unsigned char>(text[1]) == 0xBB && static_cast<unsigned char>(text[2]) == 0xBF)
        return JsonParser(text.substr(3)).run();
    return JsonParser(text).run();
}

}  // namespace tf
