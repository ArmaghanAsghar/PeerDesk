#pragma once

#include "peerdesk/protocol.hpp"

#include <string>

namespace peerdesk {

// Replays viewer mouse and keyboard events on the local X server via XTEST.
class InputInject {
public:
    ~InputInject();
    // Open $DISPLAY and check that the XTEST extension is available.
    bool open(std::string& err);
    // Move the pointer to (e.x, e.y), then press or release e.button (0 means
    // left) or scroll one wheel step. No-op if not open.
    void apply_mouse(const MouseEvent& e);
    // Press or release the key for e.keysym. Ignored if the keysym has no keycode
    // in the current keymap, or if not open.
    void apply_key(const KeyEvent& e);
    bool ready() const { return dpy_ != nullptr; }

private:
    void* dpy_ = nullptr;
};

}  // namespace peerdesk
