#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <signal.h>
#include <unistd.h>
#include <wayland-client.h>
#include <wayland-server.h>
#include <wayland-client-protocol.h>
#include <wayland-egl.h>
#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <GL/gl.h>
#include <xkbcommon/xkbcommon.h>
#include "xdg-shell-client-protocol.h"
#include "ft.h"
#include "term.h"

struct xuake_server;
#include "xklua.h"

#include "xkdefs.h"

#define XUAKE_PIXEL_BYTES 4

#define XKT_KEYBOARD_KEY_STATE_REPEAT 32

struct wl_display *display = NULL;
struct wl_compositor *compositor = NULL;
struct wl_surface *surface = NULL;
struct wl_egl_window *egl_window = NULL;
struct wl_seat *seat = NULL;
struct wl_keyboard *keyboard;
struct xdg_wm_base *wm_base = NULL;
struct wl_callback *frame_callback = NULL;

struct xdg_surface *xdg_surface;
struct xdg_toplevel *xdg_toplevel;

EGLDisplay egl_display;
EGLConfig egl_conf;
EGLSurface egl_surface;
EGLContext egl_context;

static const char *vshad_text =
    "#version 320 es\n"
    "in vec4 pos;\n"
    "in vec4 color;\n"
    "in vec2 vertuv;\n"
    "out vec2 v_uv;\n"
    "out vec4 v_color;\n"
    "void main() {\n"
    "  gl_Position = pos;\n"
    "  v_uv = vertuv;\n"
    "  v_color = color;\n"
    "}\n";

static const char *fshad_text =
    "#version 320 es\n"
    "precision mediump float;\n"
    "uniform sampler2D termTex;\n"
    "in vec2 v_uv;\n"
    "in vec4 v_color;\n"
    "out vec4 fragColor;\n"
    "void main() {\n"
    "  fragColor = texture(termTex, v_uv);\n"
    "}\n";

GLuint tex;
GLuint fshad, vshad, prog;

static int width = 1330, height = 1000;

struct xkterm xkt;
uint32_t modifiers = 0;

uint32_t kbr_rate = XK_KBR_RATE;
uint32_t kbr_delay = XK_KBR_DELAY;

struct xk_repeat_key {
    struct wl_list link;
    uint64_t next_repeat;
    struct wl_keyboard *keyboard;
    uint32_t serial;
    uint32_t time;
    uint32_t key;
};

struct wl_list kb_repeats; // struct xk_repeat_key

struct xkb_context *xkbctx;
struct xkb_keymap *xkbkeymap;
struct xkb_state *xkbstate;

struct wl_callback_listener frame_listener;

static void
keyboard_handle_key(void *data, struct wl_keyboard *keyboard, uint32_t serial, uint32_t time, uint32_t key, uint32_t state);

static void
process_keyboard_repeats(void)
{
    struct xk_repeat_key *rk;

    if (wl_list_empty(&kb_repeats))
        return;

    uint64_t unow;
    struct timespec tp;
    clock_gettime(CLOCK_MONOTONIC_COARSE, &tp);

    unow = tp.tv_sec*1000 + tp.tv_nsec/1000000;

    wl_list_for_each(rk, &kb_repeats, link) {
        if (unow >= rk->next_repeat) {
            rk->next_repeat += kbr_rate;
            keyboard_handle_key(NULL, rk->keyboard, rk->serial, rk->time, rk->key, XKT_KEYBOARD_KEY_STATE_REPEAT);
        }
    }
}

