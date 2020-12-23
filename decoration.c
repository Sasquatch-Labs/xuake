#include <stdlib.h>
#include "xuake.h"

static void
decoration_handle_destroy(struct wl_listener *listener, void *data)
{
    struct xuake_decoration *deco = wl_container_of(listener, deco, destroy);

    wl_list_remove(&deco->link);
    free(deco);
}

static void
decoration_handle_request_mode(struct wl_listener *listener, void *data)
{
    struct xuake_decoration *deco = wl_container_of(listener, deco, request_mode);

    if (deco->type == XUAKE_DECO_XDG) {
        wlr_xdg_toplevel_decoration_v1_set_mode(deco->wlr_xdg_decoration, WLR_XDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE);
    }
    /* Currently, we ignore CSD mode requests from clients
     * This is non-compliant, but fuck CSD and any client that
     * thinks it knows better than Xuake.  Maybe someday I'll
     * clean this up and properly support things, but CSD craps
     * on part of the purpose of this compositor, so probably not.
     * I'm pretty curmudgeonly about this stuff. */
}

void
handle_decoration_common(void *data, enum xuake_decoration_type type)
{
    struct wlr_xdg_toplevel_decoration_v1 *xdg_deco = NULL;
    struct wlr_server_decoration *srv_deco = NULL;
    struct xuake_view *view = NULL;

    struct xuake_decoration *deco = calloc(1, sizeof(*deco));
    if (deco == NULL) {
        return;
    }

    printf("handle_decoration_common: ");
    if (type == XUAKE_DECO_XDG) {
        printf("XDG\n");
        xdg_deco = data;
        view = xdg_deco->surface->data;
        deco->wlr_xdg_decoration = xdg_deco;

        wl_signal_add(&xdg_deco->events.destroy, &deco->destroy);
        wl_signal_add(&xdg_deco->events.request_mode, &deco->request_mode);
    } else if (type == XUAKE_DECO_SRV) {
        printf("SRV\n");
        srv_deco = data;
        view = srv_deco->surface->data;
        deco->wlr_srv_decoration = srv_deco;

        wl_signal_add(&srv_deco->events.destroy, &deco->destroy);
        wl_signal_add(&srv_deco->events.mode, &deco->request_mode);
    } else {
        printf("UNK\n");
        free(deco);
        return;
    }

    view->decoration = deco;
    deco->view = view;

    deco->destroy.notify = decoration_handle_destroy;
    deco->request_mode.notify = decoration_handle_request_mode;

    wl_list_insert(&view->server->decorations, &deco->link);

    if (type == XUAKE_DECO_XDG)
        decoration_handle_request_mode(&deco->request_mode, deco);
    else
        return;
}

void
handle_srv_decoration(struct wl_listener *listener, void *data)
{
    handle_decoration_common(data, XUAKE_DECO_SRV);
}

void
handle_xdg_decoration(struct wl_listener *listener, void *data)
{
    handle_decoration_common(data, XUAKE_DECO_XDG);
}
