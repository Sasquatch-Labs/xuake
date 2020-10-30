#include "xuake.h"
#include "ft.h"
#include "term.h"

typedef struct color_comp_s {
    unsigned char b;
    unsigned char g;
    unsigned char r;
    unsigned char a;
} color_comp_t;

static uint32_t *xkt_colors;

unsigned char *
init_widgets(struct xuake_server *server)
{
    int i;
    int width, height;
    unsigned char *data;
    uint32_t *d, color;

    if (server->widget.width == 0)
        return NULL;

    width = server->widget.width;
    height = server->widget.height;

    xkt_colors = xkterm_get_colors();

    data = malloc(XUAKE_PIXEL_BYTES*width*height*sizeof(unsigned char));
    d = (uint32_t *)data;
    for (i = 0; i < width*height; i++)
        d[i] = xkt_colors[XKT_BGCOLOR];

    init_clock(width);
    init_batteries(server->conf.batteries, width);

    return data;
}

bool
render_widgets(int width, int height, unsigned char *data)
{
    int i,j,k, hoff;
    uint32_t *d, color, alpha_blend;
    color_comp_t *cc;
    cell_pixmap_t *cell;

    // No XUAKE_PIXEL_BYTES because the pointer math makes the compiler include it for us.
    if (width == 0 || data == NULL)
        return false;

    d = (uint32_t *)data;
    cc = (color_comp_t *)data;

    j = 0;
    j += battery_get_capacity();
    j += clock_check();
    if (!j)
        return false;

    cell = battery_draw_status();

    if (cell) {
        hoff = height - 2*cell->h;
        for (j = 0; j < cell->h; j++) {
            for (i = 0; i < cell->w; i++) {
                switch (cell->bm[j*cell->w+i]) {
                case 1:
                    color = xkt_colors[XKT_FGCOLOR];
                    break;
                case 2:
                    color = xkt_colors[XKT_DARK_GREY];
                    break;
                case 3:
                    color = xkt_colors[XKT_YELLOW];
                    break;
                case 4:
                    color = xkt_colors[XKT_LIGHT_RED];
                    break;
                default:
                    color = xkt_colors[XKT_BGCOLOR];
                    break;
                }
                d[width*(j+hoff) + i] = color;
            }
        }
        hoff -= cell->h;
    } else
        hoff = height;

    cell = clock_draw();
    color = xkt_colors[XKT_FGCOLOR];
    hoff -= cell->h;

    for (j = 0; j < cell->h; j++) {
        for (i = 0; i < cell->w; i++) {
            if (cell->bm[j*cell->w+i]) {
                alpha_blend = (((65*cell->bm[j*cell->w+i])/255) + 0xbe) & 0xff;
                d[width*(j+hoff) + i] = color;
                cc[width*(j+hoff) + i].a = alpha_blend;
                cc[width*(j+hoff) + i].r = ((cc[width*(j+hoff) + i].r * cell->bm[j*cell->w+i]) / 255) & 0xff;
                cc[width*(j+hoff) + i].g = ((cc[width*(j+hoff) + i].g * cell->bm[j*cell->w+i]) / 255) & 0xff;
                cc[width*(j+hoff) + i].b = ((cc[width*(j+hoff) + i].b * cell->bm[j*cell->w+i]) / 255) & 0xff;
            } else {
                d[width*(j+hoff) + i] = xkt_colors[XKT_BGCOLOR];
            }
        }
    }

    return true;
}
