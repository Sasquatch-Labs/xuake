#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <xkbcommon/xkbcommon.h>
#include <unistd.h>
#include <pty.h>
#include <utmp.h>
#include <ctype.h>
#include <fcntl.h>
#include <termios.h>

//#include "xuake.h"
#include "xkdefs.h"
#include "ft.h"
#include "term.h"

enum xkt_modifier {
    XKT_MODIFIER_SHIFT = 1,
    XKT_MODIFIER_CTRL = 4,
    XKT_MODIFIER_ALT = 8
};

enum xkt_vte_state {
    XKT_ST_DEF = 0,
    XKT_ST_ESC = 1,
    XKT_ST_CSIP = 2,
    XKT_ST_CSI = 3,
    XKT_ST_IGN = 4,
    XKT_ST_OSC = 5,
    XKT_ST_UTF = 6
};

static uint32_t colors[16] = {
    0x000000, // COLOR_BLACK, X11 black
    0xcd0000, // COLOR_RED, X11 red3
    0x00cd00, // COLOR_GREEN, X11 green3
    0xcdcd00, // COLOR_YELLOW, X11 yellow3
    0x0000cd, // COLOR_BLUE, X11 blue3
    0xcd00cd, // COLOR_MAGENTA, X11 magenta3
    0x00cdcd, // COLOR_CYAN, X11 cyan3
    0xe5e5e5, // COLOR_LIGHT_GREY, X11 grey90
    0x4d4d4d, // COLOR_DARK_GREY, X11 grey30
    0xff0000, // COLOR_LIGHT_RED, X11 red
    0x00ff00, // COLOR_LIGHT_GREEN, X11 green
    0xffff00, // COLOR_LIGHT_YELLOW, X11 yellow
    0x0000ff, // COLOR_LIGHT_BLUE, X11 blue
    0xff00ff, // COLOR_LIGHT_MAGENTA, X11 magenta
    0x00ffff, // COLOR_LIGHT_CYAN, X11 cyan
    0xffffff, // COLOR_WHITE, X11 white
};

static uint8_t fgalpha = 0xff;
static uint8_t bgalpha = 0xbf;

int fgcolor = XKT_LIGHT_GREY;
int bgcolor = XKT_BLACK;

//void (* xkterm_clear)(struct xkterm *t, int w, int h, unsigned char *data) = xkterm_clear_dirty;
void (* xkterm_clear)(struct xkterm *t, int w, int h, unsigned char *data) = xkterm_clear_full;

