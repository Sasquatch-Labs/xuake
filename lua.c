#define _GNU_SOURCE
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <limits.h>
#include "xkconfig.h"
#include "xuake.h"
#include "xklua.h"
#include "ft.h"
#include "key.h"
#include "term.h"

static lua_State *L;
static struct xuake_server *server;

static int
xklua_load_font(lua_State *l)
{
    const char *fontpath;
    bool bold;

    fontpath = lua_tostring(l, 1);
    bold = lua_toboolean(l, 2);

    //printf("xklua_load_font: %s isbold: %d\n", fontpath, bold);
    ft_load_font(fontpath, bold);

    return 0;
}

static int
xklua_use_full_clear(lua_State *l)
{
    xkterm_use_full_clear();

    return 0;
}

#ifndef XKTERM_BUILD
static int
xklua_get_focused_view(lua_State *l)
{
    int vid = 0;
    struct xuake_view *view;

    wl_list_for_each(view, &server->workspaces[server->ws], link) {
        if (view->focused) {
            vid = view->view_id;
            break;
        }
    }

    lua_pushinteger(l, vid);
    return 1;
}

/*
static int
xklua_list_views(lua_State *l)
{
}

static int
xklua_get_view_geom(lua_State *l)
{
}
*/

static int
xklua_move_view(lua_State *l)
{
    int vid, isnum, ws;
    struct xuake_view *view;

    vid = lua_tointegerx(l, 1, &isnum);
    if (!isnum)
        return 0;

    ws = lua_tointegerx(l, 2, &isnum);
    if (!isnum)
        return 0;

    if (ws < 0 || ws >= XUAKE_WORKSPACES)
        return 0;

    view = get_view_by_id(server, vid);

    xkutil_move_view(server, view, ws);

    return 0;
}

static int
xklua_focus_view(lua_State *l)
{
    int vid, isnum, ws;
    struct xuake_view *view;

    vid = lua_tointegerx(l, 1, &isnum);
    if (!isnum)
        return 0;

    view = get_view_by_id(server, vid);
    ws = get_ws_by_view_id(server, vid);

    xkutil_focus_view(server, view, ws);

    return 0;
}

static int
xklua_workspace(lua_State *l)
{
    int ws, isnum;

    ws = lua_tointegerx(l, 1, &isnum);
    if (!isnum)
        return 0;

    if (ws < 0 || ws > XUAKE_WORKSPACES)
        return 0;

    if (server->ws != ws)
        xkkey_workspace(server, ws);

    return 0;
}

static int
xklua_warp_view(lua_State *l)
{
    int vid, x, y, w = 0, h = 0, isnum;

    vid = lua_tointegerx(l, 1, &isnum);
    if (!isnum)
        return 0;
    x = lua_tointegerx(l, 2, &isnum);
    if (!isnum)
        return 0;
    y = lua_tointegerx(l, 3, &isnum);
    if (!isnum)
        return 0;

    if (lua_gettop(l) == 5) {
        w = lua_tointegerx(l, 4, &isnum);
        if (!isnum)
            return 0;
        h = lua_tointegerx(l, 5, &isnum);
        if (!isnum)
            return 0;
    }

    xkutil_warp_view(server, vid, x, y, w, h);

    return 0;
}

static int
xklua_resize_view(lua_State *l)
{
    int vid, w = 0, h = 0, isnum;

    vid = lua_tointegerx(l, 1, &isnum);
    if (!isnum)
        return 0;
    w = lua_tointegerx(l, 2, &isnum);
    if (!isnum)
        return 0;
    h = lua_tointegerx(l, 3, &isnum);
    if (!isnum)
        return 0;

    xkutil_warp_view(server, vid, INT_MIN, INT_MIN, w, h);

    return 0;
}

static int
xklua_close_view(lua_State *l)
{
    int vid, isnum;

    vid = lua_tointegerx(l, 1, &isnum);
    if (!isnum)
        return 0;

    close_view(server, get_view_by_id(server, vid));
    return 0;
}

