#include "capture.hpp"

#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include <chrono>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <sstream>

namespace peerdesk {

X11Capture::~X11Capture() {
    if (dpy_) {
        XCloseDisplay(static_cast<Display*>(dpy_));
        dpy_ = nullptr;
    }
}

bool X11Capture::open(std::string& err) {
    Display* dpy = XOpenDisplay(nullptr);
    if (!dpy) {
        err = "Cannot open X11 display (check DISPLAY and permissions)";
        return false;
    }
    Screen* scr = DefaultScreenOfDisplay(dpy);
    width_ = WidthOfScreen(scr);
    height_ = HeightOfScreen(scr);
    if (width_ <= 0 || height_ <= 0) {
        XCloseDisplay(dpy);
        err = "X11 screen has invalid size";
        return false;
    }
    dpy_ = dpy;
    return true;
}

bool X11Capture::grab_rgb(std::vector<uint8_t>& rgb, std::string& err) {
    auto* dpy = static_cast<Display*>(dpy_);
    if (!dpy) {
        err = "X11 display closed";
        return false;
    }
    Window root = DefaultRootWindow(dpy);
    XImage* img = XGetImage(dpy, root, 0, 0, static_cast<unsigned>(width_),
                            static_cast<unsigned>(height_), AllPlanes, ZPixmap);
    if (!img) {
        err = "XGetImage failed (capture permission?)";
        return false;
    }
    rgb.resize(static_cast<size_t>(width_) * static_cast<size_t>(height_) * 3);
    if (img->bits_per_pixel == 32) {
        for (int y = 0; y < height_; ++y) {
            const auto* row =
                reinterpret_cast<const uint32_t*>(img->data + static_cast<size_t>(y) * img->bytes_per_line);
            for (int x = 0; x < width_; ++x) {
                const uint32_t p = row[x];
                const size_t i = (static_cast<size_t>(y) * static_cast<size_t>(width_) +
                                  static_cast<size_t>(x)) *
                                 3;
                rgb[i + 0] = static_cast<uint8_t>((p >> 16) & 0xff);
                rgb[i + 1] = static_cast<uint8_t>((p >> 8) & 0xff);
                rgb[i + 2] = static_cast<uint8_t>(p & 0xff);
            }
        }
    } else {
        for (int y = 0; y < height_; ++y) {
            for (int x = 0; x < width_; ++x) {
                const unsigned long p = XGetPixel(img, x, y);
                const size_t i = (static_cast<size_t>(y) * static_cast<size_t>(width_) +
                                  static_cast<size_t>(x)) *
                                 3;
                rgb[i + 0] = static_cast<uint8_t>((p >> 16) & 0xff);
                rgb[i + 1] = static_cast<uint8_t>((p >> 8) & 0xff);
                rgb[i + 2] = static_cast<uint8_t>(p & 0xff);
            }
        }
    }
    XDestroyImage(img);
    return true;
}

SyntheticCapture::SyntheticCapture(int w, int h) : width_(w), height_(h) {}

bool SyntheticCapture::open(std::string& err) {
    if (width_ <= 0 || height_ <= 0) {
        err = "Invalid synthetic size";
        return false;
    }
    return true;
}

void SyntheticCapture::note_input(const std::string& line) {
    std::lock_guard<std::mutex> g(mu_);
    last_input_ = line;
}

namespace {

void put_pixel(std::vector<uint8_t>& rgb, int w, int h, int x, int y, uint8_t r, uint8_t g,
               uint8_t b) {
    if (x < 0 || y < 0 || x >= w || y >= h) return;
    const size_t i = (static_cast<size_t>(y) * static_cast<size_t>(w) + static_cast<size_t>(x)) * 3;
    rgb[i] = r;
    rgb[i + 1] = g;
    rgb[i + 2] = b;
}

void fill_rect(std::vector<uint8_t>& rgb, int w, int h, int x, int y, int rw, int rh, uint8_t r,
               uint8_t g, uint8_t b) {
    for (int yy = y; yy < y + rh; ++yy) {
        for (int xx = x; xx < x + rw; ++xx) put_pixel(rgb, w, h, xx, yy, r, g, b);
    }
}

// Tiny 5x7 glyph font for ASCII 32-126 (subset: digits, letters, punctuation we need).
void draw_text(std::vector<uint8_t>& rgb, int w, int h, int x, int y, const std::string& s,
               uint8_t r, uint8_t g, uint8_t b) {
    // Scale-up pixel font using a simple 8x8 pattern from the character code.
    int cx = x;
    for (unsigned char ch : s) {
        for (int row = 0; row < 10; ++row) {
            const int bits = (static_cast<int>(ch) * 17 + row * 13) & 0xff;
            for (int col = 0; col < 7; ++col) {
                const bool on = ((ch >= 32) && ((bits >> (col % 8)) & 1)) || row == 0 || row == 9;
                // Readable block letters: use a compact built-in 5x7 for common chars.
                (void)on;
            }
        }
        // Draw a stable 8x14 bitmap from a hard-coded 5x7 set for 0-9 A-Z space :.-
        auto glyph = [&](int gx, int gy, bool on) {
            if (!on) return;
            fill_rect(rgb, w, h, cx + gx * 2, y + gy * 2, 2, 2, r, g, b);
        };
        const char c = static_cast<char>(ch);
        auto plot = [&](const char* rows) {
            for (int gy = 0; gy < 7; ++gy) {
                for (int gx = 0; gx < 5; ++gx) {
                    if (rows[gy * 5 + gx] == '#') glyph(gx, gy, true);
                }
            }
        };
        const char* rows = "     "
                           "     "
                           "     "
                           "     "
                           "     "
                           "     "
                           "     ";
        switch (c) {
            case '0':
                rows = " ### "
                       "#   #"
                       "#  ##"
                       "# # #"
                       "##  #"
                       "#   #"
                       " ### ";
                break;
            case '1':
                rows = "  #  "
                       " ##  "
                       "  #  "
                       "  #  "
                       "  #  "
                       "  #  "
                       " ### ";
                break;
            case '2':
                rows = " ### "
                       "#   #"
                       "    #"
                       "  ## "
                       " #   "
                       "#    "
                       "#####";
                break;
            case '3':
                rows = " ### "
                       "#   #"
                       "    #"
                       "  ## "
                       "    #"
                       "#   #"
                       " ### ";
                break;
            case '4':
                rows = "#   #"
                       "#   #"
                       "#   #"
                       "#####"
                       "    #"
                       "    #"
                       "    #";
                break;
            case '5':
                rows = "#####"
                       "#    "
                       "#### "
                       "    #"
                       "    #"
                       "#   #"
                       " ### ";
                break;
            case '6':
                rows = " ### "
                       "#    "
                       "#### "
                       "#   #"
                       "#   #"
                       "#   #"
                       " ### ";
                break;
            case '7':
                rows = "#####"
                       "    #"
                       "   # "
                       "  #  "
                       "  #  "
                       "  #  "
                       "  #  ";
                break;
            case '8':
                rows = " ### "
                       "#   #"
                       "#   #"
                       " ### "
                       "#   #"
                       "#   #"
                       " ### ";
                break;
            case '9':
                rows = " ### "
                       "#   #"
                       "#   #"
                       " ####"
                       "    #"
                       "    #"
                       " ### ";
                break;
            case ':':
                rows = "     "
                       "  #  "
                       "  #  "
                       "     "
                       "  #  "
                       "  #  "
                       "     ";
                break;
            case '-':
                rows = "     "
                       "     "
                       "     "
                       " ### "
                       "     "
                       "     "
                       "     ";
                break;
            case '.':
                rows = "     "
                       "     "
                       "     "
                       "     "
                       "     "
                       "  #  "
                       "     ";
                break;
            case ' ':
                break;
            default: {
                // Generic letter-ish block from ASCII
                rows = " ### "
                       "#   #"
                       "#####"
                       "#   #"
                       "#   #"
                       "#   #"
                       "#   #";
                break;
            }
        }
        if (c >= 'A' && c <= 'Z') {
            switch (c) {
                case 'A':
                    rows = " ### "
                           "#   #"
                           "#   #"
                           "#####"
                           "#   #"
                           "#   #"
                           "#   #";
                    break;
                case 'B':
                    rows = "#### "
                           "#   #"
                           "#   #"
                           "#### "
                           "#   #"
                           "#   #"
                           "#### ";
                    break;
                case 'C':
                    rows = " ### "
                           "#   #"
                           "#    "
                           "#    "
                           "#    "
                           "#   #"
                           " ### ";
                    break;
                case 'D':
                    rows = "#### "
                           "#   #"
                           "#   #"
                           "#   #"
                           "#   #"
                           "#   #"
                           "#### ";
                    break;
                case 'E':
                    rows = "#####"
                           "#    "
                           "#    "
                           "#### "
                           "#    "
                           "#    "
                           "#####";
                    break;
                case 'H':
                    rows = "#   #"
                           "#   #"
                           "#   #"
                           "#####"
                           "#   #"
                           "#   #"
                           "#   #";
                    break;
                case 'I':
                    rows = " ### "
                           "  #  "
                           "  #  "
                           "  #  "
                           "  #  "
                           "  #  "
                           " ### ";
                    break;
                case 'J':
                    rows = "  ###"
                           "    #"
                           "    #"
                           "    #"
                           "#   #"
                           "#   #"
                           " ### ";
                    break;
                case 'K':
                    rows = "#   #"
                           "#  # "
                           "# #  "
                           "##   "
                           "# #  "
                           "#  # "
                           "#   #";
                    break;
                case 'L':
                    rows = "#    "
                           "#    "
                           "#    "
                           "#    "
                           "#    "
                           "#    "
                           "#####";
                    break;
                case 'N':
                    rows = "#   #"
                           "##  #"
                           "# # #"
                           "#  ##"
                           "#   #"
                           "#   #"
                           "#   #";
                    break;
                case 'O':
                    rows = " ### "
                           "#   #"
                           "#   #"
                           "#   #"
                           "#   #"
                           "#   #"
                           " ### ";
                    break;
                case 'P':
                    rows = "#### "
                           "#   #"
                           "#   #"
                           "#### "
                           "#    "
                           "#    "
                           "#    ";
                    break;
                case 'R':
                    rows = "#### "
                           "#   #"
                           "#   #"
                           "#### "
                           "# #  "
                           "#  # "
                           "#   #";
                    break;
                case 'S':
                    rows = " ####"
                           "#    "
                           "#    "
                           " ### "
                           "    #"
                           "    #"
                           "#### ";
                    break;
                case 'T':
                    rows = "#####"
                           "  #  "
                           "  #  "
                           "  #  "
                           "  #  "
                           "  #  "
                           "  #  ";
                    break;
                case 'U':
                    rows = "#   #"
                           "#   #"
                           "#   #"
                           "#   #"
                           "#   #"
                           "#   #"
                           " ### ";
                    break;
                case 'V':
                    rows = "#   #"
                           "#   #"
                           "#   #"
                           "#   #"
                           "#   #"
                           " # # "
                           "  #  ";
                    break;
                case 'Y':
                    rows = "#   #"
                           " # # "
                           "  #  "
                           "  #  "
                           "  #  "
                           "  #  "
                           "  #  ";
                    break;
                case 'W':
                    rows = "#   #"
                           "#   #"
                           "#   #"
                           "# # #"
                           "## ##"
                           "#   #"
                           "#   #";
                    break;
                default:
                    break;
            }
        }
        if (c >= 'a' && c <= 'z') {
            // reuse uppercase patterns via fallthrough of default box if not listed
            const char up = static_cast<char>(c - 32);
            (void)up;
        }
        plot(rows);
        cx += 12;
    }
}

}  // namespace

bool SyntheticCapture::grab_rgb(std::vector<uint8_t>& rgb, std::string& err) {
    (void)err;
    rgb.assign(static_cast<size_t>(width_) * static_cast<size_t>(height_) * 3, 0);
    for (int y = 0; y < height_; ++y) {
        for (int x = 0; x < width_; ++x) {
            const bool grid = (x % 64 == 0) || (y % 64 == 0);
            const uint8_t r = grid ? 28 : 12;
            const uint8_t g = grid ? 42 : 18;
            const uint8_t b = grid ? 68 : 32;
            put_pixel(rgb, width_, height_, x, y, r, g, b);
        }
    }
    fill_rect(rgb, width_, height_, 40, 36, width_ - 80, 88, 18, 32, 58);
    draw_text(rgb, width_, height_, 56, 52, "PEERDESK HOST", 230, 236, 255);
    draw_text(rgb, width_, height_, 56, 84, "JORDAN WORKSTATION", 160, 200, 255);

    const auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    std::tm tm{};
    localtime_r(&now, &tm);
    char clock[32];
    std::snprintf(clock, sizeof(clock), "%02d:%02d:%02d", tm.tm_hour, tm.tm_min, tm.tm_sec);
    draw_text(rgb, width_, height_, 56, 160, clock, 255, 210, 90);

    std::string input;
    {
        std::lock_guard<std::mutex> g(mu_);
        input = last_input_;
    }
    for (auto& ch : input) {
        if (ch >= 'a' && ch <= 'z') ch = static_cast<char>(ch - 32);
    }
    draw_text(rgb, width_, height_, 56, 220, "LAST INPUT", 120, 180, 140);
    draw_text(rgb, width_, height_, 56, 250, input.substr(0, 40), 220, 240, 220);

    fill_rect(rgb, width_, height_, width_ / 2 - 40, height_ / 2 - 40, 80, 80, 220, 80, 70);
    draw_text(rgb, width_, height_, width_ / 2 - 70, height_ / 2 + 60, "CLICK TARGET", 255, 180, 180);
    return true;
}

}  // namespace peerdesk
