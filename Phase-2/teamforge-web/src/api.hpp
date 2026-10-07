// The whole TeamForge API, independent of any HTTP library.
// main.cpp turns cpp-httplib requests into HttpRequest and back; tests call
// Api::handle directly. This keeps all logic testable without a network.
#pragma once
#include <functional>
#include <map>
#include <mutex>
#include <regex>
#include <string>
#include <utility>
#include <vector>

#include "db.hpp"
#include "json.hpp"

namespace tf {

struct HttpRequest {
    std::string method;        // GET, POST, PUT, PATCH, DELETE
    std::string path;          // already URL-decoded, e.g. /api/projects/req-001
    std::map<std::string, std::string> query;   // already URL-decoded
    std::string body;
    std::string content_type;
    std::string cookie;        // raw Cookie header
};

struct HttpResponse {
    int status = 200;
    std::string body;
    std::string content_type = "application/json";
    std::vector<std::pair<std::string, std::string>> headers;
};

struct ApiOptions {
    bool cookie_secure = false;           // set true behind HTTPS
    unsigned password_iterations = 600000;
};

class Api {
public:
    Api(const std::string& db_path, ApiOptions opts = {});

    HttpResponse handle(const HttpRequest& req);

    // Command-line helpers
    void seed(const std::string& data_dir, bool reset);
    void create_admin(const std::string& email, const std::string& password);

private:
    struct Ctx;
    using Handler = std::function<Json(Ctx&)>;
    struct Route {
        std::string method;
        std::regex pattern;
        std::string role;   // "" public, "any" signed in, "host", "admin"
        Handler fn;
    };

    Db db_;
    ApiOptions opts_;
    std::mutex mu_;   // one request at a time: simple and safe with one SQLite connection
    std::vector<Route> routes_;
    std::string dummy_hash_;

    void add(const std::string& method, const std::string& pattern, const std::string& role, Handler fn);
    void register_routes();
    Json current_user(const HttpRequest& req);
    Json me_json(const Json& user);
    std::string start_session(Ctx& c, long long user_id);
};

}  // namespace tf
