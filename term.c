#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <xkbcommon/xkbcommon.h>
#include <libtsm.h>
#include <unistd.h>
#include <pty.h>
#include <utmp.h>
#include <ctype.h>
#include <fcntl.h>
#include <termios.h>

#include "xuake.h"
#include "ft.h"
#include "term.h"

enum xkt_modifier {
    XKT_MODIFIER_CTRL = 4,
    XKT_MODIFIER_ALT = 8
};

static uint32_t colors[18] = {
    0xff000000, // COLOR_BLACK, X11 black
    0xffcd0000, // COLOR_RED, X11 red3
    0xff00cd00, // COLOR_GREEN, X11 green3
    0xffcdcd00, // COLOR_YELLOW, X11 yellow3
    0xff0000cd, // COLOR_BLUE, X11 blue3
    0xffcd00cd, // COLOR_MAGENTA, X11 magenta3
    0xff00cdcd, // COLOR_CYAN, X11 cyan3
    0xffe5e5e5, // COLOR_LIGHT_GREY, X11 grey90
    0xff4d4d4d, // COLOR_DARK_GREY, X11 grey30
    0xffff0000, // COLOR_LIGHT_RED, X11 red
    0xff00ff00, // COLOR_LIGHT_GREEN, X11 green
    0xffffff00, // COLOR_LIGHT_YELLOW, X11 yellow
    0xff0000ff, // COLOR_LIGHT_BLUE, X11 blue
    0xffff00ff, // COLOR_LIGHT_MAGENTA, X11 magenta
    0xff00ffff, // COLOR_LIGHT_CYAN, X11 cyan
    0xffffffff, // COLOR_WHITE, X11 white
    0xffffffff, // COLOR_FOREGROUND, X11, white
    0xbe000000  // COLOR_BACKGROUND X11 black
};

void (* xkterm_clear)(struct xkterm *t, int w, int h, unsigned char *data) = xkterm_clear_dirty;

void
xkterm_set_colors(uint32_t *new_colors)
{
    int i;
    for (i = 0; i < 18; i++)
        colors[i] = new_colors[i];
}

uint32_t *
xkterm_get_colors(void)
{
    return colors;
}

void
xkterm_key_input(struct xkterm *t, xkb_keysym_t sym, uint32_t modifiers)
{
    uint32_t ucs4;
    bool handled = false;

    ucs4 = xkb_keysym_to_utf32(sym);
    //printf("xkterm_key_input keysym: %x\n", ucs4);

    if (ucs4 == 0)
        ucs4 = TSM_VTE_INVALID;

#ifdef XKTERM_HARD_KEY_REMAPS
    // XXX: This was to make my fingers comfortable as this started getting usable.
    // I probably need to co-opt the xk_bind_key() lua call to deal with this kind
    // of remapping.  Or just start my screen re-write and codify this stuff there.
    // See xkdefs.h to turn this off if it annoys you.
    if (modifiers & XKT_MODIFIER_CTRL && modifiers & XKT_MODIFIER_ALT) {
        // drop through.
    } else if (modifiers & XKT_MODIFIER_CTRL) {
        if (ucs4 == 't') {
            if (tsm_vte_handle_keyboard(t->vte, XKB_KEY_a, 0, XKT_MODIFIER_CTRL, 'a'))
                tsm_screen_sb_reset(t->tsm_screen);
            if (tsm_vte_handle_keyboard(t->vte, XKB_KEY_c, 0, 0, 'c'))
                tsm_screen_sb_reset(t->tsm_screen);
            return;
        }
    } else if (modifiers & XKT_MODIFIER_ALT) {
        switch (ucs4) {
        case ',':
            if (tsm_vte_handle_keyboard(t->vte, XKB_KEY_a, 0, XKT_MODIFIER_CTRL, 'a'))
                tsm_screen_sb_reset(t->tsm_screen);
            if (tsm_vte_handle_keyboard(t->vte, XKB_KEY_p, 0, 0, 'p'))
                tsm_screen_sb_reset(t->tsm_screen);
            return;
        case '.':
            if (tsm_vte_handle_keyboard(t->vte, XKB_KEY_a, 0, XKT_MODIFIER_CTRL, 'a'))
                tsm_screen_sb_reset(t->tsm_screen);
            if (tsm_vte_handle_keyboard(t->vte, XKB_KEY_n, 0, 0, 'n'))
                tsm_screen_sb_reset(t->tsm_screen);
            return;
        }
    }
#endif // XKTERM_HARD_KEY_REMAPS

    if (tsm_vte_handle_keyboard(t->vte, sym, 0, modifiers, ucs4))
        tsm_screen_sb_reset(t->tsm_screen);
}

