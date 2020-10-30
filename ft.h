#ifndef __FT_H
#define __FT_H

#include <stdint.h>
#include <stdbool.h>

#include "xkconfig.h"

typedef struct cell_pixmap_s {
    int w;
    int h;
    unsigned char *bm;
} cell_pixmap_t;

struct psf2_header {
    unsigned char magic[4];
    uint32_t version;
    uint32_t headersize;    /* offset of bitmaps in file */
    uint32_t flags;
    uint32_t length;        /* number of glyphs */
    uint32_t charsize;      /* number of bytes for each character */
    uint32_t height, width; /* max dimensions of glyphs */
    /* charsize = height * ((width + 7) / 8) */
};

typedef struct cell_bitmap_s {
    uint32_t codepoint;
    bool primary;
    struct psf2_header *ph;
    unsigned char *bm;
    struct cell_bitmap_s *left;
    struct cell_bitmap_s *right;
} cell_bitmap_t;

void init_ft(void);
cell_pixmap_t *render_glyph(int j, int bold);
cell_bitmap_t *get_glyph(int codepoint, bool bold);
void ft_set_cell_size(struct xkconfig *conf);
void ft_load_default_fonts(void);
void ft_load_font(const char *path, bool bold);
#endif // __FT_H
