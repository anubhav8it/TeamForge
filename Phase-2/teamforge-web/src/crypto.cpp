#include "crypto.hpp"

#include <algorithm>
#include <cstring>
#include <random>
#include <stdexcept>
#include <vector>

namespace tf::crypto {
namespace {

constexpr uint32_t K[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2};

inline uint32_t rotr(uint32_t x, int n) { return (x >> n) | (x << (32 - n)); }

struct Sha256 {
    uint32_t h[8] = {0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
                     0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};
    uint8_t buf[64];
    size_t buf_len = 0;
    uint64_t total = 0;

    void block(const uint8_t* p) {
        uint32_t w[64];
        for (int i = 0; i < 16; ++i)
            w[i] = (uint32_t(p[4 * i]) << 24) | (uint32_t(p[4 * i + 1]) << 16) |
                   (uint32_t(p[4 * i + 2]) << 8) | uint32_t(p[4 * i + 3]);
        for (int i = 16; i < 64; ++i) {
            uint32_t s0 = rotr(w[i - 15], 7) ^ rotr(w[i - 15], 18) ^ (w[i - 15] >> 3);
            uint32_t s1 = rotr(w[i - 2], 17) ^ rotr(w[i - 2], 19) ^ (w[i - 2] >> 10);
            w[i] = w[i - 16] + s0 + w[i - 7] + s1;
        }
        uint32_t a = h[0], b = h[1], c = h[2], d = h[3], e = h[4], f = h[5], g = h[6], hh = h[7];
        for (int i = 0; i < 64; ++i) {
            uint32_t S1 = rotr(e, 6) ^ rotr(e, 11) ^ rotr(e, 25);
            uint32_t ch = (e & f) ^ (~e & g);
            uint32_t t1 = hh + S1 + ch + K[i] + w[i];
            uint32_t S0 = rotr(a, 2) ^ rotr(a, 13) ^ rotr(a, 22);
            uint32_t mj = (a & b) ^ (a & c) ^ (b & c);
            uint32_t t2 = S0 + mj;
            hh = g; g = f; f = e; e = d + t1; d = c; c = b; b = a; a = t1 + t2;
        }
        h[0] += a; h[1] += b; h[2] += c; h[3] += d; h[4] += e; h[5] += f; h[6] += g; h[7] += hh;
    }

    void update(const uint8_t* data, size_t n) {
        total += n;
        while (n > 0) {
            size_t take = std::min(n, 64 - buf_len);
            std::memcpy(buf + buf_len, data, take);
            buf_len += take; data += take; n -= take;
            if (buf_len == 64) { block(buf); buf_len = 0; }
        }
    }
    void update(const std::string& s) { update(reinterpret_cast<const uint8_t*>(s.data()), s.size()); }

