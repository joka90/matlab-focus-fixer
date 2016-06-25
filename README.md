# Matlab focus fixer

Prevents matlab from stealing focus. Tries to set the _NET_WM_USER_TIME to a proper value to get the correct window behavior.

The focus is only given to windows which have been started less then 2 sec (2000 ms) since last user interaction in Matlab.

# Build

```make```

# Start

```LD_PRELOAD=$PWD/focus_fixer.so matlab```

# TODO

* Make new windows minimized if main window is out of focus.
* Make new windows show up in parent windows workspace.