static void
redraw(void *data, struct wl_callback *callback, uint32_t time)
{
    uint32_t color = xkt.conf->xkt.bgcolor;
    unsigned char c;
    int i;
    GLenum e;
    GLuint gl_pos = 0, gl_col = 1, gl_uv = 2, texuni;
    static const GLfloat verts[4][2] = {
        { -1.0, -1.0 },
        {  1.0, -1.0 },
        {  1.0, 1.0 },
        {  -1.0, 1.0 }
    };
    static const GLfloat uverts[4][2] = {
        { 0.0, 1.0 },
        { 1.0, 1.0 },
        { 1.0, 0.0 },
        { 0.0, 0.0 }
    };
    static const GLfloat colors[4][4] = {
        {0.0, 1.0, 0.0, 1.0},
        {0.0, 1.0, 0.0, 1.0},
        {0.0, 1.0, 0.0, 1.0},
        {0.0, 1.0, 0.0, 1.0}
    };

    wl_callback_destroy(frame_callback);
    glBindAttribLocation(prog, gl_pos, "pos");
    glBindAttribLocation(prog, gl_col, "color");
    glBindAttribLocation(prog, gl_uv, "vertuv");
    glLinkProgram(prog);

    glBindTexture(GL_TEXTURE_2D, tex);

    process_keyboard_repeats();
    if (!xkterm_check(&xkt) && !data)
        goto rstcb;

    //printf("%p, %p, %d\n", data, callback, time);
    wl_surface_damage(surface, 0, 0, width, height);
    xkterm_render(&xkt, xkt.pixw, xkt.pixh, xkt.data);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_BGRA, width, height, 0, GL_BGRA, GL_UNSIGNED_BYTE, (void *)xkt.data);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glActiveTexture(0);
    texuni = glGetUniformLocation(prog, "termTex");
    glUniform1i(texuni, 0);

    glViewport(0, 0, width, height);
    glClearColor(0.0, 0.0, 0.0, 0.0);
    glClear(GL_COLOR_BUFFER_BIT);
    glVertexAttribPointer(gl_pos, 2, GL_FLOAT, GL_FALSE, 0, verts);
    glVertexAttribPointer(gl_col, 4, GL_FLOAT, GL_FALSE, 0, colors);
    glVertexAttribPointer(gl_uv, 2, GL_FLOAT, GL_FALSE, 0, uverts);
    glEnableVertexAttribArray(gl_pos);
    glEnableVertexAttribArray(gl_col);
    glEnableVertexAttribArray(gl_uv);

    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

    glDisableVertexAttribArray(gl_pos);
    glDisableVertexAttribArray(gl_col);
    glDisableVertexAttribArray(gl_uv);

    glFlush();

    if (!eglSwapBuffers(egl_display, egl_surface)) {
        fprintf(stderr, "Swapped buffers failed\n");
    }
    //fprintf(stderr, "Swapped buffers\n");

rstcb:
    frame_callback = wl_surface_frame(surface);
    wl_callback_add_listener(frame_callback, &frame_listener, NULL);
    wl_surface_commit(surface);
}

struct wl_callback_listener frame_listener = {
    redraw
};

static void
xdg_surface_handle_configure(void *data, struct xdg_surface *xdg_surface, uint32_t serial)
{
    xdg_surface_ack_configure(xdg_surface, serial);
}

static const struct xdg_surface_listener xdg_surface_listener = {
    .configure = xdg_surface_handle_configure,
};

static void
xdg_toplevel_handle_configure(void *data, struct xdg_toplevel *xdg_toplevel, int32_t w, int32_t h, struct wl_array *states)
{

    //int oldw, oldh;

    if (w == 0 || h == 0)
        return;

    //oldw = width;
    //oldh = height;
    //printf("got configure: [%d, %d]\n", w, h);

    xkt.pixw = width = w;
    xkt.pixh = height = h;
    if (xkt.data)
        free(xkt.data);
    xkt.data = malloc(width*height*XUAKE_PIXEL_BYTES*sizeof(unsigned char));
    xkterm_resize(&xkt, w, h);
    xkterm_clear_full(&xkt, xkt.pixw, xkt.pixh, xkt.data);

    wl_egl_window_resize(egl_window, width, height, 0, 0);

    redraw((void *)0x1, NULL, 0);
}

static void
xdg_toplevel_handle_close(void *data, struct xdg_toplevel *xdg_toplevel)
{
    exit(0);
}

static const struct xdg_toplevel_listener xdg_toplevel_listener = {
    .configure = xdg_toplevel_handle_configure,
    .close = xdg_toplevel_handle_close,
};

