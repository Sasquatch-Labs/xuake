#include <linux/input.h>
#include "xuake.h"
#include "key.h"

static void
server_new_pointer(struct xuake_server *server, struct wlr_input_device *device)
{
    /* We don't do anything special with pointers. All of our pointer handling
     * is proxied through wlr_cursor. On another compositor, you might take this
     * opportunity to do libinput configuration on the device to set
     * acceleration, etc. */
    wlr_cursor_attach_input_device(server->cursor, device);
}

void
server_new_input(struct wl_listener *listener, void *data)
{
    /* This event is raised by the backend when a new input device becomes
     * available. */
    struct xuake_server *server = wl_container_of(listener, server, new_input);
    struct wlr_input_device *device = data;
    switch (device->type) {
    case WLR_INPUT_DEVICE_KEYBOARD:
        server_new_keyboard(server, device);
        break;
    case WLR_INPUT_DEVICE_POINTER:
        server_new_pointer(server, device);
        break;
    default:
        break;
    }
    /* We need to let the wlr_seat know what our capabilities are, which is
     * communiciated to the client. In Xuake we always have a cursor, even if
     * there are no pointer devices, so we always include that capability. */
    uint32_t caps = WL_SEAT_CAPABILITY_POINTER;
    if (!wl_list_empty(&server->keyboards)) {
        caps |= WL_SEAT_CAPABILITY_KEYBOARD;
    }
    wlr_seat_set_capabilities(server->seat, caps);
}

void
seat_request_cursor(struct wl_listener *listener, void *data)
{
    struct xuake_server *server = wl_container_of(listener, server, request_cursor);
    /* This event is rasied by the seat when a client provides a cursor image */
    struct wlr_seat_pointer_request_set_cursor_event *event = data;
    struct wlr_seat_client *focused_client =
        server->seat->pointer_state.focused_client;
    /* This can be sent by any client, so we check to make sure this one is
     * actually has pointer focus first. */
    if (focused_client == event->seat_client) {
        /* Once we've vetted the client, we can tell the cursor to use the
         * provided surface as the cursor image. It will set the hardware cursor
         * on the output that it's currently on and continue to do so as the
         * cursor moves between outputs. */
        wlr_cursor_set_surface(server->cursor, event->surface,
                event->hotspot_x, event->hotspot_y);
    }
}

static void
process_cursor_move(struct xuake_server *server, uint32_t time)
{
    /* Move the grabbed view to the new position. */
    server->grabbed_view->x = server->cursor->x - server->grab_x;
    server->grabbed_view->y = server->cursor->y - server->grab_y;
    if (server->grabbed_view->type == XUAKE_XWAYLAND_VIEW)
        wlr_xwayland_surface_configure(
            server->grabbed_view->xw_surface,
            server->grabbed_view->x,
            server->grabbed_view->y,
            server->grabbed_view->xw_surface->width,
            server->grabbed_view->xw_surface->height
        );
}

static void
process_cursor_motion(struct xuake_server *server, uint32_t time)
{
    /* If the mode is non-passthrough, delegate to those functions. */
    if (server->cursor_mode == XUAKE_CURSOR_MOVE) {
        process_cursor_move(server, time);
        return;
    } else if (server->cursor_mode == XUAKE_CURSOR_RESIZE) {
        process_cursor_resize(server, time);
        return;
    }

    /* Otherwise, find the view under the pointer and send the event along. */
    double sx, sy;
    struct wlr_seat *seat = server->seat;
    struct wlr_surface *surface = NULL;
    struct xuake_view *view = desktop_view_at(server, server->cursor->x, server->cursor->y, &surface, &sx, &sy);
    if (!view) {
        /* If there's no view under the cursor, set the cursor image to a
         * default. This is what makes the cursor image appear when you move it
         * around the screen, not over any views. */
        wlr_xcursor_manager_set_cursor_image(server->cursor_mgr, "left_ptr", server->cursor);
    }
    if (surface) {
        bool focus_changed = seat->pointer_state.focused_surface != surface;
        /*
         * "Enter" the surface if necessary. This lets the client know that the
         * cursor has entered one of its surfaces.
         *
         * Note that this gives the surface "pointer focus", which is distinct
         * from keyboard focus. You get pointer focus by moving the pointer over
         * a window.
         */
        wlr_seat_pointer_notify_enter(seat, surface, sx, sy);
        // XXX: focus_view() here to implement focus-follows-mouse?
        if (!focus_changed) {
            /* The enter event contains coordinates, so we only need to notify
             * on motion if the focus did not change. */
            wlr_seat_pointer_notify_motion(seat, time, sx, sy);
        }
    } else {
        /* Clear pointer focus so future button events and such are not sent to
         * the last client to have the cursor over it. */
        wlr_seat_pointer_clear_focus(seat);
    }
}