void
exec_startcmd(char *cmd)
{
    char **args;
    char *prog;
    unsigned int i, j, spaces, len;

    spaces = 2; // Count the null terminator as a space, and one more for the final NULL ptr
    len = strlen(cmd);
    if (len < 1)
        return;

    for (i = 0; i < len; i++)
        if (isspace(cmd[i]))
            spaces++;

    args = malloc(spaces*sizeof(char *));

    args[0] = cmd;
    args[spaces - 1] = NULL;
    for (i = 0, j = 1; i < len; i++)
        if (cmd[i] == ' ') {
            cmd[i] = 0;
            args[j++] = &(cmd[i+1]);
        }

    prog = strdup(args[0]);

    setenv("TERM", "xterm-color", 1);
    execvp(prog, args);
}

void
set_pty_size(struct xkterm *t, int apty, uint32_t w, uint32_t h)
{
    struct winsize ws;
    int r;

    r = ioctl(apty, TIOCGWINSZ, &ws);
    ws.ws_col = w/t->conf->xkt.cell_width;
    ws.ws_row = h/t->conf->xkt.cell_height;
    r = ioctl(apty, TIOCSWINSZ, &ws);
}

pid_t
xkterm_forkpty(struct xkterm *t)
{
    int mpty, apty, r, flags;
    uint32_t h, w;
    pid_t p;

    h = t->height;
    w = t->width;

    //memset(&t, 0, sizeof(struct termios));

    r = openpty(&mpty, &apty, NULL, NULL, NULL);
    if (r < 0)
        return r;

    t->pty = mpty;
    t->apty = apty;

    set_pty_size(t, apty, w, h);
    //r = tcgetattr(apty, &t);
    //cfmakeraw(&t);
    //r = tcsetattr(apty, TCSANOW, &t);
    flags = fcntl(t->pty, F_GETFL, 0);
    fcntl(t->pty, F_SETFL, flags | O_NONBLOCK);

    p = fork();

    if (p == 0)
        r = login_tty(apty);

    return p;
}

void
xkterm_resize(struct xkterm *t, uint32_t w, uint32_t h)
{
    size_t dirtysz;
    tsm_screen_resize(t->tsm_screen, w/t->conf->xkt.cell_width, h/t->conf->xkt.cell_height);
    set_pty_size(t, t->apty, w, h);
    t->width = w;
    t->height = h;

    if (t->cell_dirty)
        free(t->cell_dirty);

    dirtysz = (w/t->conf->xkt.cell_width)*(h/t->conf->xkt.cell_height)*sizeof(bool);

    t->cell_dirty = malloc(dirtysz);
    memset(t->cell_dirty, 0, dirtysz);
}

void
xkt_vte_write_cb(struct tsm_vte *vte, const char *u8, size_t len, void *data)
{
    struct xkterm *t;
    int r;

    t = data;

    //printf("write_cb: %s\n", u8);
    r = write(t->pty, u8, len);
}