static void
keyboard_handle_keymap(void *data, struct wl_keyboard *keyboard, uint32_t format, int fd, uint32_t size)
{
    //fprintf(stderr, "Keyboard handle keymap\n");
    void *buf;

    buf = mmap(NULL, size, PROT_READ, MAP_SHARED, fd, 0);
    if (buf == MAP_FAILED) {
        fprintf(stderr, "Failed to mmap keymap: %d\n", errno);
        close(fd);
        return;
    }

    xkbkeymap = xkb_keymap_new_from_buffer(xkbctx, buf, size - 1, XKB_KEYMAP_FORMAT_TEXT_V1, XKB_KEYMAP_COMPILE_NO_FLAGS);
    munmap(buf, size);
    close(fd);
    if (!xkbkeymap) {
        fprintf(stderr, "Failed to compile keymap!\n");
        return;
    }

    xkbstate = xkb_state_new(xkbkeymap);
    if (!xkbstate) {
        fprintf(stderr, "Failed to create XKB state!\n");
        return;
    }
}

static void
keyboard_handle_enter(void *data, struct wl_keyboard *keyboard, uint32_t serial, struct wl_surface *surface, struct wl_array *keys)
{
    //fprintf(stderr, "Keyboard gained focus\n");
}

static void
keyboard_handle_leave(void *data, struct wl_keyboard *keyboard, uint32_t serial, struct wl_surface *surface)
{
    struct xk_repeat_key *rk;

    // Clear key repeats when we lose focus.
    while (!wl_list_empty(&kb_repeats)) {
        wl_list_for_each(rk, &kb_repeats, link) {
            wl_list_remove(&rk->link);
            free(rk);
            break;
        }
    }
}

static void
keyboard_handle_key(void *data, struct wl_keyboard *keyboard, uint32_t serial, uint32_t time, uint32_t key, uint32_t state)
{
    /* Translate libinput keycode -> xkbcommon */
    uint32_t keycode = key + 8;
    /* Get a list of keysyms based on the keymap for this keyboard */
    const xkb_keysym_t *syms;
    int i;
    struct xk_repeat_key *rk;

    if (state == WL_KEYBOARD_KEY_STATE_RELEASED) {
        /* A couple subtle bugs here:
         * 
         * It's possible the button release for one key event will 
         * go to a different client, in which case, we want to be able 
         * to kill run away repeats by hitting the same key over again.
         *
         * So, we have to be able to delete _all_ of the repeats for a 
         * given key on release.
         *
         * However, if you run wl_list_remove in a wl_list_for_each, the
         * latter is a fragile CPP macro from wayland-utils.h that does 
         * some gritty pointer math.  So, when we permute the wl_list 
         * in a wl_list_for_each, the next tiem around the whole machine 
         * goes dancing into some random corner of core and segfaults.
         *
         * So, if we need to remove an item, break out after removal 
         * and request another trip through the nested loops go BRRRRRR
         */
        bool again = true;
        while (again) {
            again = false;
            wl_list_for_each(rk, &kb_repeats, link) {
                if (rk->key == key) {
                    wl_list_remove(&rk->link);
                    free(rk);
                    again = true;
                    break;
                }
            }
        }
        return;
    }

    if (state == WL_KEYBOARD_KEY_STATE_PRESSED) {
        uint64_t unow;
        struct timespec tp;
        clock_gettime(CLOCK_MONOTONIC_COARSE, &tp);

        unow = tp.tv_sec*1000 + tp.tv_nsec/1000000;

        rk = malloc(sizeof(struct xk_repeat_key));
        if (rk) {
            rk->keyboard = keyboard;
            rk->serial = serial;
            rk->time = time;
            rk->key = key;
            rk->next_repeat = unow + kbr_delay;
            wl_list_insert(&kb_repeats, &rk->link);
        }
    }

    int nsyms = xkb_state_key_get_syms(xkbstate, keycode, &syms);
    uint32_t u32sym = xkb_state_key_get_utf32(xkbstate, keycode);

    //fprintf(stderr, "Key is %d [%d/%d] state is %d;; %d u32sym\n", key, nsyms ? syms[0] : 0, modifiers, state, u32sym);
    for (i = 0; i < nsyms; i++)
        xkterm_key_input(&xkt, syms[i], modifiers);
}

