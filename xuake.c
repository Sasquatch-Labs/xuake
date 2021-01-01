//#define _POSIX_C_SOURCE 200112L
#define _GNU_SOURCE
#include <stdlib.h>
#include <unistd.h>

#include "xuake.h"
#include "readpng.h"
#include "ft.h"
#include "term.h"
#include "xklua.h"
#include "key.h"

static int next_view_id = 1;

struct wlr_surface*
get_wlr_surface_from_view(struct xuake_view *view)
{
    switch (view->type) {
    case XUAKE_XWAYLAND_VIEW:
        return view->xw_surface->surface;
    case XUAKE_XDG_SHELL_VIEW:
        return view->xdg_surface->surface;
    default:
        return NULL;
    }
}

int
get_next_view_id(void)
{
    return next_view_id++;
}

static void
dcm_handle_new_device(struct wl_listener *listener, void *data)
{
    printf("New data control device\n");
}

void
init_xkterm(struct xuake_server *server)
{
    struct xuake_output *xkt_output;
    uint32_t w, h;
    uint32_t *d;
    unsigned int i, j, spaces, len, widget_width;
    int flags, cell_width, cell_height;
    pid_t p;
    size_t dirtysz;

    xkt_output = wl_container_of(server->outputs.next, xkt_output, link);

    cell_width = server->conf.xkt.cell_width;
    cell_height = server->conf.xkt.cell_height;
    server->xkt.conf = &server->conf;

    w = xkt_output->wlr_output->width;
    h = xkt_output->wlr_output->height/2;
    printf("init_xkterm: [width height] [%d %d]\n", w, h);

    server->xkt_mode = 0;

    widget_width = w/16;
    if (widget_width <= 50) {
        widget_width = 0;
    } else if (widget_width < 80) {
        widget_width = 80;
    }

    server->xkt.pixw = w - widget_width;
    server->xkt.pixh = h;
    server->xkt.cellw = (w - widget_width)/cell_width;
    server->xkt.cellh = h/cell_height;

    server->xkt.data = malloc(server->xkt.pixw*h*XUAKE_PIXEL_BYTES*sizeof(unsigned char));
    if (!server->xkt.data) {
        printf("MALLOC FAILED");
        server->xkt.texture = NULL;
        return;
    }

    printf("init_xkterm: data: %p\n", server->xkt.data);

    p = xkterm_forkpty(&server->xkt);

    if (p == 0) {
        // child
        exec_startcmd(server->conf.xkt.cmd);
        exit(125); // never reached
    } else if (p < 0) {
        return;
    }

    server->xkt.child = p;

    xkt_vte_init(&server->xkt);

    printf("init_xkterm: resize [%d, %d] -> [%d, %d]\n", w, h, server->xkt.cellw, server->xkt.cellh);

    xkterm_use_full_clear();
    xkterm_clear_full(&server->xkt, server->xkt.pixw, server->xkt.pixh, server->xkt.data);

    xkterm_render(&server->xkt, server->xkt.pixw, server->xkt.pixh, server->xkt.data);

    server->xkt.texture = wlr_texture_from_pixels(server->renderer, WL_SHM_FORMAT_ARGB8888, server->xkt.pixw*XUAKE_PIXEL_BYTES,
        server->xkt.pixw, server->xkt.pixh, server->xkt.data);

    server->widget.width = widget_width;
    if (widget_width) {
        server->widget.height = h;
        server->widget.data = init_widgets(server);
        render_widgets(server->widget.width, server->widget.height, server->widget.data);
        server->widget.texture = wlr_texture_from_pixels(server->renderer, WL_SHM_FORMAT_ARGB8888,
            server->widget.width*XUAKE_PIXEL_BYTES, server->widget.width, server->widget.height, server->widget.data);
    }
}

void
handle_xuake_control_impulse(struct wl_client *client, struct wl_resource *resource, uint32_t slot)
{
    xklua_impulse(resource->data, slot);
}

void
handle_xuake_control_exit(struct wl_client *client, struct wl_resource *resource)
{
    xkkey_xuake_exit(resource->data);
}

void
handle_xuake_control_about(struct wl_client *client, struct wl_resource *resource)
{
    xuake_control_send_about_info(resource, "Xuake, version " XUAKE_VERSION);
}

