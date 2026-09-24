#include "peerdesk/jpeg.hpp"

#include <algorithm>
#include <csetjmp>
#include <cstdio>
#include <cstdlib>
#include <stdio.h>
#include <jpeglib.h>

namespace peerdesk {
namespace {

struct JpegErr {
    jpeg_error_mgr pub{};
    jmp_buf jump{};
};

void jpeg_error_exit(j_common_ptr cinfo) {
    auto* err = reinterpret_cast<JpegErr*>(cinfo->err);
    longjmp(err->jump, 1);
}

}  // namespace

bool encode_jpeg_rgb(std::span<const uint8_t> rgb, int width, int height, int quality,
                     std::vector<uint8_t>& out) {
    out.clear();
    if (width <= 0 || height <= 0) return false;
    const size_t need = static_cast<size_t>(width) * static_cast<size_t>(height) * 3;
    if (rgb.size() < need) return false;

    jpeg_compress_struct cinfo{};
    JpegErr jerr{};
    cinfo.err = jpeg_std_error(&jerr.pub);
    jerr.pub.error_exit = jpeg_error_exit;
    if (setjmp(jerr.jump)) {
        jpeg_destroy_compress(&cinfo);
        out.clear();
        return false;
    }

    jpeg_create_compress(&cinfo);
    unsigned char* buf = nullptr;
    unsigned long buf_size = 0;
    jpeg_mem_dest(&cinfo, &buf, &buf_size);
    cinfo.image_width = static_cast<JDIMENSION>(width);
    cinfo.image_height = static_cast<JDIMENSION>(height);
    cinfo.input_components = 3;
    cinfo.in_color_space = JCS_RGB;
    jpeg_set_defaults(&cinfo);
    jpeg_set_quality(&cinfo, std::clamp(quality, 20, 95), TRUE);
    jpeg_start_compress(&cinfo, TRUE);
    while (cinfo.next_scanline < cinfo.image_height) {
        auto* row = const_cast<JSAMPROW>(
            rgb.data() + static_cast<size_t>(cinfo.next_scanline) * static_cast<size_t>(width) * 3);
        jpeg_write_scanlines(&cinfo, &row, 1);
    }
    jpeg_finish_compress(&cinfo);
    out.assign(buf, buf + buf_size);
    jpeg_destroy_compress(&cinfo);
    if (buf) free(buf);
    return !out.empty();
}

bool decode_jpeg_rgb(std::span<const uint8_t> jpeg, std::vector<uint8_t>& rgb, int& width,
                     int& height) {
    rgb.clear();
    width = 0;
    height = 0;
    if (jpeg.empty()) return false;

    jpeg_decompress_struct cinfo{};
    JpegErr jerr{};
    cinfo.err = jpeg_std_error(&jerr.pub);
    jerr.pub.error_exit = jpeg_error_exit;
    if (setjmp(jerr.jump)) {
        jpeg_destroy_decompress(&cinfo);
        rgb.clear();
        return false;
    }

    jpeg_create_decompress(&cinfo);
    jpeg_mem_src(&cinfo, jpeg.data(), static_cast<unsigned long>(jpeg.size()));
    if (jpeg_read_header(&cinfo, TRUE) != JPEG_HEADER_OK) {
        jpeg_destroy_decompress(&cinfo);
        return false;
    }
    cinfo.out_color_space = JCS_RGB;
    jpeg_start_decompress(&cinfo);
    width = static_cast<int>(cinfo.output_width);
    height = static_cast<int>(cinfo.output_height);
    rgb.resize(static_cast<size_t>(width) * static_cast<size_t>(height) * 3);
    while (cinfo.output_scanline < cinfo.output_height) {
        auto* row = rgb.data() +
                    static_cast<size_t>(cinfo.output_scanline) * static_cast<size_t>(width) * 3;
        jpeg_read_scanlines(&cinfo, &row, 1);
    }
    jpeg_finish_decompress(&cinfo);
    jpeg_destroy_decompress(&cinfo);
    return true;
}

}  // namespace peerdesk
