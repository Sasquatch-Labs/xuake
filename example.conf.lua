-- Copy this file to $HOME/.xuake/conf.lua and customize as you see fit.

-- These paths work on a minimal Debian install, for other distros, YMMV.
-- You can load more than one font, it will add any new unicode characters
-- from later fonts to its cache.  Only PSF2 fonts with unicode tables work.
-- ALL FONTS MUST HAVE THE SAME CELL SIZE.  Xuake will silently ignore any
-- mismatched fonts.
xk_load_font("/usr/share/consolefonts/Uni2-Terminus20x10.psf.gz", false)
xk_load_font("/usr/share/consolefonts/Uni2-TerminusBold20x10.psf.gz", true)

-- Reasonable keybindings.
-- QUIRK: if you want to bind Shift as a modifier to a key that is affected
-- by the shift key (eg, a letter or number key), you need to both or XK_MOD_SHIFT
-- into the modifier field and use the shifted version of the key, so alt-shift-a
-- gets rendered as XK_MOD_ALT|XK_MOD_SHIFT, "A".  This is because the sym that
-- is seen by the keybinding system is post-translation by xkbcommon, and the
-- modifiers seen include all relevant modifier flags, which includes the shift
-- key.
xk_bind_key("win_next_ws", XK_MOD_CTRL|XK_MOD_ALT, "n")
xk_bind_key("win_prev_ws", XK_MOD_CTRL|XK_MOD_ALT, "p")
xk_bind_key("win_close", XK_MOD_CTRL|XK_MOD_ALT, "x")
xk_bind_key("ws", XK_MOD_ALT, XK_F1, 0)
xk_bind_key("ws", XK_MOD_ALT, XK_F2, 1)
xk_bind_key("ws", XK_MOD_ALT, XK_F3, 2)
xk_bind_key("ws", XK_MOD_ALT, XK_F4, 3)
xk_bind_key("win_cycle", XK_MOD_ALT, XK_Tab)
xk_bind_key("toggle_xkt", 0, XK_F1)
xk_bind_key("xuake_exit", XK_MOD_ALT, XK_Escape)

-- Default: check to see if the system has a battery, and if so, display
-- a battery status bar below the clock (if possible)

-- xk_batteries = 1

-- This has to be an 8bpc RGBA PNG.  Nothing else works.  The PNG code is
-- very simplistic and has a bunch of hardcoded assumptions.  If your image
-- doesn't work, open it up in the GIMP, set the color mode to RGB, then
-- export-as and be sure to set the pixel format in the drop down (the default
-- is automatic, which may not work).

-- bg_image = "/path/to/your/background.png"

-- Startup command behavior options:
-- xkt_respawn tells the drop down terminal to restart xkt_cmd when it exits.
-- Default: exiting the xkt_cmd program causes xuake to terminate.
xkt_respawn = false
xkt_cmd = "/bin/bash"

-- Color scheme based on/inspired by White/Green/Amber phosphors
-- When I was a kid in the 80s, My first computer had a 4-color
-- amber monitor, we had a black & white TV and my school had
-- Apple IIs with green phosphor monitors.  This is mostly nostalgia
-- kick, but I think it may be easier on my eyes.

-- Colors are 32-bit ARGB 8bpc -or- first two hex digits are your alpha
-- channel, use ff if you don't want see-through.  Values close to 00 will
-- be faint.  The last 6 hex digits are the same as for WWW colors, google
-- for a hex color translator.  Eg, https://en.wikipedia.org/wiki/X11_color_names
-- has a good list.
xkt_color0 = 0xff000000 -- COLOR_BLACK -> X11 black
xkt_color1 = 0xffdaa520 -- COLOR_RED -> X11 goldenrod
xkt_color2 = 0xffffffff -- COLOR_GREEN -> X11 white
xkt_color3 = 0xffcdcd00 -- COLOR_YELLOW -> X11 yellow3
xkt_color4 = 0xff999999 -- COLOR_BLUE -> X11 grey60
xkt_color5 = 0xffcdcd00 -- COLOR_MAGENTA -> X11 yellow3
xkt_color6 = 0xffffff00 -- COLOR_CYAN -> X11 yellow
xkt_color7 = 0xff00ff00 -- COLOR_LIGHT_GREY -> X11 green
xkt_color8 = 0xff4d4d4d -- COLOR_DARK_GREY -> X11 grey30
xkt_color9 = 0xffdaa520 -- COLOR_LIGHT_RED -> X11 goldenrod
xkt_color10 = 0xffffffff -- COLOR_LIGHT_GREEN -> X11 white
xkt_color11 = 0xffffff00 -- COLOR_LIGHT_YELLOW -> X11 yellow
xkt_color12 = 0xffe5e5e5 -- COLOR_LIGHT_BLUE -> X11 grey90
xkt_color13 = 0xffffff00 -- COLOR_LIGHT_MAGENTA -> X11 yellow
xkt_color14 = 0xff00ff00 -- COLOR_LIGHT_CYAN -> X11 green
xkt_color15 = 0xff00ff00 -- COLOR_WHITE -> X11 green

xkt_fgcolor = 0xff00cd00
xkt_bgcolor = 0xbe000000

-- These are bit maps.  They are 8x4 pixels for left, right & mid8.  4x4 for mid4,
-- 2x4 for mid2 and 1x2 for mid1.  Decorations are only drawn along the top of
-- windows.  left is the upper left corner, right the upper right, and the middle
-- is filled by repeating mid8 to fill the middle as much as possible, then mid4,
-- mid2 & mid1 fill in the remainder.
--
-- Example: alternating pixel dither:
-- *.*.*.*. 1010 1010 aa  *.*. 1010 a  *.         *
-- .*.*.*.* 0101 0101 55  .*.* 0101 5  .* 1001 9  .
-- *.*.*.*. 1010 1010 aa  *.*. 1010 a  *.         *
-- .*.*.*.* 0101 0101 55  .*.* 0101 5  .* 1001 9  . 1010 a
decorations["left"] = "aa55aa55"
decorations["right"] = "aa55aa55"
decorations["mid8"] = "aa55aa55"
decorations["mid4"] = "a5a5"
decorations["mid2"] = "99"
decorations["mid1"] = "a"
decorations["fg_focus"] = 0xffcccccc
decorations["fg_unfocus"] = 0xff000000
decorations["bg"] = 0xff000000
