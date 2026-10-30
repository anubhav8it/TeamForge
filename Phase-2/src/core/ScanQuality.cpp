#include "core/ScanQuality.h"

#include "core/Errors.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <string>

namespace teamforge {

namespace {

constexpr int kTile = 24;                 // pixels per tile side, at the working scale
constexpr double kDetailDeviation = 8;    // a tile with a grey-level deviation below this is plain
// Calibrated on rendered resume pages (Arial, A4 and phone-photo sizes) blurred by a known
// amount, then scaled like the app does (long side 1600 px, x-height about 8-11 px): sharp print
// scores 1.5-2.5; 0.6-0.7 at a Gaussian blur of sigma 1 px; about 0.42 at 1.3 px; 0.27-0.29 at
// 1.6 px, where letters start to merge. The limit errs on the side of letting a picture
// through, since the OCR's word confidence is checked afterwards.
constexpr double kMinSharpness = 0.35;
constexpr double kMinDetail = 0.01;
constexpr double kMinContrast = 30;
constexpr double kMinBrightness = 45;

} // namespace

ScanQuality measureScan(const GreyImage& image)
{
    ScanQuality q;
    const int w = image.width;
    const int h = image.height;
    if (w <= 0 || h <= 0 || image.pixels.size() != static_cast<std::size_t>(w) * static_cast<std::size_t>(h))
        return q;
    const auto at = [&](int x, int y) { return static_cast<double>(image.pixels[static_cast<std::size_t>(y) * static_cast<std::size_t>(w) + static_cast<std::size_t>(x)]); };

    std::array<std::size_t, 256> histogram{};
    double sum = 0;
    for (std::uint8_t p : image.pixels) {
        ++histogram[p];
        sum += p;
    }
    q.brightness = sum / static_cast<double>(image.pixels.size());
    const auto percentile = [&](double share) {
        const auto target = static_cast<std::size_t>(share * static_cast<double>(image.pixels.size()));
        std::size_t seen = 0;
        for (int level = 0; level < 256; ++level) {
            seen += histogram[static_cast<std::size_t>(level)];
            if (seen > target)
                return level;
        }
        return 255;
    };
    q.contrast = percentile(0.995) - percentile(0.005);

    // Tiles away from the border; the Laplacian needs one pixel round each.
    std::vector<double> ratios;
    int tiles = 0;
    for (int ty = 1; ty + kTile < h; ty += kTile) {
        for (int tx = 1; tx + kTile < w; tx += kTile) {
            ++tiles;
            double s = 0;
            double s2 = 0;
            double laplace2 = 0;
            for (int y = ty; y < ty + kTile; ++y)
                for (int x = tx; x < tx + kTile; ++x) {
                    const double p = at(x, y);
                    s += p;
                    s2 += p * p;
                    const double l = 4 * p - at(x - 1, y) - at(x + 1, y) - at(x, y - 1) - at(x, y + 1);
                    laplace2 += l * l;
                }
            const double n = kTile * kTile;
            const double variance = std::max(0.0, s2 / n - (s / n) * (s / n));
            const double deviation = std::sqrt(variance);
            if (deviation < kDetailDeviation)
                continue;
            ratios.push_back(std::sqrt(laplace2 / n) / deviation);
        }
    }
    q.detail = tiles > 0 ? static_cast<double>(ratios.size()) / tiles : 0;
    if (!ratios.empty()) {
        const auto middle = ratios.begin() + static_cast<std::ptrdiff_t>(ratios.size() / 2);
        std::nth_element(ratios.begin(), middle, ratios.end());
        q.sharpness = *middle;
    }
    return q;
}

void requireReadableScan(int width, int height, const ScanQuality& quality)
{
    const std::string retake = " Take the photo again, or upload the original file.";
    if (std::min(width, height) < kMinScanShortSide || std::max(width, height) < kMinScanLongSide)
        throw DocumentError("This image is only " + std::to_string(width) + " × " + std::to_string(height)
                            + " pixels, too small for the text to be read. Use a photo or scan of at least "
                            + std::to_string(kMinScanShortSide) + " × " + std::to_string(kMinScanLongSide) + " pixels.");
    if (quality.brightness < kMinBrightness)
        throw DocumentError("This photo is too dark to read. Take it again in good light, or upload the original file.");
    if (quality.detail < kMinDetail)
        throw DocumentError("No text can be seen in this image: it is blank, or far too blurred." + retake);
    if (quality.sharpness < kMinSharpness)
        throw DocumentError("This image is too blurry to read. Hold the camera steady, let it focus on the page, "
                            "and make sure the text is sharp before you take the photo; or upload the original file.");
    if (quality.contrast < kMinContrast)
        throw DocumentError("The text in this image is too faint (too little contrast between the text and the page)." + retake);
}

} // namespace teamforge
