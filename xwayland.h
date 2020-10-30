#ifndef __XWAYLAND_H
#define __XWAYLAND_H

/* Blatantly stolen from sway -- jaf
 */

#include <wlr/xwayland.h>
#include <xcb/xproto.h>

enum xuake_atom_name {
    XUAKE_WINDOW_TYPE_NORMAL,
    XUAKE_WINDOW_TYPE_DIALOG,
    XUAKE_WINDOW_TYPE_UTILITY,
    XUAKE_WINDOW_TYPE_TOOLBAR,
    XUAKE_WINDOW_TYPE_SPLASH,
    XUAKE_WINDOW_TYPE_MENU,
    XUAKE_WINDOW_TYPE_DROPDOWN_MENU,
    XUAKE_WINDOW_TYPE_POPUP_MENU,
    XUAKE_WINDOW_TYPE_TOOLTIP,
    XUAKE_WINDOW_TYPE_NOTIFICATION,
    XUAKE_STATE_MODAL,
    XUAKE_ATOM_LAST,
};

struct xuake_xwayland {
    struct wlr_xwayland *wlr_xwayland;
    struct wlr_xcursor_manager *xcursor_manager;

    xcb_atom_t atoms[XUAKE_ATOM_LAST];
};

void handle_xwayland_ready(struct wl_listener *listener, void *data);
void handle_xwayland_surface(struct wl_listener *listener, void *data);

#endif // __XWAYLAND_H
