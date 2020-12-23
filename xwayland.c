#include <stdlib.h>
#include "xuake.h"
#include "xwayland.h"
#include "xklua.h"

// much of this is cribbed from sway -- jaf

static const char *xuake_atom_map[XUAKE_ATOM_LAST] = {
    "_NET_WM_WINDOW_TYPE_NORMAL",
    "_NET_WM_WINDOW_TYPE_DIALOG",
    "_NET_WM_WINDOW_TYPE_UTILITY",
    "_NET_WM_WINDOW_TYPE_TOOLBAR",
    "_NET_WM_WINDOW_TYPE_SPLASH",
    "_NET_WM_WINDOW_TYPE_MENU",
    "_NET_WM_WINDOW_TYPE_DROPDOWN_MENU",
    "_NET_WM_WINDOW_TYPE_POPUP_MENU",
    "_NET_WM_WINDOW_TYPE_TOOLTIP",
    "_NET_WM_WINDOW_TYPE_NOTIFICATION",
    "_NET_WM_STATE_MODAL",
};

void
xwayland_destroy(struct wl_listener *listener, void *data)
{
    /* Called when the surface is destroyed and should never be shown again. */
    struct xuake_view *view = wl_container_of(listener, view, destroy);
    surface_unmap(&view->unmap, NULL);
    wl_list_remove(&view->link);
    free(view);
}

void
xwayland_request_move(struct wl_listener *listener, void *data)
{
    printf("Xwayland requested move\n");
}

void
xwayland_request_resize(struct wl_listener *listener, void *data)
{
}

void
xwayland_request_configure(struct wl_listener *listener, void *data)
{
    struct wlr_xwayland_surface_configure_event *ev = data;
    struct xuake_view *view = wl_container_of(listener, view, request_configure);

    struct wlr_xwayland_surface *xsurface = view->xw_surface;

    if (!xsurface)
        return;

    if (view->is_unmanaged) {
        view->x = ev->x;
        view->y = ev->y;
        wlr_xwayland_surface_configure(xsurface, ev->x, ev->y, ev->width, ev->height);
    } else if (!view->mapped) {
        wlr_xwayland_surface_configure(xsurface, ev->x, ev->y, ev->width, ev->height);
    }

    printf("Xwayland requested configure\n");
}

void
xwayland_set_fullscreen(struct xuake_view *view, bool fullscreen)
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

        view->saved_geom.x = view->x;
        view->saved_geom.y = view->y;
        view->x = olay->x;
        view->y = olay->y;
        view->saved_geom.width = view->xw_surface->width;
        view->saved_geom.height = view->xw_surface->height;
        wlr_xwayland_surface_set_fullscreen(view->xw_surface, view->fullscreen);
        wlr_xwayland_surface_configure(view->xw_surface, 0, 0, width, height);
    } else {
        view->x = view->saved_geom.x;
        view->y = view->saved_geom.y;
        if (view->y == 0)
            view->y = 4;
        view->fullscreen = false;

        wlr_xwayland_surface_set_fullscreen(view->xw_surface, view->fullscreen);
        wlr_xwayland_surface_configure(view->xw_surface, view->x, view->y, view->saved_geom.width, view->saved_geom.height);
    }
}

void
xwayland_request_fullscreen(struct wl_listener *listener, void *data)
{
    /* Called when the surface is unmapped, and should no longer be shown. */
    struct xuake_view *view = wl_container_of(listener, view, request_fullscreen);
    struct wlr_xwayland_surface *xsurface = data;


    if (!view->mapped) {
        return;
    }

    xwayland_set_fullscreen(view, xsurface->fullscreen);
}

