#define _GNU_SOURCE
#include <unistd.h>

#include "xuake.h"
#include "readpng.h"
#include "key.h"

uint32_t
pixels_from_string(char *str, size_t nibs)
{
    int i;
    uint32_t tmp = 0;
    uint32_t acc = 0;

    for (i = 0; i < nibs; i++) {
        switch (str[i]) {
        case '0': case '1': case '2': case '3': case '4':
        case '5': case '6': case '7': case '8': case '9':
            tmp = str[i] - '0';
            break;
        case 'A': case 'B': case 'C': case 'D': case 'E': case 'F':
            tmp = 10 + str[i] - 'A';
            break;
        case 'a': case 'b': case 'c': case 'd': case 'e': case 'f':
            tmp = 10 + str[i] - 'a';
            break;
        default:
            tmp = 0;
            break;
        }
        acc |= tmp << (4*(nibs-(i+1)));
    }

    return acc;
}

struct wlr_texture *
render_single_decotex(struct xuake_server *server, char *ascii_pixels, uint32_t *buf, size_t nibs, uint32_t fg, uint32_t bg)
{
    int i;
    uint32_t pixels;
    uint32_t base_mask = 0x1 << (nibs*4 - 1);

    pixels = pixels_from_string(ascii_pixels, nibs);
    memset(buf, 0, nibs*4*sizeof(uint32_t));
    for (i = 0; i < nibs*4; i++)
        if (pixels & (base_mask >> i))
            buf[i] = fg;
        else
            buf[i] = bg;

    return wlr_texture_from_pixels(server->renderer, WL_SHM_FORMAT_ARGB8888, nibs*XUAKE_PIXEL_BYTES, nibs, 4, buf);
}


void
render_decotex_set(struct xuake_server *server, struct xuake_decotex_set *texset, uint32_t fg, uint32_t bg)
{
    uint32_t eightby4[32], fourby4[16], twoby4[8], oneby4[4];

    texset->left = render_single_decotex(server, server->conf.decorations.left, eightby4, 8, fg, bg);

    if (strncmp(server->conf.decorations.right, server->conf.decorations.left, 8))
        texset->right = render_single_decotex(server, server->conf.decorations.right, eightby4, 8, fg, bg);
    else
        texset->right = texset->left;

    if (strncmp(server->conf.decorations.mid8, server->conf.decorations.right, 8))
        texset->mid8 = render_single_decotex(server, server->conf.decorations.mid8, eightby4, 8, fg, bg);
    else
        texset->mid8 = texset->right;

    texset->mid4 = render_single_decotex(server, server->conf.decorations.mid4, fourby4, 4, fg, bg);
    texset->mid2 = render_single_decotex(server, server->conf.decorations.mid2, twoby4, 2, fg, bg);
    texset->mid1 = render_single_decotex(server, server->conf.decorations.mid1, oneby4, 1, fg, bg);
}

/* Used to move all of the data necessary to render a surface from the top-level
 * frame handler to the per-surface render function. */
struct render_data {
    struct wlr_output *output;
    struct wlr_renderer *renderer;
    struct xuake_view *view;
    struct timespec *when;
};

void
paint_decotex(struct wlr_renderer *renderer, struct wlr_output *output, struct xuake_decotex_set *texset,
    int xtop, int ytop, int w)
{
    int i,j;
    int midw;
    int accw;

    //printf("painting decorations: %d, [%d,%d];[%d,%d]\n", w, xtop, ytop, xbot, ybot);

    wlr_render_texture(renderer, texset->left, output->transform_matrix, xtop, ytop, 1.0);
    wlr_render_texture(renderer, texset->right, output->transform_matrix, xtop + w - 8, ytop, 1.0);

    midw = w - 16;
    j = midw >> 3;

    //printf("Middle width: %d [%d]\n", midw,j);

    if (midw > 8) {
        for (accw = 8, i = 0; i < j; accw += 8, i++)
            wlr_render_texture(renderer, texset->mid8, output->transform_matrix, xtop + accw, ytop, 1.0);
        //printf("mid8 renders: %d, final xoff: %d\n", i, accw);
    }

    if (midw & 0x4) {
        //printf("rendering mid4 [%d]\n", accw);
        wlr_render_texture(renderer, texset->mid4, output->transform_matrix, xtop + accw, ytop, 1.0);
        accw += 4;
    }

    if (midw & 0x2) {
        //printf("rendering mid2 [%d]\n", accw);
        wlr_render_texture(renderer, texset->mid2, output->transform_matrix, xtop + accw, ytop, 1.0);
        accw += 2;
    }

    if (midw & 0x1) {
        //printf("rendering mid1 [%d]\n", accw);
        wlr_render_texture(renderer, texset->mid1, output->transform_matrix, xtop + accw, ytop, 1.0);
    }
}