static int
xklua_is_testing(lua_State *l)
{
    lua_pushboolean(l, server->testing);

    return 1;
}

static int
xklua_exit(lua_State *l)
{
    xkkey_xuake_exit(server);
    return 0;
}

void
xklua_impulse(struct xuake_server *s, uint32_t imp)
{
    char istr[] = "impulseXX.";

    if (imp > XK_MAX_IMPULSE)
        return;

    snprintf(istr, strlen(istr), "impulse%02d", imp);

    lua_getglobal(L, istr);
    if (lua_isfunction(L, 1)) {
        lua_pcall(L, 0, 0, 0);
    } else {
        lua_pop(L, 1);
    }
}

static int
xklua_register_cb(lua_State *l)
{
    int xke, isnum;
    char *fname;
    struct xuake_lua_cb *cb;

    xke = lua_tointegerx(l, 1, &isnum);
    if (!isnum)
        return 0;

    if (!lua_isstring(l, 2))
        return 0;
    fname = strdup(lua_tostring(l, 2));

    cb = malloc(sizeof(struct xuake_lua_cb));
    memset(cb, 0, sizeof(struct xuake_lua_cb));

    cb->lua_function = fname;
    switch (xke) {
    case XKEV_NEW_SURFACE:
        wl_list_insert(&server->lua_callbacks.new_surface, &cb->link);
        break;
    case XKEV_MAP_SURFACE:
        wl_list_insert(&server->lua_callbacks.map_surface, &cb->link);
        break;
    case XKEV_UNMAP_SURFACE:
        wl_list_insert(&server->lua_callbacks.unmap_surface, &cb->link);
        break;
    case XKEV_FOCUS_SURFACE:
    case XKEV_NONE:
    default:
        free(fname);
        free(cb);
    }
    return 0;
}
#endif // XKTERM_BUILD

