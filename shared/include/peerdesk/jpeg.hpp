#pragma once

#include <cstdint>
#include <span>
#include <vector>

namespace peerdesk {

// Compress a packed 8-bit RGB image (width * height * 3 bytes) to JPEG in `out`.
// `quality` is clamped to [20, 95]. Returns false on bad dimensions, a short
// buffer, or a libjpeg error; `out` is empty on failure.
bool encode_jpeg_rgb(std::span<const uint8_t> rgb, int width, int height, int quality,
                     std::vector<uint8_t>& out);

// Decompress `jpeg` into packed 8-bit RGB and report its dimensions.
// Returns false (with `rgb` empty) if the data is empty or not a valid JPEG.
bool decode_jpeg_rgb(std::span<const uint8_t> jpeg, std::vector<uint8_t>& rgb, int& width,
                     int& height);

}  // namespace peerdesk
