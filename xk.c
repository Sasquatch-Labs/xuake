#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <wayland-client.h>
#include <wayland-client-protocol.h>

#include "xuake-control-client.h"

#include "xkdefs.h"

struct wl_display *display = NULL;
struct wl_compositor *compositor = NULL;
struct wl_seat *seat = NULL;
struct xuake_control *xc = NULL;

static void
seat_handle_capabilities(void *data, struct wl_seat *seat, enum wl_seat_capability caps)
{
}

static void
seat_handle_name(void *data, struct wl_seat *wl_seat, const char *name)
{
}

static const struct wl_seat_listener seat_listener = {
    seat_handle_capabilities,
    seat_handle_name
};

void
handle_about_info(void *data, struct xuake_control *xc, const char *info)
{
    printf("%s\n", info);
    exit(0);
}

void
handle_view_info(void *data, struct xuake_control *xuake_control, int32_t view_id,
    int32_t workspace, int32_t type, int32_t fullscreen, int32_t output,
    int32_t x, int32_t y, int32_t width, int32_t height, const char *title)
{
    if (view_id == 0 && type == 0)
        exit(0);

    printf("%d %d %d [%d,%d+%d+%d] %s\n", view_id, workspace, type, x, y, width, height, title);
}

static struct xuake_control_listener xc_listener = {
    .about_info = handle_about_info,
    .view_info = handle_view_info
};

static void
global_registry_handler(void *data, struct wl_registry *registry, uint32_t id,
        const char *interface, uint32_t version)
{
    //printf("registry_handler: %d %s %d\n", id, interface, version);
    if (strcmp(interface, "wl_compositor") == 0) {
        compositor = wl_registry_bind(registry, id, &wl_compositor_interface, 1);
    } else if (strcmp(interface, wl_seat_interface.name) == 0) {
        seat = wl_registry_bind(registry, id, &wl_seat_interface, version);
        wl_seat_add_listener(seat, &seat_listener, NULL);
    } else if (strcmp(interface, xuake_control_interface.name) == 0) {
        xc = wl_registry_bind(registry, id, &xuake_control_interface, version);
        xuake_control_add_listener(xc, &xc_listener, NULL);
    }
}

static void
global_registry_remover(void *data, struct wl_registry *registry, uint32_t id)
{
    //printf("Got a registry losing event for %d\n", id);
}

static const struct wl_registry_listener registry_listener = {
    global_registry_handler,
    global_registry_remover
};

void
usage(void)
{
    printf("Usage: xk <subcmd> [args]\n"
        "  help: this message\n"
        "  version: print version info for xk\n"
        "  about: query xuake compositor for version information\n"
        "  exit: request xuake compositor to terminate\n"
        "  impulse <slot-number>: trigger impulse lua function in xuake, slot can be from 0 to %d\n"
        "  ws <workspace>: switch workspaces\n"
        "  ls [workspace]: list views\n"
        "  focus <view_id>: focus view by id\n"
        "  close <view_id>: close view by id\n"
        "  warp <view_id> <x>,<y>+<w>+<h>: move and resize view by id\n"
        "  free: set free layout mode (default)\n"
        "  mono: set monacle layout mode\n",
        XK_MAX_IMPULSE
    );
}

int
main(int argc, char **argv)
{
    int i;

    display = wl_display_connect(NULL);

    if (display == NULL) {
        fprintf(stderr, "Can't connect to display\n");
        return 1;
    }

    struct wl_registry *registry = wl_display_get_registry(display);
    wl_registry_add_listener(registry, &registry_listener, NULL);

    wl_display_dispatch(display);
    //wl_display_roundtrip(display);

    if (compositor == NULL) {
        fprintf(stderr, "Can't find compositor\n");
        return 1;
    }

    // XXX: this could be a lot better.
    if (xc && argc > 1) {
        if (!strcmp("about", argv[1])) {
            xuake_control_about(xc);

            while (wl_display_dispatch(display) != -1);
        } else if (!strcmp("exit", argv[1])) {
            xuake_control_exit(xc);
            wl_display_dispatch(display);
        } else if (!strcmp("free", argv[1])) {
            xuake_control_set_monacle(xc, 0);
            wl_display_dispatch(display);
        } else if (!strcmp("mono", argv[1])) {
            xuake_control_set_monacle(xc, 1);
            wl_display_dispatch(display);
        } else if (!strcmp("ls", argv[1]) && argc == 2) {
            xuake_control_list_views(xc, -1);
            while (wl_display_dispatch(display) != -1);
        } else if (!strcmp("ls", argv[1]) && argc == 3) {
            int ws = strtol(argv[2], NULL, 10);
            xuake_control_list_views(xc, ws);
            while (wl_display_dispatch(display) != -1);
        } else if (!strcmp("ws", argv[1]) && argc == 3) {
            int ws = strtol(argv[2], NULL, 10);
            xuake_control_workspace(xc, ws);
            wl_display_dispatch(display);
        } else if (!strcmp("focus", argv[1]) && argc == 3) {
            int view_id = strtol(argv[2], NULL, 10);
            xuake_control_focus_view(xc, view_id);
            wl_display_dispatch(display);
        } else if (!strcmp("impulse", argv[1]) && argc > 2) {
            uint32_t slot = strtol(argv[2], NULL, 10);
            if (!errno && slot <= XK_MAX_IMPULSE) {
                xuake_control_impulse(xc, slot);
                wl_display_dispatch(display);
            }
        } else if (!strcmp("warp", argv[1]) && argc > 3) {
            char *s, *S[4];
            int i, x, y, w = 0, h = 0;
            int view_id = strtol(argv[2], NULL, 10);
            if (errno || !view_id)
                exit(1);

            S[0] = argv[3];
            for (s = argv[3], i = 1; *s; s++) {
                switch (*s) {
                case ',': case '+':
                    *s = 0;
                    S[i++] = ++s;
                    break;
                }
            }
            x = strtol(S[0], NULL, 10);
            y = strtol(S[1], NULL, 10);
            if (i >= 3) {
                w = strtol(S[2], NULL, 10);
                h = strtol(S[3], NULL, 10);
            }

            xuake_control_warp_view(xc, view_id, x, y, w, h);
            wl_display_dispatch(display);
        } else if (!strcmp("close", argv[1]) && argc > 2) {
            int view_id = strtol(argv[2], NULL, 10);
            if (errno || !view_id)
                exit(1);
            xuake_control_close_view(xc, view_id);
            wl_display_dispatch(display);
        } else if (!strcmp("version", argv[1])) {
            printf("xk version %s\n", XUAKE_VERSION);
        } else if (!strcmp("help", argv[1])) {
            usage();
        } else {
            usage();
        }
    } else {
        usage();
    }

    wl_display_disconnect(display);

    return 0;
}
