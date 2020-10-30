#include <string.h>

#include "xkdefs.h"
#include "xkconfig.h"
#include "ft.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdbool.h>
#include <zlib.h>

#include <errno.h>
#include <stdio.h>

cell_bitmap_t *ascii_cache[0x5f];
cell_bitmap_t *ascii_bold_cache[0x5f];

cell_bitmap_t *normal_cache;
cell_bitmap_t *bold_cache;

struct psf_font_data {
    union {
        struct psf2_header *ph;
        uint8_t *file;
    };
    uint8_t *data;
    bool bold;
    struct psf_font_data *next;
};

struct psf_font_data *fonts = NULL;

void
ft_set_cell_size(struct xkconfig *conf)
{
    struct psf_font_data *f = fonts;
    if (f) {
        for (; f->next; f = f->next);
        conf->xkt.cell_width = f->ph->width;
        conf->xkt.cell_height = f->ph->height;
    }
}

void
cache_glyph(cell_bitmap_t *cache, struct psf2_header *ph, uint32_t codepoint, uint8_t *bm, bool primary)
{
    cell_bitmap_t *tmp;

    if (cache->codepoint == 0) { // empty cache
        cache->codepoint = codepoint;
        cache->ph = ph;
        cache->bm = bm;
        return;
    }

    tmp = cache;

    // XXX: Should rebalance this.
    while (1) {
        if (codepoint == tmp->codepoint) {
            if (tmp->primary)
                return;
            if (primary) {
                break;
            } else
                return;
        }
        if (codepoint > tmp->codepoint) {
            if (tmp->right)
                tmp = tmp->right;
            else {
                tmp->right = malloc(sizeof(cell_bitmap_t));
                tmp = tmp->right;
                break;
            }
        } else {
            if (tmp->left)
                tmp = tmp->left;
            else {
                tmp->left = malloc(sizeof(cell_bitmap_t));
                tmp = tmp->left;
                break;
            }
        }
    }

    memset(tmp, 0, sizeof(cell_bitmap_t));
    tmp->codepoint = codepoint;
    tmp->ph = ph;
    tmp->bm = bm;
    tmp->primary = primary;
}

cell_bitmap_t *
search_cache(cell_bitmap_t *cache, uint32_t codepoint)
{
    cell_bitmap_t *tmp = cache;

    while (1) {
        if (codepoint == tmp->codepoint) {
            return tmp;
        }
        if (codepoint > tmp->codepoint) {
            if (tmp->right)
                tmp = tmp->right;
            else
                break;
        } else {
            if (tmp->left)
                tmp = tmp->left;
            else
                break;
        }
    }

    if (codepoint == 0xfffd)
        return NULL;

    return search_cache(cache, 0xfffd);
}

