# Matlab focus fixer

Prevents Matlab from stealing focus. Tries to set the _NET_WM_USER_TIME to a proper value to get the correct window behavior.

The focus is only given to windows which have been started less then 2 sec (2000 ms) since last user interaction in Matlab.

The idea is to intercept the X11 calls with a dynamic library (via LD_PRELOAD) and patch the calls to comply with the X11 documentation.


# Build

```make```

# Start

```LD_PRELOAD=$PWD/focus_fixer.so matlab```

# TODO

* Make new windows minimized if main window is out of focus (not the active window).
* Make new windows show up in parent windows workspace.
* Set the PID of the splash screen and make it not on always on top 
* Use the splash screen workspace location for the main window.
* Clean the code up :)
* License, wmctrl is GPL ?, the initial version worked without snippets of code from wmctrl.

# Reference list
* https://bugs.openjdk.java.net/browse/JDK-6553134 (Java bug which Matlab implements ;))
* https://github.com/dancor/wmctrl/blob/master/main.c
* https://chromium.googlesource.com/chromium/src/+/5e16d46c92747ae76914b9a5db114a19cdb00bde/ui/views/widget/desktop_aura/desktop_window_tree_host_x11.cc
* https://mail.gnome.org/archives/wm-spec-list/2009-February/msg00006.html (why to only use key/button press)
* http://www.sbin.org/doc/Xlib/chapt_16.html
* http://www.semicomplete.com/blog/geekery/xsendevent-xdotool-and-ld_preload.html ( mock this method to get event time stamps )
* https://github.com/deepfire/ld-preload-xcreatewindow-net-wm-pid/blob/master/ld-preload-xcreatewindow.c (add PID to splash)
* https://codereview.chromium.org/317783012/patch/50001/60001 (Fixes for chromes focus behavior)
* https://mail.gnome.org/archives/desktop-devel-list/2004-August/msg00159.html (Good discussion how it is suppose to work)
* http://www.chiark.greenend.org.uk/~sgtatham/xtruss/ (To trace the X11 chit chat ;) )

