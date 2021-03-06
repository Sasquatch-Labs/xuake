#ifndef __TERM_H
#define __TERM_H

#include "xkconfig.h"
#include "ft.h"

#define XKT_BLACK 0
#define XKT_RED 1
#define XKT_GREEN 2
#define XKT_YELLOW 3
#define XKT_BLUE 4
#define XKT_MAGENTA 5
#define XKT_CYAN 6
#define XKT_LIGHT_GREY 7
#define XKT_DARK_GREY 8
#define XKT_LIGHT_RED 9
#define XKT_LIGHT_GREEN 10
#define XKT_LIGHT_YELLOW 11
#define XKT_LIGHT_BLUE 12
#define XKT_LIGHT_MAGENTA 13
#define XKT_LIGHT_CYAN 14
#define XKT_WHITE 15

#define XKT_PBUF_SZ 32
#define XKT_NPAR 16

enum {
    XKT_ATTR_BOLD = 1,
    XKT_ATTR_BLINK = 2,
    XKT_ATTR_RVID = 4,
    /* I kinda hate the XKT_ATTR_PREMOV code.  This is to cover for some
     * questionable behavior on vim's part when running within tmux.
     * When vim clears the screen for the area after the displayed files
     * (The lines that start with ~), vim jumps to the beginning of the line
     * with ^[[H and then prints '~' followed by enough spaces to fill out
     * the line.  Which means it trips a line wrap with the last space unless
     * the final column is handled specially.  I don't know why ^[[K isn't
     * good enough.
     */
    XKT_ATTR_PREMOV = 128
};

struct xkt_cell {
    uint32_t rune;
    uint8_t fgcolor;
    uint8_t bgcolor;
    uint8_t attr;
    bool dirty;
};

struct xkt_vte {
    unsigned int state;
    bool qmark;
    unsigned int n;
    unsigned int param[XKT_NPAR];
    char *p;
    char pbuf[XKT_PBUF_SZ];
    uint32_t ucs4;
    int un;
    unsigned int usavstate;
    int ncells;
    struct xkt_cell *cells;
    struct xkt_cell **rows;
    int cx, cy;
    bool cvis;
    int wtop, wbot;
    uint8_t fgcolor;
    uint8_t bgcolor;
    uint8_t attr;
    bool decckm;
    bool wrap;
};

struct xkterm {
    int pty;
    int apty;
    pid_t child;
    uint32_t pixw;
    uint32_t pixh;
    uint32_t cellw;
    uint32_t cellh;
    unsigned char *data;
    struct wlr_texture *texture;
    struct xkt_vte vte;
    struct xkconfig *conf;
};

void
exec_startcmd(char *cmd);

pid_t xkterm_forkpty(struct xkterm *t);
void xkterm_resize(struct xkterm *t, uint32_t w, uint32_t h);
void xkt_vte_init(struct xkterm *t);
//void xkt_vte_write_cb(struct tsm_vte *vte, const char *u8, size_t len, void *data);
void xkterm_key_input(struct xkterm *t, xkb_keysym_t sym, uint32_t modifiers);
bool xkterm_check(struct xkterm *t);
void xkterm_clear_dirty(struct xkterm *t, int width, int height, unsigned char *data);
void xkterm_clear_full(struct xkterm *t, int width, int height, unsigned char *data);
void xkterm_use_full_clear(void);
void xkterm_render(struct xkterm *t, int width, int height, unsigned char *data);
void xkterm_set_colors(struct xkconfig *conf);
uint32_t *xkterm_get_colors(void);

void init_batteries(int b, int w);
int battery_get_capacity(void);
cell_pixmap_t *battery_draw_status(void);

void init_clock(int w);
int clock_check(void);
cell_pixmap_t *clock_draw(void);

#endif // __TERM_H
