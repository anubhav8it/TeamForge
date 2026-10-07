// Unit tests for the parts with no external dependencies.
// Build target: teamforge_tests  (ctest runs it)
#include <algorithm>
#include <iostream>
#include <string>

#include "../src/crypto.hpp"
#include "../src/json.hpp"
#include "../src/matching.hpp"

static int failures = 0;
#define CHECK(cond)                                                                 \
    do {                                                                            \
        if (!(cond)) { ++failures; std::cerr << "FAIL " << __LINE__ << ": " #cond "\n"; } \
    } while (0)

using namespace tf;
namespace m = tf::matching;

static void json_tests() {
    Json j = Json::parse(R"({"a":1,"b":[true,null,2.5],"c":"x\"y","d":{"e":-3}})");
    CHECK(j["a"].as_int() == 1);
    CHECK(j["b"].size() == 3 && j["b"].at(0).as_bool() && j["b"].at(1).is_null());
    CHECK(j["b"].at(2).as_double() == 2.5);
    CHECK(j["c"].as_string() == "x\"y");
    CHECK(j["d"]["e"].as_int() == -3);
    CHECK(j["missing"].is_null());
    CHECK(Json::parse(j.dump()).dump() == j.dump());                       // round trip
    CHECK(Json::parse("\"\\u00b7\"").as_string() == "\xC2\xB7");          // middle dot
    CHECK(Json::parse("\"\\ud83d\\ude00\"").as_string() == "\xF0\x9F\x98\x80");  // emoji pair
    CHECK(Json::parse("\xEF\xBB\xBF{\"k\":1}")["k"].as_int() == 1);       // BOM skipped
    bool threw = false;
    try { Json::parse(std::string(500, '[')); } catch (const JsonError&) { threw = true; }
    CHECK(threw);                                                           // depth limit
    threw = false;
    try { Json::parse("{\"a\":1,}"); } catch (const JsonError&) { threw = true; }
    CHECK(threw);
    CHECK(Json("a\nb").dump() == "\"a\\nb\"");
}

static void crypto_tests() {
    CHECK(crypto::to_hex(crypto::sha256("")) ==
          "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
    CHECK(crypto::to_hex(crypto::sha256("abc")) ==
          "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
    CHECK(crypto::to_hex(crypto::sha256(std::string(1000, 'a'))) ==
          "41edece42d63e8d9bf515a9ba6932e1c20cbc9f5a5d134645adb5db1b9737ea3");
    // RFC 7914 section 11 PBKDF2-HMAC-SHA256 test vector
    CHECK(crypto::to_hex(crypto::pbkdf2_sha256("passwd", "salt", 1, 64)) ==
          "55ac046e56e3089fec1691c22544b605f94185216dde0465e68b9d57c20dacbc"
          "49ca9cccf179b645991664b39d77ef317c71b845b1e30bd509112041d3a19783");
    std::string h = crypto::hash_password("correct horse", 1000);
    CHECK(crypto::verify_password("correct horse", h));
    CHECK(!crypto::verify_password("wrong horse", h));
    CHECK(!crypto::verify_password("x", "garbage"));
    CHECK(crypto::hash_password("same", 10) != crypto::hash_password("same", 10));  // random salt
    CHECK(crypto::random_bytes(32).size() == 32);
}

static void matching_tests() {
    m::Skills req{{"python", 3}, {"sql", 2}};
    CHECK(m::score({{"python", 5}, {"sql", 2}}, req) == 100);
    CHECK(m::score({{"python", 2}}, req) == 40);
    auto rows = m::explain({{"python", 2}}, req);
    CHECK(rows.size() == 2 && rows[0].skill == "python" && rows[0].status == "partial");
    CHECK(rows[1].status == "missing");
    CHECK(m::normalize_skill("  Machine   Learning ") == "machine learning");
    CHECK(m::normalize_skill(std::string(41, 'x')).empty());
    CHECK(m::normalize_skill("   ").empty());

    std::map<std::string, m::Skills> cands{
        {"a", {{"python", 3}}}, {"b", {{"python", 3}}}, {"c", {{"sql", 2}}}};
    auto team = m::suggest_team(cands, req, 3);
    CHECK(team.size() == 2);   // stops once fully covered
    CHECK(std::find(team.begin(), team.end(), "c") != team.end());
    std::vector<const m::Skills*> members;
    for (auto& id : team) members.push_back(&cands.at(id));
    CHECK(m::coverage(members, req).percent == 100);
    auto kept = m::suggest_team(cands, req, 3, {"a"}, 3);
    CHECK(kept.size() == 3 && kept[0] == "a");
}

int main() {
    json_tests();
    crypto_tests();
    matching_tests();
    if (failures) { std::cerr << failures << " check(s) failed\n"; return 1; }
    std::cout << "All core tests passed\n";
    return 0;
}
