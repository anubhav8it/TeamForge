// Password hashing (PBKDF2-HMAC-SHA256) and random tokens.
// Implemented here so the project has no OpenSSL dependency. The SHA-256 and
// PBKDF2 code is checked against standard test vectors in tests/test_core.cpp.
#pragma once
#include <array>
#include <cstdint>
#include <string>

namespace tf::crypto {

using Digest = std::array<uint8_t, 32>;

Digest sha256(const std::string& data);
Digest hmac_sha256(const std::string& key, const std::string& msg);
std::string pbkdf2_sha256(const std::string& password, const std::string& salt,
                          unsigned iterations, size_t length = 32);

std::string to_hex(const std::string& bytes);
std::string to_hex(const Digest& d);
std::string random_bytes(size_t n);
bool equal_constant_time(const std::string& a, const std::string& b);

// Stored format: pbkdf2_sha256$<iterations>$<salt hex>$<hash hex>
constexpr unsigned kDefaultIterations = 600000;   // OWASP 2023 guidance for PBKDF2-SHA256
std::string hash_password(const std::string& password, unsigned iterations = kDefaultIterations);
bool verify_password(const std::string& password, const std::string& stored);

}  // namespace tf::crypto