void
xkterm_set_colors(uint32_t *new_colors)
{
    int i;
    for (i = 0; i < 16; i++)
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
    char sb[16];
    int i = 0, n;
    bool handled = false;

    ucs4 = xkb_keysym_to_utf32(sym);

    //printf("xkterm_key_input keysym: %x, %x, %x\n", ucs4, sym, modifiers);
#ifdef XKTERM_HARD_KEY_REMAPS
    // XXX: This was to make my fingers comfortable as this started getting usable.
    // I probably need to co-opt the xk_bind_key() lua call to deal with this kind
    // of remapping.  Or just start my screen re-write and codify this stuff there.
    // See xkdefs.h to turn this off if it annoys you.
    //printf("checking for key remaps: %d %d\n", modifiers, ucs4);
    if (modifiers & XKT_MODIFIER_CTRL && modifiers & XKT_MODIFIER_ALT) {
        // drop through.
    } else if (modifiers & XKT_MODIFIER_CTRL) {
        if (ucs4 == 't') {
            write(t->pty, "\x01""c", 2);
            return;
        }
    } else if (modifiers & XKT_MODIFIER_ALT) {
        switch (ucs4) {
        case ',':
            write(t->pty, "\x01""p", 2);
            return;
        case '.':
            write(t->pty, "\x01""n", 2);
            return;
        }
    }
#endif // XKTERM_HARD_KEY_REMAPS

    if (modifiers & XKT_MODIFIER_ALT) {
        sb[i++] = '\x1b';
        modifiers &= ~XKT_MODIFIER_ALT;
    }

    if (ucs4 == 0) {
        switch (sym) {
        case XKB_KEY_Home:
            n = 'H';
            goto gensb;
        case XKB_KEY_End:
            n = 'F';
            goto gensb;
        case XKB_KEY_Up:
            n = 'A';
            goto gensb;
        case XKB_KEY_Down:
            n = 'B';
            goto gensb;
        case XKB_KEY_Right:
            n = 'C';
            goto gensb;
        case XKB_KEY_Left:
            n = 'D';
        gensb:
            sb[i++] = '\x1b';
            if (t->vte.decckm)
                sb[i++] = 'O';
            else
                sb[i++] = '[';
            sb[i++] = n;
            break;
        case XKB_KEY_Page_Up:
            sb[i++] = '\x1b'; sb[i++] = '['; sb[i++] = '5'; sb[i++] = '~';
            break;
        case XKB_KEY_Page_Down:
            sb[i++] = '\x1b'; sb[i++] = '['; sb[i++] = '6'; sb[i++] = '~';
            break;
        case XKB_KEY_F1: case XKB_KEY_F2: case XKB_KEY_F3: case XKB_KEY_F4:
            sb[i++] = '\x1b'; sb[i++] = 'O';
            n = (sym - XKB_KEY_F1);
            sb[i++] = 'P' + n;
            break;
        case XKB_KEY_F5: case XKB_KEY_F6: case XKB_KEY_F7: case XKB_KEY_F8:
            sb[i++] = '\x1b'; sb[i++] = '['; sb[i++] = '1';
            n = (sym - XKB_KEY_F5);
            sb[i++] = '5' + n;
            sb[i++] = '~';
            break;
        case XKB_KEY_F9: case XKB_KEY_F10: case XKB_KEY_F11: case XKB_KEY_F12:
            sb[i++] = '\x1b'; sb[i++] = '['; sb[i++] = '2';
            n = (sym - XKB_KEY_F5);
            sb[i++] = '0' + n;
            sb[i++] = '~';
            break;
        default:
            return;
        }
        write(t->pty, sb, i);
        return;
    }

    if (modifiers == 0 || modifiers == XKT_MODIFIER_SHIFT) {
        if ((ucs4 < 127 && ucs4 >= ' ')
            || ucs4 == '\n'
            || ucs4 == '\r'
            || ucs4 == '\t'
            || ucs4 == 0x1b
            || ucs4 == '\b') {
            sb[i++] = ucs4 & 0x7f;
            sb[i] = 0;
            write(t->pty, sb, i);
        } else if (ucs4 > 127) { // UTF-8 encode and send!
            // XXX...
        }
        return;
    }

    /*
    Tested empirically with old libtsm based xkterm typing
    ^V^[A-Z] &c. into `cat | hexdump -C`
    Ctrl A-Z and a-z: 0x01 - 0x1a
    Ctrl Space and 2: 0x00
    Ctrl 3-7: 0x1b - 0x1f
    Ctrl 8: 0x7f
    */

    if (modifiers & XKT_MODIFIER_CTRL) {
        if (ucs4 >= 'a' && ucs4 <= 'z') {
            ucs4 = ucs4 - 'a' + 1;
        } else if (ucs4 >= 'A' && ucs4 <= 'Z') {
            ucs4 = ucs4 - 'A' + 1;
        } else if (ucs4 >= '3' && ucs4 <= '7') {
            ucs4 = ucs4 - '3' + 1;
        } else if (ucs4 == ' ' || ucs4 == '2') {
            ucs4 = 0;
        } else {
            return;
        }
        sb[i++] = ucs4 & 0x7f;
        sb[i] = 0;
        write(t->pty, sb, i);
    }
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

    setenv("TERM", "xkterm", 1);
    execvp(prog, args);
}

void
set_pty_size(struct xkterm *t, int apty)
{
    struct winsize ws;
    int r;

    r = ioctl(apty, TIOCGWINSZ, &ws);
    ws.ws_col = t->cellw;
    ws.ws_row = t->cellh;
    r = ioctl(apty, TIOCSWINSZ, &ws);
}

pid_t
xkterm_forkpty(struct xkterm *t)
{
    int mpty, apty, r, flags;
    pid_t p;

    //memset(&t, 0, sizeof(struct termios));

    r = openpty(&mpty, &apty, NULL, NULL, NULL);
    if (r < 0)
        return r;

    t->pty = mpty;
    t->apty = apty;

    set_pty_size(t, apty);
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
    struct xkt_cell *oc;
    struct xkt_cell **or;
    uint32_t ocw, och, minw, minh, newoff, oldoff;
    int onc, i, j;

    ocw = t->cellw;
    och = t->cellh;
    onc = t->vte.ncells;

    t->pixw = w;
    t->pixh = h;
    t->cellw = w/t->conf->xkt.cell_width;
    t->cellh = h/t->conf->xkt.cell_height;
    t->vte.ncells = t->cellw*t->cellh;

    if (onc == t->vte.ncells && ocw == t->cellw && och == t->cellh)
        return;

    set_pty_size(t, t->apty);

    oc = t->vte.cells;
    or = t->vte.rows;

    t->vte.cells = malloc(t->vte.ncells*sizeof(struct xkt_cell));
    t->vte.rows = malloc(t->cellh*sizeof(struct xkt_cell *));
    for (i = 0; i < t->vte.ncells; i++) {
        t->vte.cells[i].rune = 0x20;
        t->vte.cells[i].dirty = true;
        t->vte.cells[i].fgcolor = fgcolor;
        t->vte.cells[i].bgcolor = bgcolor;
        t->vte.cells[i].attr = 0;
    }
    for (i = 0; i < t->cellh; i++)
        t->vte.rows[i] = &t->vte.cells[i*t->cellw];

    t->vte.wtop = 0;
    t->vte.wbot = t->cellh - 1;

    minw = ocw <= t->cellw ? ocw : t->cellw;

    if (t->vte.cx >= t->cellw)
        t->vte.cx = t->cellw - 1;

    if (och <= t->cellh) {
        minh = och;
        oldoff = 0;
        newoff = 0;
    } else {
        minh = t->cellh;
        oldoff = och - t->cellh;
        if (t->vte.cy >= t->cellh)
            t->vte.cy = t->cellh - 1;
    }

    for (j = 0; j < minh; j++) {
    for (i = 0; i < minw; i++) {
        t->vte.rows[j][i].rune = or[j+oldoff][i].rune;
        t->vte.cells[i].fgcolor = or[j+oldoff][i].fgcolor;
        t->vte.cells[i].bgcolor = or[j+oldoff][i].bgcolor;
        t->vte.cells[i].attr = or[j+oldoff][i].attr;
    }}

    free(oc);
    free(or);
}

