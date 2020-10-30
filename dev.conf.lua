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
xkt_cmd = "/bin/bash"

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

decorations["left"] = "aa55aa55"
decorations["right"] = "aa55aa55"
decorations["mid8"] = "aa55aa55"
decorations["mid4"] = "a5a5"
decorations["mid2"] = "99"
decorations["mid1"] = "a"
decorations["fg_focus"] = 0xffcccccc
decorations["fg_unfocus"] = 0xff000000
decorations["bg"] = 0xff000000