static int
xklua_bind_key(lua_State *l)
{
#ifndef XKTERM_BUILD
    uint32_t ws, impulse, mod, sym;
    int isnum;
    const char *cmdname, *tmpsym;
    struct xuake_key_binding *newkb;

    cmdname = lua_tostring(l, 1);

    if (!cmdname)
        return 0;

    mod = (uint32_t)(lua_tointegerx(L, 2, &isnum) & 0xffffffff);
    if (!isnum)
        return 0;

    if (lua_isinteger(L, 3)) {
        sym = (uint32_t)(lua_tointeger(L, 3) & 0xffffffff);
    } else if (lua_isstring(L, 3)) {
        tmpsym = lua_tostring(L, 3);
        if (!tmpsym)
            return 0;
        if (strlen(tmpsym) > 1)
            return 0;
        sym = tmpsym[0];
    } else
        return 0;

    newkb = malloc(sizeof(struct xuake_key_binding));
    memset(newkb, 0, sizeof(struct xuake_key_binding));

    newkb->modifiers = mod;
    newkb->sym = sym;

    if (!strcmp(cmdname, "win_next_ws")) {
        newkb->cmd = strdup(cmdname);
        newkb->pressed0 = xkkey_win_next_ws;
    } else if (!strcmp(cmdname, "win_prev_ws")) {
        newkb->cmd = strdup(cmdname);
        newkb->pressed0 = xkkey_win_prev_ws;
    } else if (!strcmp(cmdname, "win_close")) {
        newkb->cmd = strdup(cmdname);
        newkb->pressed0 = xkkey_win_close;
    } else if (!strcmp(cmdname, "win_cycle")) {
        newkb->cmd = strdup(cmdname);
        newkb->pressed0 = xkkey_win_cycle;
    } else if (!strcmp(cmdname, "win_acycle")) {
        newkb->cmd = strdup(cmdname);
        newkb->pressed0 = xkkey_win_acycle;
    } else if (!strcmp(cmdname, "toggle_xkt")) {
        newkb->cmd = strdup(cmdname);
        newkb->pressed0 = xkkey_toggle_xkt_press;
        newkb->released0 = xkkey_toggle_xkt_release;
    } else if (!strcmp(cmdname, "ws")) {
        if (!lua_isinteger(l, 4))
            goto out;
        ws = lua_tointeger(l, 4);
        if (ws > XUAKE_WORKSPACES)
            goto out;
        asprintf(&newkb->cmd, "ws %d", ws);
        newkb->argc = 1;
        newkb->arg1 = ws;
        newkb->pressed1 = xkkey_workspace;
    } else if (!strcmp(cmdname, "win_to_ws")) {
        if (!lua_isinteger(l, 4))
            goto out;
        ws = lua_tointeger(l, 4);
        if (ws > XUAKE_WORKSPACES)
            goto out;
        asprintf(&newkb->cmd, "win_to_ws %d", ws);
        newkb->argc = 1;
        newkb->arg1 = ws;
        newkb->pressed1 = xkkey_win_to_workspace;
    } else if (!strcmp(cmdname, "impulse")) {
        if (!lua_isinteger(l, 4))
            goto out;
        impulse = lua_tointeger(l, 4);
        if (impulse > XK_MAX_IMPULSE)
            goto out;
        asprintf(&newkb->cmd, "impulse%02d", impulse);
        newkb->argc = 1;
        newkb->arg1 = impulse;
        newkb->pressed1 = xklua_impulse;
    } else if (!strcmp(cmdname, "xuake_exit")) {
        newkb->cmd = strdup(cmdname);
        newkb->pressed0 = xkkey_xuake_exit;
    } else {
        goto out;
    }

    // We just tack onto the end.  No dup checking, no overwriting, nothing.
    // The first bind() for a modifier/sym pair in the list is the real one.
    // You can bind multiple keys to one command.
    // Don't put beans in your nose.
    wl_list_insert(&server->keybindings, &newkb->link);

    return 0;
out:
    if (newkb->cmd)
        free(newkb->cmd);
    free(newkb);
#endif // XKTERM_BUILD
    return 0;
}

