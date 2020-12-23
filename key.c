#include <stdlib.h>
#include "xuake.h"
#include "key.h"

void
xkutil_move_view(struct xuake_server *server, struct xuake_view *view, int new_ws)
{
    if (!view)
        return;

    if (new_ws > XUAKE_WORKSPACES)
        return;

    if (view->focused) {
        view->focused = 0;
        if (view->type == XUAKE_XWAYLAND_VIEW)
            wlr_xwayland_surface_activate(view->xw_surface, false);
        wlr_seat_keyboard_clear_focus(server->seat);
    }
    wl_list_remove(&view->link);
    wl_list_insert(&server->workspaces[new_ws], &view->link);
}

void
xkkey_win_next_ws(struct xuake_server *server)
{
    struct xuake_view *top_view;
    int ws;

    if (wl_list_length(&server->workspaces[server->ws]) == 0)
        return;

    ws = server->ws + 1;
    if (ws > XUAKE_WORKSPACES)
        ws = 0;
    top_view = wl_container_of(server->workspaces[server->ws].next, top_view, link);

    xkutil_move_view(server, top_view, ws);
}

void
xkkey_win_prev_ws(struct xuake_server *server)
{
    struct xuake_view *top_view;
    int ws;

    if (wl_list_length(&server->workspaces[server->ws]) == 0)
        return;

    ws = server->ws - 1;
    if (ws < 0)
        ws = XUAKE_WORKSPACES - 1;
    top_view = wl_container_of(server->workspaces[server->ws].next, top_view, link);

    xkutil_move_view(server, top_view, ws);
}

void
xkkey_win_close(struct xuake_server *server)
{
    struct xuake_view *top_view;

    if (wl_list_length(&server->workspaces[server->ws]) == 0)
        return;

    top_view = wl_container_of(server->workspaces[server->ws].next, top_view, link);
    close_view(server, top_view);
}

void
xkkey_win_cycle(struct xuake_server *server)
{
    struct xuake_view *top_view;
    struct xuake_view *current_view;
    struct xuake_view *next_view;

    if (server->xkt_mode)
        return;
    /* Cycle to the next view */
    if (wl_list_length(&server->workspaces[server->ws]) == 0)
        return;
    if (wl_list_length(&server->workspaces[server->ws]) == 1) {
        top_view = wl_container_of(server->workspaces[server->ws].next, top_view, link);
        if (!top_view->focused)
            focus_view(top_view, get_wlr_surface_from_view(top_view));
        return;
    }
    while (1) {
        current_view = wl_container_of(server->workspaces[server->ws].next, current_view, link);
        next_view = wl_container_of(current_view->link.next, next_view, link);
        /* Move the previous view to the end of the list */
        wl_list_remove(&current_view->link);
        wl_list_insert(server->workspaces[server->ws].prev, &current_view->link);
        if (get_wlr_surface_from_view(next_view))
            focus_view(next_view, get_wlr_surface_from_view(next_view));
        else
            continue;
        break;
    }
}

void
xkkey_win_acycle(struct xuake_server *server)
{
    struct xuake_view *top_view;
    struct xuake_view *prev_view;

    if (server->xkt_mode)
        return;
    /* Cycle to the next view */
    if (wl_list_length(&server->workspaces[server->ws]) == 0)
        return;
    if (wl_list_length(&server->workspaces[server->ws]) == 1) {
        top_view = wl_container_of(server->workspaces[server->ws].next, top_view, link);
        if (!top_view->focused)
            focus_view(top_view, get_wlr_surface_from_view(top_view));
        return;
    }
    while (1) {
        prev_view = wl_container_of(server->workspaces[server->ws].prev, prev_view, link);
        /* Move the previous view to the end of the list */
        wl_list_remove(&prev_view->link);
        wl_list_insert(server->workspaces[server->ws].next, &prev_view->link);
        if (get_wlr_surface_from_view(prev_view))
            focus_view(prev_view, get_wlr_surface_from_view(prev_view));
        else
            continue;
        break;
    }
}

void
xkkey_win_to_workspace(struct xuake_server *server, uint32_t ws)
{
    struct xuake_view *top_view;

    printf("win_to_ws %d\n", ws);
    if (ws > XUAKE_WORKSPACES)
        return;

    if (wl_list_length(&server->workspaces[server->ws]) == 0)
        return;

    top_view = wl_container_of(server->workspaces[server->ws].next, top_view, link);
    if (top_view->focused) {
        top_view->focused = 0;
        if (top_view->type == XUAKE_XWAYLAND_VIEW)
            wlr_xwayland_surface_activate(top_view->xw_surface, false);
        wlr_seat_keyboard_clear_focus(server->seat);
    }
    wl_list_remove(&top_view->link);
    wl_list_insert(&server->workspaces[ws], &top_view->link);
}

