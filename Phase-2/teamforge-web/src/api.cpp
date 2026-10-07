#include "api.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iostream>
#include <set>
#include <sstream>

#include "crypto.hpp"
#include "matching.hpp"

namespace tf {
namespace m = tf::matching;

// ====================================================================== schema

static const char* kSchema = R"SQL(
CREATE TABLE IF NOT EXISTS students (
    id TEXT PRIMARY KEY, name TEXT NOT NULL, focus TEXT NOT NULL DEFAULT '',
    summary TEXT NOT NULL DEFAULT '', program TEXT NOT NULL DEFAULT '',
    experience TEXT NOT NULL DEFAULT '');
CREATE TABLE IF NOT EXISTS student_skills (
    student_id TEXT NOT NULL REFERENCES students(id) ON DELETE CASCADE,
    skill TEXT NOT NULL, level INTEGER NOT NULL CHECK (level BETWEEN 1 AND 5),
    PRIMARY KEY (student_id, skill));
CREATE INDEX IF NOT EXISTS idx_student_skills_skill ON student_skills(skill);
CREATE TABLE IF NOT EXISTS student_wanted (
    student_id TEXT NOT NULL REFERENCES students(id) ON DELETE CASCADE,
    skill TEXT NOT NULL, PRIMARY KEY (student_id, skill));
CREATE TABLE IF NOT EXISTS users (
    id INTEGER PRIMARY KEY AUTOINCREMENT, email TEXT NOT NULL UNIQUE,
    password_hash TEXT NOT NULL,
    role TEXT NOT NULL DEFAULT 'participant' CHECK (role IN ('participant','host','admin')),
    student_id TEXT UNIQUE REFERENCES students(id) ON DELETE SET NULL,
    created_at TEXT NOT NULL DEFAULT (datetime('now')));
CREATE TABLE IF NOT EXISTS sessions (
    token_hash TEXT PRIMARY KEY,
    user_id INTEGER NOT NULL REFERENCES users(id) ON DELETE CASCADE,
    created_at TEXT NOT NULL DEFAULT (datetime('now')));
CREATE TABLE IF NOT EXISTS skills (
    name TEXT PRIMARY KEY, category TEXT NOT NULL DEFAULT 'Uncategorised');
CREATE TABLE IF NOT EXISTS projects (
    id TEXT PRIMARY KEY, name TEXT NOT NULL UNIQUE COLLATE NOCASE, type TEXT NOT NULL,
    summary TEXT NOT NULL DEFAULT '',
    min_team_size INTEGER NOT NULL CHECK (min_team_size >= 1),
    max_team_size INTEGER NOT NULL CHECK (max_team_size >= min_team_size),
    owner_id INTEGER REFERENCES users(id) ON DELETE SET NULL);
CREATE TABLE IF NOT EXISTS project_skills (
    project_id TEXT NOT NULL REFERENCES projects(id) ON DELETE CASCADE,
    skill TEXT NOT NULL, min_level INTEGER NOT NULL CHECK (min_level BETWEEN 1 AND 5),
    PRIMARY KEY (project_id, skill));
CREATE TABLE IF NOT EXISTS interests (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    student_id TEXT NOT NULL REFERENCES students(id) ON DELETE CASCADE,
    project_id TEXT NOT NULL REFERENCES projects(id) ON DELETE CASCADE,
    status TEXT NOT NULL DEFAULT 'interested'
        CHECK (status IN ('interested','under_review','accepted','declined')),
    UNIQUE (student_id, project_id));
CREATE TABLE IF NOT EXISTS teams (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    project_id TEXT NOT NULL REFERENCES projects(id) ON DELETE CASCADE,
    name TEXT NOT NULL COLLATE NOCASE, created_at TEXT NOT NULL DEFAULT (datetime('now')),
    UNIQUE (project_id, name));
CREATE TABLE IF NOT EXISTS team_members (
    team_id INTEGER NOT NULL REFERENCES teams(id) ON DELETE CASCADE,
    student_id TEXT NOT NULL REFERENCES students(id) ON DELETE CASCADE,
    PRIMARY KEY (team_id, student_id));
)SQL";

// ====================================================================== helpers