void
xkt_vte_init(struct xkterm *t)
{
    int i;

    t->vte.ncells = t->cellw*t->cellh;
    t->vte.cells = malloc(t->vte.ncells*sizeof(struct xkt_cell));
    t->vte.rows = malloc(t->cellh*sizeof(struct xkt_cell *));
    for (i = 0; i < t->vte.ncells; i++) {
        t->vte.cells[i].rune = 0x20;
        t->vte.cells[i].dirty = false;
        t->vte.cells[i].fgcolor = fgcolor;
        t->vte.cells[i].bgcolor = bgcolor;
        t->vte.cells[i].attr = 0;
    }
    for (i = 0; i < t->cellh; i++)
        t->vte.rows[i] = &t->vte.cells[i*t->cellw];
    t->vte.cx = 0;
    t->vte.cy = 0;
    t->vte.cvis = true;
    t->vte.state = 0;
    t->vte.wtop = 0;
    t->vte.wbot = t->cellh - 1;
    t->vte.fgcolor = fgcolor;
    t->vte.bgcolor = bgcolor;
    t->vte.attr = 0;
    t->vte.decckm = false;
    t->vte.wrap = true;
}

#if 0
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
#endif

void
xkt_vte_clear(struct xkterm *t)
{
    int i;

    for (i = 0; i < t->vte.ncells; i++) {
        t->vte.cells[i].rune = 0x20;
        t->vte.cells[i].dirty = true;
        t->vte.cells[i].fgcolor = fgcolor;
        t->vte.cells[i].bgcolor = bgcolor;
        t->vte.cells[i].attr = 0;
    }
}

void
xkt_vte_partial_clearline(struct xkterm *t, int row, int col0, int col1)
{
    int i;

    for (i = col0; i < col1; i++) {
        t->vte.rows[row][i].rune = 0x20;
        t->vte.rows[row][i].dirty = true;
        t->vte.rows[row][i].fgcolor = fgcolor;
        t->vte.rows[row][i].bgcolor = bgcolor;
        t->vte.rows[row][i].attr = 0;
    }
}

void
xkt_vte_clearline(struct xkterm *t, int row)
{
    int i;

    xkt_vte_partial_clearline(t, row, 0, t->cellw);
}

void
xkt_vte_clearline_from(struct xkterm *t, int row, int col)
{
    int i;

    xkt_vte_partial_clearline(t, row, col, t->cellw);
}

void
xkt_vte_erasedisplay(struct xkterm *t, int type)
{
    int i;

    switch (type) {
    case 0:
        if (t->vte.cy < 0) goto fullclear;
        if (t->vte.cy >= t->cellh) break;
        xkt_vte_clearline_from(t, t->vte.cy, t->vte.cx);
        if (t->vte.cy == t->cellh - 1) break;
        for (i = t->vte.cy + 1; i < t->cellh; i++)
            xkt_vte_clearline(t, i);
        break;
    case 1: // handled below.
        break;
    case 2:
    case 3: // no scrollback, same as 2
    fullclear:
        xkt_vte_clear(t);
        t->vte.cx = 0;
        t->vte.cy = 0;
        return;
    default:
        return;
    }

    if (t->vte.cy < 0)
        return;
    if (t->vte.cy >= t->cellh)
        goto fullclear;

    for (i = 0; i < t->vte.cy; i++)
        xkt_vte_clearline(t, i);

    if (t->vte.cx < 0)
        return;

    if (t->vte.cx >= t->cellw) {
        xkt_vte_clearline(t, t->vte.cy);
        return;
    }

    xkt_vte_partial_clearline(t, t->vte.cy, 0, t->vte.cx);
}