void
xkkey_workspace(struct xuake_server *server, uint32_t ws)
{
    struct xuake_view *top_view;

    if (ws > XUAKE_WORKSPACES)
        return;

    if (wl_list_length(&server->workspaces[server->ws]) == 0)
        top_view = NULL;
    else
        top_view = wl_container_of(server->workspaces[server->ws].next, top_view, link);
    if (top_view && top_view->focused && top_view->type == XUAKE_XWAYLAND_VIEW)
        wlr_xwayland_surface_activate(top_view->xw_surface, false);
    server->ws = ws;
    wlr_seat_keyboard_clear_focus(server->seat);
    if (wl_list_length(&server->workspaces[server->ws]) >= 1) {
        top_view = wl_container_of(server->workspaces[server->ws].next, top_view, link);
        focus_view(top_view, get_wlr_surface_from_view(top_view));
    }
}

void
xkkey_toggle_xkt_press(struct xuake_server *server)
{
    struct xuake_view *top_view;
    struct xk_repeat_key *rk;

    if (server->xkt_mode) {
        printf("Exiting XKT mode\n");
        server->xkt_mode = 0;

restart_free:
        wl_list_for_each(rk, &server->kb_repeats, link) {
            wl_list_remove(&rk->link);
            free(rk);
            goto restart_free;
        }
    } else {
        printf("Entering XKT mode\n");
        wlr_seat_keyboard_clear_focus(server->seat);
        top_view = wl_container_of(server->workspaces[server->ws].next, top_view, link);
        if (top_view && top_view->focused && top_view->type == XUAKE_XWAYLAND_VIEW)
            wlr_xwayland_surface_activate(top_view->xw_surface, false);
        server->xkt_mode = 1;
    }
}

void
xkkey_toggle_xkt_release(struct xuake_server *server)
{
    struct xuake_view *top_view;

    if (!server->xkt_mode) {
        // Exiting XKT mode, refocus top window
        top_view = wl_container_of(server->workspaces[server->ws].next, top_view, link);
        focus_view(top_view, get_wlr_surface_from_view(top_view));
    }
}

void
xkkey_xuake_exit(struct xuake_server *server)
{
    server->conf.xkt.respawn = false;
    wl_display_terminate(server->wl_display);
}


static void
keyboard_handle_modifiers(struct wl_listener *listener, void *data)
{
    /* This event is raised when a modifier key, such as shift or alt, is
     * pressed. We simply communicate this to the client. */
    struct xuake_keyboard *keyboard = wl_container_of(listener, keyboard, modifiers);
    /*
     * A seat can only have one keyboard, but this is a limitation of the
     * Wayland protocol - not wlroots. We assign all connected keyboards to the
     * same seat. You can swap out the underlying wlr_keyboard like this and
     * wlr_seat handles this transparently.
     */
    if (keyboard->server->xkt_mode)
        return;

#if 0
    printf("%08x %08x %08x %08x\n",
        keyboard->device->keyboard->modifiers.depressed,
        keyboard->device->keyboard->modifiers.latched,
        keyboard->device->keyboard->modifiers.locked,
        keyboard->device->keyboard->modifiers.group
    );
#endif
    wlr_seat_set_keyboard(keyboard->server->seat, keyboard->device);

    /* Send modifiers to the client. */
    wlr_seat_keyboard_notify_modifiers(keyboard->server->seat,
        &keyboard->device->keyboard->modifiers);
}

static bool
release_handle_keybinding(struct xuake_server *server, xkb_keysym_t sym, uint32_t modifiers)
{
    /*
     * Dummy check to see if Xuake would handle the keybinding.
     *
     * This will go away with the configurable keybinding refactor
     * In the meantime this is to stop leaks of button releases
     * when switching workspaces or switching out of xkt mode
     */
    struct xuake_key_binding *kb;

    wl_list_for_each(kb, &server->keybindings, link) {
        if (kb->modifiers == modifiers && kb->sym == sym) {
            if (!kb->released)
                return true;
            switch (kb->argc) {
            case 2:
                kb->released2(server, kb->arg1, kb->arg2);
                break;
            case 1:
                kb->released1(server, kb->arg1);
                break;
            case 0:
                kb->released0(server);
                break;
            }
            return true;
        }
    }
    return false;
}

static bool
press_handle_keybinding(struct xuake_server *server, xkb_keysym_t sym, uint32_t modifiers)
{
    /*
     * Here we handle compositor keybindings. This is when the compositor is
     * processing keys, rather than passing them on to the client for its own
     * processing.
     */

    struct xuake_key_binding *kb;

    wl_list_for_each(kb, &server->keybindings, link) {
        if (kb->modifiers == modifiers && kb->sym == sym) {
            if (!kb->pressed)
                return true;
            switch (kb->argc) {
            case 2:
                kb->pressed2(server, kb->arg1, kb->arg2);
                break;
            case 1:
                kb->pressed1(server, kb->arg1);
                break;
            case 0:
                kb->pressed0(server);
                break;
            }
            return true;
        }
    }

    return false;
}