int
xkterm_render_draw_cell_cb(struct tsm_screen *screen, uint32_t id, const uint32_t *ch, size_t len,
    unsigned int cwidth, unsigned int posx, unsigned int posy, const struct tsm_screen_attr *attr,
    tsm_age_t age, void *data)
{
    struct xkterm *t;
    cell_bitmap_t *cell;
    int i,j,k, hoff, woff, stride, bw;
    uint32_t *d, color, alpha_blend;
#if 0
    uint32_t colors[18] = {
    0xff000000, // COLOR_BLACK, X11 black
    0xffdaa520, // COLOR_RED, X11 goldenrod
    0xffffffff, // COLOR_GREEN, X11 white
    0xffcdcd00, // COLOR_YELLOW, X11 yellow3
    0xff999999, // COLOR_BLUE, X11 grey60
    0xffcdcd00, // COLOR_MAGENTA, X11 yellow3
    0xffffff00, // COLOR_CYAN, X11 yellow
    0xff00ff00, // COLOR_LIGHT_GREY, X11 green
    0xff4d4d4d, // COLOR_DARK_GREY, X11 grey30
    0xffdaa520, // COLOR_LIGHT_RED, X11 goldenrod
    0xffffffff, // COLOR_LIGHT_GREEN, X11 white
    0xffffff00, // COLOR_LIGHT_YELLOW, X11 yellow
    0xffe5e5e5, // COLOR_LIGHT_BLUE, X11 grey90
    0xffffff00, // COLOR_LIGHT_MAGENTA, X11 yellow
    0xff00ff00, // COLOR_LIGHT_CYAN, X11 green
    0xff00ff00, // COLOR_WHITE, X11 green
    0xff00cd00, // COLOR_FOREGROUND, X11, green3
    0xbe000000  // COLOR_BACKGROUND X11 black
    };
#endif

    t = data;
    stride = t->width;
    d = (uint32_t *)(t->data);

    woff = t->conf->xkt.cell_width*posx;
    hoff = t->conf->xkt.cell_height*posy;

    if (len == 0) {
        if ((attr->bccode > 0 && attr->bccode < 17) || attr->inverse) {
            if (attr->inverse)
                color = colors[attr->fccode] & (colors[XKT_BGCOLOR] | 0xffffff);
            else
                color = colors[attr->bccode] & (colors[XKT_BGCOLOR] | 0xffffff);
            for (j = 0; j < t->conf->xkt.cell_height; j++) {
                for (i = 0; i < t->conf->xkt.cell_width; i++) {
                    d[stride*(j+hoff) + i + woff] = color;
                }
            }
            t->cell_dirty[(posy*stride/t->conf->xkt.cell_width)+posx] = true;
        }

        return 0;
    }

    for (k = 0; k < len; k++) {
        cell = get_glyph(ch[k], attr->bold);

        if ((attr->bccode > 0 && attr->bccode < 17) || attr->inverse) {
            if (attr->inverse)
                color = colors[attr->fccode] & (colors[XKT_BGCOLOR] | 0xffffff);
            else
                color = colors[attr->bccode] & (colors[XKT_BGCOLOR] | 0xffffff);
            for (j = 0; j < cell->ph->height; j++) {
                for (i = 0; i < cell->ph->width; i++) {
                    d[stride*(j+hoff) + i + woff] = color;
                }
            }
        }

        if (attr->inverse)
            color = colors[attr->bccode] | (colors[XKT_FGCOLOR] & 0xff000000);
        else
            color = colors[attr->fccode];

        t->cell_dirty[(posy*stride/t->conf->xkt.cell_width)+posx+k] = true;
        if (cell->ph->width > t->conf->xkt.cell_width)
            t->cell_dirty[(posy*stride/t->conf->xkt.cell_width)+posx+k+1] = true;

        bw = (cell->ph->width + 7)/8;
        for (j = 0; j < cell->ph->height; j++) {
            for (i = 0; i < cell->ph->width; i++) {
                if (stride*(j+hoff) + i + woff > stride * t->height)
                    break;
                if (cell->bm[j*bw + (i/8)] & (0x80 >> (i & 7)))
                    d[stride*(j+hoff) + i + woff] = color;
            }
        }
        woff += cell->ph->width;
    }

    return 0;
}

bool
xkterm_check(struct xkterm *t)
{
    int i, j;
    char buf[128];

    j = 0; // check for "did any reads happen"
    while (1) {
        memset(buf, 0, 128);
        i = read(t->pty, buf, 128);
        if (i > 0) {
            //printf("reader worker: %s\n", buf);
            tsm_vte_input(t->vte, buf, i);
            j++;
        } else if (i < 0) {
            break;
        }
    }

    if (j == 0)
        return false;

    return true;
}


void
xkterm_clear_full(struct xkterm *t, int w, int h, unsigned char *data)
{
    uint32_t *d, i;

    d = (uint32_t *)data;
    for (i = 0; i < w*h; i++)
        d[i] = colors[XKT_BGCOLOR];
}

void
xkterm_clear_dirty(struct xkterm *t, int w, int h, unsigned char *data)
{
    int width = w/t->conf->xkt.cell_width;
    int height = h/t->conf->xkt.cell_height;
    int x,y,i,j,hoff,woff;
    uint32_t *d = (uint32_t *)data;

    for (y = 0; y < height; y++) {
        for (x = 0; x < width; x++) {
            if (!t->cell_dirty[y*width + x])
                continue;
            t->cell_dirty[y*width + x] = false;
            hoff = y*t->conf->xkt.cell_height;
            woff = x*t->conf->xkt.cell_width;
            for (j = 0; j < t->conf->xkt.cell_height; j++) {
                for (i = 0; i < t->conf->xkt.cell_width; i++) {
                    if (w*(j+hoff) + i + woff > w * h)
                        break;
                    d[w*(j+hoff) + i + woff] = colors[XKT_BGCOLOR];
                }
            }
        }
    }
}

void
xkterm_use_full_clear(void)
{
    xkterm_clear = xkterm_clear_full;
}

void
xkterm_render(struct xkterm *t, int width, int height, unsigned char *data)
{
    /*
    if (!xkterm_check(t))
        return;
    */

    xkterm_clear(t, width, height, data);
    tsm_screen_draw(t->tsm_screen, xkterm_render_draw_cell_cb, t);
}