void
xkt_vte_insert_line(struct xkterm *t, int line, int bot)
{
    struct xkt_cell *tmp;
    int i;

    if (bot > t->vte.wbot
        || line < t->vte.wtop
        || line >= bot)
        return;

    xkt_vte_clearline(t, bot);
    tmp = t->vte.rows[bot];
    // XXX: mark everything dirty.
    for (i = bot; i > line; i--)
        t->vte.rows[i] = t->vte.rows[i-1];
    t->vte.rows[line] = tmp;
}

void
xkt_vte_kill_line(struct xkterm *t, int line, int bot)
{
    struct xkt_cell *tmp;
    int i;

    if (bot > t->vte.wbot
        || line < t->vte.wtop
        || line >= bot)
        return;

    xkt_vte_clearline(t, line);
    tmp = t->vte.rows[line];
    // XXX: mark everything dirty.
    for (i = line; i < bot; i++)
        t->vte.rows[i] = t->vte.rows[i+1];
    t->vte.rows[bot] = tmp;
}

void
xkt_vte_movecursor_nowrap(struct xkterm *t, int x, int y)
{
    if (t->vte.rows[t->vte.cy][t->vte.cx].attr & XKT_ATTR_PREMOV)
        t->vte.rows[t->vte.cy][t->vte.cx].attr ^= XKT_ATTR_PREMOV;

    if (x < 0)
        x = 0;
    else if (x >= t->cellw)
        x = t->cellw - 1;

    if (y < 0)
        y = 0;
    else if (y >= t->cellh)
        y = t->cellh - 1;

    t->vte.cx = x;
    t->vte.cy = y;
}

void
xkt_vte_movecursor(struct xkterm *t, int x, int y)
{
    if (t->vte.rows[t->vte.cy][t->vte.cx].attr & XKT_ATTR_PREMOV)
        t->vte.rows[t->vte.cy][t->vte.cx].attr ^= XKT_ATTR_PREMOV;

    if (x < 0)
        x = 0;
    else if (t->vte.wrap && x == t->cellw && x == t->vte.cx + 1) {
        x = 0;
        y++;
    } else if (x >= t->cellw) {
        x = t->cellw - 1;
    }

    if (y == t->vte.cy + 1 && t->vte.cy == t->vte.wbot) {
        //printf("WRAP [%d, %d] -> [%d, %d]\n", t->vte.cx, t->vte.cy, x, y);
        xkt_vte_kill_line(t, t->vte.wtop, t->vte.wbot);
        y = t->vte.wbot;
    } else if (y == t->vte.cy - 1 && t->vte.cy == t->vte.wtop) {
        //printf("REV WRAP\n");
        xkt_vte_insert_line(t, t->vte.cy, t->vte.wbot);
        y = t->vte.wtop;
    } else if (y < 0)
        y = 0;
    else if (y >= t->cellh)
        y = t->cellh - 1;

    t->vte.cx = x;
    t->vte.cy = y;
}

void
xkt_vte_setattr(struct xkterm *t)
{
    int i;

    if (t->vte.n == 0) {
        t->vte.fgcolor = fgcolor;
        t->vte.bgcolor = bgcolor;
        t->vte.attr = 0;
        return;
    }

    if (t->vte.n > XKT_NPAR)
        t->vte.n = XKT_NPAR;

    for (i = 0; i < t->vte.n; i++) {
        switch (t->vte.param[i]) {
        case 0:
            t->vte.fgcolor = fgcolor;
            t->vte.bgcolor = bgcolor;
            t->vte.attr = 0;
            break;
        case 1:
        case 4: // Underscore is simulated with bold
        case 21:
            t->vte.attr |= XKT_ATTR_BOLD;
            break;
        case 2:
        case 10: case 11: case 12:
            // XXX: ignored
            break;
        case 5:
            t->vte.attr |= XKT_ATTR_BLINK;
            break;
        case 7:
            t->vte.attr |= XKT_ATTR_RVID;
            break;
        case 22:
            t->vte.attr = 0;
            t->vte.bgcolor = bgcolor;
            if (t->vte.fgcolor > XKT_LIGHT_GREY)
                t->vte.fgcolor -= 8;
            if (t->vte.fgcolor == t->vte.bgcolor)
                t->vte.fgcolor = fgcolor;
            break;
        case 24:
            t->vte.attr &= ~XKT_ATTR_BOLD;
            break;
        case 25:
            t->vte.attr &= ~XKT_ATTR_BLINK;
            break;
        case 27:
            t->vte.attr &= ~XKT_ATTR_RVID;
            break;
        case 30: case 31: case 32: case 33:
        case 34: case 35: case 36: case 37:
            t->vte.fgcolor = t->vte.param[i] - 30;
            break;
        case 38:
        case 48:
            // 256 color set.  next params are a color.
            if (t->vte.param[i+1] == 2)
                i += 4;
            else if (t->vte.param[i+1] == 5)
                i += 2;
            break;
        case 39:
            t->vte.fgcolor = fgcolor;
            break;
        case 40: case 41: case 42: case 43:
        case 44: case 45: case 46: case 47:
            t->vte.bgcolor = t->vte.param[i] - 40;
            break;
        case 90: case 91: case 92: case 93:
        case 94: case 95: case 96: case 97:
            t->vte.fgcolor = t->vte.param[i] - 82;
            break;
        case 100: case 101: case 102: case 103:
        case 104: case 105: case 106: case 107:
            t->vte.bgcolor = t->vte.param[i] - 92;
            break;
        }
    }
}

