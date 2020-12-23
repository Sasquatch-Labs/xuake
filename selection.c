#include <stdio.h>
#include <stdlib.h>

#include "xuake.h"
#include <wlr/types/wlr_primary_selection.h>
//#include <wlr/types/wlr_data_device.h>

// XXX: WIP.  Long term goal: build this into a built-in selection server that
// strips formatting off of all text (by picking the `text/plain` mime-type when slurping
// offers).  I don't know about you, but I'm sick of having to paste things copied from
// firefox into xterm before re-copying them and pasting them back into a different tab
// in firefox.  I don't use any WYSIWYG garbage if I can avoid it, so I never want the
// formatting.

bool
valid_mime_type(char *mt)
{
    if (!strcmp(mt, "text/plain") ||
        !strcmp(mt, "text/plain;charset=utf-8") ||
        !strcmp(mt, "TEXT") ||
        !strcmp(mt, "STRING") ||
        !strcmp(mt, "UTF8_STRING"))
        return true;
    return false;
}

int
filter_mime_types(struct wl_array *mime_types)
{
    struct wl_array atmp;
    int i = 0;
    char **mt, **mtmp;

    wl_array_init(&atmp);
    wl_array_copy(&atmp, mime_types);
    wl_array_release(mime_types);
    wl_array_init(mime_types);

    wl_array_for_each(mt, &atmp) {
        if (*mt && valid_mime_type(*mt)) {
            printf("  type: %s\n", *mt);
            mtmp = wl_array_add(mime_types, sizeof(*mt));
            *mtmp = *mt;
            i++;
        } else if (*mt) {
            printf("  skipped type: %s\n", *mt);
            free(*mt);
        } else
            printf("  empty type\n");
    }

    wl_array_release(&atmp);

    return i;
}

void
xuake_seat_handle_request_set_selection(struct wl_listener *listener, void *data)
{
    struct xuake_server *server = wl_container_of(listener, server, request_set_selection);
    struct wlr_seat_request_set_selection_event *event = data;
    int i = 0;

    printf("got request set selection event\n");
    if (!event->source)
        return;

    i = filter_mime_types(&event->source->mime_types);

    if (i)
        wlr_seat_set_selection(server->seat, event->source, event->serial);
}

void
xuake_seat_handle_request_set_primary_selection(struct wl_listener *listener, void *data)
{
    struct xuake_server *server = wl_container_of(listener, server, request_set_primary_selection);
    struct wlr_seat_request_set_primary_selection_event *event = data;
    int i = 0;

    printf("got request set primary selection event\n");
    if (!event->source)
        return;

    i = filter_mime_types(&event->source->mime_types);

    char **mt;
    wl_array_for_each(mt, &event->source->mime_types) {
        if (*mt) {
            printf("  type: %s\n", *mt);
        } else
            printf("  empty type\n");
    }

    if (i)
        wlr_seat_set_primary_selection(server->seat, event->source, event->serial);
}

void
xuake_seat_handle_set_selection(struct wl_listener *listener, void *data)
{
    struct wlr_seat *seat = data;
    printf("got set selection event\n");

    if (!seat->selection_source)
        return;

    char **mt;
    wl_array_for_each(mt, &seat->selection_source->mime_types) {
        if (*mt) {
            printf("  type: %s\n", *mt);
        } else
            printf("  empty type\n");
    }
}
