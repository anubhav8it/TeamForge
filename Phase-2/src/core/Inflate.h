#pragma once

#include <cstddef>
#include <string>
#include <string_view>

namespace teamforge {

// DEFLATE decompression (RFC 1951), the compression inside DOCX files (ZIP) and PDF streams
// (FlateDecode, RFC 1950 zlib wrapper). Standard C++ only, so the core needs no zlib.
//
// Course concept: canonical Huffman codes. Each block's code lengths are turned into a
// decoding table (codes of the same length are consecutive integers), and symbols are read
// one bit at a time, walking down the code lengths: the classic table-driven Huffman decoder.
//
// Throws DocumentError on damaged data, or when the output would exceed `maxOutput`
// (a "zip bomb" guard).

// Raw DEFLATE data (ZIP entries).
std::string inflateRaw(std::string_view data, std::size_t maxOutput);
// zlib-wrapped DEFLATE data (PDF FlateDecode). The Adler-32 checksum is not checked, so a
// slightly truncated stream still yields its text.
std::string inflateZlib(std::string_view data, std::size_t maxOutput);

} // namespace teamforge
