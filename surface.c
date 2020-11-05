#include "xuake.h"
#include "xklua.h"
#include "limits.h"

void
focus_view(struct xuake_view *view, struct wlr_surface *surface)
{
    /* Note: this function only deals primarily with keyboard focus. */
    if (view == NULL)
        return;
    if (view->is_unmanaged)
        return;
    if (surface == NULL)
        return;
    struct xuake_server *server = view->server;
    struct wlr_seat *seat = server->seat;
    struct wlr_surface *prev_surface = seat->keyboard_state.focused_surface;
    struct xuake_view *v;
    void *vsurf = NULL;
    struct xuake_view *prev_view = NULL;
    if (prev_surface == surface) {
        /* Don't re-focus an already focused surface. */
        return;
    }
    if (prev_surface && wlr_surface_is_xwayland_surface(seat->keyboard_state.focused_surface)) {
        struct wlr_xwayland_surface *xw_previous = wlr_xwayland_surface_from_wlr_surface(seat->keyboard_state.focused_surface);
        vsurf = xw_previous;
        wlr_xwayland_surface_activate(xw_previous, false);
    } else if (prev_surface) {
        /*
         * Deactivate the previously focused surface. This lets the client know
         * it no longer has focus and the client will repaint accordingly, e.g.
         * stop displaying a caret.
         */
        struct wlr_xdg_surface *previous = wlr_xdg_surface_from_wlr_surface(seat->keyboard_state.focused_surface);
        vsurf = previous;
        wlr_xdg_toplevel_set_activated(previous, false);
    }
    if (vsurf)
        wl_list_for_each(v, &server->workspaces[server->ws], link) {
            if (v->v_surface == vsurf) {
                prev_view = v;
                break;
            }
        }
    struct wlr_keyboard *keyboard = wlr_seat_get_keyboard(seat);
    /* Move the view to the front */
    wl_list_remove(&view->link);
    wl_list_insert(&server->workspaces[server->ws], &view->link);
    /* Activate the new surface */
    switch (view->type) {
    case XUAKE_XWAYLAND_VIEW:
        wlr_xwayland_surface_activate(view->xw_surface, true);
        break;
    case XUAKE_XDG_SHELL_VIEW:
        wlr_xdg_toplevel_set_activated(view->xdg_surface, true);
        break;
    // XXX: No equivalent for wl_shell?
    }
    /*
     * Tell the seat to have the keyboard enter this surface. wlroots will keep
     * track of this and automatically send key events to the appropriate
     * clients without additional work on your part.
     */
    wlr_seat_keyboard_notify_enter(seat, get_wlr_surface_from_view(view),
        keyboard->keycodes, keyboard->num_keycodes, &keyboard->modifiers);

    view->focused = 1;
    if (prev_view)
        prev_view->focused = 0;

    xklua_run_callback(&server->lua_callbacks.focus_surface, "i", view->view_id);
}

struct xuake_view *
get_view_by_id(struct xuake_server *server, int view_id)
{
    int i;
    struct xuake_view *view;

    for (i = 0; i < XUAKE_WORKSPACES; i++) {
        wl_list_for_each(view, &server->workspaces[i], link) {
            if (view->view_id == view_id)
                return view;
        }
    }

    return NULL;
}

int
get_ws_by_view_id(struct xuake_server *server, int view_id)
{
    int i;
    struct xuake_view *view;

    for (i = 0; i < XUAKE_WORKSPACES; i++) {
        wl_list_for_each(view, &server->workspaces[i], link) {
            if (view->view_id == view_id)
                return i;
        }
    }

    return -1;
}



void
close_view(struct xuake_server *server, struct xuake_view *view)
{
    struct wlr_xdg_popup *popup = NULL;

    if (!view)
        return;

    switch (view->type) {
    case XUAKE_XWAYLAND_VIEW:
        wlr_xwayland_surface_close(view->xw_surface);
        break;
    case XUAKE_XDG_SHELL_VIEW:
        wl_list_for_each(popup, &view->xdg_surface->popups, link) {
            wlr_xdg_popup_destroy(popup->base);
        }
        wlr_xdg_toplevel_send_close(view->xdg_surface);
        break;
    }
}