void
init_lua(struct xuake_server *s)
{
    server = s;

    L = luaL_newstate();
    luaL_openlibs(L);

    lua_pushcfunction(L, xklua_load_font);
    lua_setglobal(L, "xk_load_font");

    lua_pushcfunction(L, xklua_bind_key);
    lua_setglobal(L, "xk_bind_key");

    lua_pushcfunction(L, xklua_use_full_clear);
    lua_setglobal(L, "xk_use_full_clear");

#ifndef XKTERM_BUILD
    lua_pushcfunction(L, xklua_is_testing);
    lua_setglobal(L, "xk_is_testing");
#endif // XKTERM_BUILD

    char lua_constants[1024];
    snprintf(lua_constants, 1024, "decorations = { left=\"" XKL_LEFT_DEFAULT "\","
        "right= \"" XKL_RIGHT_DEFAULT "\","
        "mid8=\"" XKL_MID8_DEFAULT "\","
        "mid4=\"" XKL_MID4_DEFAULT "\","
        "mid2=\"" XKL_MID2_DEFAULT "\","
        "mid1=\"" XKL_MID1_DEFAULT "\","
        "fg_focus=0xffffffff,"
        "fg_unfocus=0xff4d4d4d,"
        "bg=0xbe000000 }\n"
        "gl_clear_color = { r=0.3, g=0.3, b=0.3 }\n"
        "xk_batteries=1\n"

        "XKEV_NEW_SURFACE = %d\n" "XKEV_MAP_SURFACE = %d\n"
        "XKEV_UNMAP_SURFACE = %d\n" "XKEV_FOCUS_SURFACE = %d\n"

        "XK_MOD_SHIFT = 0x%x\n" "XK_MOD_CAPS = 0x%x\n" "XK_MOD_CTRL = 0x%x\n"
        "XK_MOD_ALT = 0x%x\n" "XK_MOD_MOD2 = 0x%x\n" "XK_MOD_MOD3 = 0x%x\n"
        "XK_MOD_LOGO = 0x%x\n" "XK_MOD_MOD5 = 0x%x\n"

        "XK_BackSpace = 0x%x\n" "XK_Tab = 0x%x\n" "XK_Linefeed = 0x%x\n"
        "XK_Return = 0x%x\n" "XK_Pause = 0x%x\n" "XK_Sys_Req = 0x%x\n"
        "XK_Escape = 0x%x\n" "XK_Delete = 0x%x\n"

        "XK_Home = 0x%x\n" "XK_Left = 0x%x\n" "XK_Up = 0x%x\n"
        "XK_Right = 0x%x\n" "XK_Down = 0x%x\n" "XK_Page_Up = 0x%x\n"
        "XK_Page_Down = 0x%x\n" "XK_End = 0x%x\n"

        "XK_F1 = 0x%x\n" "XK_F2 = 0x%x\n" "XK_F3 = 0x%x\n" "XK_F4 = 0x%x\n"
        "XK_F5 = 0x%x\n" "XK_F6 = 0x%x\n" "XK_F7 = 0x%x\n" "XK_F8 = 0x%x\n"
        "XK_F9 = 0x%x\n" "XK_F10 = 0x%x\n" "XK_F11 = 0x%x\n" "XK_F12 = 0x%x\n",

        XKEV_NEW_SURFACE, XKEV_MAP_SURFACE, XKEV_UNMAP_SURFACE, XKEV_FOCUS_SURFACE,

        WLR_MODIFIER_SHIFT, WLR_MODIFIER_CAPS, WLR_MODIFIER_CTRL,
        WLR_MODIFIER_ALT, WLR_MODIFIER_MOD2, WLR_MODIFIER_MOD3,
        WLR_MODIFIER_LOGO, WLR_MODIFIER_MOD5,

        XKB_KEY_BackSpace, XKB_KEY_Tab, XKB_KEY_Linefeed,
        XKB_KEY_Return, XKB_KEY_Pause, XKB_KEY_Sys_Req,
        XKB_KEY_Escape, XKB_KEY_Delete,

        XKB_KEY_Home, XKB_KEY_Left, XKB_KEY_Up,
        XKB_KEY_Right, XKB_KEY_Down, XKB_KEY_Page_Up,
        XKB_KEY_Page_Down, XKB_KEY_End,

        XKB_KEY_F1, XKB_KEY_F2, XKB_KEY_F3, XKB_KEY_F4,
        XKB_KEY_F5, XKB_KEY_F6, XKB_KEY_F7, XKB_KEY_F8,
        XKB_KEY_F9, XKB_KEY_F10, XKB_KEY_F11, XKB_KEY_F12
    );

    luaL_loadbuffer(L, lua_constants, strlen(lua_constants), "xk_constants.lua");
    lua_pcall(L, 0, 0, 0);
}

void
xklua_default_keybindings(void)
{
    char *lua_default_bindings = "xk_bind_key(\"win_next_ws\", XK_MOD_CTRL|XK_MOD_ALT, \"n\")\n"
        "xk_bind_key(\"win_prev_ws\", XK_MOD_CTRL|XK_MOD_ALT, \"p\")\n"
        "xk_bind_key(\"win_close\", XK_MOD_CTRL|XK_MOD_ALT, \"x\")\n"
        "xk_bind_key(\"win_cycle\", XK_MOD_ALT, XK_Tab)\n"
        "xk_bind_key(\"toggle_xkt\", 0, XK_F1)\n"
        "xk_bind_key(\"xuake_exit\", XK_MOD_ALT, XK_Escape)\n";
    luaL_loadbuffer(L, lua_default_bindings, strlen(lua_default_bindings), "xk_bindings.lua");
    lua_pcall(L, 0, 0, 0);
}

