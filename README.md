# Xuake

Are you sick of WYSIWYG everyone-says-this-is-intuitive-but-it's-dumb RAM
eating desktop environments?  Not a huge fan of tiling Window Managers?
Quake player from back in the day?

This is Xuake, A WYGIWYG Anti-Desktop.

(And if you are a fan of tiling window managers, check out Sway.  That crew
is awesome and they wrote wlroots which makes this project possible.)

## How do you use this stupid thing?  It's just a grey screen.

Hit F1.  That's the default keybinding for the drop-down terminal.  Alt+left click
to move a window.  Alt+right click to resize a window.  Alt-tab cycles windows.
Alt+Escape is the default quit hotkey.  `xk exit` will terminate xuake, as will
exiting the program started in the drop down terminal.

Lua config location is `$HOME/.xuake/conf.lua`  Copy `example.conf.lua` to there
and edit it.  The comments should cover most options.

It's very spartan.  That's the point.

## Building and Installing

Ain't no fancy build system here.  Just a plain ol' Makefile.  Build and
install are:

> `make`

> `sudo make install`

By default it just drops two executables (`xuake` and `xkterm`) into
`/usr/local/bin`.

You'll need to have these packages installed:
* wlroots (== 0.11.0)
* Wayland (>= 1.18)
* OpenGL/EGL/GLESv2
* Lua (>= 5.4.0)
* libtsm
* zlib
* xcb
* xkbcommon
* libpng
* pixman

Many are probably installed by default on your system.  If you are running a
Debian derived system, you'll need to have dev packages for all of those.

My development system is Debian Bullseye.  I do not test on anything else.

## Copying

Unless otherwise specified, the code is licensed under the MIT license.  See
LICENSE file for the full terms.

`readpng.c` and `readpng.h` were originally copied from the example source for
Chapter 13 of "PNG: The Definitive Guide" by Greg Roelofs.  The dual BSD-GPL2+
license for those files is at the top the files and in the file LICENSE.pngbook.
They are used here under the terms of the BSD license.  So.  Without further ado:

> This product includes software developed by Greg Roelofs
> and contributors for the book, "PNG: The Definitive Guide,"
> published by O'Reilly and Associates.

This codebase started with the TinyWL code from the wlroots repo.  `tinywl.c`
was been moved to `xuake.c` and heavily modified.  The modifications to `xuake.c`
should be enough to be transformative and `xuake.c` is claimed under the same
MIT license as the rest of the project, and I believe this to be compatible with
the TinyWL licence, reproduced in LICENSE.tinywl.

