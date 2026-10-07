// API tests: call Api::handle directly, no network needed.
// Run from the project folder (needs data/*.json).
#include <cstdio>
#include <iostream>
#include <string>

#include "../src/api.hpp"

using namespace tf;

static int failures = 0;
#define CHECK(cond)                                                                       \
    do {                                                                                  \
        if (!(cond)) { ++failures; std::cerr << "FAIL line " << __LINE__ << ": " #cond "\n"; } \
    } while (0)

// A tiny "browser": keeps the session cookie between calls.
struct Client {
    explicit Client(Api& a) : api(a) {}
    Api& api;
    std::string cookie;
    int status = 0;

    Json call(const std::string& method, const std::string& path_and_query, const Json& body = Json(),
              const std::string& content_type = "application/json") {
        HttpRequest r;
        r.method = method;
        size_t q = path_and_query.find('?');
        r.path = path_and_query.substr(0, q);
        if (q != std::string::npos) {
            std::string qs = path_and_query.substr(q + 1);
            size_t start = 0;
            while (start <= qs.size()) {
                size_t amp = qs.find('&', start);
                std::string kv = qs.substr(start, amp == std::string::npos ? std::string::npos : amp - start);
                size_t eq = kv.find('=');
                if (!kv.empty()) r.query[kv.substr(0, eq)] = eq == std::string::npos ? "" : kv.substr(eq + 1);
                if (amp == std::string::npos) break;
                start = amp + 1;
            }
        }
        if (method != "GET") { r.content_type = content_type; r.body = body.is_null() ? "{}" : body.dump(); }
        r.cookie = cookie;
        HttpResponse res = api.handle(r);
        status = res.status;
        for (auto& [k, v] : res.headers)
            if (k == "Set-Cookie") cookie = v.substr(0, v.find(';'));
        return Json::parse(res.body);
    }
    Json get(const std::string& p) { return call("GET", p); }
    Json post(const std::string& p, const Json& b = Json()) { return call("POST", p, b); }
};

static Json skills(std::initializer_list<std::pair<const char*, int>> list) {
    Json a = Json::array();
    for (auto& [n, l] : list) a.push(Json::object().set("name", n).set("level", l));
    return a;
}

int main() {
    const char* db_path = "test_api.db";
    std::remove(db_path);
    {
        ApiOptions opts;
        opts.password_iterations = 1000;   // fast for tests
        Api api(db_path, opts);
        api.seed("data", false);
        api.create_admin("admin@test.io", "adminpass1");

        Client admin{api};
        admin.post("/api/auth/login", Json::object().set("email", "admin@test.io").set("password", "adminpass1"));
        CHECK(admin.status == 200);
        Client bad{api};
        bad.post("/api/auth/login", Json::object().set("email", "admin@test.io").set("password", "wrong"));
        CHECK(bad.status == 401);

        // seed + data checks
        Json st = admin.get("/api/admin/stats");
        CHECK(st["participants"].as_int() == 560 && st["projects"].as_int() == 8);
        CHECK(admin.get("/api/admin/validate")["ok"].as_bool());

        // participant flow
        Client p{api};
        Json reg = p.post("/api/auth/register", Json::object().set("email", "P@Test.io").set("password", "password1"));
        CHECK(p.status == 200 && reg["user"]["email"].as_string() == "p@test.io");
        p.get("/api/opportunities");
        CHECK(p.status == 409);   // no profile yet
        Json prof = Json::object().set("name", "Test Student").set("program", "B.Tech CSE · 2nd year")
                        .set("skills", skills({{"Python", 3}, {"Brand New Skill", 2}}))
                        .set("wanted", Json::array().push("sql"));
        p.call("PUT", "/api/profile", prof);
        CHECK(p.status == 200);
        Json found = p.get("/api/skills?q=brand");
        bool has_new = false;
        for (const Json& it : found["items"].items()) has_new = has_new || it["name"].as_string() == "brand new skill";
        CHECK(has_new);
        Json opps = p.get("/api/opportunities")["items"];
        CHECK(opps.size() == 8);
        p.post("/api/projects/req-002/interest");
        CHECK(p.status == 200);
        p.post("/api/projects/req-002/interest");
        CHECK(p.status == 409);

        // permissions
        Client h{api};
        Json hreg = h.post("/api/auth/register", Json::object().set("email", "h@test.io").set("password", "password1"));
        admin.call("PATCH", "/api/admin/users/" + std::to_string(hreg["user"]["id"].as_int()),
                   Json::object().set("role", "host"));
        CHECK(admin.status == 200);
        p.get("/api/students");          CHECK(p.status == 403);
        p.get("/api/admin/stats");       CHECK(p.status == 403);
        h.get("/api/admin/stats");       CHECK(h.status == 403);
        h.get("/api/projects/req-001");  CHECK(h.status == 403);   // not their project
        Client anon{api};
        anon.get("/api/students");       CHECK(anon.status == 401);

        // host project + team
        Json proj = Json::object().set("name", "Test Project").set("type", "Hackathon")
                        .set("minTeamSize", 2).set("maxTeamSize", 3)
                        .set("required", skills({{"python", 3}, {"sql", 3}}));
        Json created = h.post("/api/projects", proj);
        CHECK(h.status == 201);
        std::string pid = created["id"].as_string();
        h.post("/api/projects", proj);
        CHECK(h.status == 409);   // duplicate name
        Json bad_size = Json::parse(proj.dump()).set("name", "Other").set("minTeamSize", 4);
        h.post("/api/projects", bad_size);
        CHECK(h.status == 400);

        Json top = h.get("/api/projects/" + pid + "/matches?limit=5")["items"];
        CHECK(top.size() == 5);
        for (size_t i = 1; i < top.size(); ++i) CHECK(top.at(i - 1)["match"].as_int() >= top.at(i)["match"].as_int());

        Json sug = h.post("/api/projects/" + pid + "/suggest", Json::object().set("members", Json::array()));
        CHECK(sug["size"].as_int() >= 2 && sug["size"].as_int() <= 3);
        CHECK(sug["coverage"]["percent"].as_int() == 100);
        Json ids = Json::array(), one = Json::array();
        for (const Json& mbr : sug["members"].items()) ids.push(mbr["id"]);
        one.push(ids.at(0));
        h.post("/api/projects/" + pid + "/teams", Json::object().set("name", "A").set("members", one));
        CHECK(h.status == 400);   // too small
        h.post("/api/projects/" + pid + "/teams", Json::object().set("name", "A").set("members", ids));
        CHECK(h.status == 201);
        h.post("/api/projects/" + pid + "/teams", Json::object().set("name", "a").set("members", ids));
        CHECK(h.status == 409);   // duplicate name (case-insensitive)
        h.post("/api/projects/" + pid + "/teams", Json::object().set("name", "B").set("members", ids));
        CHECK(h.status == 409);   // members already in team A
        CHECK(h.get("/api/projects/" + pid + "/teams")["items"].size() == 1);

        // directory
        Json dir = admin.get("/api/students?q=anubhav");
        CHECK(dir["total"].as_int() >= 1);
        Json by_skill = admin.get("/api/students?skill=python&minLevel=4&per=5&project=req-002&sort=match");
        CHECK(by_skill["items"].size() == 5 && by_skill["items"].at(0).has("match"));

        // CSRF guard
        admin.call("POST", "/api/admin/skills", Json::object().set("name", "x"), "application/x-www-form-urlencoded");
        CHECK(admin.status == 415);

        // skill merge keeps the higher level
        admin.post("/api/admin/skills", Json::object().set("name", "py"));
        CHECK(admin.status == 201);
        p.call("PUT", "/api/profile", Json::parse(prof.dump()).set("skills", skills({{"py", 4}, {"python", 2}})));
        admin.call("PATCH", "/api/admin/skills/py", Json::object().set("newName", "python"));
        CHECK(admin.status == 200);
        Json mine = p.get("/api/me")["profile"]["skills"];
        CHECK(mine.size() == 1 && mine.at(0)["name"].as_string() == "python" && mine.at(0)["level"].as_int() == 4);

        // bad input never crashes the server
        p.call("PUT", "/api/profile", Json(), "application/json");
        CHECK(p.status == 400);
        HttpRequest garbage;
        garbage.method = "POST"; garbage.path = "/api/auth/login"; garbage.content_type = "application/json";
        garbage.body = "{\"email\": [[[[";
        CHECK(api.handle(garbage).status == 400);
        p.get("/api/nope");                CHECK(p.status == 404);

        // logout ends the session
        p.post("/api/auth/logout");
        CHECK(p.get("/api/me")["user"].is_null());
    }
    std::remove(db_path);
    if (failures) { std::cerr << failures << " check(s) failed\n"; return 1; }
    std::cout << "All API tests passed\n";
    return 0;
}
