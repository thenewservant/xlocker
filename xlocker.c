/*
 * xlocker.c
 *
 * minimal X11 screen locker
 *
 * Copyright (C)1993,1994 Ian Jackson
 * Copyright (C)2024 thenewservant
 *
 * This is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3, or (at your option)
 * any later version.
 *
 * This is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <X11/Xos.h>

#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <pwd.h>
#include <grp.h>
#include <limits.h>
#include <string.h>
#include <crypt.h>
#include <unistd.h>
#include <math.h>
#include <ctype.h>
#include <values.h>

#include <shadow.h>
#include "patchlevel.h"

Display *display;
Window window, root;

struct passwd *pw;
int passwordok(const char *s)
{
  return !strcmp(crypt(s, pw->pw_passwd), pw->pw_passwd);
}

int main(int argc, char **argv)
{
  (void)argc;
  (void)argv;
  XEvent ev;
  KeySym ks;
  char cbuf[10], rbuf[128]; /* shadow appears to suggest 127 a good value here */
  int clen, rlen = 0;
  XSetWindowAttributes attrib;
  XColor csr_fg, csr_bg, dummy;
  int ret;
  struct spwd *sp;
  struct timeval tv;
  int tvt, gs;

  if (getenv("WAYLAND_DISPLAY"))
    fprintf(stderr, "WARNING: Wayland X server detected: xlocker"
                    " cannot intercept all user input.\n");

  errno = 0;
  pw = getpwuid(getuid());
  if (!pw)
  {
    perror("password entry for uid not found");
    exit(1);
  }
  sp = getspnam(pw->pw_name);
  if (sp == NULL)
  {
    perror("getspnam error");
    return 1;
  }
  pw->pw_passwd = sp->sp_pwdp;
  endspent();

  /* logically, if we need to do the following then the same
     applies to being installed setgid shadow.
     we do this first, because of a bug in linux. --jdamery */
  if (setgid(getgid()))
  {
    perror("setgid");
    exit(1);
  }
  /* we can be installed setuid root to support shadow passwords,
     and we don't need root privileges any longer.  --marekm */
  if (setuid(getuid()))
  {
    perror("setuid");
    exit(1);
  }

  if (strlen(pw->pw_passwd) < 13)
  {
    printf("ERROR: password has no pwd");
    ;
    exit(1);
  }

  display = XOpenDisplay(0);

  if (display == NULL)
  {
    fprintf(stderr, "xlocker (version %s): cannot open display\n",
            program_version);
    exit(1);
  }

  attrib.override_redirect = True;

  window = XCreateWindow(display, DefaultRootWindow(display),
                         0, 0, 1, 1, 0, CopyFromParent, InputOnly, CopyFromParent,
                         CWOverrideRedirect, &attrib);

  XSelectInput(display, window, KeyPressMask | KeyReleaseMask);
  ret = XAllocNamedColor(display,
                         DefaultColormap(display, DefaultScreen(display)),
                         "steelblue3",
                         &dummy, &csr_bg);
  if (ret == 0)
    XAllocNamedColor(display,
                     DefaultColormap(display, DefaultScreen(display)),
                     "black",
                     &dummy, &csr_bg);

  ret = XAllocNamedColor(display,
                         DefaultColormap(display, DefaultScreen(display)),
                         "grey25",
                         &dummy, &csr_fg);
  if (ret == 0)
    XAllocNamedColor(display,
                     DefaultColormap(display, DefaultScreen(display)),
                     "white",
                     &dummy, &csr_bg);

  XMapWindow(display, window);

  /*Sometimes the WM doesn't ungrab the keyboard quickly enough if
   *launching xlocker from a keystroke shortcut, meaning xlocker fails
   *to start We deal with this by waiting (up to 100 times) for 10,000
   *microsecs and trying to grab each time. If we still fail
   *(i.e. after 1s in total), then give up, and emit an error
   */

  gs = 0; /*gs==grab successful*/
  for (tvt = 0; tvt < 100; tvt++)
  {
    ret = XGrabKeyboard(display, window, False, GrabModeAsync, GrabModeAsync,
                        CurrentTime);
    if (ret == GrabSuccess)
    {
      gs = 1;
      break;
    }
    /*grab failed; wait .01s*/
    tv.tv_sec = 0;
    tv.tv_usec = 10000;
    select(1, NULL, NULL, NULL, &tv);
  }
  if (gs == 0)
  {
    fprintf(stderr, "xlocker (version %s): cannot grab keyboard\n",
            program_version);
    exit(1);
  }

  if (XGrabPointer(display, window, False, (KeyPressMask | KeyReleaseMask) & 0,
                   GrabModeAsync, GrabModeAsync, None,
                   (Cursor)NULL, CurrentTime) != GrabSuccess)
  {
    XUngrabKeyboard(display, CurrentTime);
    fprintf(stderr, "xlocker (version %s): cannot grab pointer\n",
            program_version);
    exit(1);
  }

  while (1)
  {
    XNextEvent(display, &ev);
    switch (ev.type)
    {
    case KeyPress:
      clen = XLookupString(&ev.xkey, cbuf, 9, &ks, 0);
      switch (ks)
      {
      case XK_Escape:
      case XK_Clear:
        rlen = 0;
        break;
      case XK_Delete:
      case XK_BackSpace:
        if (rlen > 0)
        {
          rlen--;
        }
        break;
      case XK_Linefeed:
      case XK_Return:
      case XK_KP_Enter:
        if (rlen == 0)
          break;
        rbuf[rlen] = 0;
        if (passwordok(rbuf))
          exit(0);
        XBell(display, 0);
        rlen = 0;
        break;
      default:
        if (clen != 1)
          break;
        /* allow space for the trailing \0 */
        if ((long unsigned int)rlen < (sizeof(rbuf) - 1))
        {
          rbuf[rlen] = cbuf[0];
          rlen++;
        }
        break;
      }
      break;
    default:
      break;
    }
  }
}
