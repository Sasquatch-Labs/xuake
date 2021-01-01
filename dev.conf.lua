-- Dev config for testing, almost no keys bound, and they are bound such that
-- they don't get masked by the xuake I'm running as my desktop

-- works on Debian, YMMV.
xk_load_font("/usr/share/consolefonts/Uni2-Terminus20x10.psf.gz", false)
xk_load_font("/usr/share/consolefonts/Uni2-TerminusBold20x10.psf.gz", true)

-- Dev keybindings.  Use C-F1 for drop terminal, and use dwm keys which don't
-- conflict with my day-to-day-defaults (see example.conf.lua)
xk_bind_key("toggle_xkt", XK_MOD_CTRL, XK_F1)
xk_bind_key("win_cycle", XK_MOD_ALT, "j")
xk_bind_key("win_acycle", XK_MOD_ALT, "k")
xk_bind_key("ws", XK_MOD_ALT, '1', 0)
xk_bind_key("ws", XK_MOD_ALT, '2', 1)
xk_bind_key("ws", XK_MOD_ALT, '3', 2)
xk_bind_key("ws", XK_MOD_ALT, '4', 3)
xk_bind_key("win_to_ws", XK_MOD_ALT|XK_MOD_SHIFT, '!', 0)
xk_bind_key("win_to_ws", XK_MOD_ALT|XK_MOD_SHIFT, '@', 1)
xk_bind_key("win_to_ws", XK_MOD_ALT|XK_MOD_SHIFT, '#', 2)
xk_bind_key("win_to_ws", XK_MOD_ALT|XK_MOD_SHIFT, '$', 3)
xk_bind_key("impulse", XK_MOD_CTRL|XK_MOD_ALT, "i", 0)

xkt_respawn = false
xkt_cmd = "/bin/dash -l"

-- This is my personal color scheme.  It's based on colors from the
-- P1, P3 and P4 phosphors, which are the colors that dominated the
-- computer displays of my youth.
xkt_color0 = 0x000000 -- COLOR_BLACK, 0%
xkt_color1 = 0xcc9900 -- COLOR_RED, P3 80%
xkt_color2 = 0xcccccc -- COLOR_GREEN, P4 80%
xkt_color3 = 0xcc9900 -- COLOR_YELLOW, P3 80%
xkt_color4 = 0xaaaaaa -- COLOR_BLUE, P4 67%
xkt_color5 = 0xffbf00 -- COLOR_MAGENTA, P3 100%
xkt_color6 = 0xaaaaaa -- COLOR_CYAN, P4 67%
xkt_color7 = 0x00cc00 -- COLOR_LIGHT_GREY, P1 80%
xkt_color8 = 0x555555 -- COLOR_DARK_GREY, P4 33%
xkt_color9 = 0xffbf00 -- COLOR_LIGHT_RED, P3 100%
xkt_color10 = 0xffffff -- COLOR_LIGHT_GREEN, P4 100%
xkt_color11 = 0xffbf00 -- COLOR_LIGHT_YELLOW, P3 100%
xkt_color12 = 0xffffff -- COLOR_LIGHT_BLUE, P4 100%
xkt_color13 = 0xffbf00 -- COLOR_LIGHT_MAGENTA, P3 100%
xkt_color14 = 0xffffff -- COLOR_LIGHT_CYAN, P4 100%
xkt_color15 = 0x00ff00 -- COLOR_WHITE, P1 100%

-- xkt_bgalpha = 255
-- xkt_bg = 0
-- xkt_fg = 7

decorations["left"] = "aa55aa55"
decorations["right"] = "aa55aa55"
decorations["mid8"] = "aa55aa55"
decorations["mid4"] = "a5a5"
decorations["mid2"] = "99"
decorations["mid1"] = "a"
decorations["fg_focus"] = 0xffcccccc
decorations["fg_unfocus"] = 0xff000000
decorations["bg"] = 0xff000000
