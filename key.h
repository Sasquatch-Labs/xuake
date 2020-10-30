#ifndef __KEY_H
#define __KEY_H

struct xuake_key_binding {
    struct wl_list link;
    
    char *cmd;

    uint32_t modifiers;
    uint32_t sym;

    uint32_t argc, arg1, arg2;

    union {
        struct {
            void* pressed;
            void* released;
        };
        struct {
            void (* pressed0) (struct xuake_server *);
            void (* released0) (struct xuake_server *);
        };
        struct {
            void (* pressed1) (struct xuake_server *, uint32_t);
            void (* released1) (struct xuake_server *, uint32_t);
        };
        struct {
            void (* pressed2) (struct xuake_server *, uint32_t, uint32_t);
            void (* released2) (struct xuake_server *, uint32_t, uint32_t);
        };
    };
};

void xkkey_win_next_ws(struct xuake_server *server);
void xkkey_win_prev_ws(struct xuake_server *server);
void xkkey_win_close(struct xuake_server *server);
void xkkey_win_cycle(struct xuake_server *server);
void xkkey_win_acycle(struct xuake_server *server);
void xkkey_win_to_workspace(struct xuake_server *server, uint32_t ws);
void xkkey_workspace(struct xuake_server *server, uint32_t ws);
void xkkey_toggle_xkt_press(struct xuake_server *server);
void xkkey_toggle_xkt_release(struct xuake_server *server);
void xkkey_xuake_exit(struct xuake_server *server);
void process_keyboard_repeats(struct xuake_server *server);
void server_new_keyboard(struct xuake_server *server, struct wlr_input_device *device);

#endif // __KEY_H