void
xkt_vte_setmode(struct xkterm *t, int mode, bool private, bool set)
{
    if (private)
        switch (mode) {
        case 1:
            t->vte.decckm = set;
            break;
        case 7:
            t->vte.wrap = set;
            break;
        case 25: // Cursor Visible
            t->vte.cvis = set;
            break;
        }
}

void
xkt_vte_in_csi(struct xkterm *t, uint32_t ucs4)
{
    int i;

    switch (ucs4) {
    case '1': case '2': case '3':
    case '4': case '5': case '6':
    case '7': case '8': case '9':
              case '0':
        *t->vte.p++ = ucs4;
        return;
    case ';':
        *t->vte.p = 0;
        if (t->vte.n < XKT_NPAR) {
            if (t->vte.p > t->vte.pbuf)
                t->vte.param[t->vte.n++] = strtol(t->vte.pbuf, NULL, 10);
            else
                t->vte.n++;
        }
        memset(t->vte.pbuf, 0, XKT_PBUF_SZ);
        t->vte.p = t->vte.pbuf;
        return;
    case 0x1a: case 0x18:
        t->vte.state = XKT_ST_DEF;
        return;
    }

    *t->vte.p = 0;
    if (t->vte.p > t->vte.pbuf && t->vte.n < XKT_NPAR)
        t->vte.param[t->vte.n++] = strtol(t->vte.pbuf, NULL, 10);
    memset(t->vte.pbuf, 0, XKT_PBUF_SZ);
    t->vte.p = t->vte.pbuf;

    switch (ucs4) {
    case '@': // Ins Blank Characters
        break;
    case 'A': // Cursor up
        if (t->vte.n > 1) break;
        if (t->vte.param[0] == 0) t->vte.param[0]++;
        xkt_vte_movecursor_nowrap(t, t->vte.cx, t->vte.cy - t->vte.param[0]);
        break;
    case 'B': // Cursor down
    case 'e': // move cursor down n rows
        if (t->vte.n > 1) break;
        if (t->vte.param[0] == 0) t->vte.param[0]++;
        xkt_vte_movecursor_nowrap(t, t->vte.cx, t->vte.cy + t->vte.param[0]);
        break;
    case 'C': // Cursor right
    case 'a': // move the cursor right by n col
        if (t->vte.n > 1) break;
        if (t->vte.param[0] == 0) t->vte.param[0]++;
        xkt_vte_movecursor_nowrap(t, t->vte.cx + t->vte.param[0], t->vte.cy);
        break;
    case 'D': // Cursor left
        if (t->vte.n > 1) break;
        if (t->vte.param[0] == 0) t->vte.param[0]++;
        xkt_vte_movecursor_nowrap(t, t->vte.cx - t->vte.param[0], t->vte.cy);
        break;
    case 'E': // Cursor down n rows, to col 1
        if (t->vte.n > 1) break;
        if (t->vte.param[0] == 0) t->vte.param[0]++;
        xkt_vte_movecursor_nowrap(t, 0, t->vte.cy + t->vte.param[0]);
        break;
    case 'F': // Cursor up n rows, to col 1
        if (t->vte.n > 1) break;
        if (t->vte.param[0] == 0) t->vte.param[0]++;
        xkt_vte_movecursor_nowrap(t, 0, t->vte.cy - t->vte.param[0]);
        break;
    case 'G': // Cursor to indicated column in cur row
    case '`': // move cursor to col in current row
        if (t->vte.n > 1) break;
        if (t->vte.param[0] > 0) t->vte.param[0]--;
        xkt_vte_movecursor_nowrap(t, t->vte.param[0], t->vte.cy);
        break;
    case 'd': // move cursor to indicated row, cur col
        if (t->vte.n > 1) break;
        if (t->vte.param[0] > 0) t->vte.param[0]--;
        xkt_vte_movecursor_nowrap(t, t->vte.cx, t->vte.param[0]);
        break;
    case 'H': // Cursor to row;col
    case 'f': // move cursor to indicate row;col
        if (t->vte.n > 2) break;
        if (t->vte.param[0] > 0) t->vte.param[0]--;
        if (t->vte.param[1] > 0) t->vte.param[1]--;
        xkt_vte_movecursor_nowrap(t, t->vte.param[1], t->vte.param[0]);
        break;
    case 'J': // Erase display
        if (t->vte.n == 1)
            xkt_vte_erasedisplay(t, t->vte.param[0]);
        else  if (t->vte.n == 0)
            xkt_vte_erasedisplay(t, 0);
        break;
    case 'K': // Erase line
        if (t->vte.n == 1 && t->vte.param[0] == 1) {
            if (t->vte.cx >= t->cellw) {
                xkt_vte_clearline(t, t->vte.cy);
            } else if (t->vte.cx < 0) {
                break;
            } else {
                xkt_vte_partial_clearline(t, t->vte.cy, 0, t->vte.cx + 1); // partial_clear doesn't clear the last cell
            }
        } else if (t->vte.n == 1 && t->vte.param[0] == 2) {
            xkt_vte_clearline(t, t->vte.cy);
        } else if (t->vte.n == 0) {
            xkt_vte_clearline_from(t, t->vte.cy, t->vte.cx);
        }
        break;
    case 'L': // Insert blank lines
        if (t->vte.n == 0) {
            xkt_vte_insert_line(t, t->vte.cy, t->vte.wbot);
        } else if (t->vte.n == 1) {
            while (t->vte.param[0]--)
                xkt_vte_insert_line(t, t->vte.cy, t->vte.wbot);
        }
        break;
    case 'M': // delete lines
        if (t->vte.n == 0) {
            xkt_vte_kill_line(t, t->vte.cy, t->vte.wbot);
        } else if (t->vte.n == 1) {
            while (t->vte.param[0]--)
                xkt_vte_kill_line(t, t->vte.cy, t->vte.wbot);
        }
        break;
    case 'P': // delete chars from line
        if (t->vte.n > 1) break;
        if (t->vte.n == 0) {
            t->vte.param[0] = t->vte.n = 1;
        }
        memmove(&t->vte.rows[t->vte.cy][t->vte.cx],
            &t->vte.rows[t->vte.cy][t->vte.cx + t->vte.param[0]],
            t->vte.param[0]*sizeof(struct xkt_cell));
        xkt_vte_partial_clearline(t, t->vte.cy, t->vte.cx + t->vte.param[0], t->cellw);
        break;
    case 'X': // erase chars from line
        if (t->vte.n > 1) break;
        if (t->vte.n == 0) {
            t->vte.param[0] = t->vte.n = 1;
        }
        xkt_vte_partial_clearline(t, t->vte.cy, t->vte.cx, t->vte.cx + t->vte.param[0]);
        break;
    case 'c': // Answer I am a VT102
        write(t->pty, "\x1b[?6c", 5);
        break;
    case 'g': // clear tab stop
        break;
    case 'h': // set mode
        if (t->vte.n > 1) break;
        xkt_vte_setmode(t, t->vte.param[0], t->vte.qmark, true);
        break;
    case 'l': // reset mode
        if (t->vte.n > 1) break;
        xkt_vte_setmode(t, t->vte.param[0], t->vte.qmark, false);
        break;
    case 'm': // set attributes
        xkt_vte_setattr(t);
        break;
    case 'n': // status
        if (t->vte.n == 1 && t->vte.param[0] == 5) {
            write(t->pty, "\x1b[0n", 4);
        } else if (t->vte.n == 1 && t->vte.param[0] == 6) {
            char buf[XKT_PBUF_SZ];
            int sz;
            sz = snprintf(buf, XKT_PBUF_SZ, "\x1b[%d;%dR", t->vte.cy+1, t->vte.cx+1);
            if (sz >= XKT_PBUF_SZ)
                break;
            write(t->pty, buf, sz);
        }
        break;
    case 'q': // set leds
        break;
    case 'r': // set scrolling region
        if (t->vte.n > 2) break;
        if (t->vte.param[0]) t->vte.param[0]--;
        if (t->vte.param[1]) t->vte.param[1]--;
        if (t->vte.param[0] >= t->cellh
            || t->vte.param[1] >= t->cellh
            || t->vte.param[0] >= t->vte.param[1])
            break;
        t->vte.wtop = t->vte.param[0];
        t->vte.wbot = t->vte.param[1];
    case 's': // save cursor location
    case 'u': // restore cursor location
        break;
    default:
        return;
    }
    //printf("  CSI-%c [", (char)ucs4, t->vte.n);
    //for (i = 0; i < t->vte.n; i++)
        //printf("%d ", t->vte.param[i]);
    //printf("] [%d,%d]\n", t->vte.cx, t->vte.cy);

    t->vte.state = XKT_ST_DEF;
}

