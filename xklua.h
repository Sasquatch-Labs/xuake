#ifndef __XK_LUA_H
#define __XK_LUA_H

/*
Default topleft:
.******. 0111 1110 7e
******** 1111 1111 ff
******** 1111 1111 ff
.******. 0111 1110 7e
 */
#define XKL_RIGHT_DEFAULT "00000000"
#define XKL_LEFT_DEFAULT "7effff7e"
#define XKL_MID8_DEFAULT "00000000"
#define XKL_MID4_DEFAULT "0000"
#define XKL_MID2_DEFAULT "00"
#define XKL_MID1_DEFAULT "0"

/* Specifically excluding key events here.
 * May want to add events specifically for bindable commands
 * (except impulse commands, as that would be redundant)
 */
enum xuake_lua_events {
    XKEV_NONE,
    XKEV_NEW_SURFACE,
    XKEV_MAP_SURFACE,
    XKEV_UNMAP_SURFACE,
    XKEV_FOCUS_SURFACE,
    XKEV_FULLSCREEN,
};

struct xuake_lua_cb {
    struct wl_list link;

    char *lua_function;
};

void init_lua(struct xuake_server *s);
void xklua_load_config(struct xkconfig *conf, char *filename);
void xklua_load_impulse(char *filename);
void xklua_default_keybindings(void);
#ifndef XKTERM_BUILD
void xklua_impulse(struct xuake_server *s, uint32_t imp);
void xklua_run_callback(struct wl_list *cb_list, const char *atypes, ...);
#endif // XKTERM_BUILD

#endif // __XK_LUA_H
