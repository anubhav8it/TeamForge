// Minimal JSON value: parse, build, serialize.
// Objects keep insertion order. Integers and decimals are stored separately so
// IDs and levels stay exact. Enough for this project; for bigger projects use
// nlohmann/json.
#pragma once
#include <cstdint>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace tf {

struct JsonError : std::runtime_error {
    using std::runtime_error::runtime_error;
};

class Json {
public:
    enum class Type { Null, Bool, Int, Double, String, Array, Object };

    Json() = default;
    Json(std::nullptr_t) {}
    Json(bool v) : t_(Type::Bool), b_(v) {}
    Json(int v) : t_(Type::Int), i_(v) {}
    Json(long v) : t_(Type::Int), i_(v) {}
    Json(long long v) : t_(Type::Int), i_(v) {}
    Json(unsigned v) : t_(Type::Int), i_(v) {}
    Json(size_t v) : t_(Type::Int), i_(static_cast<long long>(v)) {}
    Json(double v) : t_(Type::Double), d_(v) {}
    Json(const char* v) : t_(Type::String), s_(v) {}
    Json(std::string v) : t_(Type::String), s_(std::move(v)) {}

    static Json array() { Json j; j.t_ = Type::Array; return j; }
    static Json object() { Json j; j.t_ = Type::Object; return j; }
    static Json parse(const std::string& text);

    Type type() const { return t_; }
    bool is_null() const { return t_ == Type::Null; }
    bool is_bool() const { return t_ == Type::Bool; }
    bool is_int() const { return t_ == Type::Int; }
    bool is_number() const { return t_ == Type::Int || t_ == Type::Double; }
    bool is_string() const { return t_ == Type::String; }
    bool is_array() const { return t_ == Type::Array; }
    bool is_object() const { return t_ == Type::Object; }

    bool as_bool() const { return t_ == Type::Bool && b_; }
    long long as_int() const { return t_ == Type::Int ? i_ : t_ == Type::Double ? static_cast<long long>(d_) : 0; }
    double as_double() const { return t_ == Type::Double ? d_ : t_ == Type::Int ? static_cast<double>(i_) : 0.0; }
    const std::string& as_string() const { static const std::string empty; return t_ == Type::String ? s_ : empty; }

    // Arrays
    size_t size() const { return t_ == Type::Array ? arr_.size() : t_ == Type::Object ? keys_.size() : 0; }
    const Json& at(size_t i) const { return arr_.at(i); }
    Json& push(Json v) { if (t_ == Type::Null) t_ = Type::Array; arr_.push_back(std::move(v)); return *this; }
    // On a temporary (e.g. db.query(...).items()) return by value so a
    // range-for loop doesn't keep a reference into a destroyed object.
    const std::vector<Json>& items() const& { return arr_; }
    std::vector<Json> items() && { return std::move(arr_); }

    // Objects
    bool has(const std::string& k) const;
    const Json& operator[](const std::string& k) const&;   // null if missing
    Json operator[](const std::string& k) &&;              // copy when called on a temporary
    Json& set(const std::string& k, Json v);               // insert or replace, chainable
    const std::vector<std::string>& keys() const { return keys_; }

    std::string dump() const;

private:
    Type t_ = Type::Null;
    bool b_ = false;
    long long i_ = 0;
    double d_ = 0;
    std::string s_;
    std::vector<Json> arr_;            // array items, or object values
    std::vector<std::string> keys_;    // object keys (parallel to arr_)
    void dump_to(std::string& out) const;
    friend class JsonParser;
};

void json_escape(const std::string& s, std::string& out);

}  // namespace tf