void
handle_xuake_control_list_views(struct wl_client *client, struct wl_resource *resource, int ws)
{
    struct xuake_server *server = resource->data;
    struct xuake_view *view;
    char *title;
    int w, h, ws_max;
    struct wlr_box geo_box;

    if (ws > XUAKE_WORKSPACES)
        goto out;

    if (ws < 0) {
        ws = 0;
        ws_max = XUAKE_WORKSPACES;
    } else {
        ws_max = ws+1;
    }

    for (; ws < ws_max; ws++) {
        wl_list_for_each(view, &server->workspaces[ws], link) {
            switch (view->type) {
            case XUAKE_XDG_SHELL_VIEW:
                title = view->xdg_surface->toplevel->title;
                wlr_xdg_surface_get_geometry(view->xdg_surface, &geo_box);
                w = geo_box.width;
                h = geo_box.height;
                break;
            case XUAKE_XWAYLAND_VIEW:
                title = view->xw_surface->title;
                w = view->xw_surface->width;
                h = view->xw_surface->height;
                break;
            }
            xuake_control_send_view_info(resource, view->view_id, ws, view->type,
                view->fullscreen, 0, view->x, view->y, w, h, title);
        }
    }

out:
    xuake_control_send_view_info(resource, 0, 0, 0, 0, 0, 0, 0, 0, 0, ""); // Done sentinel
}

void
handle_xuake_control_warp_view(struct wl_client *client, struct wl_resource *resource,
    int32_t view_id, int32_t x, int32_t y, int32_t width, int32_t height)
{
    struct xuake_server *server = resource->data;

    xkutil_warp_view(server, view_id, x, y, width, height);
}

void
handle_xuake_control_move_view(struct wl_client *client, struct wl_resource *resource, int32_t view_id, int32_t ws)
{
    struct xuake_server *server = resource->data;

    if (ws > XUAKE_WORKSPACES)
        return;

    xkutil_move_view(server, get_view_by_id(server, view_id), ws);
}

void
handle_xuake_control_close_view(struct wl_client *client, struct wl_resource *resource, int32_t view_id)
{
    struct xuake_server *server = resource->data;

    close_view(server, get_view_by_id(server, view_id));
}

void
xkutil_focus_view(struct xuake_server *server, struct xuake_view *view, int ws)
{
    struct xuake_view *top_view;

    if (!view || ws < 0 || ws > XUAKE_WORKSPACES)
        return;

    if (wl_list_length(&server->workspaces[ws]) == 0)
        return;

    if (ws != server->ws)
        xkkey_workspace(server, ws);

    if (wl_list_length(&server->workspaces[server->ws]) == 1) {
        top_view = wl_container_of(server->workspaces[server->ws].next, top_view, link);
        if (top_view != view)
            return;
        if (!top_view->focused)
            focus_view(top_view, get_wlr_surface_from_view(top_view));
        return;
    }
    wl_list_remove(&view->link);
    wl_list_insert(server->workspaces[ws].next, &view->link);
    if (get_wlr_surface_from_view(view))
        focus_view(view, get_wlr_surface_from_view(view));
}

void
handle_xuake_control_focus_view(struct wl_client *client, struct wl_resource *resource, int32_t view_id)
{
    struct xuake_server *server = resource->data;
    struct xuake_view *view = get_view_by_id(server, view_id);
    int ws = get_ws_by_view_id(server, view_id);

    xkutil_focus_view(server, view, ws);
}

void
handle_xuake_control_workspace(struct wl_client *client, struct wl_resource *resource, int32_t ws)
{
    struct xuake_server *server = resource->data;

    if (ws < 0 || ws > XUAKE_WORKSPACES)
        return;

    if (ws != server->ws)
        xkkey_workspace(server, ws);
}

void
handle_xuake_control_monacle(struct wl_client *client, struct wl_resource *resource, int32_t mode)
{
    struct xuake_server *server = resource->data;

    xkutil_set_monacle(server, mode);
}

static struct xuake_control_interface xuake_control_impl = {
    .about = handle_xuake_control_about,
    .exit = handle_xuake_control_exit,
    .impulse = handle_xuake_control_impulse,
    .list_views = handle_xuake_control_list_views,
    .warp_view = handle_xuake_control_warp_view,
    .move_view = handle_xuake_control_move_view,
    .close_view = handle_xuake_control_close_view,
    .focus_view = handle_xuake_control_focus_view,
    .workspace = handle_xuake_control_workspace,
    .set_monacle = handle_xuake_control_monacle
};

static void
xuake_control_handle_bind(struct wl_client *client, void *data, uint32_t version, uint32_t id)
{
    struct xuake_server *server = data;
    struct wl_resource *resource;

    resource = wl_resource_create(client, &xuake_control_interface, xuake_control_interface.version, id);

    // XXX: will need to set data eventually.
    wl_resource_set_implementation(resource, &xuake_control_impl, data, NULL);
}

