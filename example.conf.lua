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
-- If you want xkterm and the drop down terminal to have different start commands
-- use xkt_cmd for xkterm and xuake_cmd for the drop down terminal.  If you want
-- them to be both the same, just use xkt_cmd.
-- xuake_cmd = "/bin/bash"

-- Colors are 24-bit RGB 8bpc Values close to 00 willbe faint.  The hex digits are
-- the same as for WWW colors, google for a hex color translator.
-- Eg, https://en.wikipedia.org/wiki/X11_color_names has a good list.
-- These are the default colors:
-- xkt_color0 = 0x000000 -- COLOR_BLACK
-- xkt_color1 = 0xcc0000 -- COLOR_RED
-- xkt_color2 = 0x00cc00 -- COLOR_GREEN
-- xkt_color3 = 0xcccc00 -- COLOR_YELLOW
-- xkt_color4 = 0x0000cc -- COLOR_BLUE
-- xkt_color5 = 0xcc00cc -- COLOR_MAGENTA
-- xkt_color6 = 0x00cccc -- COLOR_CYAN
-- xkt_color7 = 0xcccccc -- COLOR_LIGHT_GREY
-- xkt_color8 = 0x555555 -- COLOR_DARK_GREY
-- xkt_color9 = 0xff0000 -- COLOR_LIGHT_RED
-- xkt_color10 = 0x00ff00 -- COLOR_LIGHT_GREEN
-- xkt_color11 = 0xffff00 -- COLOR_LIGHT_YELLOW
-- xkt_color12 = 0x0000ff -- COLOR_LIGHT_BLUE
-- xkt_color13 = 0xff00ff -- COLOR_LIGHT_MAGENTA
-- xkt_color14 = 0x00ffff -- COLOR_LIGHT_CYAN
-- xkt_color15 = 0xffffff -- COLOR_WHITE

-- Background alpha is controlled with xkt_bgalpha, value is from 0-255, both
-- decimal and hexadecimal notation accepted.
-- xkt_bgalpha = 255
-- These are the default foreground and background colors, they should be 0-15,
-- and correspond to the colors above.  These are the default values:
-- xkt_bg = 0
-- xkt_fg = 7

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