static void
keyboard_handle_modifiers(void *data, struct wl_keyboard *keyboard, uint32_t serial, uint32_t mods_depressed, uint32_t mods_latched, uint32_t mods_locked, uint32_t group)
{
    //fprintf(stderr, "Modifiers depressed %d, latched %d, locked %d, group %d\n", mods_depressed, mods_latched, mods_locked, group);
    modifiers = mods_depressed;
    xkb_state_update_mask(xkbstate, mods_depressed, mods_latched, mods_locked, 0, 0, group);
}

static void
keyboard_handle_repeat(void *data, struct wl_keyboard *wl_keyboard, int32_t rate, int32_t delay)
{
    kbr_delay = delay;
    kbr_rate = rate;
    //fprintf(stderr, "Keyboard repeat\n");
}

static const struct wl_keyboard_listener keyboard_listener = {
    keyboard_handle_keymap,
    keyboard_handle_enter,
    keyboard_handle_leave,
    keyboard_handle_key,
    keyboard_handle_modifiers,
    keyboard_handle_repeat,
};

static void
seat_handle_capabilities(void *data, struct wl_seat *seat, enum wl_seat_capability caps)
{
    if (caps & WL_SEAT_CAPABILITY_KEYBOARD) {
        keyboard = wl_seat_get_keyboard(seat);
        wl_keyboard_add_listener(keyboard, &keyboard_listener, NULL);
    } else if (!(caps & WL_SEAT_CAPABILITY_KEYBOARD)) {
        wl_keyboard_destroy(keyboard);
        keyboard = NULL;
    }
}

static void
seat_handle_name(void *data, struct wl_seat *wl_seat, const char *name)
{
}

static const struct wl_seat_listener seat_listener = {
    seat_handle_capabilities,
    seat_handle_name
};