void
ft_load_font(const char *path, bool bold) 
{
    int i, r;
    bool pri;
    uint8_t *psf, *p;
    uint8_t *psf_gzip;
    uint32_t uncomp_len;
    uint32_t codepoint;
    z_stream strm;
    struct stat sb;
    struct psf2_header *ph;
    cell_bitmap_t *cache = normal_cache;
    struct psf_font_data *font_tmp;

    if (bold)
        cache = bold_cache;

    // XXX: Nowhere near enough error handling in this.
    i = open(path, O_RDONLY);
    fstat(i, &sb);
    psf_gzip = mmap(NULL, sb.st_size, PROT_READ, MAP_PRIVATE, i, 0);

    if (psf_gzip[0] == 0x1f && psf_gzip[1] == 0x8b) { // gzip magic numbers
        // This is a bit dirty and unreliable (uncompressed length at file end is not guaranteed to be correct)
        uncomp_len = (uint32_t)psf_gzip[sb.st_size - 1] << 24 
            | (uint32_t)psf_gzip[sb.st_size - 2] << 16 
            | (uint32_t)psf_gzip[sb.st_size - 3] << 8 
            | (uint32_t)psf_gzip[sb.st_size - 4];

        //printf("Uncompressed font length: 0x%x: %02x %02x %02x %02x\n", uncomp_len, psf_gzip[sb.st_size - 1] << 24,
        //    psf_gzip[sb.st_size - 2], psf_gzip[sb.st_size - 3], psf_gzip[sb.st_size - 4]);

        psf = mmap(NULL, uncomp_len, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
        if (psf == MAP_FAILED) {
            //printf("Map Failed: %d %s\n", errno, strerror(errno));
            exit(1);
        }

        /* allocate inflate state */
        strm.zalloc = Z_NULL;
        strm.zfree = Z_NULL;
        strm.opaque = Z_NULL;
        strm.avail_in = 0;
        strm.next_in = Z_NULL;
        r = inflateInit2(&strm, 16+MAX_WBITS);
        
        strm.avail_in = sb.st_size;
        strm.next_in = psf_gzip;

        strm.avail_out = uncomp_len;
        strm.next_out = psf;

        inflate(&strm, Z_NO_FLUSH);
        
        munmap(psf_gzip, sb.st_size);
        close(i);
    } else if (psf_gzip[0] == 0x72 && psf_gzip[1] == 0xb5 && psf_gzip[2] == 0x4a && psf_gzip[3] == 0x86) {
        uncomp_len = sb.st_size;
        psf = psf_gzip;
    } else {
        return;
    }

    ph = (struct psf2_header *)psf;

    if (!ph->flags)
        return;

    if (fonts)
        if (ph->width % fonts->ph->width || ph->height != fonts->ph->height)
            return;

    font_tmp = malloc(sizeof(struct psf_font_data));
    memset(font_tmp, 0, sizeof(struct psf_font_data));
    font_tmp->ph = ph;
    font_tmp->bold = bold;
    font_tmp->data = &(font_tmp->file[ph->headersize]);
    font_tmp->next = fonts;
    fonts = font_tmp;

    p = &(psf[ph->headersize]);
    p += ph->charsize * ph->length;
    codepoint = 0;
    i = 0;
    pri = true;
    while (p < &(psf[uncomp_len])) {
        if (*p == 0xff) { // table terminator
            p++; i++;
            codepoint = 0;
            pri = true;
            continue;
        } else if ((*p & 0xf8) == 0xf0) { // 4 byte sequence
            if ((p[1] & 0xc0) == 0x80 &&
                (p[2] & 0xc0) == 0x80 &&
                (p[3] & 0xc0) == 0x80) {
                codepoint = (p[0] & 0x07) << 18 | (p[1] & 0x3f) << 12 | (p[2] & 0x3f) << 6 | p[3] & 0x3f;
                p += 4;
            } else {
                p++; continue;
            }
        } else if ((*p & 0xf0) == 0xe0) { // 3 byte sequence
            if ((p[1] & 0xc0) == 0x80 &&
                (p[2] & 0xc0) == 0x80) {
                codepoint = (p[0] & 0x0f) << 12 | (p[1] & 0x3f) << 6 | p[2] & 0x3f;
                p += 3;
            } else {
                p++; continue;
            }
        } else if ((*p & 0xe0) == 0xc0) { // 2 byte sequence
            if ((p[1] & 0xc0) == 0x80) {
                codepoint = (p[0] & 0x1f) << 6 | p[1] & 0x3f;
                p += 2;
            } else {
                p++; continue;
            }
        } else if (*p & 0x80) { // illegal byte sequence
            p++; continue;
        } else {
            codepoint = *p++;
        }
        cache_glyph(cache, ph, codepoint, &(psf[ph->headersize + i*ph->charsize]), pri);
        pri = false;
    }
}

void
ft_load_default_fonts(void)
{
    if (fonts)
        return;
    ft_load_font(XK_FT_DEFAULT_REGULAR, false);
    ft_load_font(XK_FT_DEFAULT_BOLD, true);
}

void
init_ft(void)
{
    bold_cache = malloc(sizeof(cell_bitmap_t));
    normal_cache = malloc(sizeof(cell_bitmap_t));
    memset(bold_cache, 0, sizeof(cell_bitmap_t));
    memset(normal_cache, 0, sizeof(cell_bitmap_t));

    memset(ascii_cache, 0, 0x5e*sizeof(cell_bitmap_t *));
    memset(ascii_bold_cache, 0, 0x5e*sizeof(cell_bitmap_t *));
}

cell_bitmap_t *
get_glyph(int codepoint, bool bold)
{
    cell_bitmap_t *g;

    if (codepoint <= 0x7e && codepoint >= 0x20) {
        if (bold) {
            g = ascii_bold_cache[codepoint - 0x20];
        } else {
            g = ascii_cache[codepoint - 0x20];
        }
        if (g) return g;
    }
    if (bold)
        g = search_cache(bold_cache, codepoint);
    else 
        g = search_cache(normal_cache, codepoint);

    if (codepoint <= 0x7e && codepoint >= 0x20) {
        if (bold)
            ascii_bold_cache[codepoint - 0x20] = g;
        else
            ascii_cache[codepoint - 0x20] = g;
    }
    return g;
}