void
xkt_vte_in_escape(struct xkterm *t, uint32_t ucs4)
{
    switch (ucs4) {
    case '[':
        t->vte.state = XKT_ST_CSIP;
        t->vte.n = 0;
        memset(t->vte.param, 0, XKT_NPAR*sizeof(unsigned int));
        memset(t->vte.pbuf, 0, XKT_PBUF_SZ);
        t->vte.p = t->vte.pbuf;
        t->vte.qmark = false;
        return;
    case '%': case '#': case '(': case ')':
        // Ignore alignment command and character set commands.  UTF-8 or GTFO.
        t->vte.state = XKT_ST_IGN;
        t->vte.n = 1;
        break;
    case 'c': // XXX: RESET
        break;
    case 'E': // Newline
        xkt_vte_movecursor(t, 0, t->vte.cy+1);
        break;
    case 'D': // Linefeed
        xkt_vte_movecursor(t, t->vte.cx, t->vte.cy+1);
        break;
    case 'H': // XXX: set tabstop
        t->vte.state = XKT_ST_DEF;
        break;
    case 'M': // reverse linefeed
        t->vte.state = XKT_ST_DEF;
        xkt_vte_movecursor(t, t->vte.cx, t->vte.cy-1);
        break;
    case 'Z': // XXX: DECID
        write(t->pty, "\x1b[?6c", 5);
        t->vte.state = XKT_ST_DEF;
        break;
    case '\\': // String terminator
    case '7': case '8': // XXX: save/restore state
    case '>': case '=': // XXX: keypad mode
    case 0x1a: case 0x18:
        t->vte.state = XKT_ST_DEF;
        break;
    case ']': // OSC
        t->vte.state = XKT_ST_OSC;
        break;
    }
    //printf("  ESC-%c", (char)ucs4);
}

