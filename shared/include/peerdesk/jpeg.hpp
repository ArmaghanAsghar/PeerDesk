#pragma once

#include <cstdint>
#include <span>
#include <vector>

namespace peerdesk {

bool encode_jpeg_rgb(std::span<const uint8_t> rgb, int width, int height, int quality,
                     std::vector<uint8_t>& out);

bool decode_jpeg_rgb(std::span<const uint8_t> jpeg, std::vector<uint8_t>& rgb, int& width,
                     int& height);

}  // namespace peerdesk
