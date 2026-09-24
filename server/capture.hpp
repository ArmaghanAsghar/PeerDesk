#pragma once

#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

namespace peerdesk {

class ScreenSource {
public:
    virtual ~ScreenSource() = default;
    virtual bool open(std::string& err) = 0;
    virtual bool grab_rgb(std::vector<uint8_t>& rgb, std::string& err) = 0;
    virtual int width() const = 0;
    virtual int height() const = 0;
    virtual void note_input(const std::string& line) = 0;
};

class X11Capture : public ScreenSource {
public:
    ~X11Capture() override;
    bool open(std::string& err) override;
    bool grab_rgb(std::vector<uint8_t>& rgb, std::string& err) override;
    int width() const override { return width_; }
    int height() const override { return height_; }
    void note_input(const std::string&) override {}

private:
    void* dpy_ = nullptr;  // Display*
    int width_ = 0;
    int height_ = 0;
};

class SyntheticCapture : public ScreenSource {
public:
    explicit SyntheticCapture(int w = 1280, int h = 720);
    bool open(std::string& err) override;
    bool grab_rgb(std::vector<uint8_t>& rgb, std::string& err) override;
    int width() const override { return width_; }
    int height() const override { return height_; }
    void note_input(const std::string& line) override;

private:
    int width_;
    int height_;
    std::mutex mu_;
    std::string last_input_ = "waiting for viewer input";
};

}  // namespace peerdesk