void
check_for_x11_or_wl_backend(struct wlr_backend *b, void *d)
{
    struct xuake_server *server = (struct xuake_server *)d;

    if (wlr_backend_is_wl(b)) {
        server->testing = 1;
        return;
    }
    if (wlr_backend_is_x11(b)) {
        server->testing = 1;
        return;
    }
}

void
usage(char *name)
{
    printf("Xuake, version %s\n", XUAKE_VERSION);
    printf("Usage: %s [-b background image] [-c startup command] [-C lua config file]\n", name);
}

int
main(int argc, char *argv[])
{
    wlr_log_init(WLR_DEBUG, NULL);
    char *conffile_name = NULL;
    char *cwd = NULL;
    FILE *bgfile = NULL;
    char *xdg_runtime_dir;
    struct xuake_server server;

    xdg_runtime_dir = getenv("XDG_RUNTIME_DIR");
    if (!xdg_runtime_dir) {
        printf("XDG_RUNTIME_DIR not set!\n");
        return 1;
    }

    memset(&server, 0, sizeof(struct xuake_server));
    server.conf.batteries = 1;

    int c;
    while ((c = getopt(argc, argv, "rc:C:I:b:B:hv")) != -1) {
        switch (c) {
        case 'c':
            server.conf.xkt.cmd = strdup(optarg);
            break;
        case 'b':
            server.conf.bg_image = strdup(optarg);
            break;
        case 'B':
            server.conf.batteries = strtol(optarg, NULL, 10);
            break;
        case 'C':
            conffile_name = strdup(optarg);
            break;
        case 'I':
            server.conf.impulse_file = strdup(optarg);
            break;
        case 'r':
            server.conf.xkt.respawn = 1;
            break;
        case 'v':
            printf("Xuake, version %s\n", XUAKE_VERSION);
            return 0;
        default:
            usage(argv[0]);
            return 0;
        }
    }
    if (optind < argc) {
        usage(argv[0]);
        return 0;
    }

    if (!conffile_name)
        asprintf(&conffile_name, "%s/.xuake/conf.lua", getenv("HOME"));

    if (!server.conf.impulse_file)
        asprintf(&server.conf.impulse_file, "%s/.xuake/impulse.lua", getenv("HOME"));

    init_ft();
    memcpy(server.conf.xkt.colors, xkterm_get_colors(), 16*sizeof(uint32_t));
    wl_list_init(&server.keybindings);

    init_lua(&server);
    xklua_load_config(&server.conf, conffile_name);
    ft_load_default_fonts();

    ft_set_cell_size(&server.conf);
    xkterm_set_colors(server.conf.xkt.colors);
    if (server.conf.xkt.cell_width == 0 || server.conf.xkt.cell_height == 0) {
        printf("Could not load any fonts!\n");
        exit(111);
    }
    init_signals(&server);
    /* The Wayland display is managed by libwayland. It handles accepting
     * clients from the Unix socket, manging Wayland globals, and so on. */
    server.wl_display = wl_display_create();
    /* The backend is a wlroots feature which abstracts the underlying input and
     * output hardware. The autocreate option will choose the most suitable
     * backend based on the current environment, such as opening an X11 window
     * if an X11 server is running. The NULL argument here optionally allows you
     * to pass in a custom renderer if wlr_renderer doesn't meet your needs. The
     * backend uses the renderer, for example, to fall back to software cursors
     * if the backend does not support hardware cursors (some older GPUs
     * don't). */
    server.backend = wlr_backend_autocreate(server.wl_display, NULL);

    if (wlr_multi_is_empty(server.backend)) {
        printf("Backend is empty!\n");
        exit(137);
    }

    wlr_multi_for_each_backend(server.backend, check_for_x11_or_wl_backend, &server);

    /* If we don't provide a renderer, autocreate makes a GLES2 renderer for us.
     * The renderer is responsible for defining the various pixel formats it
     * supports for shared memory, this configures that for clients. */
    server.renderer = wlr_backend_get_renderer(server.backend);
    wlr_renderer_init_wl_display(server.renderer, server.wl_display);

    /* This creates some hands-off wlroots interfaces. The compositor is
     * necessary for clients to allocate surfaces and the data device manager
     * handles the clipboard. Each of these wlroots interfaces has room for you
     * to dig your fingers in and play with their behavior if you want. */
    server.compositor = wlr_compositor_create(server.wl_display, server.renderer);

    /* Creates an output layout, which a wlroots utility for working with an
     * arrangement of screens in a physical layout. */
    server.output_layout = wlr_output_layout_create();

    /* Configure a listener to be notified when new outputs are available on the
     * backend. */
    wl_list_init(&server.outputs);
    server.new_output.notify = server_new_output;
    wl_signal_add(&server.backend->events.new_output, &server.new_output);

    /* Set up our list of views and the xdg-shell. The xdg-shell is a Wayland
     * protocol which is used for application windows. For more detail on
     * shells, refer to my article:
     *
     * https://drewdevault.com/2018/07/29/Wayland-shells.html
     */
    server.ws = 0;
    for (c = 0; c < XUAKE_WORKSPACES; c++)
        wl_list_init(&server.workspaces[c]);
    wl_list_init(&server.unmanaged);
    server.xdg_shell = wlr_xdg_shell_create(server.wl_display);
    server.new_xdg_surface.notify = server_new_xdg_surface;
    wl_signal_add(&server.xdg_shell->events.new_surface, &server.new_xdg_surface);

    /*
     * Creates a cursor, which is a wlroots utility for tracking the cursor
     * image shown on screen.
     */
    server.cursor = wlr_cursor_create();
    wlr_cursor_attach_output_layout(server.cursor, server.output_layout);

    /* Creates an xcursor manager, another wlroots utility which loads up
     * Xcursor themes to source cursor images from and makes sure that cursor
     * images are available at all scale factors on the screen (necessary for
     * HiDPI support). We add a cursor theme at scale factor 1 to begin with. */
    server.cursor_mgr = wlr_xcursor_manager_create(NULL, 24);
    wlr_xcursor_manager_load(server.cursor_mgr, 1);

    /*
     * wlr_cursor *only* displays an image on screen. It does not move around
     * when the pointer moves. However, we can attach input devices to it, and
     * it will generate aggregate events for all of them. In these events, we
     * can choose how we want to process them, forwarding them to clients and
     * moving the cursor around. More detail on this process is described in my
     * input handling blog post:
     *
     * https://drewdevault.com/2018/07/17/Input-handling-in-wlroots.html
     *
     * And more comments are sprinkled throughout the notify functions above.
     */
    server.cursor_motion.notify = server_cursor_motion;
    wl_signal_add(&server.cursor->events.motion, &server.cursor_motion);
    server.cursor_motion_absolute.notify = server_cursor_motion_absolute;
    wl_signal_add(&server.cursor->events.motion_absolute,
            &server.cursor_motion_absolute);
    server.cursor_button.notify = server_cursor_button;
    wl_signal_add(&server.cursor->events.button, &server.cursor_button);
    server.cursor_axis.notify = server_cursor_axis;
    wl_signal_add(&server.cursor->events.axis, &server.cursor_axis);
    server.cursor_frame.notify = server_cursor_frame;
    wl_signal_add(&server.cursor->events.frame, &server.cursor_frame);

    /* Decoration manager
     */
    server.xdg_decoration_manager = wlr_xdg_decoration_manager_v1_create(server.wl_display);
    wl_signal_add(&server.xdg_decoration_manager->events.new_toplevel_decoration, &server.xdg_decoration);
    server.xdg_decoration.notify = handle_xdg_decoration;

    server.srv_decoration_manager = wlr_server_decoration_manager_create(server.wl_display);
    wlr_server_decoration_manager_set_default_mode(server.srv_decoration_manager, WLR_SERVER_DECORATION_MANAGER_MODE_SERVER);
    wl_signal_add(&server.srv_decoration_manager->events.new_decoration, &server.srv_decoration);
    server.srv_decoration.notify = handle_srv_decoration;
    wl_list_init(&server.decorations);

    /*
     * Configures a seat, which is a single "seat" at which a user sits and
     * operates the computer. This conceptually includes up to one keyboard,
     * pointer, touch, and drawing tablet device. We also rig up a listener to
     * let us know when new input devices are available on the backend.
     */
    wl_list_init(&server.keyboards);
    server.new_input.notify = server_new_input;
    wl_signal_add(&server.backend->events.new_input, &server.new_input);
    server.seat = wlr_seat_create(server.wl_display, "seat0");
    server.request_cursor.notify = seat_request_cursor;
    wl_signal_add(&server.seat->events.request_set_cursor, &server.request_cursor);
    server.request_set_selection.notify = xuake_seat_handle_request_set_selection;
    wl_signal_add(&server.seat->events.request_set_selection, &server.request_set_selection);
    server.request_set_primary_selection.notify = xuake_seat_handle_request_set_primary_selection;
    wl_signal_add(&server.seat->events.request_set_primary_selection, &server.request_set_primary_selection);
    server.set_selection.notify = xuake_seat_handle_set_selection;
    wl_signal_add(&server.seat->events.set_selection, &server.set_selection);

    wlr_primary_selection_v1_device_manager_create(server.wl_display);
    wlr_gtk_primary_selection_device_manager_create(server.wl_display);

    /* Setup Xwayland support
     */
    if (!server.testing) {
        server.xwayland.wlr_xwayland = wlr_xwayland_create(server.wl_display, server.compositor, 1);
        wl_signal_add(&(server.xwayland.wlr_xwayland->events.new_surface), &(server.xwayland_surface));
        server.xwayland_surface.notify = handle_xwayland_surface;
        wl_signal_add(&(server.xwayland.wlr_xwayland->events.ready), &(server.xwayland_ready));
        server.xwayland_ready.notify = handle_xwayland_ready;
        wlr_xwayland_set_seat(server.xwayland.wlr_xwayland, server.seat);

        setenv("DISPLAY", server.xwayland.wlr_xwayland->display_name, true);
    } else if (getenv("DISPLAY")) {
        unsetenv("DISPLAY");
    }

    /* Data Device and Data Control Manager support
     */
    server.ddm = wlr_data_device_manager_create(server.wl_display);
    //server.dcm = wlr_data_control_manager_v1_create(server.wl_display);
    //server.dcm_new_device.notify = dcm_handle_new_device;
    //wl_signal_add(&server.dcm->events.new_device, &server.dcm_new_device);

    wl_global_create(server.wl_display, &xuake_control_interface, 1, &server, xuake_control_handle_bind);

    /* Add a Unix socket to the Wayland display. */
    const char *socket = wl_display_add_socket_auto(server.wl_display);
    if (!socket) {
        wlr_backend_destroy(server.backend);
        return 1;
    }

    /* Start the backend. This will enumerate outputs and inputs, become the DRM
     * master, etc */
    if (!wlr_backend_start(server.backend)) {
        wlr_backend_destroy(server.backend);
        wl_display_destroy(server.wl_display);
        return 1;
    }

    /* Set the WAYLAND_DISPLAY environment variable to our socket and run the
     * startup command if requested. */
    setenv("WAYLAND_DISPLAY", socket, true);

    cwd = getcwd(NULL,0);

    chdir(getenv("XDG_RUNTIME_DIR"));
    chown(socket, getuid(), getgid());

    chdir(cwd);
    free(cwd);

    // drop privileges
    seteuid(getuid());
    setegid(getgid());

    if (!server.conf.xkt.cmd) {
        server.conf.xkt.cmd = strdup("/bin/sh");
    }

    init_xkterm(&server);
    if (!server.xkt.data)
        goto error;
    wl_list_init(&server.kb_repeats);

    wl_list_init(&server.lua_callbacks.new_surface);
    wl_list_init(&server.lua_callbacks.map_surface);
    wl_list_init(&server.lua_callbacks.unmap_surface);
    wl_list_init(&server.lua_callbacks.focus_surface);

    /* load background image */
    bg_image_t *bgimg;
    if (server.conf.bg_image) {
        bgfile = fopen(server.conf.bg_image, "rb");
        if (bgfile) {
            init_bg_image(bgfile);
            bgimg = get_bg_image();
            fclose(bgfile);

            server.bg = wlr_texture_from_pixels(server.renderer, bgimg->format, bgimg->rowbytes, bgimg->width,
                bgimg->height, bgimg->data);
        } else
            server.bg = NULL;
    } else {
        server.bg = NULL;
    }

    render_decotex_set(&server, &server.decotex.focus, server.conf.decorations.fg_focus, server.conf.decorations.bg);
    render_decotex_set(&server, &server.decotex.unfocus, server.conf.decorations.fg_unfocus, server.conf.decorations.bg);

    xklua_load_impulse(server.conf.impulse_file);

    if (wl_list_empty(&server.keybindings))
        xklua_default_keybindings();

    /* Run the Wayland event loop. This does not return until you exit the
     * compositor. Starting the backend rigged up all of the necessary event
     * loop configuration to listen to libinput events, DRM events, generate
     * frame events at the refresh rate, and so on. */
    server.initialized = 1;
    wlr_log(WLR_INFO, "Running Wayland compositor on WAYLAND_DISPLAY=%s", socket);
    wl_display_run(server.wl_display);

error:
    /* Once wl_display_run returns, we shut down the server. */
    wl_display_destroy_clients(server.wl_display);
    wl_display_destroy(server.wl_display);
    return 0;
}