void
xklua_load_impulse(char *filename)
{
    struct stat sb;

#ifndef XKTERM_BUILD
    lua_pushcfunction(L, xklua_exit);
    lua_setglobal(L, "xk_exit");

    lua_pushcfunction(L, xklua_get_focused_view);
    lua_setglobal(L, "xk_get_focused_view");

    lua_pushcfunction(L, xklua_warp_view);
    lua_setglobal(L, "xk_warp_view");

    lua_pushcfunction(L, xklua_resize_view);
    lua_setglobal(L, "xk_resize_view");

    lua_pushcfunction(L, xklua_move_view);
    lua_setglobal(L, "xk_move_view");

    lua_pushcfunction(L, xklua_close_view);
    lua_setglobal(L, "xk_close_view");

    lua_pushcfunction(L, xklua_focus_view);
    lua_setglobal(L, "xk_focus_view");

    lua_pushcfunction(L, xklua_workspace);
    lua_setglobal(L, "xk_ws");

    lua_pushcfunction(L, xklua_register_cb);
    lua_setglobal(L, "xk_register_cb");
#endif // XKTERM_BUILD

    if (stat(filename, &sb))
        return;

    if (luaL_loadfile(L, filename) || lua_pcall(L, 0, 0, 0)) {
        fprintf(stderr, "cannot run configuration file: %s", lua_tostring(L, -1));
        exit(1);
    }
}

#ifndef XKTERM_BUILD
union cb_args {
    int i;
    char *s;
};

void
xklua_run_callback(struct wl_list *cb_list, const char *atypes, ...)
{
    union cb_args *args = NULL;
    size_t nargs = strlen(atypes);
    va_list ap;
    int i;
    struct xuake_lua_cb *lcb;

    if (wl_list_empty(cb_list))
        return;

    va_start(ap, atypes);

    if (nargs) {
        args = malloc(nargs*sizeof(union cb_args));
        memset(args, 0, nargs*sizeof(union cb_args));
        for (i = 0; i < nargs; i++) {
            switch (atypes[i]) {
            case 'i':
                args[i].i = va_arg(ap, int);
                break;
            case 's':
                args[i].s = va_arg(ap, char *);
                break;
            }
        }
    }

    va_end(ap);

    wl_list_for_each(lcb, cb_list, link) {
        lua_getglobal(L, lcb->lua_function);
        if (!lua_isfunction(L, 1)) {
            lua_pop(L, 1);
            continue;
        }

        for (i = 0; i < nargs; i++) {
            switch (atypes[i]) {
            case 'i':
                lua_pushinteger(L, args[i].i);
                break;
            case 's':
                lua_pushstring(L, args[i].s);
                break;
            }
        }

        lua_pcall(L, nargs, 0, 0);
    }
}
#endif // XKTERM_BUILD

