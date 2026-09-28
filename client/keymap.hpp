#pragma once

#include <QtGui/QKeyEvent>

#include <cstdint>

namespace peerdesk {

// Translate a Qt key code plus its produced text into an X11 keysym for the host.
// Named keys and modifiers map directly. Otherwise a single Latin-1 character in
// `text` is used, then A-Z as lowercase letters. Returns 0 if the key is unmapped.
uint32_t qt_to_xkeysym(int qt_key, const QString& text);

}  // namespace peerdesk
