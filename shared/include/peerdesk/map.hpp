#pragma once

namespace peerdesk {

// Map a point from a local widget into host pixels. Returns 0 if span is invalid.
int map_coord(int local, int local_span, int remote_span);

struct Letterbox {
    int dest_x = 0;
    int dest_y = 0;
    int dest_w = 0;
    int dest_h = 0;
};

// Fit host_w x host_h into widget_w x widget_h, preserving aspect ratio.
Letterbox fit_letterbox(int widget_w, int widget_h, int host_w, int host_h);

bool map_letterbox_point(int widget_x, int widget_y, const Letterbox& box, int host_w, int host_h,
                         int& host_x, int& host_y);

}  // namespace peerdesk
