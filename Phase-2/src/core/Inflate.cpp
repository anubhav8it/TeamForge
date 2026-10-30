#include "core/Inflate.h"

#include "core/Errors.h"

#include <array>
#include <cstdint>

namespace teamforge {

namespace {

constexpr int kMaxBits = 15;      // longest DEFLATE code
constexpr int kLiteralCodes = 288;
constexpr int kDistanceCodes = 30;

struct Truncated {}; // input ran out; caught below

class BitReader
{
public:
    explicit BitReader(std::string_view data) : data_(data) {}

    // Bits are packed least significant first.
    int bits(int count)
    {
        std::uint32_t value = buffer_;
        while (bitCount_ < count) {
            if (pos_ >= data_.size())
                throw Truncated{};
            value |= static_cast<std::uint32_t>(static_cast<unsigned char>(data_[pos_++])) << bitCount_;
            bitCount_ += 8;
        }
        buffer_ = value >> count;
        bitCount_ -= count;
        return static_cast<int>(value & ((1u << count) - 1u));
    }
    void alignToByte()
    {
        buffer_ = 0;
        bitCount_ = 0;
    }
    std::uint8_t byte()
    {
        if (pos_ >= data_.size())
            throw Truncated{};
        return static_cast<std::uint8_t>(data_[pos_++]);
    }

private:
    std::string_view data_;
    std::size_t pos_ = 0;
    std::uint32_t buffer_ = 0;
    int bitCount_ = 0;
};

// A canonical Huffman code: how many codes have each length, and the symbols sorted by code.
struct Huffman
{
    std::array<short, kMaxBits + 1> count{};
    std::array<short, kLiteralCodes> symbol{};

    // Returns false for an over-subscribed code (an invalid stream). Incomplete codes are allowed
    // (a single distance code is common).
    bool build(const short *lengths, int n)
    {
        count.fill(0);
        for (int s = 0; s < n; ++s)
            ++count[static_cast<std::size_t>(lengths[s])];
        if (count[0] == n)
            return true; // no codes: only valid if never used
        int left = 1;
        for (int len = 1; len <= kMaxBits; ++len) {
            left <<= 1;
            left -= count[static_cast<std::size_t>(len)];
            if (left < 0)
                return false;
        }
        std::array<short, kMaxBits + 1> offsets{};
        for (int len = 1; len < kMaxBits; ++len)
            offsets[static_cast<std::size_t>(len + 1)] = static_cast<short>(offsets[static_cast<std::size_t>(len)] + count[static_cast<std::size_t>(len)]);
        for (int s = 0; s < n; ++s) {
            if (lengths[s] != 0)
                symbol[static_cast<std::size_t>(offsets[static_cast<std::size_t>(lengths[s])]++)] = static_cast<short>(s);
        }
        return true;
    }

    // Reads one symbol: codes of each length are consecutive, so compare against the first
    // code of that length while adding one bit at a time.
    int decode(BitReader& in) const
    {
        int code = 0;
        int first = 0;
        int index = 0;
        for (int len = 1; len <= kMaxBits; ++len) {
            code |= in.bits(1);
            const int n = count[static_cast<std::size_t>(len)];
            if (code - n < first)
                return symbol[static_cast<std::size_t>(index + (code - first))];
            index += n;
            first += n;
            first <<= 1;
            code <<= 1;
        }
        throw DocumentError("The file is damaged (bad compressed data).");
    }
};

constexpr std::array<short, 29> kLengthBase{3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31,
                                           35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258};
constexpr std::array<short, 29> kLengthExtra{0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2,
                                            3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0};
constexpr std::array<short, 30> kDistanceBase{1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129,
                                             193, 257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097,
                                             6145, 8193, 12289, 16385, 24577};
constexpr std::array<short, 30> kDistanceExtra{0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6,
                                              7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13};

class Inflater
{
public:
    Inflater(std::string_view data, std::size_t maxOutput) : in_(data), maxOutput_(maxOutput) {}

    // Decodes every block; on truncated input keeps what was decoded and reports it.
    bool run()
    {
        try {
            bool last = false;
            while (!last) {
                last = in_.bits(1) == 1;
                switch (in_.bits(2)) {
                case 0: stored(); break;
                case 1: fixed(); break;
                case 2: dynamic(); break;
                default: throw DocumentError("The file is damaged (bad compressed block).");
                }
            }
            return true;
        } catch (const Truncated&) {
            return false;
        }
    }
    std::string& output() { return out_; }

private:
    void put(char c)
    {
        if (out_.size() >= maxOutput_)
            throw DocumentError("The file expands to more data than TeamForge accepts.");
        out_.push_back(c);
    }

    void stored()
    {
        in_.alignToByte();
        const unsigned len = in_.byte() | (in_.byte() << 8u);
        const unsigned nlen = in_.byte() | (in_.byte() << 8u);
        if ((len ^ 0xFFFFu) != nlen)
            throw DocumentError("The file is damaged (bad stored block).");
        for (unsigned i = 0; i < len; ++i)
            put(static_cast<char>(in_.byte()));
    }

