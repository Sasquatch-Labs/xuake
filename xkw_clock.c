#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <ctype.h>
#include <math.h>

#include "ft.h"

static int width = 0;

cell_pixmap_t *clk = NULL;

time_t clk_lastupdate = 0;

void
init_clock(int w)
{
    width = w;
}

void
blit(unsigned char *b, int x, int y, int w, int h)
{
    //printf("Attempting to blit [%d, %d] in [%d, %d]\n", x, y, w, h);
    if (x*y > w*h)
        return;

    if (x*y < 0)
        return;

    b[x+y*w] = 255;
}

void
draw_circle(unsigned char *b, int w, int h, unsigned int radius)
{

    unsigned int x0 = w/2;
    unsigned int y0 = h/2;
    //unsigned int radius = w <= h ? w/2 : h/2;
    //radius -= 2;

    int f = 1 - radius;
    int ddF_x = 0;
    int ddF_y = -2 * radius;
    int x = 0;
    int y = radius;

    blit(b, x0, y0 + radius, w, h);
    blit(b, x0, y0 - radius, w, h);
    blit(b, x0 + radius, y0, w, h);
    blit(b, x0 - radius, y0, w, h);

    while(x < y) {
        if(f >= 0) {
            y--;
            ddF_y += 2;
            f += ddF_y;
        }
        x++;
        ddF_x += 2;
        f += ddF_x + 1;
        blit(b, x0 + x, y0 + y, w, h);
        blit(b, x0 - x, y0 + y, w, h);
        blit(b, x0 + x, y0 - y, w, h);
        blit(b, x0 - x, y0 - y, w, h);
        blit(b, x0 + y, y0 + x, w, h);
        blit(b, x0 - y, y0 + x, w, h);
        blit(b, x0 + y, y0 - x, w, h);
        blit(b, x0 - y, y0 - x, w, h);
    }
}

void
draw_line(unsigned char *b, int w, int h, int x0, int y0, int x1, int y1)
{
    int x, y;
    int dx, dy;
    int m_new;
    int slope_error_new;

    dx = abs(x1 - x0);
    dy = abs(y1 - y0);

    if (dy <= dx) {
        if (x0 > x1) {
            x = x1;
            x1 = x0;
            x0 = x;

            y = y1;
            y1 = y0;
            y0 = y;
        }

        m_new = 2 * (y1 - y0);
        slope_error_new = m_new - (x1 - x0);

        //printf("Draw Line from: [%d, %d] to [%d, %d] in field [%d, %d]\n", x0, y0, x1, y1, w, h);

        for (x = x0, y = y0; x <= x1; x++) {
            blit(b, x, y, w, h);
            //printf("Blitting [%d, %d]\n", x, y);

            // Add slope to increment angle formed
            slope_error_new += m_new;

            // Slope error reached limit, time to
            // increment y and update slope error.
            if (m_new > 0 && slope_error_new >= 0) {
                y++;
                slope_error_new  -= 2 * (x1 - x0);
            } else if (m_new < 0 && slope_error_new <= 0) {
                y--;
                slope_error_new  += 2 * (x1 - x0);
            }
        }
    } else {
        if (y0 > y1) {
            x = x1;
            x1 = x0;
            x0 = x;

            y = y1;
            y1 = y0;
            y0 = y;
        }

        m_new = 2 * (x1 - x0);
        slope_error_new = m_new - (y1 - y0);

        //printf("Draw Line from (y-axis): [%d, %d] to [%d, %d] in field [%d, %d]\n", x0, y0, x1, y1, w, h);

        for (x = x0, y = y0; y <= y1; y++) {
            blit(b, x, y, w, h);
            //printf("Blitting [%d, %d]\n", x, y);

            // Add slope to increment angle formed
            slope_error_new += m_new;

            // Slope error reached limit, time to
            // increment y and update slope error.
            if (m_new > 0 && slope_error_new >= 0) {
                x++;
                slope_error_new  -= 2 * (y1 - y0);
            } else if (m_new < 0 && slope_error_new <= 0) {
                x--;
                slope_error_new  += 2 * (y1 - y0);
            }
        }
    }
}