void
server_cursor_motion(struct wl_listener *listener, void *data)
{
    /* This event is forwarded by the cursor when a pointer emits a _relative_
     * pointer motion event (i.e. a delta) */
    struct xuake_server *server =
        wl_container_of(listener, server, cursor_motion);
    struct wlr_event_pointer_motion *event = data;
    /* The cursor doesn't move unless we tell it to. The cursor automatically
     * handles constraining the motion to the output layout, as well as any
     * special configuration applied for the specific input device which
     * generated the event. You can pass NULL for the device if you want to move
     * the cursor around without any input. */
    wlr_cursor_move(server->cursor, event->device,
            event->delta_x, event->delta_y);
    process_cursor_motion(server, event->time_msec);
}

void
server_cursor_motion_absolute(struct wl_listener *listener, void *data)
{
    /* This event is forwarded by the cursor when a pointer emits an _absolute_
     * motion event, from 0..1 on each axis. This happens, for example, when
     * wlroots is running under a Wayland window rather than KMS+DRM, and you
     * move the mouse over the window. You could enter the window from any edge,
     * so we have to warp the mouse there. There is also some hardware which
     * emits these events. */
    struct xuake_server *server =
        wl_container_of(listener, server, cursor_motion_absolute);
    struct wlr_event_pointer_motion_absolute *event = data;
    wlr_cursor_warp_absolute(server->cursor, event->device, event->x, event->y);
    process_cursor_motion(server, event->time_msec);
}

void
server_cursor_button(struct wl_listener *listener, void *data)
{
    /* This event is forwarded by the cursor when a pointer emits a button
     * event. */
    struct xuake_server *server = wl_container_of(listener, server, cursor_button);
    struct wlr_event_pointer_button *event = data;
    /* Notify the client with pointer focus that a button press has occurred */
    double sx, sy;
    struct wlr_seat *seat = server->seat;
    struct wlr_surface *surface;
    struct xuake_view *view = desktop_view_at(server, server->cursor->x, server->cursor->y, &surface, &sx, &sy);

    uint32_t modifiers = 0, edges = 0;
    struct xuake_keyboard *kbd;

    //printf("BUTTON: %d\n", event->button);
    wl_list_for_each(kbd, &server->keyboards, link) {
        modifiers |= wlr_keyboard_get_modifiers(kbd->device->keyboard);
    }
    if (event->state == WLR_BUTTON_RELEASED) {
        /* If you released any buttons, we exit interactive move/resize mode. */
        if (server->cursor_mode != XUAKE_CURSOR_PASSTHROUGH) {
            wlr_xcursor_manager_set_cursor_image(server->cursor_mgr, "left_ptr", server->cursor);
            server->cursor_mode = XUAKE_CURSOR_PASSTHROUGH;
            return;
        }
        if (!(modifiers & WLR_MODIFIER_ALT))
            wlr_seat_pointer_notify_button(server->seat, event->time_msec, event->button, event->state);
    } else {
        /* Focus that client if the button was _pressed_ */
        focus_view(view, surface);
        //if (modifiers & WLR_MODIFIER_CTRL) {
        if (modifiers & WLR_MODIFIER_ALT) {
            if (view && event->button == BTN_LEFT) {
                wlr_xcursor_manager_set_cursor_image(server->cursor_mgr, "grabbing", server->cursor);
                begin_interactive(view, XUAKE_CURSOR_MOVE, 0);
            } else if (view && event->button == BTN_RIGHT) {
                wlr_xcursor_manager_set_cursor_image(server->cursor_mgr, "grabbing", server->cursor);
                begin_interactive(view, XUAKE_CURSOR_RESIZE, WLR_EDGE_BOTTOM|WLR_EDGE_RIGHT);
            }
        } else {
            wlr_seat_pointer_notify_button(server->seat, event->time_msec, event->button, event->state);
        }
    }
}

void
server_cursor_axis(struct wl_listener *listener, void *data)
{
    /* This event is forwarded by the cursor when a pointer emits an axis event,
     * for example when you move the scroll wheel. */
    struct xuake_server *server =
        wl_container_of(listener, server, cursor_axis);
    struct wlr_event_pointer_axis *event = data;
    /* Notify the client with pointer focus of the axis event. */
    wlr_seat_pointer_notify_axis(server->seat,
            event->time_msec, event->orientation, event->delta,
            event->delta_discrete, event->source);
}

void
server_cursor_frame(struct wl_listener *listener, void *data)
{
    /* This event is forwarded by the cursor when a pointer emits an frame
     * event. Frame events are sent after regular pointer events to group
     * multiple events together. For instance, two axis events may happen at the
     * same time, in which case a frame event won't be sent in between. */
    struct xuake_server *server =
        wl_container_of(listener, server, cursor_frame);
    /* Notify the client with pointer focus of the frame event. */
    wlr_seat_pointer_notify_frame(server->seat);
}