void
handle_xwayland_ready(struct wl_listener *listener, void *data)
{
    struct xuake_server *server = wl_container_of(listener, server, xwayland_ready);
    struct xuake_xwayland *xwayland = &server->xwayland;

    xcb_connection_t *xcb_conn = xcb_connect(NULL, NULL);
    int err = xcb_connection_has_error(xcb_conn);
    if (err) {
        //sway_log(SWAY_ERROR, "XCB connect failed: %d", err);
        return;
    }

    xcb_intern_atom_cookie_t cookies[XUAKE_ATOM_LAST];
    for (size_t i = 0; i < XUAKE_ATOM_LAST; i++) {
        cookies[i] =
        xcb_intern_atom(xcb_conn, 0, strlen(xuake_atom_map[i]), xuake_atom_map[i]);
    }
    for (size_t i = 0; i < XUAKE_ATOM_LAST; i++) {
        xcb_generic_error_t *error = NULL;
        xcb_intern_atom_reply_t *reply =
        xcb_intern_atom_reply(xcb_conn, cookies[i], &error);
        if (reply != NULL && error == NULL) {
            xwayland->atoms[i] = reply->atom;
        }
        free(reply);

        if (error != NULL) {
            //sway_log(SWAY_ERROR, "could not resolve atom %s, X11 error code %d", atom_map[i], error->error_code);
            free(error);
            break;
        }
    }

    xcb_disconnect(xcb_conn);
}

void
handle_xwayland_surface(struct wl_listener *listener, void *data)
{
    struct xuake_server *server = wl_container_of(listener, server, xwayland_surface);
    struct wlr_xwayland_surface *xsurface = data;

    printf("handle_xwayland_surface: xsurface == %p\n", xsurface);

    struct xuake_view *xw_view = calloc(1, sizeof(struct xuake_view));
    if (xw_view == NULL) {
        return;
    }

    xw_view->server = server;
    
    xw_view->xw_surface = xsurface;

    xw_view->type = XUAKE_XWAYLAND_VIEW;

    xw_view->view_id = get_next_view_id();

    wl_signal_add(&xsurface->events.destroy, &xw_view->destroy);
    xw_view->destroy.notify = xwayland_destroy;

    wl_signal_add(&xsurface->events.request_move, &xw_view->request_move);
    xw_view->request_move.notify = xwayland_request_move;

    wl_signal_add(&xsurface->events.request_resize, &xw_view->request_resize);
    xw_view->request_resize.notify = xwayland_request_resize;

    wl_signal_add(&xsurface->events.unmap, &xw_view->unmap);
    xw_view->unmap.notify = surface_unmap;

    wl_signal_add(&xsurface->events.map, &xw_view->map);
    xw_view->map.notify = surface_map;

    wl_signal_add(&xsurface->events.request_configure, &xw_view->request_configure);
    xw_view->request_configure.notify = xwayland_request_configure;

    wl_signal_add(&xsurface->events.request_fullscreen, &xw_view->request_fullscreen);
    xw_view->request_fullscreen.notify = xwayland_request_fullscreen;

    xsurface->data = xw_view;

    if (xsurface->override_redirect) {
        xw_view->is_unmanaged = 1;
        wl_list_insert(&server->unmanaged, &xw_view->link);
    } else {
        wl_list_insert(&server->workspaces[server->ws], &xw_view->link);
    }

    xklua_run_callback(&server->lua_callbacks.new_surface, "i", xw_view->view_id);
#if 0
    /*
     * Unimplemented events.  Will want to add some of these to support window operations
     * in Lua and xk/xuake_control.
     */
struct wl_listener map;
struct wl_listener unmap;
struct wl_listener request_move;
struct wl_listener request_resize;

    wl_signal_add(&xsurface->events.request_activate,
    &xwayland_view->request_activate);
    xwayland_view->request_activate.notify = handle_request_activate;

    wl_signal_add(&xsurface->events.set_title, &xwayland_view->set_title);
    xwayland_view->set_title.notify = handle_set_title;

    wl_signal_add(&xsurface->events.set_class, &xwayland_view->set_class);
    xwayland_view->set_class.notify = handle_set_class;

    wl_signal_add(&xsurface->events.set_role, &xwayland_view->set_role);
    xwayland_view->set_role.notify = handle_set_role;

    wl_signal_add(&xsurface->events.set_window_type,
    &xwayland_view->set_window_type);
    xwayland_view->set_window_type.notify = handle_set_window_type;

    wl_signal_add(&xsurface->events.set_hints, &xwayland_view->set_hints);
    xwayland_view->set_hints.notify = handle_set_hints;

    wl_signal_add(&xsurface->events.set_decorations,
    &xwayland_view->set_decorations);
    xwayland_view->set_decorations.notify = handle_set_decorations;

#endif
}