namespace {

struct ApiError : std::runtime_error {
    int status;
    ApiError(std::string msg, int st = 400) : std::runtime_error(std::move(msg)), status(st) {}
};

const char* kCookie = "tf_session";

std::string lower(std::string s) {
    for (auto& c : s) c = char(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

std::string trim(const std::string& s) {
    size_t a = s.find_first_not_of(" \t\r\n\f\v"), b = s.find_last_not_of(" \t\r\n\f\v");
    return a == std::string::npos ? "" : s.substr(a, b - a + 1);
}

std::string placeholders(size_t n) {
    std::string s;
    for (size_t i = 0; i < n; ++i) s += i ? ",?" : "?";
    return s;
}

std::string new_id(const std::string& prefix) {
    return prefix + crypto::to_hex(crypto::random_bytes(4));
}

std::string text(const Json& d, const std::string& key, bool required = false, size_t max_len = 200) {
    const Json& v = d[key];
    if (!v.is_null() && !v.is_string()) throw ApiError("'" + key + "' must be text.");
    std::string s = trim(v.as_string());
    if (required && s.empty()) throw ApiError("'" + key + "' is required.");
    if (m::utf8_length(s) > max_len)
        throw ApiError("'" + key + "' must be at most " + std::to_string(max_len) + " characters.");
    return s;
}

int int_in(const Json& d, const std::string& key, int lo, int hi) {
    const Json& v = d[key];
    if (!v.is_int() || v.as_int() < lo || v.as_int() > hi)
        throw ApiError("'" + key + "' must be a whole number from " + std::to_string(lo) + " to " +
                       std::to_string(hi) + ".");
    return int(v.as_int());
}

const std::string kSkillLenMsg = "Skill names must be 1-" + std::to_string(m::kMaxSkillLen) + " characters.";

// [{name, level}] -> ordered list of (normalized name, level)
std::vector<std::pair<std::string, int>> skill_levels(const Json& items, const std::string& key,
                                                      size_t max_items = 30, size_t min_items = 1) {
    if (!items.is_array()) throw ApiError("'" + key + "' must be a list.");
    std::vector<std::pair<std::string, int>> out;
    for (const Json& it : items.items()) {
        if (!it.is_object()) throw ApiError("Each entry in '" + key + "' needs a name and a level.");
        std::string name = m::normalize_skill(it["name"].as_string());
        if (name.empty()) throw ApiError(kSkillLenMsg);
        const Json& lvl = it["level"];
        if (!lvl.is_int() || lvl.as_int() < 1 || lvl.as_int() > 5)
            throw ApiError("Level for '" + name + "' must be 1-5.");
        for (auto& p : out) if (p.first == name) throw ApiError("'" + name + "' is listed twice.");
        out.emplace_back(name, int(lvl.as_int()));
    }
    if (out.size() < min_items || out.size() > max_items)
        throw ApiError("Add between " + std::to_string(min_items) + " and " + std::to_string(max_items) + " skills.");
    return out;
}

Json skill_list(const m::Skills& s) {
    std::vector<std::pair<std::string, int>> v(s.begin(), s.end());
    std::stable_sort(v.begin(), v.end(), [](auto& a, auto& b) {
        return a.second != b.second ? a.second > b.second : a.first < b.first;
    });
    Json arr = Json::array();
    for (auto& [k, lvl] : v) arr.push(Json::object().set("name", k).set("level", lvl));
    return arr;
}

Json why_json(const std::vector<m::Row>& rows) {
    Json arr = Json::array();
    for (auto& r : rows)
        arr.push(Json::object().set("skill", r.skill).set("required", r.required)
                     .set("have", r.have).set("status", r.status));
    return arr;
}

int arg_int(const HttpRequest& req, const std::string& key, int dflt) {
    auto it = req.query.find(key);
    if (it == req.query.end()) return dflt;
    try {
        size_t used = 0;
        int v = std::stoi(it->second, &used);
        return used == it->second.size() ? v : dflt;
    } catch (...) { return dflt; }
}

std::string arg(const HttpRequest& req, const std::string& key) {
    auto it = req.query.find(key);
    return it == req.query.end() ? "" : it->second;
}

std::string read_file(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) throw std::runtime_error("Can't read " + path);
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

}  // namespace

// ====================================================================== context

struct Api::Ctx {
    Ctx(const HttpRequest& r, Db& d, Json u = Json(), std::smatch mm = std::smatch())
        : req(r), db(d), user(std::move(u)), match(std::move(mm)) {}
    const HttpRequest& req;
    Db& db;
    Json user;
    std::smatch match;
    int status = 200;
    std::vector<std::pair<std::string, std::string>> headers;
    Json parsed;
    bool did_parse = false;

    const Json& body() {
        if (!did_parse) {
            did_parse = true;
            try { parsed = Json::parse(req.body); } catch (const JsonError&) { parsed = Json(); }
        }
        if (!parsed.is_object()) throw ApiError("Send a JSON object.");
        return parsed;
    }
    std::string param(size_t i) const { return match[i].str(); }

    // ---- data helpers shared by many routes
    std::map<std::string, m::Skills> skills_for(const std::vector<std::string>* ids = nullptr) {
        std::string sql = "SELECT student_id, skill, level FROM student_skills";
        Args args;
        if (ids) {
            if (ids->empty()) return {};
            sql += " WHERE student_id IN (" + placeholders(ids->size()) + ")";
            for (auto& i : *ids) args.emplace_back(i);
        }
        std::map<std::string, m::Skills> out;
        for (const Json& r : db.query(sql, args).items())
            out[r["student_id"].as_string()][r["skill"].as_string()] = int(r["level"].as_int());
        return out;
    }
    m::Skills skills_of(const std::string& sid) {
        std::vector<std::string> one{sid};
        auto all = skills_for(&one);
        return all.count(sid) ? all[sid] : m::Skills{};
    }

    Json student_json(const Json& row, const m::Skills& skills, bool full = false) {
        Json d = Json::object();
        d.set("id", row["id"]).set("name", row["name"]).set("focus", row["focus"])
         .set("program", row["program"]).set("summary", row["summary"]).set("skills", skill_list(skills));
        if (full) {
            d.set("experience", row["experience"]);
            Json wanted = Json::array();
            for (const Json& r : db.query("SELECT skill FROM student_wanted WHERE student_id = ? ORDER BY skill",
                                          {row["id"]}).items())
                wanted.push(r["skill"]);
            d.set("wanted", wanted);
        }
        return d;
    }

    Json get_project(const std::string& pid) {
        Json row = db.one("SELECT * FROM projects WHERE id = ?", {pid});
        if (row.is_null()) throw ApiError("Project not found.", 404);
        return row;
    }
    m::Skills required_for(const std::string& pid) {
        m::Skills need;
        for (const Json& r : db.query("SELECT skill, min_level FROM project_skills WHERE project_id = ?", {pid}).items())
            need[r["skill"].as_string()] = int(r["min_level"].as_int());
        return need;
    }
    Json project_json(const Json& row) {
        return Json::object().set("id", row["id"]).set("name", row["name"]).set("type", row["type"])
            .set("summary", row["summary"]).set("minTeamSize", row["min_team_size"])
            .set("maxTeamSize", row["max_team_size"]).set("ownerId", row["owner_id"])
            .set("required", skill_list(required_for(row["id"].as_string())));
    }
    Json manageable(const std::string& pid) {
        Json row = get_project(pid);
        if (user["role"].as_string() != "admin" && row["owner_id"].as_int() != user["id"].as_int())
            throw ApiError("Only this project's host can do that.", 403);
        return row;
    }
    void register_new_skills(const std::vector<std::string>& names) {
        for (auto& n : names) db.run("INSERT OR IGNORE INTO skills(name) VALUES (?)", {n});
    }

    std::vector<std::string> member_ids() {
        const Json& ids = body()["members"];
        std::vector<std::string> out;
        if (ids.is_null()) return out;
        if (!ids.is_array()) throw ApiError("'members' must be a list of participant ids.");
        for (const Json& i : ids.items()) {
            if (!i.is_string()) throw ApiError("'members' must be a list of participant ids.");
            if (std::find(out.begin(), out.end(), i.as_string()) != out.end())
                throw ApiError("A participant is listed twice.");
            out.push_back(i.as_string());
        }
        if (!out.empty()) {
            Args a(out.begin(), out.end());
            std::set<std::string> found;
            for (const Json& r : db.query("SELECT id FROM students WHERE id IN (" + placeholders(out.size()) + ")", a).items())
                found.insert(r["id"].as_string());
            for (auto& i : out) if (!found.count(i)) throw ApiError("Unknown participant: " + i + ".");
        }
        return out;
    }

    Json evaluate(const Json& project, const std::vector<std::string>& ids) {
        m::Skills need = required_for(project["id"].as_string());
        auto sk = skills_for(&ids);
        std::vector<const m::Skills*> members;
        static const m::Skills empty;
        for (auto& i : ids) members.push_back(sk.count(i) ? &sk[i] : &empty);
        m::Coverage cov = m::coverage(members, need);
        long long n = static_cast<long long>(ids.size());
        long long lo = project["min_team_size"].as_int(), hi = project["max_team_size"].as_int();
        std::string msg = n < lo ? "Add " + std::to_string(lo - n) + " more to reach the minimum of " + std::to_string(lo) + "."
                        : n > hi ? "Remove " + std::to_string(n - hi) + " to fit the maximum of " + std::to_string(hi) + "."
                                 : "Team size is within limits.";
        Json missing = Json::array();
        for (auto& s : cov.missing) missing.push(s);
        Json mem = Json::array();
        if (!ids.empty()) {
            Args a(ids.begin(), ids.end());
            Json rows = db.query("SELECT * FROM students WHERE id IN (" + placeholders(ids.size()) + ")", a);
            for (auto& id : ids)
                for (const Json& r : rows.items())
                    if (r["id"].as_string() == id) {
                        const m::Skills& s = sk.count(id) ? sk[id] : empty;
                        mem.push(student_json(r, s).set("match", m::score(s, need)));
                    }
        }
        return Json::object()
            .set("coverage", Json::object().set("percent", cov.percent).set("skills", why_json(cov.rows)).set("missing", missing))
            .set("size", n).set("sizeOk", lo <= n && n <= hi).set("sizeMessage", msg).set("members", mem);
    }

    std::string my_student() {
        if (user["student_id"].is_null()) throw ApiError("Complete your profile first.", 409);
        return user["student_id"].as_string();
    }
};

// ====================================================================== core

Api::Api(const std::string& db_path, ApiOptions opts) : db_(db_path), opts_(opts) {
    db_.exec(kSchema);
    dummy_hash_ = crypto::hash_password("timing-equaliser", opts_.password_iterations);
    register_routes();
}

void Api::add(const std::string& method, const std::string& pattern, const std::string& role, Handler fn) {
    routes_.push_back({method, std::regex("^" + pattern + "$"), role, std::move(fn)});
}

Json Api::current_user(const HttpRequest& req) {
    std::string prefix = std::string(kCookie) + "=";
    size_t pos = 0;
    std::string token;
    while ((pos = req.cookie.find(prefix, pos)) != std::string::npos) {
        if (pos == 0 || req.cookie[pos - 1] == ' ' || req.cookie[pos - 1] == ';') {
            size_t start = pos + prefix.size(), end = req.cookie.find(';', start);
            token = req.cookie.substr(start, end == std::string::npos ? std::string::npos : end - start);
            break;
        }
        pos += prefix.size();
    }
    if (token.empty()) return Json();
    return db_.one("SELECT u.* FROM sessions s JOIN users u ON u.id = s.user_id "
                   "WHERE s.token_hash = ? AND s.created_at > datetime('now', '-14 days')",
                   {crypto::to_hex(crypto::sha256(token))});
}

std::string Api::start_session(Ctx& c, long long user_id) {
    // Only a hash of the token is stored, so a leaked database can't be used to log in.
    std::string token = crypto::to_hex(crypto::random_bytes(32));
    db_.run("DELETE FROM sessions WHERE created_at <= datetime('now', '-14 days')");
    db_.run("INSERT INTO sessions(token_hash, user_id) VALUES (?, ?)", {crypto::to_hex(crypto::sha256(token)), user_id});
    c.headers.emplace_back("Set-Cookie", std::string(kCookie) + "=" + token +
                           "; Path=/; HttpOnly; SameSite=Lax; Max-Age=1209600" +
                           (opts_.cookie_secure ? "; Secure" : ""));
    return token;
}

Json Api::me_json(const Json& user) {
    if (user.is_null()) return Json::object().set("user", Json()).set("profile", Json());
    Json profile;
    if (!user["student_id"].is_null()) {
        Json row = db_.one("SELECT * FROM students WHERE id = ?", {user["student_id"]});
        if (!row.is_null()) {
            HttpRequest dummy;
            Ctx c{dummy, db_, user, {}};
            profile = c.student_json(row, c.skills_of(row["id"].as_string()), true);
        }
    }
    return Json::object()
        .set("user", Json::object().set("id", user["id"]).set("email", user["email"]).set("role", user["role"]))
        .set("profile", profile);
}

HttpResponse Api::handle(const HttpRequest& req) {
    std::lock_guard<std::mutex> lock(mu_);
    HttpResponse res;
    auto error = [&](int status, const std::string& msg) {
        res.status = status;
        res.body = Json::object().set("error", msg).dump();
    };

    // Mutating calls must be JSON. Browsers can't send cross-site JSON without a
    // CORS preflight, so this plus SameSite cookies blocks CSRF.
    const std::string& mth = req.method;
    if ((mth == "POST" || mth == "PUT" || mth == "PATCH" || mth == "DELETE") &&
        lower(req.content_type).rfind("application/json", 0) != 0) {
        error(415, "Send requests as JSON.");
        return res;
    }

    bool path_matched = false;
    for (auto& r : routes_) {
        std::smatch sm;
        if (!std::regex_match(req.path, sm, r.pattern)) continue;
        path_matched = true;
        if (r.method != req.method) continue;
        Ctx c{req, db_, Json(), sm};
        try {
            if (!r.role.empty()) {
                c.user = current_user(req);
                if (c.user.is_null()) throw ApiError("Sign in first.", 401);
                std::string role = c.user["role"].as_string();
                if (r.role != "any" && role != "admin" && role != r.role)
                    throw ApiError("You don't have access to this.", 403);
            }
            Json out = r.fn(c);
            res.status = c.status;
            res.body = out.dump();
            res.headers = c.headers;
        } catch (const ApiError& e) {
            error(e.status, e.what());
        } catch (const DbConstraint& e) {
            error(409, "That conflicts with existing data.");
        } catch (const std::exception& e) {
            std::cerr << "[error] " << req.method << " " << req.path << ": " << e.what() << "\n";
            error(500, "Something went wrong on the server.");
        }
        return res;
    }
    error(path_matched ? 405 : 404, path_matched ? "Method not allowed." : "Not found.");
    return res;
}

// ====================================================================== routes

void Api::register_routes() {
    const std::string ID = "([^/]+)", NUM = "([0-9]+)";

    // ---------------------------------------------------------------- auth
    add("POST", "/api/auth/register", "", [this](Ctx& c) {
        const Json& d = c.body();
        std::string email = lower(text(d, "email", true, 120));
        static const std::regex email_re(R"(^[^@\s]+@[^@\s]+\.[^@\s]+$)");
        if (!std::regex_match(email, email_re)) throw ApiError("Enter a valid email address.");
        const Json& pw = d["password"];
        size_t len = m::utf8_length(pw.as_string());
        if (!pw.is_string() || len < 8 || len > 128) throw ApiError("Password must be 8-128 characters.");
        if (!c.db.one("SELECT 1 FROM users WHERE email = ?", {email}).is_null())
            throw ApiError("An account with this email already exists.", 409);
        c.db.run("INSERT INTO users(email, password_hash) VALUES (?, ?)",
                 {email, crypto::hash_password(pw.as_string(), opts_.password_iterations)});
        long long uid = c.db.last_id();
        start_session(c, uid);
        return me_json(c.db.one("SELECT * FROM users WHERE id = ?", {uid}));
    });

    add("POST", "/api/auth/login", "", [this](Ctx& c) {
        const Json& d = c.body();
        std::string email = lower(text(d, "email", true, 120));
        Json user = c.db.one("SELECT * FROM users WHERE email = ?", {email});
        // Hash even when the email is unknown so response time doesn't reveal which emails exist.
        bool ok = crypto::verify_password(d["password"].as_string(),
                                          user.is_null() ? dummy_hash_ : user["password_hash"].as_string());
        if (user.is_null() || !ok) throw ApiError("Email or password is incorrect.", 401);
        start_session(c, user["id"].as_int());
        return me_json(user);
    });

    add("POST", "/api/auth/logout", "", [this](Ctx& c) {
        Json user = current_user(c.req);
        if (!user.is_null()) {
            std::string prefix = std::string(kCookie) + "=";
            size_t p = c.req.cookie.find(prefix);
            if (p != std::string::npos) {
                size_t s = p + prefix.size(), e = c.req.cookie.find(';', s);
                std::string tok = c.req.cookie.substr(s, e == std::string::npos ? std::string::npos : e - s);
                c.db.run("DELETE FROM sessions WHERE token_hash = ?", {crypto::to_hex(crypto::sha256(tok))});
            }
        }
        c.headers.emplace_back("Set-Cookie", std::string(kCookie) + "=; Path=/; HttpOnly; SameSite=Lax; Max-Age=0");
        return Json::object().set("ok", true);
    });

    add("GET", "/api/me", "", [this](Ctx& c) { return me_json(current_user(c.req)); });

    // ---------------------------------------------------------------- skills autocomplete
    add("GET", "/api/skills", "any", [](Ctx& c) {
        std::string q = m::normalize_skill(arg(c.req, "q"));
        Json items = Json::array();
        if (q.empty()) return Json::object().set("items", items);
        Json rows = c.db.query(
            "SELECT name, category FROM skills WHERE instr(name, ?) > 0 "
            "ORDER BY (substr(name, 1, length(?)) = ?) DESC, length(name), name LIMIT 12", {q, q, q});
        return Json::object().set("items", rows);
    });

    // ---------------------------------------------------------------- participant
    add("PUT", "/api/profile", "any", [this](Ctx& c) {
        const Json& d = c.body();
        std::string name = text(d, "name", true, 80), program = text(d, "program", true, 80);
        std::string focus = text(d, "focus", false, 40), summary = text(d, "summary", false, 280);
        std::string experience = text(d, "experience", false, 600);
        auto skills = skill_levels(d["skills"].is_null() ? Json::array() : d["skills"], "skills");
        const Json& wraw = d["wanted"];
        if (!(wraw.is_null() || wraw.is_array()) || wraw.size() > 15) throw ApiError("Add at most 15 learning interests.");
        std::vector<std::string> wanted;
        for (const Json& w : wraw.items()) {
            std::string n = m::normalize_skill(w.as_string());
            if (n.empty()) throw ApiError(kSkillLenMsg);
            if (std::find(wanted.begin(), wanted.end(), n) == wanted.end()) wanted.push_back(n);
        }

        Transaction tx(c.db);
        Json sid = c.user["student_id"];
        if (sid.is_null()) {
            sid = new_id("u-");
            c.db.run("INSERT INTO students(id, name) VALUES (?, ?)", {sid, name});
            c.db.run("UPDATE users SET student_id = ? WHERE id = ?", {sid, c.user["id"]});
        }
        c.db.run("UPDATE students SET name=?, program=?, focus=?, summary=?, experience=? WHERE id=?",
                 {name, program, focus, summary, experience, sid});
        c.db.run("DELETE FROM student_skills WHERE student_id = ?", {sid});
        std::vector<std::string> names;
        for (auto& [k, lvl] : skills) {
            c.db.run("INSERT INTO student_skills VALUES (?, ?, ?)", {sid, k, lvl});
            names.push_back(k);
        }
        c.db.run("DELETE FROM student_wanted WHERE student_id = ?", {sid});
        for (auto& w : wanted) { c.db.run("INSERT INTO student_wanted VALUES (?, ?)", {sid, w}); names.push_back(w); }
        c.register_new_skills(names);
        tx.commit();
        return me_json(c.db.one("SELECT * FROM users WHERE id = ?", {c.user["id"]}));
    });

    add("GET", "/api/opportunities", "any", [](Ctx& c) {
        std::string sid = c.my_student();
        m::Skills mine = c.skills_of(sid);
        std::map<std::string, std::string> status;
        for (const Json& r : c.db.query("SELECT project_id, status FROM interests WHERE student_id = ?", {sid}).items())
            status[r["project_id"].as_string()] = r["status"].as_string();
        std::vector<std::pair<int, Json>> list;
        for (const Json& row : c.db.query("SELECT * FROM projects ORDER BY name").items()) {
            std::string pid = row["id"].as_string();
            m::Skills req = c.required_for(pid);
            int sc = m::score(mine, req);
            Json p = c.project_json(row);
            p.set("match", sc).set("why", why_json(m::explain(mine, req)))
             .set("interest", status.count(pid) ? Json(status[pid]) : Json());
            list.emplace_back(sc, p);
        }
        std::stable_sort(list.begin(), list.end(), [](auto& a, auto& b) { return a.first > b.first; });
        Json items = Json::array();
        for (auto& [_, p] : list) items.push(p);
        return Json::object().set("items", items);
    });

    add("POST", "/api/projects/" + ID + "/interest", "any", [](Ctx& c) {
        std::string sid = c.my_student();
        std::string pid = c.param(1);
        c.get_project(pid);
        try {
            c.db.run("INSERT INTO interests(student_id, project_id) VALUES (?, ?)", {sid, pid});
        } catch (const DbConstraint&) {
            throw ApiError("You've already shown interest in this project.", 409);
        }
        return Json::object().set("status", "interested");
    });

    add("DELETE", "/api/projects/" + ID + "/interest", "any", [](Ctx& c) {
        std::string sid = c.my_student();
        Json row = c.db.one("SELECT status FROM interests WHERE student_id=? AND project_id=?", {sid, c.param(1)});
        if (row.is_null()) throw ApiError("No interest to withdraw.", 404);
        std::string st = row["status"].as_string();
        if (st != "interested" && st != "under_review")
            throw ApiError("The host has already decided; this can't be withdrawn.", 409);
        c.db.run("DELETE FROM interests WHERE student_id=? AND project_id=?", {sid, c.param(1)});
        return Json::object().set("status", Json());
    });

    // ---------------------------------------------------------------- host: projects
    struct ProjectInput {
        std::string name, type, summary;
        int lo, hi;
        std::vector<std::pair<std::string, int>> req;
    };
    auto project_payload = [](Ctx& c, const std::string& pid) {
        const Json& d = c.body();
        ProjectInput p;
        p.name = text(d, "name", true, 80);
        p.type = text(d, "type", true, 60);
        p.summary = text(d, "summary", false, 300);
        p.lo = int_in(d, "minTeamSize", 1, 20);
        p.hi = int_in(d, "maxTeamSize", 1, 20);
        if (p.lo > p.hi) throw ApiError("Minimum team size can't be larger than the maximum.");
        p.req = skill_levels(d["required"].is_null() ? Json::array() : d["required"], "required", 20);
        if (!c.db.one("SELECT id FROM projects WHERE name = ? COLLATE NOCASE AND id != ?", {p.name, pid}).is_null())
            throw ApiError("A project with this name already exists.", 409);
        return p;
    };
    auto write_required = [](Ctx& c, const std::string& pid, const ProjectInput& p) {
        c.db.run("DELETE FROM project_skills WHERE project_id = ?", {pid});
        std::vector<std::string> names;
        for (auto& [k, lvl] : p.req) {
            c.db.run("INSERT INTO project_skills VALUES (?, ?, ?)", {pid, k, lvl});
            names.push_back(k);
        }
        c.register_new_skills(names);
    };

    add("GET", "/api/projects", "host", [](Ctx& c) {
        std::string sql =
            "SELECT p.*, (SELECT COUNT(*) FROM interests i WHERE i.project_id = p.id) AS applicants, "
            "(SELECT COUNT(*) FROM interests i WHERE i.project_id = p.id AND i.status='accepted') AS accepted, "
            "(SELECT COUNT(*) FROM teams t WHERE t.project_id = p.id) AS teams FROM projects p";
        Args args;
        if (c.user["role"].as_string() != "admin") { sql += " WHERE p.owner_id = ?"; args.push_back(c.user["id"]); }
        Json items = Json::array();
        for (const Json& r : c.db.query(sql + " ORDER BY p.name", args).items())
            items.push(c.project_json(r).set("applicants", r["applicants"]).set("accepted", r["accepted"]).set("teams", r["teams"]));
        return Json::object().set("items", items);
    });

    add("POST", "/api/projects", "host", [project_payload, write_required](Ctx& c) {
        ProjectInput p = project_payload(c, "");
        std::string pid = new_id("p-");
        Transaction tx(c.db);
        c.db.run("INSERT INTO projects VALUES (?, ?, ?, ?, ?, ?, ?)", {pid, p.name, p.type, p.summary, p.lo, p.hi, c.user["id"]});
        write_required(c, pid, p);
        tx.commit();
        c.status = 201;
        return c.project_json(c.get_project(pid));
    });

    add("GET", "/api/projects/" + ID, "host", [](Ctx& c) { return c.project_json(c.manageable(c.param(1))); });

    add("PUT", "/api/projects/" + ID, "host", [project_payload, write_required](Ctx& c) {
        std::string pid = c.param(1);
        c.manageable(pid);
        ProjectInput p = project_payload(c, pid);
        Transaction tx(c.db);
        c.db.run("UPDATE projects SET name=?, type=?, summary=?, min_team_size=?, max_team_size=? WHERE id=?",
                 {p.name, p.type, p.summary, p.lo, p.hi, pid});
        write_required(c, pid, p);
        tx.commit();
        return c.project_json(c.get_project(pid));
    });

    add("DELETE", "/api/projects/" + ID, "host", [](Ctx& c) {
        c.manageable(c.param(1));
        c.db.run("DELETE FROM projects WHERE id = ?", {c.param(1)});
        return Json::object().set("ok", true);
    });

    // ---------------------------------------------------------------- host: matching & applicants
    add("GET", "/api/projects/" + ID + "/matches", "host", [](Ctx& c) {
        std::string pid = c.param(1);
        c.manageable(pid);
        m::Skills req = c.required_for(pid);
        int limit = std::clamp(arg_int(c.req, "limit", 30), 1, 100);
        std::map<std::string, m::Skills> sk;
        if (arg(c.req, "pool") == "accepted") {
            std::vector<std::string> ids;
            for (const Json& r : c.db.query("SELECT student_id FROM interests WHERE project_id=? AND status='accepted'", {pid}).items())
                ids.push_back(r["student_id"].as_string());
            sk = c.skills_for(&ids);
        } else {
            sk = c.skills_for();
        }
        std::vector<std::pair<int, std::string>> ranked;
        for (auto& [sid, s] : sk) { int sc = m::score(s, req); if (sc > 0) ranked.emplace_back(sc, sid); }
        std::sort(ranked.begin(), ranked.end(), [](auto& a, auto& b) { return a.first != b.first ? a.first > b.first : a.second < b.second; });
        if (ranked.size() > size_t(limit)) ranked.resize(limit);
        Json items = Json::array();
        if (!ranked.empty()) {
            Args a;
            for (auto& r : ranked) a.emplace_back(r.second);
            Json rows = c.db.query("SELECT * FROM students WHERE id IN (" + placeholders(a.size()) + ")", a);
            std::map<std::string, Json> by_id;
            for (const Json& r : rows.items()) by_id[r["id"].as_string()] = r;
            for (auto& [sc, sid] : ranked) {
                m::Skills relevant;   // only the skills this project asks for
                for (auto& [k, v] : sk[sid]) if (req.count(k)) relevant[k] = v;
                items.push(c.student_json(by_id[sid], relevant).set("match", sc).set("why", why_json(m::explain(sk[sid], req))));
            }
        }
        return Json::object().set("items", items);
    });

    add("GET", "/api/projects/" + ID + "/applicants", "host", [](Ctx& c) {
        std::string pid = c.param(1);
        c.manageable(pid);
        m::Skills req = c.required_for(pid);
        Json rows = c.db.query("SELECT i.id AS interest_id, i.status, s.* FROM interests i "
                               "JOIN students s ON s.id = i.student_id WHERE i.project_id = ?", {pid});
        std::vector<std::string> ids;
        for (const Json& r : rows.items()) ids.push_back(r["id"].as_string());
        auto sk = c.skills_for(&ids);
        static const std::map<std::string, int> order{{"interested", 0}, {"under_review", 1}, {"accepted", 2}, {"declined", 3}};
        std::vector<std::tuple<int, int, Json>> list;
        for (const Json& r : rows.items()) {
            const m::Skills& s = sk[r["id"].as_string()];
            int sc = m::score(s, req);
            Json d = c.student_json(r, s).set("interestId", r["interest_id"]).set("status", r["status"])
                         .set("match", sc).set("why", why_json(m::explain(s, req)));
            list.emplace_back(order.at(r["status"].as_string()), -sc, d);
        }
        std::stable_sort(list.begin(), list.end(), [](auto& a, auto& b) {
            return std::get<0>(a) != std::get<0>(b) ? std::get<0>(a) < std::get<0>(b) : std::get<1>(a) < std::get<1>(b);
        });
        Json items = Json::array();
        for (auto& t : list) items.push(std::get<2>(t));
        return Json::object().set("items", items);
    });

    add("PATCH", "/api/interests/" + NUM, "host", [](Ctx& c) {
        Json row = c.db.one("SELECT * FROM interests WHERE id = ?", {std::stoll(c.param(1))});
        if (row.is_null()) throw ApiError("Interest not found.", 404);
        c.manageable(row["project_id"].as_string());
        std::string st = c.body()["status"].as_string();
        if (st != "under_review" && st != "accepted" && st != "declined")
            throw ApiError("Status must be under_review, accepted or declined.");
        c.db.run("UPDATE interests SET status = ? WHERE id = ?", {st, row["id"]});
        return Json::object().set("status", st);
    });

    // ---------------------------------------------------------------- host: team builder
    add("POST", "/api/projects/" + ID + "/evaluate", "host", [](Ctx& c) {
        Json p = c.manageable(c.param(1));
        return c.evaluate(p, c.member_ids());
    });

    add("POST", "/api/projects/" + ID + "/suggest", "host", [](Ctx& c) {
        std::string pid = c.param(1);
        Json p = c.manageable(pid);
        std::vector<std::string> start = c.member_ids();
        m::Skills req = c.required_for(pid);
        std::map<std::string, m::Skills> cands;
        if (c.body()["pool"].as_string() == "accepted") {
            std::vector<std::string> ids = start;
            for (const Json& r : c.db.query("SELECT student_id FROM interests WHERE project_id=? AND status='accepted'", {pid}).items())
                ids.push_back(r["student_id"].as_string());
            cands = c.skills_for(&ids);
        } else {
            cands = c.skills_for();
        }
        // People already in another saved team for this project are not suggested.
        std::set<std::string> taken;
        for (const Json& r : c.db.query("SELECT tm.student_id FROM team_members tm JOIN teams t ON t.id = tm.team_id "
                                        "WHERE t.project_id = ?", {pid}).items())
            taken.insert(r["student_id"].as_string());
        std::map<std::string, m::Skills> pool;
        for (auto& [id, s] : cands) {
            bool in_start = std::find(start.begin(), start.end(), id) != start.end();
            if (in_start || (!taken.count(id) && m::score(s, req) > 0)) pool[id] = s;
        }
        auto team = m::suggest_team(pool, req, size_t(p["max_team_size"].as_int()), start, size_t(p["min_team_size"].as_int()));
        return c.evaluate(p, team);
    });

    add("GET", "/api/projects/" + ID + "/teams", "host", [](Ctx& c) {
        Json p = c.manageable(c.param(1));
        Json items = Json::array();
        for (const Json& t : c.db.query("SELECT * FROM teams WHERE project_id = ? ORDER BY created_at, id", {p["id"]}).items()) {
            std::vector<std::string> ids;
            for (const Json& r : c.db.query("SELECT student_id FROM team_members WHERE team_id = ?", {t["id"]}).items())
                ids.push_back(r["student_id"].as_string());
            items.push(c.evaluate(p, ids).set("id", t["id"]).set("name", t["name"]));
        }
        return Json::object().set("items", items);
    });

    add("POST", "/api/projects/" + ID + "/teams", "host", [](Ctx& c) {
        std::string pid = c.param(1);
        Json p = c.manageable(pid);
        std::string name = text(c.body(), "name", true, 60);
        std::vector<std::string> ids = c.member_ids();
        long long lo = p["min_team_size"].as_int(), hi = p["max_team_size"].as_int();
        if (static_cast<long long>(ids.size()) < lo || static_cast<long long>(ids.size()) > hi)
            throw ApiError("A team needs " + std::to_string(lo) + "-" + std::to_string(hi) + " members.");
        if (!c.db.one("SELECT 1 FROM teams WHERE project_id=? AND name=? COLLATE NOCASE", {pid, name}).is_null())
            throw ApiError("This project already has a team with that name.", 409);
        Args a{pid};
        a.insert(a.end(), ids.begin(), ids.end());
        Json clash = c.db.one("SELECT s.name FROM team_members tm JOIN teams t ON t.id = tm.team_id "
                              "JOIN students s ON s.id = tm.student_id WHERE t.project_id = ? "
                              "AND tm.student_id IN (" + placeholders(ids.size()) + ")", a);
        if (!clash.is_null())
            throw ApiError(clash["name"].as_string() + " is already in another team for this project.", 409);
        Transaction tx(c.db);
        c.db.run("INSERT INTO teams(project_id, name) VALUES (?, ?)", {pid, name});
        long long tid = c.db.last_id();
        for (auto& i : ids) c.db.run("INSERT INTO team_members VALUES (?, ?)", {tid, i});
        tx.commit();
        c.status = 201;
        return c.evaluate(p, ids).set("id", tid).set("name", name);
    });

    add("DELETE", "/api/teams/" + NUM, "host", [](Ctx& c) {
        Json t = c.db.one("SELECT * FROM teams WHERE id = ?", {std::stoll(c.param(1))});
        if (t.is_null()) throw ApiError("Team not found.", 404);
        c.manageable(t["project_id"].as_string());
        c.db.run("DELETE FROM teams WHERE id = ?", {t["id"]});
        return Json::object().set("ok", true);
    });

    // ---------------------------------------------------------------- participant directory
    add("GET", "/api/students", "host", [](Ctx& c) {
        std::vector<std::string> where;
        Args args;
        std::string q = trim(arg(c.req, "q"));
        if (!q.empty()) {
            where.push_back("(instr(lower(s.name), lower(?)) > 0 OR instr(lower(s.id), lower(?)) > 0 OR "
                            "instr(lower(s.summary), lower(?)) > 0 OR instr(lower(s.focus), lower(?)) > 0)");
            for (int i = 0; i < 4; ++i) args.emplace_back(q);
        }
        std::string skill = m::normalize_skill(arg(c.req, "skill"));
        if (!skill.empty()) {
            where.push_back("EXISTS (SELECT 1 FROM student_skills x WHERE x.student_id = s.id AND x.skill = ? AND x.level >= ?)");
            args.emplace_back(skill);
            args.emplace_back(arg_int(c.req, "minLevel", 1));
        }
        int year = arg_int(c.req, "year", 0);
        if (year >= 1 && year <= 4) {
            static const char* suffix[] = {"", "st", "nd", "rd", "th"};
            where.push_back("instr(s.program, ?) > 0");
            args.emplace_back(std::to_string(year) + suffix[year] + " year");
        }
        std::string sql = "SELECT s.* FROM students s";
        for (size_t i = 0; i < where.size(); ++i) sql += (i ? " AND " : " WHERE ") + where[i];
        Json rows = c.db.query(sql, args);
        std::vector<Json> list(rows.items().begin(), rows.items().end());
        auto sk = c.skills_for();

        std::string project = arg(c.req, "project");
        bool has_req = !project.empty();
        m::Skills req = has_req ? c.required_for(project) : m::Skills{};
        std::string sort = arg(c.req, "sort");
        auto name_lc = [](const Json& r) { return lower(r["name"].as_string()); };
        auto total_level = [&](const Json& r) {
            int t = 0;
            for (auto& [_, v] : sk[r["id"].as_string()]) t += v;
            return t;
        };
        if (sort == "match" && has_req) {
            std::stable_sort(list.begin(), list.end(), [&](const Json& a, const Json& b) {
                int x = m::score(sk[a["id"].as_string()], req), y = m::score(sk[b["id"].as_string()], req);
                return x != y ? x > y : a["name"].as_string() < b["name"].as_string();
            });
        } else if (sort == "skills") {
            std::stable_sort(list.begin(), list.end(), [&](const Json& a, const Json& b) {
                int x = total_level(a), y = total_level(b);
                return x != y ? x > y : a["name"].as_string() < b["name"].as_string();
            });
        } else if (sort == "id") {
            std::stable_sort(list.begin(), list.end(), [](const Json& a, const Json& b) { return a["id"].as_string() < b["id"].as_string(); });
        } else {
            std::stable_sort(list.begin(), list.end(), [&](const Json& a, const Json& b) { return name_lc(a) < name_lc(b); });
        }

        int per = std::clamp(arg_int(c.req, "per", 25), 1, 100);
        int page = std::max(arg_int(c.req, "page", 1), 1);
        Json items = Json::array();
        size_t from = size_t(page - 1) * size_t(per);
        for (size_t i = from; i < list.size() && i < from + size_t(per); ++i) {
            const m::Skills& s = sk[list[i]["id"].as_string()];
            Json d = c.student_json(list[i], s).set("number", static_cast<long long>(i + 1));
            if (has_req) d.set("match", m::score(s, req));
            items.push(d);
        }
        return Json::object().set("total", list.size()).set("page", page).set("per", per).set("items", items);
    });

    add("GET", "/api/students/" + ID, "host", [](Ctx& c) {
        Json row = c.db.one("SELECT * FROM students WHERE id = ?", {c.param(1)});
        if (row.is_null()) throw ApiError("Participant not found.", 404);
        Json d = c.student_json(row, c.skills_of(c.param(1)), true);
        d.set("interests", c.db.query("SELECT p.id, p.name, i.status FROM interests i JOIN projects p ON p.id = i.project_id "
                                      "WHERE i.student_id = ? ORDER BY p.name", {c.param(1)}));
        return d;
    });

    // ---------------------------------------------------------------- admin
    add("GET", "/api/admin/stats", "admin", [](Ctx& c) {
        auto n = [&](const char* t) { return c.db.scalar(std::string("SELECT COUNT(*) FROM ") + t); };
        return Json::object().set("participants", n("students")).set("projects", n("projects"))
            .set("skills", n("skills")).set("users", n("users")).set("interests", n("interests")).set("teams", n("teams"));
    });

    add("GET", "/api/admin/users", "admin", [](Ctx& c) {
        return Json::object().set("items", c.db.query(
            "SELECT u.id, u.email, u.role, u.created_at, s.name FROM users u "
            "LEFT JOIN students s ON s.id = u.student_id ORDER BY u.id"));
    });

    add("PATCH", "/api/admin/users/" + NUM, "admin", [](Ctx& c) {
        std::string role = c.body()["role"].as_string();
        if (role != "participant" && role != "host" && role != "admin")
            throw ApiError("Role must be participant, host or admin.");
        long long uid = std::stoll(c.param(1));
        if (uid == c.user["id"].as_int() && role != "admin") throw ApiError("You can't remove your own admin access.", 409);
        if (c.db.run("UPDATE users SET role = ? WHERE id = ?", {role, uid}) == 0) throw ApiError("User not found.", 404);
        return Json::object().set("ok", true).set("role", role);
    });

    add("GET", "/api/admin/skills", "admin", [](Ctx& c) {
        Json rows = c.db.query(
            "SELECT k.name, k.category, "
            "(SELECT COUNT(*) FROM student_skills x WHERE x.skill = k.name) AS people, "
            "(SELECT COUNT(*) FROM project_skills x WHERE x.skill = k.name) AS projects "
            "FROM skills k ORDER BY k.name");
        Json cats = Json::array();
        for (const Json& r : c.db.query("SELECT DISTINCT category FROM skills ORDER BY category").items()) cats.push(r["category"]);
        return Json::object().set("items", rows).set("categories", cats);
    });

    add("POST", "/api/admin/skills", "admin", [](Ctx& c) {
        const Json& d = c.body();
        std::string name = m::normalize_skill(d["name"].as_string());
        if (name.empty()) throw ApiError(kSkillLenMsg);
        std::string cat = text(d, "category", false, 40);
        if (cat.empty()) cat = "Uncategorised";
        try {
            c.db.run("INSERT INTO skills VALUES (?, ?)", {name, cat});
        } catch (const DbConstraint&) {
            throw ApiError("This skill already exists.", 409);
        }
        c.status = 201;
        return Json::object().set("name", name).set("category", cat);
    });

    add("PATCH", "/api/admin/skills/(.+)", "admin", [](Ctx& c) {
        const Json& d = c.body();
        std::string old = m::normalize_skill(c.param(1));
        if (c.db.one("SELECT 1 FROM skills WHERE name = ?", {old}).is_null()) throw ApiError("Skill not found.", 404);
        Transaction tx(c.db);
        if (d.has("category")) c.db.run("UPDATE skills SET category = ? WHERE name = ?", {text(d, "category", true, 40), old});
        std::string nn = old;
        if (d["newName"].is_string() && !d["newName"].as_string().empty()) {
            nn = m::normalize_skill(d["newName"].as_string());
            if (nn.empty()) throw ApiError(kSkillLenMsg);
        }
        if (nn != old) {
            bool merge = !c.db.one("SELECT 1 FROM skills WHERE name = ?", {nn}).is_null();
            // If a person/project already has the new name (merging "js" into
            // "javascript"), keep the higher level.
            struct T { const char* table; const char* key; const char* col; };
            for (T t : {T{"student_skills", "student_id", "level"}, T{"project_skills", "project_id", "min_level"}}) {
                std::string tb = t.table, k = t.key, col = t.col;
                for (const Json& r : c.db.query("SELECT " + k + " AS k, " + col + " AS lvl FROM " + tb + " WHERE skill = ?", {old}).items()) {
                    Json both = c.db.one("SELECT " + col + " AS lvl FROM " + tb + " WHERE " + k + "=? AND skill=?", {r["k"], nn});
                    if (!both.is_null()) {
                        c.db.run("UPDATE " + tb + " SET " + col + "=? WHERE " + k + "=? AND skill=?",
                                 {std::max(both["lvl"].as_int(), r["lvl"].as_int()), r["k"], nn});
                        c.db.run("DELETE FROM " + tb + " WHERE " + k + "=? AND skill=?", {r["k"], old});
                    }
                }
                c.db.run("UPDATE " + tb + " SET skill = ? WHERE skill = ?", {nn, old});
            }
            c.db.run("DELETE FROM student_wanted WHERE skill = ? AND student_id IN "
                     "(SELECT student_id FROM student_wanted WHERE skill = ?)", {old, nn});
            c.db.run("UPDATE student_wanted SET skill = ? WHERE skill = ?", {nn, old});
            if (merge) c.db.run("DELETE FROM skills WHERE name = ?", {old});
            else c.db.run("UPDATE skills SET name = ? WHERE name = ?", {nn, old});
        }
        tx.commit();
        return Json::object().set("ok", true).set("name", nn);
    });

    add("DELETE", "/api/admin/skills/(.+)", "admin", [](Ctx& c) {
        std::string n = m::normalize_skill(c.param(1));
        long long used = c.db.scalar("SELECT (SELECT COUNT(*) FROM student_skills WHERE skill = ?) + "
                                     "(SELECT COUNT(*) FROM project_skills WHERE skill = ?)", {n, n});
        if (used) throw ApiError("'" + n + "' is used " + std::to_string(used) + " times. Rename or merge it instead.", 409);
        if (c.db.run("DELETE FROM skills WHERE name = ?", {n}) == 0) throw ApiError("Skill not found.", 404);
        return Json::object().set("ok", true);
    });

    add("GET", "/api/admin/validate", "admin", [](Ctx& c) {
        const std::vector<std::pair<const char*, const char*>> checks = {
            {"Participants with no skills",
             "SELECT COUNT(*) FROM students s WHERE NOT EXISTS (SELECT 1 FROM student_skills x WHERE x.student_id = s.id)"},
            {"Projects with no required skills",
             "SELECT COUNT(*) FROM projects p WHERE NOT EXISTS (SELECT 1 FROM project_skills x WHERE x.project_id = p.id)"},
            {"Skills used but missing from catalogue",
             "SELECT COUNT(DISTINCT skill) FROM (SELECT skill FROM student_skills UNION SELECT skill FROM project_skills) "
             "WHERE skill NOT IN (SELECT name FROM skills)"},
            {"Saved teams outside size limits",
             "SELECT COUNT(*) FROM teams t JOIN projects p ON p.id = t.project_id WHERE "
             "(SELECT COUNT(*) FROM team_members m WHERE m.team_id = t.id) NOT BETWEEN p.min_team_size AND p.max_team_size"},
        };
        Json items = Json::array();
        bool ok = true;
        for (auto& [name, sql] : checks) {
            long long n = c.db.scalar(sql);
            ok = ok && n == 0;
            items.push(Json::object().set("check", name).set("problems", n));
        }
        long long fk = static_cast<long long>(c.db.query("PRAGMA foreign_key_check").size());
        ok = ok && fk == 0;
        items.push(Json::object().set("check", "Broken references").set("problems", fk));
        return Json::object().set("items", items).set("ok", ok);
    });
}

// ====================================================================== CLI helpers

void Api::seed(const std::string& data_dir, bool reset) {
    std::lock_guard<std::mutex> lock(mu_);
    auto load = [&](const std::string& file, const char* key) {
        return Json::parse(read_file(data_dir + "/" + file))[key];
    };
    auto level = [](const Json& v) {
        if (!v.is_int() || v.as_int() < 1 || v.as_int() > 5) throw std::runtime_error("skill level out of range 1-5");
        return v.as_int();
    };
    Json skills = load("skills.json", "skills"), students = load("students.json", "students");
    Json projects = load("requirements.json", "requirements"), interests = load("interest_requests.json", "requests");

    Transaction tx(db_);
    if (reset) {
        for (const char* t : {"team_members", "teams", "interests", "project_skills", "projects",
                              "student_wanted", "student_skills", "skills"})
            db_.run(std::string("DELETE FROM ") + t);
        db_.run("UPDATE users SET student_id = NULL");
        db_.run("DELETE FROM students");
    }
    for (const Json& s : skills.items()) {
        std::string n = m::normalize_skill(s["name"].as_string());
        std::string cat = s["category"].as_string().empty() ? "Uncategorised" : s["category"].as_string();
        if (!n.empty()) db_.run("INSERT OR IGNORE INTO skills(name, category) VALUES (?, ?)", {n, cat});
    }
    for (const Json& st : students.items()) {
        db_.run("INSERT OR IGNORE INTO students(id, name, focus, summary, program) VALUES (?, ?, ?, ?, ?)",
                {st["id"], st["name"], st["role"].as_string(), st["summary"].as_string(), st["program"].as_string()});
        const Json& off = st["skillsOffered"];
        for (const auto& k : off.keys()) {
            std::string n = m::normalize_skill(k);
            if (!n.empty()) db_.run("INSERT OR IGNORE INTO student_skills VALUES (?, ?, ?)", {st["id"], n, level(off[k])});
        }
        for (const Json& w : st["skillsWanted"].items()) {
            std::string n = m::normalize_skill(w.as_string());
            if (!n.empty()) db_.run("INSERT OR IGNORE INTO student_wanted VALUES (?, ?)", {st["id"], n});
        }
    }
    for (const Json& r : projects.items()) {
        db_.run("INSERT OR IGNORE INTO projects(id, name, type, summary, min_team_size, max_team_size) VALUES (?, ?, ?, ?, ?, ?)",
                {r["id"], r["name"], r["type"], r["summary"].as_string(), r["minTeamSize"], r["maxTeamSize"]});
        const Json& req = r["requiredSkills"];
        for (const auto& k : req.keys()) {
            std::string n = m::normalize_skill(k);
            if (!n.empty()) db_.run("INSERT OR IGNORE INTO project_skills VALUES (?, ?, ?)", {r["id"], n, level(req[k])});
        }
    }
    for (const Json& i : interests.items())
        db_.run("INSERT OR IGNORE INTO interests(student_id, project_id, status) SELECT ?, ?, ? "
                "WHERE EXISTS (SELECT 1 FROM students WHERE id = ?) AND EXISTS (SELECT 1 FROM projects WHERE id = ?)",
                {i["studentId"], i["requirementId"], i["status"], i["studentId"], i["requirementId"]});
    tx.commit();
}

void Api::create_admin(const std::string& raw_email, const std::string& password) {
    std::lock_guard<std::mutex> lock(mu_);
    std::string email = lower(trim(raw_email));
    if (!db_.one("SELECT 1 FROM users WHERE email = ?", {email}).is_null()) {
        db_.run("UPDATE users SET role = 'admin' WHERE email = ?", {email});
        return;
    }
    if (m::utf8_length(password) < 8) throw std::runtime_error("Password must be at least 8 characters.");
    db_.run("INSERT INTO users(email, password_hash, role) VALUES (?, ?, 'admin')",
            {email, crypto::hash_password(password, opts_.password_iterations)});
}

}  // namespace tf
