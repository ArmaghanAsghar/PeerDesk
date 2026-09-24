#include "peerdesk/map.hpp"

#include <algorithm>

namespace peerdesk {

int map_coord(int local, int local_span, int remote_span) {
    if (local_span <= 0 || remote_span <= 0) return 0;
    const long mapped = (static_cast<long>(local) * remote_span) / local_span;
    return static_cast<int>(std::clamp(mapped, 0L, static_cast<long>(remote_span - 1)));
}

Letterbox fit_letterbox(int widget_w, int widget_h, int host_w, int host_h) {
    Letterbox b;
    if (widget_w <= 0 || widget_h <= 0 || host_w <= 0 || host_h <= 0) return b;
    const double wa = static_cast<double>(widget_w) / widget_h;
    const double ha = static_cast<double>(host_w) / host_h;
    if (wa > ha) {
        b.dest_h = widget_h;
        b.dest_w = static_cast<int>(widget_h * ha);
        b.dest_x = (widget_w - b.dest_w) / 2;
        b.dest_y = 0;
    } else {
        b.dest_w = widget_w;
        b.dest_h = static_cast<int>(widget_w / ha);
        b.dest_x = 0;
        b.dest_y = (widget_h - b.dest_h) / 2;
    }
    return b;
}

bool map_letterbox_point(int widget_x, int widget_y, const Letterbox& box, int host_w, int host_h,
                         int& host_x, int& host_y) {
    if (box.dest_w <= 0 || box.dest_h <= 0) return false;
    if (widget_x < box.dest_x || widget_y < box.dest_y || widget_x >= box.dest_x + box.dest_w ||
        widget_y >= box.dest_y + box.dest_h) {
        return false;
    }
    host_x = map_coord(widget_x - box.dest_x, box.dest_w, host_w);
    host_y = map_coord(widget_y - box.dest_y, box.dest_h, host_h);
    return true;
}

}  // namespace peerdesk
