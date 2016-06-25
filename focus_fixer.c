#define _GNU_SOURCE
#include <dlfcn.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/X.h>
#include <X11/Xatom.h>

#include <stdio.h>
#include <string.h>

static Time last_access;
static Window last_focus;

void GetCurrWindow(Display * d, Window * win);
void TimeFromXEvent(XEvent xev, Time * t);


int
XNextEvent(Display * display, XEvent * event)
{
  Time t;
  TimeFromXEvent(*event, &t);

  return ((int (*)(Display *, XEvent *))
          dlsym(RTLD_NEXT, "XNextEvent"))(display, event);
}

Window
XCreateWindow(Display * display,
              Window parent,
              int x,
              int y,
              unsigned int width,
              unsigned int height,
              unsigned int border_width,
              int depth,
              unsigned int class,
              Visual * visual,
              unsigned long valuemask, XSetWindowAttributes * attributes)
{

  /* multihead screen handling loop */
  for(int i = 0; i < ScreenCount(display); i++) {
    /* if the window is created as a toplevel window */
    if(parent == RootWindow(display, i)) {
      Window new_win =
        ((Window(*)
          (Display *, Window, int, int, unsigned int, unsigned int,
           unsigned int, int, unsigned int, Visual *, unsigned long,
           XSetWindowAttributes *)) dlsym(RTLD_NEXT,
                                          "XCreateWindow"))(display,
                                              parent, x,
                                              y, width,
                                              height,
                                              border_width,
                                              depth,
                                              class,
                                              visual,
                                              valuemask,
                                              attributes);
      /* set properties */

      // If SHOW_STATE_INACTIVE, tell the window manager not to focus the window
      // when mapping. This is done by setting the _NET_WM_USER_TIME to 0. See e.g.
      // http://standards.freedesktop.org/wm-spec/latest/ar01s05.html
      // last_access defaults to 0
      XChangeProperty(display, new_win,
                      XInternAtom(display, "_NET_WM_USER_TIME", False),
                      XA_CARDINAL, 32, PropModeReplace,
                      (unsigned char *) &last_access, 1);
      fprintf(stderr,
              "XCreateWindow - window 0x%08lx - _NET_WM_USER_TIME: %u \n",
              new_win, last_access);
      return new_win;
    }
  }



  return ((Window(*)
           (Display *, Window, int, int, unsigned int, unsigned int,
            unsigned int, int, unsigned int, Visual *, unsigned long,
            XSetWindowAttributes *)) dlsym(RTLD_NEXT,
                "XCreateWindow"))(display,
                                  parent, x, y,
                                  width, height,
                                  border_width,
                                  depth, class,
                                  visual,
                                  valuemask,
                                  attributes);

}

int
XConfigureWindow(Display * display, Window w, unsigned int value_mask,
                 XWindowChanges * values)
{
  values->stack_mode = Below;
  return ((int (*)(Display *, Window, unsigned int, XWindowChanges *))
          dlsym(RTLD_NEXT, "XConfigureWindow"))(display, w, value_mask,
              values);
}

int
XSetInputFocus(Display * display, Window focus, int revert_to, Time time)
{
  /*
   * We can't prevent XSetInputFocus use entirely because
   * applications can negotiate with the window manager to use
   * the WM_TAKE_FOCUS protocol and take over focus handling.
   * However, all clients participating in this protocol should
   * be specifying a legitimate "time" (presumably to prevent
   * races). Client specifying CurrentTime, however, are doing
   * something sneaky and should be stopped.
   *
   * See the "Input Focus" section of the ICCCM.
   */

  int x, y;
  unsigned width, height, border_width, depth;
  Window root;
  XGetGeometry(display, focus, &root,
               &x, &y, &width, &height, &border_width, &depth);

  XClassHint *cls = XAllocClassHint();
  XGetClassHint(display, focus, cls);


  fprintf(stderr,
          "XSetInputFocus - window 0x%08lx - name: %s, class: %s, "
          "width: %4u, height: %4u, time: %u\n",
          (unsigned long) focus,
          cls->res_name ? cls->res_name : "<NULL>",
          cls->res_class ? cls->res_class : "<NULL>",
          width, height, (unsigned long) time);

  Window acctive_win;
  GetCurrWindow(display, &acctive_win);
  XGetGeometry(display, focus, &root,
               &x, &y, &width, &height, &border_width, &depth);

  XClassHint *cls2 = XAllocClassHint();
  XGetClassHint(display, acctive_win, cls2);


  fprintf(stderr,
          "acctive_win - window 0x%08lx - name: %s, class: %s, "
          "width: %4u, height: %4u, time: %u\n",
          (unsigned long) acctive_win,
          cls2->res_name ? cls2->res_name : "<NULL>",
          cls2->res_class ? cls2->res_class : "<NULL>",
          width, height, (unsigned long) time);
  XFree(cls2);


  if(time < last_access) {
    fprintf(stderr,
            "older then last acces - time: %u lastaccess: %u \n",
            (unsigned long) time, (unsigned long) last_access);
  } else {
    fprintf(stderr,
            "newer then last acces - time: %u lastaccess: %u diff %u\n",
            (unsigned long) time, (unsigned long) last_access,
            (unsigned long)(time - last_access));
  }

  if((time == CurrentTime || time - last_access > 2000) &&
      cls->res_name && cls->res_class &&
      !strcmp(cls->res_name, "Focus-Proxy-Window") &&
      !strcmp(cls->res_class, "FocusProxy")) {
    XFree(cls);
    fprintf(stderr, "BLOCKING time: %u , lastaccess: %u , diff: %u \n",
            time, last_access, time - last_access);
    return 0;
  }
  // Update last window with focus
  last_focus = focus;
  return ((int (*)(Display *, Window, int, Time))
          dlsym(RTLD_NEXT, "XSetInputFocus"))(display, focus, revert_to,
              time);
}

void
GetCurrWindow(Display * d, Window * win)
{
  Window foo;
  int bar;

  do {
    (void) XQueryPointer(d, DefaultRootWindow(d), &foo, win,
                         &bar, &bar, &bar, &bar, &bar);
  } while(win <= 0);


#ifdef VROOT
  {
    int n;
    Window *wins;
    XWindowAttributes xwa;

    (void) fputs("=xwa=", stdout);

    /* do{  */
    XQueryTree(d, win, &foo, &foo, &wins, &n);
    /* } while(wins <= 0); */
    bar = 0;
    while(--n >= 0) {
      XGetWindowAttributes(d, wins[n], &xwa);
      if((xwa.width * xwa.height) > bar) {
        *win = wins[n];
        bar = xwa.width * xwa.height;
      }
      n--;
    }
    XFree(wins);
  }
#endif
}


void
TimeFromXEvent(XEvent xev, Time * t)
{
  switch(xev.type) {
  case KeyPress:
    //case KeyRelease:
  {
    *t = xev.xkey.time;
    last_access = xev.xkey.time;
    fprintf(stderr,
            "TimeFromXEvent - KeyPress time: %u\n",
            (unsigned long) *t);

  }
  break;
  case ButtonPress:
    //case ButtonRelease:
  {
    *t = xev.xbutton.time;
    last_access = xev.xbutton.time;
    fprintf(stderr,
            "TimeFromXEvent - ButtonPress time: %u\n",
            (unsigned long) *t);

  }
  break;
//     case MotionNotify:
//      *t=xev.xmotion.time;
//       break;
//     case EnterNotify:
//     case LeaveNotify:
//       *t=xev.xcrossing.time;
//       break;
  }
}


