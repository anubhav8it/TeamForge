#pragma once

#include <cstdint>
#include <vector>

namespace teamforge {

// Checks a photo or scan of a resume before its text is read (OCR), so a picture that cannot be
// read well is refused at once with a reason, instead of being sent off and read wrongly.
// Standard C++ only: the app decodes the image, turns it grey and scales it so its long side is
// at most kScanWorkingSide pixels (the measures below assume that scale).
//
// These are heuristics, not guarantees; the OCR's own word confidence (acceptReadText) is the
// second check, after reading.

constexpr int kMinScanShortSide = 500;   // pixels, in the original image
constexpr int kMinScanLongSide = 700;
constexpr int kScanWorkingSide = 1600;

struct GreyImage
{
    int width = 0;
    int height = 0;
    std::vector<std::uint8_t> pixels; // row by row, 0 = black, 255 = white
};

struct ScanQuality
{
    double brightness = 0; // mean grey level, 0..255
    double contrast = 0;   // spread between the 0.5th and 99.5th percentile grey levels, 0..255
    double detail = 0;     // share of tiles with detail in them (text, lines), 0..1
    // Median over the tiles with detail of: RMS Laplacian / standard deviation of the tile.
    // Contrast-independent; sharp print scores 1.5 or more, blurred print far less.
    double sharpness = 0;
};

ScanQuality measureScan(const GreyImage& image);

// Throws DocumentError naming the first problem found: too small, too dark, washed out or
// faint, blank, or blurred. `width` and `height` are the original image's size.
void requireReadableScan(int width, int height, const ScanQuality& quality);

} // namespace teamforge
