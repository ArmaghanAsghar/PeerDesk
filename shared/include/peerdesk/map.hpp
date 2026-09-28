#pragma once

namespace peerdesk {

// Scale `local` from [0, local_span) to [0, remote_span), clamped to the last pixel.
// Returns 0 if either span is not positive.
int map_coord(int local, int local_span, int remote_span);

// Where the host image is drawn inside the widget, in widget pixels.
struct Letterbox {
    int dest_x = 0;
    int dest_y = 0;
    int dest_w = 0;
    int dest_h = 0;
};

// Fit host_w x host_h into widget_w x widget_h, preserving aspect ratio and
// centring the result. Returns an all-zero box if any size is not positive.
Letterbox fit_letterbox(int widget_w, int widget_h, int host_w, int host_h);

// Convert a widget point into host pixels via `box`. Returns false (outputs
// untouched) if the point falls in the letterbox bars or the box is empty.
bool map_letterbox_point(int widget_x, int widget_y, const Letterbox& box, int host_w, int host_h,
                         int& host_x, int& host_y);

}  // namespace peerdesk