void
xkt_vte_in_normal(struct xkterm *t, uint32_t ucs4)
{
    switch (ucs4) {
    case '\n':
    case '\v':
    case '\f':
        //printf("  NL\n");
        xkt_vte_movecursor(t, t->vte.cx, t->vte.cy+1);
        break;
    case '\r':
        //printf("  CR\n");
        xkt_vte_movecursor(t, 0, t->vte.cy);
        break;
    case '\b':
        //printf("  BS\n");
        xkt_vte_movecursor(t, t->vte.cx-1, t->vte.cy);
        break;
    case '\t':
        //printf("  TAB\n");
        xkt_vte_movecursor(t, 8 + (t->vte.cx & 0x7ffffff8), t->vte.cy);
        break;
    case 0x1b:
        t->vte.state = XKT_ST_ESC;
        break;
    case 0x00:
    case '\a': // XXX Take Bell!  HONK!!
    case 0x0e: case 0x0f: // No alternate character sets
    case 0x7f:
        // IGNORED.
        break;
    default:
        if (ucs4 >= ' ') {
            if (t->vte.rows[t->vte.cy][t->vte.cx].attr & XKT_ATTR_PREMOV)
                xkt_vte_movecursor(t, t->vte.cx+1, t->vte.cy);
            // XXX: Check for double width cells!
            if (t->vte.cx < t->cellw) {
                t->vte.rows[t->vte.cy][t->vte.cx].rune = ucs4;
                t->vte.rows[t->vte.cy][t->vte.cx].fgcolor = t->vte.fgcolor;
                t->vte.rows[t->vte.cy][t->vte.cx].bgcolor = t->vte.bgcolor;
                t->vte.rows[t->vte.cy][t->vte.cx].attr = t->vte.attr;
            }
            if (t->vte.cx == t->cellw - 1)
                t->vte.rows[t->vte.cy][t->vte.cx].attr |= XKT_ATTR_PREMOV;
            else
                xkt_vte_movecursor(t, t->vte.cx+1, t->vte.cy);
        }
    }
}