void
aa_pass(cell_pixmap_t *bm)
{
    int i,j,k,n,m;

    // Nested for loop go BRRRRRRR

    // First pass: calculate neighbors minesweeper style.  >2 neighbors gets a marker (128)
    for (j = 0; j < bm->h; j++) {
        for (i = 0; i < bm->w; i++) {
            if (bm->bm[j*bm->w + i] == 255)
                continue;
            k = 0;
            for (n = j ? -1 : 0; n < 2 && n+j < bm->h; n++) {
                for (m = i ? -1 : 0; m < 2 && m+i < bm->w; m++) {
                    if (bm->bm[(n+j)*bm->w + i+m] == 255)
                        k++;
                }
            }
            if (k >= 2) {
                bm->bm[j*bm->w + i] = 128;
            }
        }
    }

    // Markers from the previous pass get upgraded to be real pixels!
    for (j = 0; j < bm->h; j++)
        for (i = 0; i < bm->w; i++)
            if (bm->bm[j*bm->w + i] == 128)
                bm->bm[j*bm->w + i] = 255;

    // One more minesweeper pass: but this time, we use the neighbor
    // count to darken the pixel (this then gets applied to the alpha
    // channel of the final render)  This block probably doesn't do much
    // visually.  But whatevs.
    for (j = 0; j < bm->h; j++) {
        for (i = 0; i < bm->w; i++) {
            if (bm->bm[j*bm->w + i] == 255)
                continue;
            k = 0;
            for (n = j ? -1 : 0; n < 2 && n+j < bm->h; n++) {
                for (m = i ? -1 : 0; m < 2 && m+i < bm->w; m++) {
                    if (bm->bm[(n+j)*bm->w + i+m] == 255)
                        k++;
                }
            }
            bm->bm[j*bm->w + i] = k*28;
        }
    }
}

int
clock_check(void)
{
    time_t now;

    now = time(NULL);

    if (now < clk_lastupdate + 60)
        return 0;

    return 1;
}

cell_pixmap_t *
clock_draw(void)
{
    unsigned char *b;
    time_t now;
    size_t bsize;
    struct tm tm;
    double minrad;
    double hourrad;
    double radius;

    now = time(NULL);

    if (now < clk_lastupdate + 60)
        return clk;

    clk_lastupdate = now;

    if (!clk) {
        clk = malloc(sizeof(cell_pixmap_t));
        if (!clk)
            return NULL;
    } else if (clk->bm)
        free(clk->bm);

    bsize = width*width*sizeof(unsigned char);
    b = malloc(bsize);
    if (!b)
        return NULL;

    memset(b, 0, bsize);

    localtime_r(&now, &tm);

    /* Ok, to get a clock coordinate from a time, we need:
     * pi*hour/6 - pi/2) or pi*(hour - 3)/6
     * Likewise:
     * pi*min/30 - pi/2) or pi*(min - 15)/30
     *
     * With hour hand moving through the 5 minute arc:
     * pi*(hour - 3 + min/60.0)/6
     *
     * The y axis increases down, so counterclockwise in
     * normal polar coordinates become clockwise in our pixmap.
     *
     * This also means that we need to offset the angle by -90deg
     * because we're trying to start with 0 being the negative y-axis
     */

    tm.tm_hour = tm.tm_hour % 12;

    radius = (double)(width/2 - 2);

    minrad = M_PI * (tm.tm_min - 15) / 30.0;
    hourrad = M_PI * (tm.tm_hour - 3 + tm.tm_min/60.0) / 6.0;

    //printf("minutes: %d; %f\n", tm.tm_min, minrad);
    //printf("hours: %d; %f\n", tm.tm_hour, hourrad);

    draw_line(b, width, width, width/2 - (int)(0.1*radius*cos(minrad)), width/2 - (int)(0.1*radius*sin(minrad)), width/2 + (int)(0.9*radius*cos(minrad)), width/2 + (int)(0.9*radius*sin(minrad)));
    draw_line(b, width, width, width/2 - (int)(0.1*radius*cos(hourrad)), width/2 - (int)(0.1*radius*sin(hourrad)), width/2 + (int)(0.5*radius*cos(hourrad)), width/2 + (int)(0.5*radius*sin(hourrad)));

    draw_circle(b, width, width, width/2 - 2);

    clk->w = width;
    clk->h = width;
    clk->bm = b;

    aa_pass(clk);

    return clk;
}