void
process_keyboard_repeats(struct xuake_server *server)
{
    struct xk_repeat_key *rk;
    uint32_t kbr_rate = XK_KBR_RATE;
    uint32_t modifiers;

    if (wl_list_empty(&server->kb_repeats))
        return;

    uint64_t unow;
    struct timespec tp;
    clock_gettime(CLOCK_MONOTONIC_COARSE, &tp);

    unow = tp.tv_sec*1000 + tp.tv_nsec/1000000;

    wl_list_for_each(rk, &server->kb_repeats, link) {
        if (unow >= rk->next_repeat) {
            rk->next_repeat += kbr_rate;
#ifndef XKT_DISABLE_KEY_REPEAT
            modifiers = wlr_keyboard_get_modifiers(rk->keyboard->device->keyboard);
            xkterm_key_input(&server->xkt, rk->sym, modifiers);
#endif // XKT_DISABLE_KEY_REPEAT
        }
    }
}


static void
keyboard_handle_key(struct wl_listener *listener, void *data)
{
    /* This event is raised when a key is pressed or released. */
    struct xuake_keyboard *keyboard = wl_container_of(listener, keyboard, key);
    struct xuake_server *server = keyboard->server;
    struct wlr_event_keyboard_key *event = data;
    struct wlr_seat *seat = server->seat;

    /* Translate libinput keycode -> xkbcommon */
    uint32_t keycode = event->keycode + 8;
    /* Get a list of keysyms based on the keymap for this keyboard */
    const xkb_keysym_t *syms;
    int nsyms = xkb_state_key_get_syms(keyboard->device->keyboard->xkb_state, keycode, &syms);
    int i;
    struct xk_repeat_key *rk;
    uint32_t kbr_delay = XK_KBR_DELAY;

    bool handled = false;
    uint32_t modifiers = wlr_keyboard_get_modifiers(keyboard->device->keyboard);

    //printf("keypress event: %s, %d; ", event->state == WLR_KEY_PRESSED ? "Pressed" : "Released", keycode);
    //for (i = 0; i < nsyms; i++)
    //    printf("%x ", syms[i]);
    //printf("\n");
    if (nsyms == 0)
        return;
    if (event->state == WLR_KEY_PRESSED) {
        /* XXX: This is missing corner cases where some of the keysyms are handled
         * by handle_keybinding() and some aren't.
         *
         * If the last sym is handled, any unhandled sym is discarded.
         * If the last sym isn't handled, handled syms are passed down the line.
         *
         * Also, this code sends the button release events to clients when the
         * compositor handles the button pressed event.
         */
        for (i = 0; i < nsyms; i++) {
            handled += press_handle_keybinding(server, syms[i], modifiers);
        }
        if (handled) {
            return;
        }

        if (server->xkt_mode) {
            uint64_t unow;
            struct timespec tp;
            clock_gettime(CLOCK_MONOTONIC_COARSE, &tp);

            unow = tp.tv_sec*1000 + tp.tv_nsec/1000000;

            for (i = 0; i < nsyms; i++) {
                xkterm_key_input(&server->xkt, syms[i], modifiers);
                rk = malloc(sizeof(struct xk_repeat_key));
                if (rk) {
                    rk->keyboard = keyboard;
                    rk->time = unow;
                    rk->sym = syms[i];
                    rk->next_repeat = unow + kbr_delay;
                    wl_list_insert(&server->kb_repeats, &rk->link);
                }
            }
            return;
        }
    } else { // WLR_KEY_RELEASED
        for (i = 0; i < nsyms; i++) {
            handled += release_handle_keybinding(server, syms[i], modifiers);
restart_free:
            wl_list_for_each(rk, &server->kb_repeats, link) {
                if (rk->sym == syms[i]) {
                    wl_list_remove(&rk->link);
                    free(rk);
                    goto restart_free;
                }
            }
        }
    }

    /* eat all button releases when terminal is active */
    if (handled || server->xkt_mode) {
        return;
    }

    /* Otherwise, we pass it along to the client. */
    wlr_seat_set_keyboard(seat, keyboard->device);
    wlr_seat_keyboard_notify_key(seat, event->time_msec, event->keycode, event->state);
}

void
server_new_keyboard(struct xuake_server *server, struct wlr_input_device *device)
{
    struct xuake_keyboard *keyboard = calloc(1, sizeof(struct xuake_keyboard));
    keyboard->server = server;
    keyboard->device = device;

    /* We need to prepare an XKB keymap and assign it to the keyboard. This
     * assumes the defaults (e.g. layout = "us"). */
    struct xkb_rule_names rules = { 0 };
    rules.options = "caps:escape";
    struct xkb_context *context = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
    struct xkb_keymap *keymap = xkb_map_new_from_names(context, &rules, XKB_KEYMAP_COMPILE_NO_FLAGS);

    wlr_keyboard_set_keymap(device->keyboard, keymap);
    xkb_keymap_unref(keymap);
    xkb_context_unref(context);
    wlr_keyboard_set_repeat_info(device->keyboard, 10, 500);

    /* Here we set up listeners for keyboard events. */
    keyboard->modifiers.notify = keyboard_handle_modifiers;
    wl_signal_add(&device->keyboard->events.modifiers, &keyboard->modifiers);
    keyboard->key.notify = keyboard_handle_key;
    wl_signal_add(&device->keyboard->events.key, &keyboard->key);

    wlr_seat_set_keyboard(server->seat, device);

    /* And add the keyboard to our list of keyboards */
    wl_list_insert(&server->keyboards, &keyboard->link);
}