static bool
view_at(struct xuake_view *view, double lx, double ly, struct wlr_surface **surface, double *sx, double *sy)
{
    /*
     * XDG toplevels may have nested surfaces, such as popup windows for context
     * menus or tooltips. This function tests if any of those are underneath the
     * coordinates lx and ly (in output Layout Coordinates). If so, it sets the
     * surface pointer to that wlr_surface and the sx and sy coordinates to the
     * coordinates relative to that surface's top-left corner.
     */
    double view_sx = lx - view->x;
    double view_sy = ly - view->y;
    int width, height;
    double _sx, _sy;
    struct wlr_surface *_surface = NULL;

    struct wlr_surface_state *state;


    switch (view->type) {
    case XUAKE_XWAYLAND_VIEW:
        *surface = view->xw_surface->surface;
        if (!*surface)
            return false;
        *sx = view_sx;
        *sy = view_sy;
        width = view->xw_surface->width;
        height = view->xw_surface->height;
        if (view_sx >= 0 && view_sx <= width && view_sy >= 0 && view_sy <= height)
            return true;
        break;
    case XUAKE_XDG_SHELL_VIEW:
        _surface = wlr_xdg_surface_surface_at(view->xdg_surface, view_sx, view_sy, &_sx, &_sy);
        if (_surface != NULL) {
            *sx = _sx;
            *sy = _sy;
            *surface = _surface;
            return true;
        }
        width = view->xdg_surface->surface->current.width;
        height = view->xdg_surface->surface->current.height;
        break;
    }

    return false;
}

struct xuake_view *
desktop_view_at(struct xuake_server *server, double lx, double ly,
    struct wlr_surface **surface, double *sx, double *sy)
{
    /* This iterates over all of our surfaces and attempts to find one under the
     * cursor. This relies on server->views being ordered from top-to-bottom. */
    struct xuake_view *view;
#if 0
    wl_list_for_each(view, &server->unmanaged, link) {
        if (view_at(view, lx, ly, surface, sx, sy)) {
            return view;
        }
    }
#endif
    wl_list_for_each(view, &server->workspaces[server->ws], link) {
        if (view_at(view, lx, ly, surface, sx, sy)) {
            return view;
        }
    }
    return NULL;
}

void
process_cursor_resize(struct xuake_server *server, uint32_t time)
{
    /*
     * Resizing the grabbed view can be a little bit complicated, because we
     * could be resizing from any corner or edge. This not only resizes the view
     * on one or two axes, but can also move the view if you resize from the top
     * or left edges (or top-left corner).
     *
     * Note that I took some shortcuts here. In a more fleshed-out compositor,
     * you'd wait for the client to prepare a buffer at the new size, then
     * commit any movement that was prepared.
     */
    struct xuake_view *view = server->grabbed_view;
    double dx = server->cursor->x - server->grab_x;
    double dy = server->cursor->y - server->grab_y;
    double x = view->x;
    double y = view->y;
    int width = server->grab_width;
    int height = server->grab_height;
    int i;
    uint32_t *d;
    if (server->resize_edges & WLR_EDGE_TOP) {
        y += dy;
        server->grab_y += dy;
        height -= dy;
        if (height < 1) {
            y += height;
        }
    } else if (server->resize_edges & WLR_EDGE_BOTTOM) {
        height += dy;
        server->grab_y += dy;
    }
    if (server->resize_edges & WLR_EDGE_LEFT) {
        x += dx;
        server->grab_x += dx;
        width -= dx;
        if (width < 1) {
            x += width;
        }
    } else if (server->resize_edges & WLR_EDGE_RIGHT) {
        width += dx;
        server->grab_x += dx;
    }
    view->x = x;
    view->y = y;
    if (height < XUAKE_MINHEIGHT)
        height = XUAKE_MINHEIGHT;
    if (width < XUAKE_MINWIDTH)
        width = XUAKE_MINWIDTH;
    server->grab_width = width;
    server->grab_height = height;

    enum wl_shell_surface_resize wsh_edges;
    switch (view->type) {
    case XUAKE_XWAYLAND_VIEW:
        wlr_xwayland_surface_configure(view->xw_surface, x, y, width, height);
        break;
    case XUAKE_XDG_SHELL_VIEW:
        wlr_xdg_toplevel_set_size(view->xdg_surface, width, height);
        break;
    }
}

void
xkutil_warp_view(struct xuake_server *server, int view_id, int x, int y, int width, int height)
{
    struct xuake_view *view;
    struct wlr_box geo_box;
    int i;


    view = get_view_by_id(server, view_id);

    if (!view)
        return;

    if (x > INT_MIN && y > INT_MIN) {
        view->x = x;
        view->y = y;
    }

    if (width == 0 || height == 0) {
        switch (view->type) {
        case XUAKE_XWAYLAND_VIEW:
            width = view->xw_surface->width;
            height = view->xw_surface->height;
            break;
        case XUAKE_XDG_SHELL_VIEW:
            wlr_xdg_surface_get_geometry(view->xdg_surface, &geo_box);
            width = geo_box.width;
            height = geo_box.height;
            break;
        }
    }

    if (width < XUAKE_MINWIDTH)
        width = XUAKE_MINWIDTH;

    if (height < XUAKE_MINHEIGHT)
        height = XUAKE_MINHEIGHT;

    switch (view->type) {
    case XUAKE_XWAYLAND_VIEW:
        wlr_xwayland_surface_configure(view->xw_surface, x, y, width, height);
        break;
    case XUAKE_XDG_SHELL_VIEW:
        wlr_xdg_toplevel_set_size(view->xdg_surface, width, height);
        break;
    }
}

