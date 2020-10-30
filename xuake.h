#ifndef __XUAKE_H
#define __XUAKE_H

#include <wayland-server-core.h>
#include <wlr/backend.h>
#include <wlr/backend/multi.h>
#include <wlr/backend/wayland.h>
#include <wlr/backend/x11.h>
#include <wlr/render/wlr_renderer.h>
#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_data_device.h>
#include <wlr/types/wlr_input_device.h>
#include <wlr/types/wlr_keyboard.h>
#include <wlr/types/wlr_matrix.h>
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_pointer.h>
#include <wlr/types/wlr_seat.h>
#include <wlr/types/wlr_xcursor_manager.h>
#include <wlr/types/wlr_xdg_shell.h>
#include <wlr/types/wlr_xdg_decoration_v1.h>
#include <wlr/types/wlr_server_decoration.h>
#include <wlr/types/wlr_data_control_v1.h>
#include <wlr/types/wlr_primary_selection_v1.h>
#include <wlr/types/wlr_gtk_primary_selection.h>
#include <wlr/util/log.h>
#include <xkbcommon/xkbcommon.h>
#include <libtsm.h>

#include "xuake-control-server.h"

#include "xkconfig.h"
#include "xwayland.h"
#include "term.h"
#include "xkdefs.h"

/* For brevity's sake, struct members are annotated where they are used. */
enum xuake_cursor_mode {
    XUAKE_CURSOR_PASSTHROUGH,
    XUAKE_CURSOR_MOVE,
    XUAKE_CURSOR_RESIZE,
};

enum xuake_decoration_type {
    XUAKE_DECO_NUL,
    XUAKE_DECO_XDG,
    XUAKE_DECO_SRV,
};

#define XUAKE_PIXEL_BYTES 4
#define XUAKE_MINWIDTH 16
#define XUAKE_MINHEIGHT 12

struct xuake_decotex_set {
    struct wlr_texture *left;
    struct wlr_texture *right;
    struct wlr_texture *mid8;
    struct wlr_texture *mid4;
    struct wlr_texture *mid2;
    struct wlr_texture *mid1;
};

struct xuake_server {
    struct wl_display *wl_display;
    struct wlr_backend *backend;
    struct wlr_renderer *renderer;
    struct wlr_compositor *compositor;

    struct wlr_xdg_shell *xdg_shell;
    struct wl_listener new_xdg_surface;

    struct xuake_xwayland xwayland;
    struct wl_listener xwayland_surface;
    struct wl_listener xwayland_ready;

    struct wlr_xdg_decoration_manager_v1 *xdg_decoration_manager;
    struct wl_listener xdg_decoration;

    struct wlr_server_decoration_manager *srv_decoration_manager;
    struct wl_listener srv_decoration;

    struct wlr_data_device_manager *ddm;
    struct wlr_data_control_manager_v1 *dcm;
    struct wl_listener dcm_new_device;

    int ws;
    struct wl_list workspaces[XUAKE_WORKSPACES];
    struct wl_list unmanaged;
    struct wl_list decorations; // xuake_decoration::link

    struct wlr_cursor *cursor;
    struct wlr_xcursor_manager *cursor_mgr;
    struct wl_listener cursor_motion;
    struct wl_listener cursor_motion_absolute;
    struct wl_listener cursor_button;
    struct wl_listener cursor_axis;
    struct wl_listener cursor_frame;

    struct wlr_seat *seat;
    struct wl_listener new_input;
    struct wl_listener request_cursor;
    struct wl_listener request_set_selection;
    struct wl_listener request_set_primary_selection;
    struct wl_listener set_selection;
    struct wl_list keyboards;
    enum xuake_cursor_mode cursor_mode;
    struct xuake_view *grabbed_view;
    double grab_x, grab_y;
    int grab_width, grab_height;
    uint32_t resize_edges;

    struct wlr_output_layout *output_layout;
    struct wl_list outputs;
    struct wl_listener new_output;

    struct wlr_texture *bg;

    bool testing;

    bool monacle;

    bool xkt_mode;
    struct xkterm xkt;
    struct {
        int width;
        int height;
        unsigned char *data;
        struct wlr_texture *texture;
    } widget;
    struct wl_list kb_repeats; // struct xk_repeat_key

    struct {
        struct xuake_decotex_set focus;
        struct xuake_decotex_set unfocus;
    } decotex;

    bool initialized;
    struct xkconfig conf;
    struct wl_list keybindings; // struct xuake_key_binding::link