static void
global_registry_handler(void *data, struct wl_registry *registry, uint32_t id,
        const char *interface, uint32_t version)
{
    if (strcmp(interface, "wl_compositor") == 0) {
        compositor = wl_registry_bind(registry, id, &wl_compositor_interface, 1);
    }  else if (strcmp(interface, xdg_wm_base_interface.name) == 0) {
        wm_base = wl_registry_bind(registry, id, &xdg_wm_base_interface, 1);
    } else if (strcmp(interface, wl_seat_interface.name) == 0) {
        seat = wl_registry_bind(registry, id, &wl_seat_interface, version);
        wl_seat_add_listener(seat, &seat_listener, NULL);
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
handle_chld(int sig)
{
    pid_t p;
    int r;

    p = waitpid(-1, &r, WNOHANG);

    //fprintf(stderr, "SIGCHLD recieved! (%d;%d)\n", p, xkt.child);
    if (p == xkt.child) {
        //fprintf(stderr, "Exiting on SIGCHLD!\n");
        wl_display_disconnect(display);
        exit(0);
    } // else
         //fprintf(stderr, "Not Exiting!\n");
    signal(SIGCHLD, handle_chld);
}

void
init_xkterm(void)
{
    unsigned int i, j, spaces, len;
    int flags;
    pid_t p;
    size_t dirtysz;

    xkt.pixw = width;
    xkt.pixh = height;
    xkt.cellw = width/xkt.conf->xkt.cell_width;
    xkt.cellh = height/xkt.conf->xkt.cell_height;
    xkt.vte.ncells = xkt.cellw*xkt.cellh;
    xkt.data = malloc(width*height*XUAKE_PIXEL_BYTES*sizeof(unsigned char));
    xkt.texture = NULL;
    if (!xkt.data) {
        printf("MALLOC FAILED");
        return;
    }

    if (getenv("STY"))
        unsetenv("STY");

    p = xkterm_forkpty(&xkt);

    if (!xkt.conf->xkt.cmd)
        xkt.conf->xkt.cmd = strdup("/bin/sh");

    if (p == 0) {
        // child
        exec_startcmd(xkt.conf->xkt.cmd);
        exit(125); // never reached
    } else if (p < 0) {
        return;
    }

    xkt.child = p;

    //xkterm_use_full_clear();
    xkt.vte.cells = malloc(xkt.vte.ncells*sizeof(struct xkt_cell));
    xkt.vte.rows = malloc(xkt.cellh*sizeof(struct xkt_cell *));
    for (i = 0; i < xkt.vte.ncells; i++) {
        xkt.vte.cells[i].rune = 0x20;
        xkt.vte.cells[i].dirty = false;
    }
    for (i = 0; i < xkt.cellh; i++)
        xkt.vte.rows[i] = &xkt.vte.cells[i*xkt.cellw];
    xkt.vte.cx = 0;
    xkt.vte.cy = 0;
    xkt.vte.state = 0;
    xkt.vte.wtop = 0;
    xkt.vte.wbot = xkt.cellh - 1;

    //tsm_screen_new(&xkt.tsm_screen, NULL, NULL);
    //tsm_screen_set_max_sb(xkt.tsm_screen, 4096);
    //tsm_screen_resize(xkt.tsm_screen, width/xkt.conf->xkt.cell_width, height/xkt.conf->xkt.cell_height);
    dirtysz = (xkt.cellw)*(xkt.cellh)*sizeof(bool);
    xkt.cell_dirty = malloc(dirtysz);
    memset(xkt.cell_dirty, 0, dirtysz);

    //tsm_vte_new(&xkt.vte, xkt.tsm_screen, xkt_vte_write_cb, &xkt, NULL, NULL);

    xkterm_clear_full(&xkt, xkt.pixw, xkt.pixh, xkt.data);
}

static void
create_window()
{
    GLuint stat;

    //printf("create_window start: [%d, %d]\n", width, height);
    egl_window = wl_egl_window_create(surface, width, height);
    if (egl_window == EGL_NO_SURFACE) {
        fprintf(stderr, "Can't create egl window\n");
        exit(1);
    }

    egl_surface = eglCreateWindowSurface(egl_display, egl_conf, egl_window, NULL);

    eglMakeCurrent(egl_display, egl_surface, egl_surface, egl_context);

    fshad = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fshad, 1, (const char **) &fshad_text, NULL);
    glCompileShader(fshad);
    glGetShaderiv(fshad, GL_COMPILE_STATUS, &stat);
    if (!stat) {
        char log[1000];
        GLsizei len;
        glGetShaderInfoLog(fshad, 1000, &len, log);
        printf("fshad compile failed: %*s\n", len, log);
    }

    vshad = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vshad, 1, (const char **) &vshad_text, NULL);
    glCompileShader(vshad);
    glGetShaderiv(vshad, GL_COMPILE_STATUS, &stat);
    if (!stat) {
        char log[1000];
        GLsizei len;
        glGetShaderInfoLog(fshad, 1000, &len, log);
        printf("vshad compile failed: %*s\n", len, log);
    }

    prog = glCreateProgram();
    glAttachShader(prog, fshad);
    glAttachShader(prog, vshad);
    glLinkProgram(prog);
    glUseProgram(prog);

    glGenTextures(1, &tex);
}

static void
init_egl() 
{
    EGLint major, minor, count, n, size;
    EGLConfig *configs;
    int i;
    EGLint config_attribs[] = {
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_RED_SIZE, 1,
        EGL_GREEN_SIZE, 1,
        EGL_BLUE_SIZE, 1,
        EGL_ALPHA_SIZE, 1,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
        EGL_NONE
    };

    static const EGLint context_attribs[] = {
        EGL_CONTEXT_CLIENT_VERSION, 2,
        EGL_NONE
    };

    egl_display = eglGetDisplay((EGLNativeDisplayType) display);
    if (egl_display == EGL_NO_DISPLAY) {
        fprintf(stderr, "Can't create egl display\n");
        exit(1);
    }

    if (eglInitialize(egl_display, &major, &minor) != EGL_TRUE) {
        fprintf(stderr, "Can't initialise egl display\n");
        exit(1);
    }
    eglGetConfigs(egl_display, NULL, 0, &count);

    configs = calloc(count, sizeof *configs);

    eglChooseConfig(egl_display, config_attribs, configs, count, &n);

    // just choose the first one
    egl_conf = configs[0];

    egl_context = eglCreateContext(egl_display, egl_conf, EGL_NO_CONTEXT, context_attribs);
}

