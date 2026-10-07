// Command line shared by the real server (main.cpp) and the sandbox test server.
//   teamforge seed [--reset]
//   teamforge create-admin EMAIL [--password PW]
//   teamforge serve [--host 0.0.0.0] [--port 8080]
// Common options: --db FILE (default teamforge.db or $DATABASE), --data DIR, --static DIR
#pragma once
#include <cstdlib>
#include <functional>
#include <iostream>
#include <string>

#include "api.hpp"

namespace tf {

struct ServeConfig {
    std::string host = "127.0.0.1";
    int port = 8080;
    std::string static_dir = "static";
};

using ServeFn = std::function<int(Api&, const ServeConfig&)>;

inline std::string env_or(const char* name, const std::string& fallback) {
    const char* v = std::getenv(name);
    return v && *v ? v : fallback;
}

inline int run_cli(int argc, char** argv, const ServeFn& serve) {
    if (argc < 2) {
        std::cerr << "Usage:\n"
                     "  teamforge seed [--reset]\n"
                     "  teamforge create-admin EMAIL [--password PW]\n"
                     "  teamforge serve [--host 0.0.0.0] [--port 8080]\n"
                     "Options: --db FILE  --data DIR  --static DIR\n"
                     "Environment: DATABASE, PORT, COOKIE_SECURE=1\n";
        return 2;
    }
    std::string cmd = argv[1], db = env_or("DATABASE", "teamforge.db"), data = "data", email, password;
    bool reset = false;
    ServeConfig cfg;
    cfg.port = std::atoi(env_or("PORT", "8080").c_str());
    for (int i = 2; i < argc; ++i) {
        std::string a = argv[i];
        auto next = [&]() -> std::string {
            if (i + 1 >= argc) { std::cerr << a << " needs a value\n"; std::exit(2); }
            return argv[++i];
        };
        if (a == "--db") db = next();
        else if (a == "--data") data = next();
        else if (a == "--static") cfg.static_dir = next();
        else if (a == "--host") cfg.host = next();
        else if (a == "--port") cfg.port = std::atoi(next().c_str());
        else if (a == "--password") password = next();
        else if (a == "--reset") reset = true;
        else if (email.empty() && a.rfind("--", 0) != 0) email = a;
        else { std::cerr << "Unknown option " << a << "\n"; return 2; }
    }

    ApiOptions opts;
    opts.cookie_secure = env_or("COOKIE_SECURE", "0") == "1";
    // Only for automated tests: fewer hash rounds so tests run fast.
    if (std::getenv("TF_TEST_FAST_HASH")) opts.password_iterations = 1000;

    try {
        Api api(db, opts);
        if (cmd == "seed") {
            api.seed(data, reset);
            std::cout << "Sample data loaded into " << db << "\n";
            return 0;
        }
        if (cmd == "create-admin") {
            if (email.empty()) { std::cerr << "Give an email: teamforge create-admin you@example.com\n"; return 2; }
            if (password.empty()) {
                std::cout << "Password for " << email << " (8+ characters; it will be visible as you type): ";
                std::getline(std::cin, password);
            }
            api.create_admin(email, password);
            std::cout << email << " is now an admin.\n";
            return 0;
        }
        if (cmd == "serve") return serve(api, cfg);
        std::cerr << "Unknown command " << cmd << "\n";
        return 2;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
}

}  // namespace tf