void
surface_set_fullscreen(struct xuake_view *view, bool fullscreen)
{
    struct xuake_server *server = view->server;
    struct wlr_seat *seat = server->seat;
    struct wlr_output *output;
    struct wlr_output_layout_output *olay;
    int32_t width, height;

    if (fullscreen) {
        output = wlr_output_layout_output_at(server->output_layout, view->x, view->y);
        if (!output)
            return;
        olay = wlr_output_layout_get(server->output_layout, output);
        if (!olay)
            return;

        wlr_output_effective_resolution(output, &width, &height);
        view->fullscreen = true;

        wlr_xdg_surface_get_geometry(view->xdg_surface, &view->saved_geom);
        view->saved_geom.x = view->x;
        view->saved_geom.y = view->y;
        view->x = olay->x;
        view->y = olay->y;
        wlr_xdg_toplevel_set_fullscreen(view->xdg_surface, view->fullscreen);
        wlr_xdg_toplevel_set_size(view->xdg_surface, width, height);
    } else {
        view->x = view->saved_geom.x;
        view->y = view->saved_geom.y;
        if (view->y == 0)
            view->y = 4;
        view->fullscreen = false;

        wlr_xdg_toplevel_set_fullscreen(view->xdg_surface, view->fullscreen);
        wlr_xdg_toplevel_set_size(view->xdg_surface, view->saved_geom.width, view->saved_geom.height);
    }
}

void
surface_map(struct wl_listener *listener, void *data)
{
    /* Called when the surface is mapped, or ready to display on-screen. */
    struct xuake_view *view = wl_container_of(listener, view, map);
    view->mapped = true;
    switch (view->type) {
    case XUAKE_XWAYLAND_VIEW:
        if (view->is_unmanaged) {
            view->x = view->xw_surface->x;
            view->y = view->xw_surface->y;
        } else {
            if (view->server->monacle) {
                xwayland_set_fullscreen(view, true);
            } else {
                if (view->y == 0)
                    view->y = 4;
                wlr_xwayland_surface_configure(
                    view->xw_surface, view->x, view->y,
                    view->xw_surface->width, view->xw_surface->height
                );
            }
        }
        focus_view(view, view->xw_surface->surface);
        break;
    case XUAKE_XDG_SHELL_VIEW:
        if (view->server->monacle) {
            surface_set_fullscreen(view, true);
        } else {
            if (view->y == 0)
                view->y = 4;
        }
        focus_view(view, view->xdg_surface->surface);
        break;
    }
    xklua_run_callback(&view->server->lua_callbacks.map_surface, "i", view->view_id);
}

void
surface_unmap(struct wl_listener *listener, void *data)
{
    /* Called when the surface is unmapped, and should no longer be shown. */
    struct xuake_view *view = wl_container_of(listener, view, unmap);
    struct xuake_server *server = view->server;
    struct wlr_seat *seat = server->seat;
    view->mapped = false;
    if (view->focused) {
        wlr_seat_keyboard_clear_focus(seat);
        view->focused = 0;
    }
    xklua_run_callback(&server->lua_callbacks.unmap_surface, "i", view->view_id);
}

void
surface_request_fullscreen(struct wl_listener *listener, void *data)
{
    /* Called when the surface is unmapped, and should no longer be shown. */
    struct xuake_view *view = wl_container_of(listener, view, request_fullscreen);
    struct wlr_xdg_toplevel_set_fullscreen_event *event = data;


    if (!view->mapped) {
        return;
    }

    surface_set_fullscreen(view, event->fullscreen);
}

static void
xdg_surface_destroy(struct wl_listener *listener, void *data)
{
    /* Called when the surface is destroyed and should never be shown again. */
    struct xuake_view *view = wl_container_of(listener, view, destroy);
    surface_unmap(&view->unmap, NULL);
    wl_list_remove(&view->link);
    free(view);
}

void
xkutil_set_monacle(struct xuake_server *server, bool mono)
{
    int ws;
    struct xuake_view *view;

    server->monacle = mono;

    for (ws = 0; ws < XUAKE_WORKSPACES; ws++) {
        wl_list_for_each(view, &server->workspaces[ws], link) {
            if (!view->mapped)
                continue;
            switch (view->type) {
            case XUAKE_XDG_SHELL_VIEW:
                surface_set_fullscreen(view, mono);
                break;
            case XUAKE_XWAYLAND_VIEW:
                xwayland_set_fullscreen(view, mono);
                break;
            }
        }
    }
}