void
usage(void)
{
    printf("Usage: xkterm [-c cmd] [-C lua_config]\n");
}

int
main(int argc, char **argv)
{
    int i;
    struct wl_shell_surface *shell_surface;
    char *conffile_name = NULL;
    struct stat sb;

    xkt.conf = malloc(sizeof(struct xkconfig));
    memset(xkt.conf, 0, sizeof(struct xkconfig));

    int c;
    while ((c = getopt(argc, argv, "C:c:vh")) != -1) {
        switch (c) {
            case 'c':
                xkt.conf->xkt.cmd = strdup(optarg);
                break;
            case 'C':
                conffile_name = strdup(optarg);
                break;
            case 'v':
                printf("xkterm version %s\n", XUAKE_VERSION);
                return 0;
            case 'h':
            default:
                usage();
                return 0;
        }
    }
    if (optind < argc) {
        usage();
        return 0;
    }
    if (!conffile_name)
        asprintf(&conffile_name, "%s/.xuake/conf.lua", getenv("HOME"));

    if (conffile_name)
        if (stat(conffile_name, &sb))
            conffile_name = NULL;

    init_ft();
    memcpy(xkt.conf->xkt.colors, xkterm_get_colors(), 18*sizeof(uint32_t));
    if (conffile_name) {
        init_lua(NULL);
        xklua_load_config(xkt.conf, conffile_name);
    } else {
        ft_load_default_fonts();
    }
    ft_set_cell_size(xkt.conf);
    xkterm_set_colors(xkt.conf->xkt.colors);
    if (xkt.conf->xkt.cell_width == 0 || xkt.conf->xkt.cell_height == 0) {
        printf("Could not load any fonts!\n");
        exit(111);
    }
                             
    signal(SIGCHLD, handle_chld);
    init_xkterm();

    display = wl_display_connect(NULL);

    if (display == NULL) {
        fprintf(stderr, "Can't connect to display\n");
        return 1;
    }
    //printf("connected to display\n");

    xkbctx = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
    wl_list_init(&kb_repeats);

    struct wl_registry *registry = wl_display_get_registry(display);
    wl_registry_add_listener(registry, &registry_listener, NULL);

    wl_display_dispatch(display);
    wl_display_roundtrip(display);

    if (compositor == NULL) {
        fprintf(stderr, "Can't find compositor\n");
        return 1;
    }
    if (wm_base == NULL) {
        fprintf(stderr, "xdg-shell not available\n");
        return 1;
    }

    surface = wl_compositor_create_surface(compositor);
    if (surface == NULL) {
        fprintf(stderr, "Can't create surface\n");
        return 1;
    }

    xkterm_render(&xkt, xkt.pixw, xkt.pixh, xkt.data);

    wl_display_roundtrip(display);

    xdg_surface = xdg_wm_base_get_xdg_surface(wm_base, surface);
    xdg_toplevel = xdg_surface_get_toplevel(xdg_surface);

    xdg_surface_add_listener(xdg_surface, &xdg_surface_listener, NULL);
    xdg_toplevel_add_listener(xdg_toplevel, &xdg_toplevel_listener, NULL);

    xdg_toplevel_set_title(xdg_toplevel, "xkterm");

    wl_surface_commit(surface);
    wl_display_roundtrip(display);

    init_egl();
    create_window();

    frame_callback = wl_surface_frame(surface);
    wl_callback_add_listener(frame_callback, &frame_listener, NULL);

    redraw(NULL, NULL, 0);

    while (wl_display_dispatch(display) != -1);

    wl_display_disconnect(display);
    //printf("disconnected from display\n");

    return 0;
}