    void codes(const Huffman& literals, const Huffman& distances)
    {
        for (;;) {
            const int symbol = literals.decode(in_);
            if (symbol < 256) {
                put(static_cast<char>(symbol));
            } else if (symbol == 256) {
                return;
            } else {
                const int lengthIndex = symbol - 257;
                if (lengthIndex >= 29)
                    throw DocumentError("The file is damaged (bad length code).");
                const int length = kLengthBase[static_cast<std::size_t>(lengthIndex)]
                                   + in_.bits(kLengthExtra[static_cast<std::size_t>(lengthIndex)]);
                const int distanceIndex = distances.decode(in_);
                if (distanceIndex >= kDistanceCodes)
                    throw DocumentError("The file is damaged (bad distance code).");
                const std::size_t distance = static_cast<std::size_t>(
                    kDistanceBase[static_cast<std::size_t>(distanceIndex)]
                    + in_.bits(kDistanceExtra[static_cast<std::size_t>(distanceIndex)]));
                if (distance > out_.size())
                    throw DocumentError("The file is damaged (distance too far back).");
                for (int i = 0; i < length; ++i)
                    put(out_[out_.size() - distance]);
            }
        }
    }

    void fixed()
    {
        static const std::pair<Huffman, Huffman> tables = [] {
            std::array<short, kLiteralCodes> lengths{};
            for (int s = 0; s < 144; ++s) lengths[static_cast<std::size_t>(s)] = 8;
            for (int s = 144; s < 256; ++s) lengths[static_cast<std::size_t>(s)] = 9;
            for (int s = 256; s < 280; ++s) lengths[static_cast<std::size_t>(s)] = 7;
            for (int s = 280; s < kLiteralCodes; ++s) lengths[static_cast<std::size_t>(s)] = 8;
            Huffman literals;
            literals.build(lengths.data(), kLiteralCodes);
            std::array<short, kDistanceCodes> distanceLengths{};
            distanceLengths.fill(5);
            Huffman distances;
            distances.build(distanceLengths.data(), kDistanceCodes);
            return std::make_pair(literals, distances);
        }();
        codes(tables.first, tables.second);
    }

    void dynamic()
    {
        const int nlen = in_.bits(5) + 257;
        const int ndist = in_.bits(5) + 1;
        const int ncode = in_.bits(4) + 4;
        if (nlen > 286 || ndist > kDistanceCodes)
            throw DocumentError("The file is damaged (bad code counts).");

        static constexpr std::array<int, 19> order{16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15};
        std::array<short, 320> lengths{};
        for (int i = 0; i < ncode; ++i)
            lengths[static_cast<std::size_t>(order[static_cast<std::size_t>(i)])] = static_cast<short>(in_.bits(3));
        Huffman lengthCode;
        if (!lengthCode.build(lengths.data(), 19))
            throw DocumentError("The file is damaged (bad code lengths).");

        int index = 0;
        while (index < nlen + ndist) {
            int symbol = lengthCode.decode(in_);
            if (symbol < 16) {
                lengths[static_cast<std::size_t>(index++)] = static_cast<short>(symbol);
                continue;
            }
            short repeat = 0;
            int times = 0;
            if (symbol == 16) {
                if (index == 0)
                    throw DocumentError("The file is damaged (repeat with no length).");
                repeat = lengths[static_cast<std::size_t>(index - 1)];
                times = 3 + in_.bits(2);
            } else if (symbol == 17) {
                times = 3 + in_.bits(3);
            } else {
                times = 11 + in_.bits(7);
            }
            if (index + times > nlen + ndist)
                throw DocumentError("The file is damaged (too many lengths).");
            while (times-- > 0)
                lengths[static_cast<std::size_t>(index++)] = repeat;
        }
        if (lengths[256] == 0)
            throw DocumentError("The file is damaged (no end-of-block code).");

        Huffman literals;
        Huffman distances;
        if (!literals.build(lengths.data(), nlen) || !distances.build(lengths.data() + nlen, ndist))
            throw DocumentError("The file is damaged (bad Huffman code).");
        codes(literals, distances);
    }

    BitReader in_;
    std::size_t maxOutput_;
    std::string out_;
};

} // namespace

std::string inflateRaw(std::string_view data, std::size_t maxOutput)
{
    Inflater inflater(data, maxOutput);
    if (!inflater.run())
        throw DocumentError("The file is damaged (compressed data ends early).");
    return std::move(inflater.output());
}

std::string inflateZlib(std::string_view data, std::size_t maxOutput)
{
    if (data.size() < 2)
        throw DocumentError("The file is damaged (empty compressed stream).");
    const auto cmf = static_cast<unsigned char>(data[0]);
    const auto flg = static_cast<unsigned char>(data[1]);
    if ((cmf & 0x0Fu) != 8 || ((cmf << 8u) | flg) % 31 != 0 || (flg & 0x20u) != 0)
        throw DocumentError("The file is damaged (bad compressed stream header).");
    Inflater inflater(data.substr(2), maxOutput);
    const bool complete = inflater.run();
    if (!complete && inflater.output().empty())
        throw DocumentError("The file is damaged (compressed data ends early).");
    return std::move(inflater.output());
}

} // namespace teamforge
