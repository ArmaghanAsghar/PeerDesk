#include "keymap.hpp"

// X11 keysyms (Latin-1 / extras) so the Ubuntu client does not need Xlib headers.
// The Mac client can use the same numbers; the host maps them with XKeysymToKeycode.
namespace {
constexpr uint32_t XK_Return = 0xff0d;
constexpr uint32_t XK_BackSpace = 0xff08;
constexpr uint32_t XK_Tab = 0xff09;
constexpr uint32_t XK_Escape = 0xff1b;
constexpr uint32_t XK_Left = 0xff51;
constexpr uint32_t XK_Up = 0xff52;
constexpr uint32_t XK_Right = 0xff53;
constexpr uint32_t XK_Down = 0xff54;
constexpr uint32_t XK_Home = 0xff50;
constexpr uint32_t XK_End = 0xff57;
constexpr uint32_t XK_Delete = 0xffff;
constexpr uint32_t XK_Insert = 0xff63;
constexpr uint32_t XK_Shift_L = 0xffe1;
constexpr uint32_t XK_Control_L = 0xffe3;
constexpr uint32_t XK_Alt_L = 0xffe9;
constexpr uint32_t XK_Super_L = 0xffeb;
constexpr uint32_t XK_space = 0x0020;
}  // namespace

namespace peerdesk {

uint32_t qt_to_xkeysym(int qt_key, const QString& text) {
    switch (qt_key) {
        case Qt::Key_Return:
        case Qt::Key_Enter:
            return XK_Return;
        case Qt::Key_Backspace:
            return XK_BackSpace;
        case Qt::Key_Tab:
            return XK_Tab;
        case Qt::Key_Escape:
            return XK_Escape;
        case Qt::Key_Left:
            return XK_Left;
        case Qt::Key_Right:
            return XK_Right;
        case Qt::Key_Up:
            return XK_Up;
        case Qt::Key_Down:
            return XK_Down;
        case Qt::Key_Home:
            return XK_Home;
        case Qt::Key_End:
            return XK_End;
        case Qt::Key_Delete:
            return XK_Delete;
        case Qt::Key_Insert:
            return XK_Insert;
        case Qt::Key_Shift:
            return XK_Shift_L;
        case Qt::Key_Control:
            return XK_Control_L;
        case Qt::Key_Alt:
            return XK_Alt_L;
        case Qt::Key_Meta:
            return XK_Super_L;
        case Qt::Key_Space:
            return XK_space;
        default:
            break;
    }
    if (text.size() == 1) {
        const auto u = text[0].unicode();
        if (u >= 0x20 && u <= 0xff) return static_cast<uint32_t>(u);
    }
    if (qt_key >= Qt::Key_A && qt_key <= Qt::Key_Z) {
        return static_cast<uint32_t>('a' + (qt_key - Qt::Key_A));
    }
    return 0;
}

}  // namespace peerdesk
