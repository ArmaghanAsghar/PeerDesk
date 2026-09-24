#pragma once

#include "peerdesk/protocol.hpp"

#include <string>

namespace peerdesk {

class InputInject {
public:
    ~InputInject();
    bool open(std::string& err);
    void apply_mouse(const MouseEvent& e);
    void apply_key(const KeyEvent& e);
    bool ready() const { return dpy_ != nullptr; }

private:
    void* dpy_ = nullptr;
};

}  // namespace peerdesk