void
xkt_vte_input(struct xkterm *t, char *buf, int n)
{
    int i;
    uint32_t ucs4;
    char *p = buf;

    while (p - buf < n) {
        if (t->vte.state != XKT_ST_UTF && *p & 0x80) {
            t->vte.usavstate = t->vte.state;
            t->vte.state = XKT_ST_UTF;
            if ((*p & 0xf8) == 0xf0) { // 4 byte sequence
                t->vte.ucs4 = (*p++ & 0x07) << 18;
                t->vte.un = 3;
                continue;
            } else if ((*p & 0xf0) == 0xe0) { // 3 byte sequence
                t->vte.ucs4 = (*p++ & 0x0f) << 12;
                t->vte.un = 2;
                continue;
            } else if ((*p & 0xe0) == 0xc0) { // 2 byte sequence
                t->vte.ucs4 = (*p++ & 0x01f) << 6;
                t->vte.un = 1;
                continue;
            } else { // illegal byte sequence
                t->vte.ucs4 = 0xfffd;
                t->vte.state = t->vte.usavstate;
                p++;
            }
        } else if (t->vte.state == XKT_ST_UTF) {
            t->vte.un--;
            if ((*p & 0xc0) == 0x80) {
                t->vte.ucs4 |= (*p & 0x3f) << (6*t->vte.un);
            } else {
                t->vte.ucs4 = 0xfffd;
                t->vte.un = 0;
            }
            p++;
            if (t->vte.un <= 0) {
                t->vte.state = t->vte.usavstate;
                ucs4 = t->vte.ucs4;
            } else
                continue;
        } else {
            ucs4 = *p++;
        }

        switch (t->vte.state) {
        case XKT_ST_DEF:
            xkt_vte_in_normal(t, ucs4);
            break;
        case XKT_ST_ESC:
            xkt_vte_in_escape(t, ucs4);
            break;
        case XKT_ST_CSIP:
            switch (ucs4) {
            case '?':
                t->vte.qmark = true;
            case '>':
            case '<':
            case '=':
                t->vte.state = XKT_ST_CSI;
                break;
            default:
                t->vte.state = XKT_ST_CSI;
                xkt_vte_in_csi(t, ucs4);
                break;
            }
            break;
        case XKT_ST_CSI:
            xkt_vte_in_csi(t, ucs4);
            break;
        case XKT_ST_OSC:
            switch (ucs4) {
            case 'P':
                t->vte.state = XKT_ST_IGN;
                t->vte.n = 6;
                break;
            case 'R':
            case '?':
            case '\a': // Eat set window title code XXX: Eventually, 0/settitle and 52/copy need to be captured.
                t->vte.state = XKT_ST_DEF;
                break;
            }
            break;
        case XKT_ST_IGN:
            if (--t->vte.n <= 0)
                t->vte.state = XKT_ST_DEF;
            break;
        }
    }
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
            xkt_vte_input(t, buf, i);
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
    uint32_t *d, i, color;

    color = colors[bgcolor] | (bgalpha << 24);
    d = (uint32_t *)data;
    for (i = 0; i < w*h; i++)
        d[i] = color;
}

#if 0
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
#endif

void
xkterm_use_full_clear(void)
{
    xkterm_clear = xkterm_clear_full;
}

void
xkterm_render(struct xkterm *t, int width, int height, unsigned char *data)
{
    int posx, posy;
    int i,j, hoff, woff, stride, bw;
    uint32_t rune;
    cell_bitmap_t *cell;
    uint32_t *d, color;
    uint8_t attr, fgcolor, bgcolor;

    xkterm_clear(t, width, height, data);

    stride = t->pixw;
    d = (uint32_t *)(t->data);

    for (posy = 0; posy < t->cellh; posy++) {
    for (posx = 0; posx < t->cellw; posx++) {
        rune = t->vte.rows[posy][posx].rune;
        attr = t->vte.rows[posy][posx].attr;
        fgcolor = t->vte.rows[posy][posx].fgcolor;
        bgcolor = t->vte.rows[posy][posx].bgcolor;

        woff = t->conf->xkt.cell_width*posx;
        hoff = t->conf->xkt.cell_height*posy;

        if (rune <= ' ')
            cell = get_glyph(' ', false);
        else
            cell = get_glyph(rune, attr & XKT_ATTR_BOLD);

        color = colors[fgcolor] | (fgalpha << 24);
        if (t->vte.cvis && posx == t->vte.cx && posy == t->vte.cy) {
            // XXX: Bit of a hack.
            if (fgcolor == XKT_BLACK)
                color = colors[bgcolor] | (fgalpha << 24);
            for (j = 0; j < cell->ph->height; j++) {
            for (i = 0; i < cell->ph->width; i++) {
                if ((i+j) & 0x1)
                    d[stride*(j+hoff) + i + woff] = color;
            }}
            color = colors[XKT_BLACK] | (fgalpha << 24);
        } else if ((bgcolor > 0 && bgcolor < 16) || attr & XKT_ATTR_RVID) {
            if (attr & XKT_ATTR_RVID)
                color = colors[fgcolor] | (bgalpha << 24);
            else
                color = colors[bgcolor] | (bgalpha << 24);
            for (j = 0; j < t->conf->xkt.cell_height; j++) {
            for (i = 0; i < t->conf->xkt.cell_width; i++) {
                d[stride*(j+hoff) + i + woff] = color;
            }}
            if (attr & XKT_ATTR_RVID)
                color = colors[bgcolor] | (fgalpha << 24);
            else
                color = colors[fgcolor] | (fgalpha << 24);
        }

        if (rune <= ' ')
            continue;

        // XXX: This code does _not_ handle a wide character at the last cell on the edge of the window.
        bw = (cell->ph->width + 7)/8;
        for (j = 0; j < cell->ph->height; j++) {
        for (i = 0; i < cell->ph->width; i++) {
            if (stride*(j+hoff) + i + woff > stride * t->pixh)
                break;
            if (cell->bm[j*bw + (i/8)] & (0x80 >> (i & 7)))
                d[stride*(j+hoff) + i + woff] = color;
        }}
        if (cell->ph->width > t->conf->xkt.cell_width) {
            posx += (cell->ph->width / t->conf->xkt.cell_width) - 1;
            if (posx >= t->cellw)
                break;
        }
    }}
}

