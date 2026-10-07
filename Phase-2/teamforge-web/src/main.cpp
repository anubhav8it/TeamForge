// TeamForge server: connects cpp-httplib to the Api class.
// This is the only file that depends on cpp-httplib.
#include <fstream>
#include <iostream>
#include <sstream>

#include "httplib.h"

#include "api.hpp"
#include "cli.hpp"

namespace {

int serve(tf::Api& api, const tf::ServeConfig& cfg) {
    httplib::Server svr;

    if (!svr.set_mount_point("/static", cfg.static_dir)) {
        std::cerr << "Static folder not found: " << cfg.static_dir << " (use --static DIR)\n";
        return 1;
    }

    svr.Get("/", [&](const httplib::Request&, httplib::Response& res) {
        std::ifstream f(cfg.static_dir + "/index.html", std::ios::binary);
        std::ostringstream ss;
        ss << f.rdbuf();
        res.set_content(ss.str(), "text/html; charset=utf-8");
    });

    auto to_api = [&](const httplib::Request& r, httplib::Response& res) {
        tf::HttpRequest q;
        q.method = r.method;
        q.path = r.path;   // httplib has already URL-decoded the path and query
        for (const auto& [k, v] : r.params) q.query[k] = v;
        q.body = r.body;
        q.content_type = r.get_header_value("Content-Type");
        q.cookie = r.get_header_value("Cookie");

        tf::HttpResponse out = api.handle(q);
        res.status = out.status;
        for (const auto& [k, v] : out.headers) res.set_header(k, v);
        res.set_header("Cache-Control", "no-store");
        res.set_header("X-Content-Type-Options", "nosniff");
        res.set_content(out.body, out.content_type);
    };
    const char* api_paths = R"(/api/.*)";
    svr.Get(api_paths, to_api);
    svr.Post(api_paths, to_api);
    svr.Put(api_paths, to_api);
    svr.Patch(api_paths, to_api);
    svr.Delete(api_paths, to_api);

    std::cout << "TeamForge running at http://" << (cfg.host == "0.0.0.0" ? "localhost" : cfg.host)
              << ":" << cfg.port << "  (Ctrl+C to stop)\n";
    if (!svr.listen(cfg.host, cfg.port)) {
        std::cerr << "Could not listen on " << cfg.host << ":" << cfg.port << " (port in use?)\n";
        return 1;
    }
    return 0;
}

}  // namespace

int main(int argc, char** argv) { return tf::run_cli(argc, argv, serve); }