static void
render_surface(struct wlr_surface *surface, int sx, int sy, void *data)
{
    /* This function is called for every surface that needs to be rendered. */
    struct render_data *rdata = data;
    struct xuake_view *view = rdata->view;
    struct wlr_output *output = rdata->output;

    /* We first obtain a wlr_texture, which is a GPU resource. wlroots
     * automatically handles negotiating these with the client. The underlying
     * resource could be an opaque handle passed from the client, or the client
     * could have sent a pixel buffer which we copied to the GPU, or a few other
     * means. You don't have to worry about this, wlroots takes care of it. */
    struct wlr_texture *texture = wlr_surface_get_texture(surface);
    if (texture == NULL) {
        return;
    }

    if (view->is_unmanaged) {
        view->x = view->xw_surface->x;
        view->y = view->xw_surface->y;
    }

    /* The view has a position in layout coordinates. If you have two displays,
     * one next to the other, both 1080p, a view on the rightmost display might
     * have layout coordinates of 2000,100. We need to translate that to
     * output-local coordinates, or (2000 - 1920). */
    double ox = 0, oy = 0;
    wlr_output_layout_output_coords(view->server->output_layout, output, &ox, &oy);
    ox += view->x + sx, oy += view->y + sy;

    /* We also have to apply the scale factor for HiDPI outputs. This is only
     * part of the puzzle, Xuake does not support HiDPI. */
    struct wlr_box box = {
        .x = ox, // * output->scale,
        .y = oy, // * output->scale,
        .width = surface->current.width, // * output->scale,
        .height = surface->current.height, // * output->scale,
    };

    /*
     * Those familiar with OpenGL are also familiar with the role of matricies
     * in graphics programming. We need to prepare a matrix to render the view
     * with. wlr_matrix_project_box is a helper which takes a box with a desired
     * x, y coordinates, width and height, and an output geometry, then
     * prepares an orthographic projection and multiplies the necessary
     * transforms to produce a model-view-projection matrix.
     *
     * Naturally you can do this any way you like, for example to make a 3D
     * compositor.
     */
    float matrix[9];
    enum wl_output_transform transform =
        wlr_output_transform_invert(surface->current.transform);
    wlr_matrix_project_box(matrix, &box, transform, 0,
        output->transform_matrix);

    /* This takes our matrix, the texture, and an alpha, and performs the actual
     * rendering on the GPU. */
    wlr_render_texture_with_matrix(rdata->renderer, texture, matrix, 1);

    // XXX: Dirty toplevel detection
    if (!view->is_unmanaged && !view->fullscreen)
        if ((view->type == XUAKE_XWAYLAND_VIEW && surface == view->xw_surface->surface) ||
                (view->type == XUAKE_XDG_SHELL_VIEW && surface == view->xdg_surface->surface)) {

            if (view->focused)
                paint_decotex(rdata->renderer, output, &view->server->decotex.focus, box.x, box.y-4, box.width);
            else
                paint_decotex(rdata->renderer, output, &view->server->decotex.unfocus, box.x, box.y-4, box.width);
        }

    /* This lets the client know that we've displayed that frame and it can
     * prepare another one now if it likes. */
    wlr_surface_send_frame_done(surface, rdata->when);
}

