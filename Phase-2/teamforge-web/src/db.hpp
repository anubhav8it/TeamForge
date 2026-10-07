// Thin RAII wrapper over the SQLite C API. Query results come back as Json
// (arrays of objects keyed by column name), which keeps the API code short.
#pragma once
#include <stdexcept>
#include <string>
#include <vector>

#include "json.hpp"

struct sqlite3;

namespace tf {

struct DbError : std::runtime_error {
    using std::runtime_error::runtime_error;
};
// Thrown for UNIQUE / CHECK / FOREIGN KEY violations.
struct DbConstraint : DbError {
    using DbError::DbError;
};

using Args = std::vector<Json>;

class Db {
public:
    explicit Db(const std::string& path);
    ~Db();
    Db(const Db&) = delete;
    Db& operator=(const Db&) = delete;

    void exec(const std::string& sql);                       // many statements, no params
    Json query(const std::string& sql, const Args& args = {});   // array of row objects
    Json one(const std::string& sql, const Args& args = {});     // first row or null
    long long scalar(const std::string& sql, const Args& args = {});  // first column of first row
    long long run(const std::string& sql, const Args& args = {});     // returns rows changed
    long long last_id() const;

private:
    sqlite3* h_ = nullptr;
};

// Commits on success, rolls back if an exception leaves the scope.
class Transaction {
public:
    explicit Transaction(Db& db) : db_(db) { db_.exec("BEGIN"); }
    ~Transaction() { if (!done_) try { db_.exec("ROLLBACK"); } catch (...) {} }
    void commit() { db_.exec("COMMIT"); done_ = true; }
private:
    Db& db_;
    bool done_ = false;
};

}  // namespace tf
