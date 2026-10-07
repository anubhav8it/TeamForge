#include "db.hpp"

#include <sqlite3.h>

namespace tf {
namespace {

struct Stmt {
    sqlite3_stmt* s = nullptr;
    ~Stmt() { if (s) sqlite3_finalize(s); }
};

[[noreturn]] void raise(sqlite3* h, int rc) {
    std::string msg = sqlite3_errmsg(h);
    if ((rc & 0xff) == SQLITE_CONSTRAINT) throw DbConstraint(msg);
    throw DbError(msg);
}

void bind(sqlite3* h, sqlite3_stmt* s, const Args& args) {
    for (size_t i = 0; i < args.size(); ++i) {
        int idx = int(i + 1), rc = SQLITE_OK;
        const Json& a = args[i];
        switch (a.type()) {
            case Json::Type::Null: rc = sqlite3_bind_null(s, idx); break;
            case Json::Type::Bool: rc = sqlite3_bind_int64(s, idx, a.as_bool() ? 1 : 0); break;
            case Json::Type::Int: rc = sqlite3_bind_int64(s, idx, a.as_int()); break;
            case Json::Type::Double: rc = sqlite3_bind_double(s, idx, a.as_double()); break;
            case Json::Type::String:
                rc = sqlite3_bind_text(s, idx, a.as_string().data(), int(a.as_string().size()), SQLITE_TRANSIENT);
                break;
            default: throw DbError("cannot bind an array or object");
        }
        if (rc != SQLITE_OK) raise(h, rc);
    }
}

}  // namespace

Db::Db(const std::string& path) {
    if (sqlite3_open(path.c_str(), &h_) != SQLITE_OK) {
        std::string msg = h_ ? sqlite3_errmsg(h_) : "out of memory";
        if (h_) sqlite3_close(h_);
        throw DbError("Can't open database " + path + ": " + msg);
    }
    sqlite3_busy_timeout(h_, 5000);
    exec("PRAGMA foreign_keys = ON");
}

Db::~Db() { sqlite3_close(h_); }

void Db::exec(const std::string& sql) {
    char* err = nullptr;
    int rc = sqlite3_exec(h_, sql.c_str(), nullptr, nullptr, &err);
    if (rc != SQLITE_OK) {
        std::string msg = err ? err : "unknown error";
        sqlite3_free(err);
        if ((rc & 0xff) == SQLITE_CONSTRAINT) throw DbConstraint(msg);
        throw DbError(msg);
    }
}

Json Db::query(const std::string& sql, const Args& args) {
    Stmt st;
    int rc = sqlite3_prepare_v2(h_, sql.c_str(), int(sql.size()), &st.s, nullptr);
    if (rc != SQLITE_OK) raise(h_, rc);
    bind(h_, st.s, args);
    Json rows = Json::array();
    int cols = sqlite3_column_count(st.s);
    while ((rc = sqlite3_step(st.s)) == SQLITE_ROW) {
        Json row = Json::object();
        for (int c = 0; c < cols; ++c) {
            const char* name = sqlite3_column_name(st.s, c);
            switch (sqlite3_column_type(st.s, c)) {
                case SQLITE_INTEGER: row.set(name, Json(static_cast<long long>(sqlite3_column_int64(st.s, c)))); break;
                case SQLITE_FLOAT: row.set(name, Json(sqlite3_column_double(st.s, c))); break;
                case SQLITE_NULL: row.set(name, Json()); break;
                default: {
                    const unsigned char* t = sqlite3_column_text(st.s, c);
                    int n = sqlite3_column_bytes(st.s, c);
                    row.set(name, Json(std::string(reinterpret_cast<const char*>(t), size_t(n))));
                }
            }
        }
        rows.push(std::move(row));
    }
    if (rc != SQLITE_DONE) raise(h_, rc);
    return rows;
}

Json Db::one(const std::string& sql, const Args& args) {
    Json rows = query(sql, args);
    return rows.size() ? rows.at(0) : Json();
}

long long Db::scalar(const std::string& sql, const Args& args) {
    Json row = one(sql, args);
    if (row.is_null() || row.keys().empty()) return 0;
    return row[row.keys()[0]].as_int();
}

long long Db::run(const std::string& sql, const Args& args) {
    query(sql, args);
    return sqlite3_changes(h_);
}

long long Db::last_id() const { return sqlite3_last_insert_rowid(h_); }

}  // namespace tf