static void
output_frame(struct wl_listener *listener, void *data)
{
    /* This function is called every time an output is ready to display a frame,
     * generally at the output's refresh rate (e.g. 60Hz). */
    struct xuake_output *output = wl_container_of(listener, output, frame);
    struct wlr_renderer *renderer = output->server->renderer;
    struct wlr_output *first_out = wlr_output_layout_output_at(output->server->output_layout, 0, 0);

    struct timespec now;
    bg_image_t *bgimg;
    int i,j;
    clock_gettime(CLOCK_MONOTONIC, &now);

    if (output->wlr_output == first_out && output->server->xkt_mode) {
        process_keyboard_repeats(output->server);
        if (xkterm_check(&output->server->xkt)) {
            xkterm_render(&output->server->xkt, output->server->xkt.width, output->server->xkt.height,
                output->server->xkt.data);
            wlr_texture_write_pixels(output->server->xkt.texture, XUAKE_PIXEL_BYTES*output->server->xkt.width,
                output->server->xkt.width, output->server->xkt.height,
                0, 0, 0, 0, output->server->xkt.data);
        }
        if (render_widgets(output->server->widget.width, output->server->widget.height, output->server->widget.data))
            wlr_texture_write_pixels(output->server->widget.texture, XUAKE_PIXEL_BYTES*output->server->widget.width,
                output->server->widget.width, output->server->widget.height,
                0, 0, 0, 0, output->server->widget.data);
    }

    /* wlr_output_attach_render makes the OpenGL context current. */
    if (!wlr_output_attach_render(output->wlr_output, NULL)) {
        return;
    }
    /* The "effective" resolution can change if you rotate your outputs. */
    int width, height;
    wlr_output_effective_resolution(output->wlr_output, &width, &height);

    /* Begin the renderer (calls glViewport and some other GL sanity checks) */
    wlr_renderer_begin(renderer, width, height);

    bgimg = get_bg_image();
    float color[4];
    color[0] = output->server->conf.gl_clear_color.r;
    color[1] = output->server->conf.gl_clear_color.g;
    color[2] = output->server->conf.gl_clear_color.b;
    color[3] = 1.0;
    wlr_renderer_clear(renderer, color);

    if (output->server->bg) {
        for (i = 0; i*bgimg->width < output->wlr_output->width; i++) {
            for (j = 0; j*bgimg->height < output->wlr_output->height; j++) {
                wlr_render_texture(renderer, output->server->bg, output->wlr_output->transform_matrix, i*bgimg->width, j*bgimg->height, 1.0);
            }
        }
    }

    /* Each subsequent window we render is rendered on top of the last. Because
     * our view list is ordered front-to-back, we iterate over it backwards. */
    struct xuake_view *view;
    wl_list_for_each_reverse(view, &output->server->workspaces[output->server->ws], link) {
        if (!view->mapped) {
            /* An unmapped view should not be rendered. */
            continue;
        }
        struct render_data rdata = {
            .output = output->wlr_output,
            .view = view,
            .renderer = renderer,
            .when = &now,
        };
        switch (view->type) {
        case XUAKE_XWAYLAND_VIEW:
            render_surface(view->xw_surface->surface, 0, 0, &rdata);
            break;
        case XUAKE_XDG_SHELL_VIEW:
            /* This calls our render_surface function for each surface among the
             * xdg_surface's toplevel and popups. */
            wlr_xdg_surface_for_each_surface(view->xdg_surface, render_surface, &rdata);
            break;
        }
    }

    wl_list_for_each_reverse(view, &output->server->unmanaged, link) {
        if (!view->mapped) {
            /* An unmapped view should not be rendered. */
            continue;
        }
        struct render_data rdata = {
            .output = output->wlr_output,
            .view = view,
            .renderer = renderer,
            .when = &now,
        };
        if (view->type == XUAKE_XWAYLAND_VIEW) {
            render_surface(view->xw_surface->surface, 0, 0, &rdata);
        }
    }

    // Check state of xuake, and render it here so it's _always_ on top.
    // For multihead, only do this on the 0,0 output
    if (output->wlr_output == first_out && output->server->xkt_mode) {
        wlr_render_texture(renderer, output->server->xkt.texture, output->wlr_output->transform_matrix, 0, 0, 1.0);
        wlr_render_texture(renderer, output->server->widget.texture, output->wlr_output->transform_matrix,
            output->server->xkt.width, 0, 1.0);
    }

    /* Hardware cursors are rendered by the GPU on a separate plane, and can be
     * moved around without re-rendering what's beneath them - which is more
     * efficient. However, not all hardware supports hardware cursors. For this
     * reason, wlroots provides a software fallback, which we ask it to render
     * here. wlr_cursor handles configuring hardware vs software cursors for you,
     * and this function is a no-op when hardware cursors are in use. */
    wlr_output_render_software_cursors(output->wlr_output, NULL);

    /* Conclude rendering and swap the buffers, showing the final frame
     * on-screen. */
    wlr_renderer_end(renderer);
    //printf("Commit\n");
    //wlr_output_send_frame(output->wlr_output);
    //output->wlr_output->frame_pending = false;
    wlr_output_commit(output->wlr_output);
}

void
server_new_output(struct wl_listener *listener, void *data)
{
    /* This event is rasied by the backend when a new output (aka a display or
     * monitor) becomes available. */
    struct xuake_server *server = wl_container_of(listener, server, new_output);
    struct wlr_output *wlr_output = data;

    printf("server_new_output 1\n");

    // XXX: When configurable multihead is added, this will need to
    // reconfigure the Xuake window's size.

    /* Some backends don't have modes. DRM+KMS does, and we need to set a mode
     * before we can use the output. The mode is a tuple of (width, height,
     * refresh rate), and each monitor supports only a specific set of modes. We
     * just pick the monitor's preferred mode, a more sophisticated compositor
     * would let the user configure it. */
    if (!wl_list_empty(&wlr_output->modes)) {
        struct wlr_output_mode *mode = wlr_output_preferred_mode(wlr_output);
        wlr_output_set_mode(wlr_output, mode);
        wlr_output_enable(wlr_output, true);
        if (!wlr_output_commit(wlr_output))
            return;
    }

    printf("server_new_output 2\n");
    /* Allocates and configures our state for this output */
    struct xuake_output *output = calloc(1, sizeof(struct xuake_output));
    output->wlr_output = wlr_output;
    output->server = server;
    /* Sets up a listener for the frame notify event. */
    output->frame.notify = output_frame;
    wl_signal_add(&wlr_output->events.frame, &output->frame);
    wl_list_insert(&server->outputs, &output->link);

    printf("server_new_output 3\n");
    /* Adds this to the output layout. The add_auto function arranges outputs
     * from left-to-right in the order they appear. A more sophisticated
     * compositor would let the user configure the arrangement of outputs in the
     * layout. */
    wlr_output_layout_add_auto(server->output_layout, wlr_output);

    printf("Done with output create!\n");
}