    struct {
        struct wl_list new_surface; // struct xuake_lua_cb::link
        struct wl_list map_surface; // struct xuake_lua_cb::link
        struct wl_list unmap_surface; // struct xuake_lua_cb::link
        struct wl_list focus_surface; // struct xuake_lua_cb::link
    } lua_callbacks;
};

struct xuake_output {
    struct wl_list link;
    struct xuake_server *server;
    struct wlr_output *wlr_output;
    struct wl_listener frame;
};

struct xuake_decoration;

enum xuake_view_type {
    XUAKE_XDG_SHELL_VIEW,
    XUAKE_XWAYLAND_VIEW,
};

struct xuake_view {
    struct wl_list link;
    struct xuake_server *server;
    unsigned int view_id;
    enum xuake_view_type type;
    bool is_unmanaged;
    union {
        void *v_surface;
        struct wlr_xdg_surface *xdg_surface;
        struct wlr_xwayland_surface *xw_surface;
    };

    bool mapped;
    bool focused;
    bool fullscreen;
    int x, y;

    struct wlr_box saved_geom;

    struct xuake_decoration *decoration;
    struct wl_listener map;
    struct wl_listener unmap;
    struct wl_listener destroy;
    struct wl_listener request_move;
    struct wl_listener request_resize;
    struct wl_listener request_configure; // Xwayland
    struct wl_listener request_fullscreen;
};

struct xuake_keyboard {
    struct wl_list link;
    struct xuake_server *server;
    struct wlr_input_device *device;

    struct wl_listener modifiers;
    struct wl_listener key;
};

struct xuake_decoration {
    struct wl_list link;
    enum xuake_decoration_type type;
    union {
        struct wlr_xdg_toplevel_decoration_v1 *wlr_xdg_decoration;
        struct wlr_server_decoration *wlr_srv_decoration;
    };

    struct xuake_view *view;

    struct wl_listener destroy;
    struct wl_listener request_mode;
};

struct xk_repeat_key {
    struct wl_list link;
    uint64_t next_repeat;
    struct xuake_keyboard *keyboard;
    uint32_t modifiers;
    uint32_t time;
    uint32_t sym;
};

struct wlr_surface *get_wlr_surface_from_view(struct xuake_view *view);
void focus_view(struct xuake_view *view, struct wlr_surface *surface);
void begin_interactive(struct xuake_view *view, enum xuake_cursor_mode mode, uint32_t edges);
void surface_map(struct wl_listener *listener, void *data);
void surface_unmap(struct wl_listener *listener, void *data);
void surface_request_fullscreen(struct wl_listener *listener, void *data);
void xkutil_warp_view(struct xuake_server *server, int view_id, int x, int y, int width, int height);
struct xuake_view *desktop_view_at(struct xuake_server *server, double lx, double ly,
    struct wlr_surface **surface, double *sx, double *sy);
void init_signals(struct xuake_server *server);
void handle_wl_shell_surface(struct wl_listener *listener, void *data);
void xuake_seat_handle_request_set_selection(struct wl_listener *listener, void *data);
void xuake_seat_handle_request_set_primary_selection(struct wl_listener *listener, void *data);
void xuake_seat_handle_set_selection(struct wl_listener *listener, void *data);
unsigned char *init_widgets(struct xuake_server *server);
bool render_widgets(int width, int height, unsigned char *data);
void render_decotex_set(struct xuake_server *server, struct xuake_decotex_set *texset, uint32_t fg, uint32_t bg);
void seat_request_cursor(struct wl_listener *listener, void *data);
void server_new_input(struct wl_listener *listener, void *data);
void handle_srv_decoration(struct wl_listener *listener, void *data);
void handle_xdg_decoration(struct wl_listener *listener, void *data);
void server_cursor_frame(struct wl_listener *listener, void *data);
void server_cursor_axis(struct wl_listener *listener, void *data);
void server_cursor_button(struct wl_listener *listener, void *data);
void server_cursor_motion_absolute(struct wl_listener *listener, void *data);
void server_cursor_motion(struct wl_listener *listener, void *data);
void server_new_xdg_surface(struct wl_listener *listener, void *data);
void server_new_output(struct wl_listener *listener, void *data);
void process_cursor_resize(struct xuake_server *server, uint32_t time);
void close_view(struct xuake_server *server, struct xuake_view *view);
struct xuake_view *get_view_by_id(struct xuake_server *server, int view_id);
int get_ws_by_view_id(struct xuake_server *server, int view_id);
int get_next_view_id(void);
void xkutil_set_monacle(struct xuake_server *server, bool mono);
void xkutil_focus_view(struct xuake_server *server, struct xuake_view *view, int ws);
void xwayland_set_fullscreen(struct xuake_view *view, bool fullscreen);
#endif // __XUAKE_H