void
xklua_load_config(struct xkconfig *conf, char *filename)
{
    int convscs, i;
    uint32_t tmpi;
    char buf[16];
    struct stat sb;

    if (stat(filename, &sb))
        return;

    if (luaL_loadfile(L, filename) || lua_pcall(L, 0, 0, 0)) {
        fprintf(stderr, "cannot run configuration file: %s", lua_tostring(L, -1));
        exit(1);
    }

    if (conf->bg_image == NULL) {
        lua_getglobal(L, "bg_image");
        if (lua_isstring(L, 1))
            conf->bg_image = strdup(lua_tostring(L, 1));
        lua_pop(L, 1);
    }

    if (conf->xkt.cmd == NULL) {
        lua_getglobal(L, "xkt_cmd");
        if (lua_isstring(L, 1))
            conf->xkt.cmd = strdup(lua_tostring(L, 1));
        lua_pop(L, 1);
#ifndef XKTERM_BUILD
        lua_getglobal(L, "xuake_cmd");
        if (lua_isstring(L, 1)) {
            if (conf->xkt.cmd)
                free(conf->xkt.cmd);
            conf->xkt.cmd = strdup(lua_tostring(L, 1));
        }
        lua_pop(L, 1);
#endif // XKTERM_BUILD
    }

    lua_getglobal(L, "xkt_respawn");
    if (lua_isboolean(L, 1))
        conf->xkt.respawn = lua_toboolean(L, 1);
    lua_pop(L, 1);

    lua_getglobal(L, "xk_batteries");
    tmpi = (uint32_t)(lua_tointegerx(L, 1, &convscs) & 0xffffffff);
    if (convscs)
        conf->batteries = tmpi;
    lua_pop(L, 1);

    for (i = 0; i < 16; i++) {
        memset(buf, 0, 16*sizeof(char));
        snprintf(buf, 16, "xkt_color%d", i);
        lua_getglobal(L, buf);
        tmpi = (uint32_t)(lua_tointegerx(L, 1, &convscs) & 0xffffffff);
        if (convscs)
            conf->xkt.colors[i] = tmpi;
        lua_pop(L, 1);
    }

    conf->gl_clear_color.r = 0.3;
    conf->gl_clear_color.g = 0.3;
    conf->gl_clear_color.b = 0.3;

    void
    get_clear_color(double *comp, char *key)
    {
        lua_pushstring(L, key);
        lua_gettable(L, -2);
        if (lua_isnumber(L, -1)) {
            *comp = lua_tonumber(L, -1);
            if (*comp > 1.0)
                *comp = 1.0;
            else if (*comp < 0.0)
                *comp = 0.0;
        }
        lua_pop(L, 1);
    }
    lua_getglobal(L, "gl_clear_color");
    if (lua_istable(L, -1)) {
        get_clear_color(&conf->gl_clear_color.r, "r");
        get_clear_color(&conf->gl_clear_color.g, "g");
        get_clear_color(&conf->gl_clear_color.b, "b");
    }
    lua_pop(L, 1);

    void
    get_decoration_string(char *str, char *key, char *defstr, size_t sz)
    {
        const char *dtmp;
        lua_pushstring(L, key);
        lua_gettable(L, -2);
        if (lua_isstring(L, -1)) {
            dtmp = lua_tostring(L, -1);
            if (strlen(dtmp) == sz)
                memcpy(str, dtmp, sz*sizeof(char));
            else
                memcpy(str, defstr, sz*sizeof(char));
        }
        str[sz] = 0;
        lua_pop(L, 1);
    }
    void
    get_decoration_color(uint32_t *color, char *key)
    {
        uint32_t tmpi;
        int convscs;

        lua_pushstring(L, key);
        lua_gettable(L, -2);
        tmpi = (uint32_t)(lua_tointegerx(L, -1, &convscs) & 0xffffffff);
        if (convscs)
            *color = tmpi;
        lua_pop(L, 1);
    }
    lua_getglobal(L, "decorations");
    if (lua_istable(L, -1)) {
        get_decoration_string(conf->decorations.left, "left", XKL_LEFT_DEFAULT, 8);
        get_decoration_string(conf->decorations.right, "right", XKL_RIGHT_DEFAULT, 8);
        get_decoration_string(conf->decorations.mid8, "mid8", XKL_MID8_DEFAULT, 8);
        get_decoration_string(conf->decorations.mid4, "mid4", XKL_MID4_DEFAULT, 4);
        get_decoration_string(conf->decorations.mid2, "mid2", XKL_MID2_DEFAULT, 2);
        get_decoration_string(conf->decorations.mid1, "mid1", XKL_MID1_DEFAULT, 1);

        get_decoration_color(&conf->decorations.fg_focus, "fg_focus");
        get_decoration_color(&conf->decorations.fg_unfocus, "fg_unfocus");
        get_decoration_color(&conf->decorations.bg, "bg");
    }
    lua_pop(L, 1);
}
