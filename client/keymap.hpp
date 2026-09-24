#pragma once

#include <QtGui/QKeyEvent>

#include <cstdint>

namespace peerdesk {

uint32_t qt_to_xkeysym(int qt_key, const QString& text);

}  // namespace peerdesk
