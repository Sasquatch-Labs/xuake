#ifndef __XKDEFS_H
#define __XKDEFS_H

#define XUAKE_VERSION "0.2.2"

#define XUAKE_WORKSPACES 4
#define XK_MAX_IMPULSE 99

#define XK_KBR_DELAY 500
#define XK_KBR_RATE 10

#define XK_BAT0_CAP_PATH "/sys/class/power_supply/BAT0/capacity"
#define XK_BAT0_STATUS_PATH "/sys/class/power_supply/BAT0/status"

#define XK_FT_DEFAULT_REGULAR "/usr/share/consolefonts/Uni2-Terminus20x10.psf.gz"
#define XK_FT_DEFAULT_BOLD "/usr/share/consolefonts/Uni2-TerminusBold20x10.psf.gz"

// Comment this out to remove keystroke rewriting in term.c
#define XKTERM_HARD_KEY_REMAPS 1

// There are stuck key issues with the drop down terminal.
// Uncomment to turn off key repeat tracking.
#define XKT_DISABLE_KEY_REPEAT 1

#endif // __XKDEFS_H
