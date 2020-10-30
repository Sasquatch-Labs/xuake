#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <ctype.h>

#include "xkdefs.h"

#include "ft.h"

int batteries = 0;
int bat0cap = 0;
char bat0stat = 'u';

static int width;

cell_pixmap_t *bm = NULL;

time_t lastupdate = 0;
time_t lastdraw = 0;

void
init_batteries(int b, int w)
{
    int r;
    struct stat sb;
    width = w;
    batteries = 0;
    if (b == 0)
        return;

    r = stat(XK_BAT0_CAP_PATH, &sb);
    if (r < 0)
        return;

    batteries = 1;
}

int
battery_get_capacity(void)
{
    char buf[32];
    int fd;
    ssize_t r;
    time_t now;

    if (batteries <= 0 || batteries >= 2)
        return 0;

    now = time(NULL);
    if (now < lastupdate + 1)
        return 0;

    now = lastupdate;

    //printf("getting capacity for %d batteries\n", batteries);

    fd = open(XK_BAT0_CAP_PATH,O_RDONLY);
    if (fd > 0) {
        r = read(fd, buf, 32);
        if (r)
            bat0cap = atoi(buf);
        close(fd);
    }
    fd = open(XK_BAT0_STATUS_PATH,O_RDONLY);
    if (fd > 0) {
        r = read(fd, buf, 32);
        if (r)
            bat0stat = tolower(buf[0]);
        close(fd);
    }

    return 1;
}

void
draw_frame(unsigned char *b, size_t bsize, char c, int w, int h)
{
    int i,j;

    for (j = 0; j < h; j++)
        for (i = 0; i < w; i++) {
            if (j == 0 || j == h-1)
                b[j*w + i] = c;
            else if (i == 0 || i == w-1)
                b[j*w + i] = c;
        }
}

void
fill_rectangle(unsigned char *b, size_t bsize, char c, int x, int y, int dx, int dy, int w, int h)
{
    int i,j;

    for (j = 0; j < h; j++)
        for (i = 0; i < w; i++) {
            if (i >= x && i < x+dx && j >= y && j < y+dy)
                b[j*w + i] = c;
        }
}

cell_pixmap_t *
battery_draw_status(void)
{
    unsigned char *buffer;
    int box_height = 10 * batteries + 2;
    time_t now;
    int c;
    size_t bsize;

    if (batteries <= 0 || batteries > 2)
        return NULL;

    now = time(NULL);
    if (now < lastdraw + 1)
        return bm;

    lastdraw = now;

    if (!bm) {
        bm = malloc(sizeof(cell_pixmap_t));
        if (!bm)
            return NULL;
    } else if (bm->bm)
        free(bm->bm);

    //printf("drawing status for %d batteries\n", batteries);

    buffer = malloc(width*box_height*sizeof(unsigned char));
    if (!buffer) {
        return NULL;
    }

    bsize = width*box_height*sizeof(unsigned char);

    memset(buffer, 0, bsize);

    draw_frame(buffer, bsize, 1, width, box_height);

    c = 2;
    if (bat0stat == 'd')
        c = 3;
    if (bat0stat == 'f' || bat0stat == 'c')
        c = 1;
    fill_rectangle(buffer, bsize, c, 2, 2, 8, 8, width, box_height);
    c = 1;
    if (bat0cap <= 15)
        c = 4;
    else if (bat0cap <= 30)
        c = 3;
    fill_rectangle(buffer, bsize, c, 11, 2, (bat0cap*(width - 13))/100, 8, width, box_height);

    bm->bm = buffer;
    bm->w = width;
    bm->h = box_height;

    return bm;
}