Sway (https://github.com/swaywm/sway), wlroots (https://github.com/swaywm/wlroots.),
and rootston (https://github.com/swaywm/rootston) have provided a lot of examples
and some larger bits of cribbed code (`xwayland.c` takes its skeleton from Sway's
implementation.  Many smaller chunks of copied code are scattered throughout).
These three projects are all released under the same MIT License.  Much thanks to
Drew DeVault, Jari Vetoniemi and all other contributors.  These projects have
been an invaluable source of knowledge and a foundation for Xuake.

## Known Issues

This is early alpha software written by someone who is still learning about
Wayland.

It aborts on me intermittently.  Window resize seems to be the big culprit here.
I'm just going to need to spend a lot more time running it in gdb.

There's an intermittent issue with stuck keys in the drop down terminal.  Key
repeat processing can be disabled in `xkdefs.h`.  If you want to live dangerously,
toggling the terminal clears the key repeat list.

Xwayland seems to get its input independent of the keyboard/seat handling routines.
You'll find that your hotkeys for xuake still get sent to X11 surfaces.  This is
most annoying when using Alt-Tab as the `win_cycle` keypress, as you'll find that
X11 surfaces end up having a stuck-on tab key when you focus them with Alt-Tab
cycling.  Tapping the offending stuck key without modifiers held down unstick it.

xkterm doesn't understand mice input at all.  This is intended as a feature by the
author.  wl-clipboard works well enough for copy/paste.  All other interaction
between rodents and terminals is considered harmful.

I have no idea what happens if you try to launch this from one of those graphical
login things.  I boot to the text console and start xuake directly from the
command line.

Drop down terminal doesn't resize when resizing the xuake window when using the
wayland or X11 backends.  Doesn't seem to be a good signal for this in wlroots.
Doesn't seem like a big deal.

## TODO / ROADMAP

v0.2.0 is the first public release of xuake.  It's early alpha software and
missing many intended features.  The biggest offenders here are the Lua/xk
commands and callbacks.  Most all architectural pieces are in place and working
in at least a minimal form.  The only large pieces missing are touch screen
handling and virtual keyboard support (but those are only needed for mobile
support).

### 0.3.0 Mandatory

- screen capture protocol
- Rich globals for Lua
- Rich library of commands/functions for Lua
- xdg positioner handling
- Xwayland: unmanaged windows aren't handled very well
- bind key to send different keypress -- Probably want to do this via impulse that calls an `xk_send_key` function

### Completed 0.3.0:

- Separate resize command from warp (leaves window at its location)

### Completed 0.2.0:

- Monacle mode
- enrich data stored about surfaces - WONTFIX - wlroots handles this for me if I dig
  through all the structs.
- control proto and Lua api commands:
    - list windows - DONE
    - move windows - DONE
    - resize windows - DONE
    - focus window - DONE
    - kill windows - DONE
    - switch workspace - DONE
- Battery status toggle in config & command opts
- lua configuration for drop down terminal start command
- primary selection
- Event Callbacks for Lua
- All key command functions (key.c/`xkkey_*`) should be accessible from lua/xk
- fix fullscreen
- Advanced copy-paste buffer handling (Sort-of)

### Backlog

- Advanced copy-paste buffer handling improvements
- configurable number of workspaces - WONTFIX/DONE: it's a compile option in xkdefs.h
- Xwayland hotkey passthrough bug
  (x clients get key presses that wayland clients don't, like Alt-Tab)
- Xwayland multihead mouse position bugs
  (x clients on second monitor don't see mouse events)
  (This may have been fixed by recent Wayland/Xwayland, have to double check)
- HiDPI support -- Optional, may decide not to support
- layer-shell -- maybe not needed? -- Definitely not needed. -- Definitely maybe
- Damage tracking, which tracks which parts of the screen are changing and
  minimizes redraws accordingly.
- virtual keyboards
- keyboard re-mapping
- font cache tree rebalancing

## What's with the name?

The prototype for this was a duct-taped together experiment that went on
for way too long.  In 2008, I was a slightly grumpy KDE3 user.  It worked
and was reasonably stable, but it was slow.  Then KDE4 came out.  It was
slower and buggy AF.  I regretted trying it out on the 4.0 point release.
It was bad enough that I realized I needed to move to something else, and
going back to KDE3 didn't feel right.  After some thought and frustration,
I decided to go minimalist.  I realized that you could turn off anything in
Enlightenment E16, which meant you could turn off _everything_.

So, I made a theme for E16 that turned off all window decorations except
a 4 pixel border.  There are toggles for turning off the pagers and the
desktop switcher bar, I turned them off.  Some abusive editing of the
config files got rid of the ibox.  xterm has a `-into` flag which will
cause it to reparent into the window given as its argument.  Some quality
time on tronche.com later, and I had a hacky but functional Quake-style
drop down terminal that hijacked F1.  (Which also stomped all over the
dumb GUI help systems in things like FireFox that took forever to load
and only contained useless content.)  I told e16 that it was borderless
and needed to be on all desktops and on top.

I named the drop-down terminal Xuake, a portmantaue of Xterm and Quake.  The
code was a gross mess, and though I've made a bunch of fixes and improvements,
it's still enough of a garbage hack that I'm never going to release it.  But I
couldn't give up the name.  Hence, Xuake

I've been extremely happy with this setup for over a decade.  But the party
has come to an end.  Xorg is going away.  The Enlightenment Team is converting
the latest and greatest to be Wayland compositors, which means E22 and up.
If I'm going to continue to be happy long into the future, my prototype needs
to be upgraded to a real compositor.  The other joy of turning this into a
real, cut-down compositor is being able to smooth over some rough edges
(there are some intermittent crashes in the original xuake code that I've never
been able to track down, Xorg's monitor hotplug sucks, Xorg's keyboard hotplug
sucks are the things that jump immediately to mind).  Also, I get to get rid
of a ton of legacy code.  E16 is much heavier than I really need.
