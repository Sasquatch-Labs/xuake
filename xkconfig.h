#ifndef __XKCONFIG_H
#define __XKCONFIG_H

#include <stdint.h>
#include <stdbool.h>

struct xkconfig {
    struct {
        uint32_t colors[16];

        int cell_width;
        int cell_height;

        char *cmd;
        bool respawn;
    } xkt;

    int batteries;

    char *impulse_file;

    char *bg_image;
    struct {
        double r;
        double g;
        double b;
    } gl_clear_color;
    
    struct {
        uint32_t fg_focus;
        uint32_t fg_unfocus;
        uint32_t bg;
        char left[9];
        char right[9];
        char mid8[9];
        char mid4[5];
        char mid2[3];
        char mid1[3];
    } decorations;
};

#endif // __XKCONFIG_H
