#define _GNU_SOURCE
#include <dlfcn.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/X.h>
#include <X11/Xatom.h>

#include <stdio.h>
#include <string.h>


#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <locale.h>
#include <glib.h>

static Time last_access;
static Window last_focus;

void GetCurrWindow(Display * d, Window * win);
void TimeFromXEvent(XEvent xev, Time * t);

// Start from wmctrl
static gchar *get_property (Display *disp, Window win, 
        Atom xa_prop_type, gchar *prop_name, unsigned long *size);
static Window get_active_window(Display *dpy);
static int window_state (Display *disp, Window win, char *arg);
static int client_msg(Display *disp, Window win, char *msg, 
        unsigned long data0, unsigned long data1, 
        unsigned long data2, unsigned long data3,
        unsigned long data4);

#define p_verbose(...) if (options.verbose) { \
    fprintf(stderr, __VA_ARGS__); \
}


#define MAX_PROPERTY_VALUE_LEN 4096
#define SELECT_WINDOW_MAGIC ":SELECT:"
#define ACTIVE_WINDOW_MAGIC ":ACTIVE:"

#define _NET_WM_STATE_REMOVE        0    /* remove/unset property */
#define _NET_WM_STATE_ADD           1    /* add/set property */
#define _NET_WM_STATE_TOGGLE        2    /* toggle property  */

//TODO hard coded options
static struct {
    int verbose;
    int force_utf8;
    int show_class;
    int show_pid;
    int show_geometry;
    int match_by_id;
	int match_by_cls;
    int full_window_title_match;
    int wa_desktop_titles_invalid_utf8;
    char *param_window;
    char *param;
} options;


// END from wmctrl

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


//Snippet from wmctrl
static gchar *get_property (Display *disp, Window win, /*{{{*/
        Atom xa_prop_type, gchar *prop_name, unsigned long *size) {
    Atom xa_prop_name;
    Atom xa_ret_type;
    int ret_format;
    unsigned long ret_nitems;
    unsigned long ret_bytes_after;
    unsigned long tmp_size;
    unsigned char *ret_prop;
    gchar *ret;
    
    xa_prop_name = XInternAtom(disp, prop_name, False);
    
    /* MAX_PROPERTY_VALUE_LEN / 4 explanation (XGetWindowProperty manpage):
     *
     * long_length = Specifies the length in 32-bit multiples of the
     *               data to be retrieved.
     */
    if (XGetWindowProperty(disp, win, xa_prop_name, 0, MAX_PROPERTY_VALUE_LEN / 4, False,
            xa_prop_type, &xa_ret_type, &ret_format,     
            &ret_nitems, &ret_bytes_after, &ret_prop) != Success) {
        p_verbose("Cannot get %s property.\n", prop_name);
        return NULL;
    }
  
    if (xa_ret_type != xa_prop_type) {
        p_verbose("Invalid type of %s property.\n", prop_name);
        XFree(ret_prop);
        return NULL;
    }

    /* null terminate the result to make string handling easier */
    tmp_size = (ret_format / (32 / sizeof(long))) * ret_nitems;
    ret = g_malloc(tmp_size + 1);
    memcpy(ret, ret_prop, tmp_size);
    ret[tmp_size] = '\0';

    if (size) {
        *size = tmp_size;
    }
    
    XFree(ret_prop);
    return ret;
}/*}}}*/

//Snippet from wmctrl
static Window get_active_window(Display *disp) {/*{{{*/
    char *prop;
    unsigned long size;
    Window ret = (Window)0;
    
    prop = get_property(disp, DefaultRootWindow(disp), XA_WINDOW, 
                        "_NET_ACTIVE_WINDOW", &size);
    if (prop) {
        ret = *((Window*)prop);
        g_free(prop);
    }

    return(ret);
}/*}}}*/

//Snippet from wmctrl
static int window_state (Display *disp, Window win, char *arg) {/*{{{*/
    unsigned long action;
    Atom prop1 = 0;
    Atom prop2 = 0;
    char *p1, *p2;
    const char *argerr = "The -b option expects a list of comma separated parameters: \"(remove|add|toggle),<PROP1>[,<PROP2>]\"\n";

    if (!arg || strlen(arg) == 0) {
        fputs(argerr, stderr);
        return EXIT_FAILURE;
    }

    if ((p1 = strchr(arg, ','))) {
        gchar *tmp_prop1, *tmp1;
        
        *p1 = '\0';

        /* action */
        if (strcmp(arg, "remove") == 0) {
            action = _NET_WM_STATE_REMOVE;
        }
        else if (strcmp(arg, "add") == 0) {
            action = _NET_WM_STATE_ADD;
        }
        else if (strcmp(arg, "toggle") == 0) {
            action = _NET_WM_STATE_TOGGLE;
        }
        else {
            fputs("Invalid action. Use either remove, add or toggle.\n", stderr);
            return EXIT_FAILURE;
        }
        p1++;

        /* the second property */
        if ((p2 = strchr(p1, ','))) {
            gchar *tmp_prop2, *tmp2;
            *p2 = '\0';
            p2++;
            if (strlen(p2) == 0) {
                fputs("Invalid zero length property.\n", stderr);
                return EXIT_FAILURE;
            }
            tmp_prop2 = g_strdup_printf("_NET_WM_STATE_%s", tmp2 = g_ascii_strup(p2, -1));
            p_verbose("State 2: %s\n", tmp_prop2); 
            prop2 = XInternAtom(disp, tmp_prop2, False);
            g_free(tmp2);
            g_free(tmp_prop2);
        }

        /* the first property */
        if (strlen(p1) == 0) {
            fputs("Invalid zero length property.\n", stderr);
            return EXIT_FAILURE;
        }
        tmp_prop1 = g_strdup_printf("_NET_WM_STATE_%s", tmp1 = g_ascii_strup(p1, -1));
        p_verbose("State 1: %s\n", tmp_prop1); 
        prop1 = XInternAtom(disp, tmp_prop1, False);
        g_free(tmp1);
        g_free(tmp_prop1);

        
        return client_msg(disp, win, "_NET_WM_STATE", 
            action, (unsigned long)prop1, (unsigned long)prop2, 0, 0);
    }
    else {
        fputs(argerr, stderr);
        return EXIT_FAILURE;
    }
}/*}}}*/

// Snippet from wmctrl
static int client_msg(Display *disp, Window win, char *msg, /* {{{ */
    unsigned long data0, unsigned long data1, 
    unsigned long data2, unsigned long data3,
    unsigned long data4) {
  XEvent event;
  long mask = SubstructureRedirectMask | SubstructureNotifyMask;

  event.xclient.type = ClientMessage;
  event.xclient.serial = 0;
  event.xclient.send_event = True;
  event.xclient.message_type = XInternAtom(disp, msg, False);
  event.xclient.window = win;
  event.xclient.format = 32;
  event.xclient.data.l[0] = data0;
  event.xclient.data.l[1] = data1;
  event.xclient.data.l[2] = data2;
  event.xclient.data.l[3] = data3;
  event.xclient.data.l[4] = data4;

  if (XSendEvent(disp, DefaultRootWindow(disp), False, mask, &event)) {
    return EXIT_SUCCESS;
  }
  else {
    fprintf(stderr, "Cannot send %s event.\n", msg);
    return EXIT_FAILURE;
  }
}/*}}}*/