void
begin_interactive(struct xuake_view *view, enum xuake_cursor_mode mode, uint32_t edges)
{
    /* This function sets up an interactive move or resize operation, where the
     * compositor stops propegating pointer events to clients and instead
     * consumes them itself, to move or resize windows. */
    struct xuake_server *server = view->server;
    struct wlr_surface *focused_surface = server->seat->pointer_state.focused_surface;
    printf("Begin Interactive\n");
    if (view->fullscreen)
        return;
    server->grabbed_view = view;
    server->cursor_mode = mode;
    struct wlr_box geo_box;
    switch (view->type) {
    case XUAKE_XWAYLAND_VIEW:
        geo_box.x = view->x;
        geo_box.y = view->y;
        geo_box.width = view->xw_surface->width;
        geo_box.height = view->xw_surface->height;
        break;
    case XUAKE_XDG_SHELL_VIEW:
        wlr_xdg_surface_get_geometry(view->xdg_surface, &geo_box);
        break;
    default:
        return;
    }

    if (mode == XUAKE_CURSOR_MOVE) {
        server->grab_x = server->cursor->x - view->x;
        server->grab_y = server->cursor->y - view->y;
    } else {
        server->grab_x = server->cursor->x;
        server->grab_y = server->cursor->y;
    }
    server->grab_width = geo_box.width;
    server->grab_height = geo_box.height;
    server->resize_edges = edges;
}

static void
xdg_toplevel_request_move(struct wl_listener *listener, void *data)
{
    /* This event is raised when a client would like to begin an interactive
     * move, typically because the user clicked on their client-side
     * decorations. Note that a more sophisticated compositor should check the
     * provied serial against a list of button press serials sent to this
     * client, to prevent the client from requesting this whenever they want. */
    //struct xuake_view *view = wl_container_of(listener, view, request_move);
    //begin_interactive(view, XUAKE_CURSOR_MOVE, 0);
    /* SSD or GTFO
     */
    return;
}

static void
xdg_toplevel_request_resize(struct wl_listener *listener, void *data)
{
    /* This event is raised when a client would like to begin an interactive
     * resize, typically because the user clicked on their client-side
     * decorations. Note that a more sophisticated compositor should check the
     * provied serial against a list of button press serials sent to this
     * client, to prevent the client from requesting this whenever they want. */
    //struct wlr_xdg_toplevel_resize_event *event = data;
    //struct xuake_view *view = wl_container_of(listener, view, request_resize);
    //begin_interactive(view, XUAKE_CURSOR_RESIZE, event->edges);
    /* SSD or GTFO
     */
    return;
}

void
server_new_xdg_surface(struct wl_listener *listener, void *data)
{
    /* This event is raised when wlr_xdg_shell receives a new xdg surface from a
     * client, either a toplevel (application window) or popup. */
    struct xuake_server *server = wl_container_of(listener, server, new_xdg_surface);
    struct wlr_xdg_surface *xdg_surface = data;
    if (xdg_surface->role != WLR_XDG_SURFACE_ROLE_TOPLEVEL) {
        return;
    }

    /* Allocate a xuake_view for this surface */
    struct xuake_view *view = calloc(1, sizeof(struct xuake_view));
    memset(view, 0, sizeof(struct xuake_view));
    view->server = server;
    view->xdg_surface = xdg_surface;
    view->view_id = get_next_view_id();

    xdg_surface->data = view;
    xdg_surface->surface->data = view;

    /* Listen to the various events it can emit */
    view->map.notify = surface_map;
    wl_signal_add(&xdg_surface->events.map, &view->map);
    view->unmap.notify = surface_unmap;
    wl_signal_add(&xdg_surface->events.unmap, &view->unmap);
    view->destroy.notify = xdg_surface_destroy;
    wl_signal_add(&xdg_surface->events.destroy, &view->destroy);

    /* cotd */
    struct wlr_xdg_toplevel *toplevel;
    if (xdg_surface->role == WLR_XDG_SURFACE_ROLE_TOPLEVEL) {
        toplevel = xdg_surface->toplevel;
        view->request_move.notify = xdg_toplevel_request_move;
        wl_signal_add(&toplevel->events.request_move, &view->request_move);
        view->request_resize.notify = xdg_toplevel_request_resize;
        wl_signal_add(&toplevel->events.request_resize, &view->request_resize);

        view->request_fullscreen.notify = surface_request_fullscreen;
        wl_signal_add(&toplevel->events.request_fullscreen, &view->request_fullscreen);
    }

    /* Add it to the list of views. */
    wl_list_insert(&server->workspaces[server->ws], &view->link);

    xklua_run_callback(&server->lua_callbacks.new_surface, "i", view->view_id);
}