    Digest finish() {
        uint64_t bits = total * 8;
        uint8_t pad = 0x80;
        update(&pad, 1);
        uint8_t zero = 0;
        while (buf_len != 56) update(&zero, 1);
        uint8_t len[8];
        for (int i = 0; i < 8; ++i) len[i] = uint8_t(bits >> (56 - 8 * i));
        update(len, 8);
        Digest out;
        for (int i = 0; i < 8; ++i)
            for (int j = 0; j < 4; ++j) out[4 * i + j] = uint8_t(h[i] >> (24 - 8 * j));
        return out;
    }
};

// HMAC with the inner/outer key states computed once, so PBKDF2's many
// iterations only hash the short message each time.
struct Hmac {
    Sha256 inner, outer;
    explicit Hmac(const std::string& key) {
        std::string k = key;
        if (k.size() > 64) { Digest d = sha256(k); k.assign(d.begin(), d.end()); }
        k.resize(64, '\0');
        std::string ipad(64, 0), opad(64, 0);
        for (int i = 0; i < 64; ++i) { ipad[i] = char(k[i] ^ 0x36); opad[i] = char(k[i] ^ 0x5c); }
        inner.update(ipad);
        outer.update(opad);
    }
    Digest mac(const uint8_t* msg, size_t n) const {
        Sha256 i = inner; i.update(msg, n);
        Digest d = i.finish();
        Sha256 o = outer; o.update(d.data(), d.size());
        return o.finish();
    }
};

}  // namespace

Digest sha256(const std::string& data) { Sha256 s; s.update(data); return s.finish(); }

Digest hmac_sha256(const std::string& key, const std::string& msg) {
    return Hmac(key).mac(reinterpret_cast<const uint8_t*>(msg.data()), msg.size());
}

std::string pbkdf2_sha256(const std::string& password, const std::string& salt,
                          unsigned iterations, size_t length) {
    if (iterations == 0) throw std::invalid_argument("iterations must be > 0");
    Hmac prf(password);
    std::string out;
    for (uint32_t block = 1; out.size() < length; ++block) {
        std::string msg = salt;
        msg += char(block >> 24); msg += char(block >> 16); msg += char(block >> 8); msg += char(block);
        Digest u = prf.mac(reinterpret_cast<const uint8_t*>(msg.data()), msg.size());
        Digest t = u;
        for (unsigned i = 1; i < iterations; ++i) {
            u = prf.mac(u.data(), u.size());
            for (size_t j = 0; j < t.size(); ++j) t[j] ^= u[j];
        }
        out.append(t.begin(), t.end());
    }
    out.resize(length);
    return out;
}

std::string to_hex(const std::string& bytes) {
    static const char* hex = "0123456789abcdef";
    std::string out;
    out.reserve(bytes.size() * 2);
    for (unsigned char c : bytes) { out += hex[c >> 4]; out += hex[c & 15]; }
    return out;
}
std::string to_hex(const Digest& d) { return to_hex(std::string(d.begin(), d.end())); }

static std::string from_hex(const std::string& h) {
    auto val = [](char c) -> int {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        return -1;
    };
    if (h.size() % 2) return {};
    std::string out;
    for (size_t i = 0; i < h.size(); i += 2) {
        int a = val(h[i]), b = val(h[i + 1]);
        if (a < 0 || b < 0) return {};
        out += char(a * 16 + b);
    }
    return out;
}

std::string random_bytes(size_t n) {
    // std::random_device reads the OS random source (BCryptGenRandom/rand_s on
    // Windows with GCC 9+, /dev/urandom on Linux).
    static thread_local std::random_device rd;
    std::string out;
    while (out.size() < n) {
        uint32_t v = rd();
        for (int i = 0; i < 4 && out.size() < n; ++i) out += char(v >> (8 * i));
    }
    return out;
}

bool equal_constant_time(const std::string& a, const std::string& b) {
    if (a.size() != b.size()) return false;
    unsigned char diff = 0;
    for (size_t i = 0; i < a.size(); ++i) diff |= static_cast<unsigned char>(a[i] ^ b[i]);
    return diff == 0;
}

std::string hash_password(const std::string& password, unsigned iterations) {
    std::string salt = random_bytes(16);
    return "pbkdf2_sha256$" + std::to_string(iterations) + "$" + to_hex(salt) + "$" +
           to_hex(pbkdf2_sha256(password, salt, iterations));
}

bool verify_password(const std::string& password, const std::string& stored) {
    // pbkdf2_sha256$<iterations>$<salt>$<hash>
    size_t a = stored.find('$'), b = stored.find('$', a + 1), c = stored.find('$', b + 1);
    if (a == std::string::npos || b == std::string::npos || c == std::string::npos) return false;
    if (stored.substr(0, a) != "pbkdf2_sha256") return false;
    unsigned long iters = 0;
    try { iters = std::stoul(stored.substr(a + 1, b - a - 1)); } catch (...) { return false; }
    if (iters == 0 || iters > 10000000) return false;
    std::string salt = from_hex(stored.substr(b + 1, c - b - 1));
    std::string expected = from_hex(stored.substr(c + 1));
    if (salt.empty() || expected.empty()) return false;
    return equal_constant_time(pbkdf2_sha256(password, salt, unsigned(iters), expected.size()), expected);
}

}  // namespace tf::crypto
