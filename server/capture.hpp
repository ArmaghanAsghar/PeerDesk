#pragma once

#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

namespace peerdesk {

// A source of host screen frames.
class ScreenSource {
public:
    virtual ~ScreenSource() = default;
    // Acquire the underlying display. Sets `err` and returns false on failure.
    virtual bool open(std::string& err) = 0;
    // Grab one frame as packed 8-bit RGB (width() * height() * 3 bytes).
    virtual bool grab_rgb(std::vector<uint8_t>& rgb, std::string& err) = 0;
    // Frame size in pixels. Valid after a successful open().
    virtual int width() const = 0;
    virtual int height() const = 0;
    // Report a human-readable description of the latest viewer input. Sources may
    // show it on screen or ignore it. May be called from a different thread than
    // grab_rgb().
    virtual void note_input(const std::string& line) = 0;
};

// Captures the X11 root window of $DISPLAY with XGetImage.
class X11Capture : public ScreenSource {
public:
    ~X11Capture() override;
    // Open $DISPLAY and record the default screen size.
    bool open(std::string& err) override;
    // Copy the whole root window, converting 32 bpp (fast path) or other depths
    // (via XGetPixel) to RGB. Assumes a 0xRRGGBB pixel layout.
    bool grab_rgb(std::vector<uint8_t>& rgb, std::string& err) override;
    int width() const override { return width_; }
    int height() const override { return height_; }
    void note_input(const std::string&) override {}

private:
    void* dpy_ = nullptr;  // Display*
    int width_ = 0;
    int height_ = 0;
};

// Draws a generated host canvas (grid, title, clock, last input, click target)
// so the server can run without X11, e.g. in tests or under Wayland.
class SyntheticCapture : public ScreenSource {
public:
    explicit SyntheticCapture(int w = 1280, int h = 720);
    // Only validates the configured size.
    bool open(std::string& err) override;
    // Render the canvas with the current wall-clock time and last input line. Never fails.
    bool grab_rgb(std::vector<uint8_t>& rgb, std::string& err) override;
    int width() const override { return width_; }
    int height() const override { return height_; }
    // Store `line` for display on the next frame. Thread-safe.
    void note_input(const std::string& line) override;

private:
    int width_;
    int height_;
    std::mutex mu_;
    std::string last_input_ = "waiting for viewer input";
};

}  // namespace peerdesk
