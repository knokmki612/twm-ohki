/*****************************************************************************/
/*

Copyright 1989, 1998  The Open Group

Permission to use, copy, modify, distribute, and sell this software and its
documentation for any purpose is hereby granted without fee, provided that
the above copyright notice appear in all copies and that both that
copyright notice and this permission notice appear in supporting
documentation.

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
OPEN GROUP BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN
AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the name of The Open Group shall not be
used in advertising or otherwise to promote the sale, use or other dealings
in this Software without prior written authorization from The Open Group.

*/
/**       Copyright 1988 by Evans & Sutherland Computer Corporation,        **/
/**                          Salt Lake City, Utah                           **/
/**                        Cambridge, Massachusetts                         **/
/**                                                                         **/
/**                           All Rights Reserved                           **/
/**                                                                         **/
/**    Permission to use, copy, modify, and distribute this software and    **/
/**    its documentation  for  any  purpose  and  without  fee is hereby    **/
/**    granted, provided that the above copyright notice appear  in  all    **/
/**    copies and that both  that  copyright  notice  and  this  permis-    **/
/**    sion  notice appear in supporting  documentation,  and  that  the    **/
/**    name of Evans & Sutherland not be used in advertising    **/
/**    in publicity pertaining to distribution of the  software  without    **/
/**    specific, written prior permission.                                  **/
/**                                                                         **/
/**    EVANS & SUTHERLAND DISCLAIMs ALL WARRANTIES WITH REGARD    **/
/**    TO THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES  OF  MERCHANT-    **/
/**    ABILITY  AND  FITNESS,  IN  NO  EVENT SHALL EVANS & SUTHERLAND    **/
/**    BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL  DAM-    **/
/**    AGES OR  ANY DAMAGES WHATSOEVER  RESULTING FROM LOSS OF USE, DATA    **/
/**    OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER    **/
/**    TORTIOUS ACTION, ARISING OUT OF OR IN  CONNECTION  WITH  THE  USE    **/
/**    OR PERFORMANCE OF THIS SOFTWARE.                                     **/
/*****************************************************************************/


/***********************************************************************
 *
 * window resizing borrowed from the "wm" window manager
 *
 * 11-Dec-87 Thomas E. LaStrange                File created
 *
 ***********************************************************************/

#include <stdio.h>
#include "twm.h"
#include "parse.h"
#include "util.h"
#include "resize.h"
#include "iconmgr.h"
#include "add_window.h"
#include "screen.h"
#include "events.h"

static void DisplaySize ( TwmWindow *tmp_win, int width, int height );

#define MINHEIGHT 0     /* had been 32 */
#define MINWIDTH 0      /* had been 60 */

static int dragx;       /* all these variables are used */
static int dragy;       /* in resize operations */
static int dragWidth;
static int dragHeight;

static int origx;
static int origy;
static int origWidth;
static int origHeight;

static int clampTop;
static int clampBottom;
static int clampLeft;
static int clampRight;
static int clampDX;
static int clampDY;

static int last_width;
static int last_height;


static void
do_auto_clamp (TwmWindow *tmp_win, XEvent *evp)
{
    Window junkRoot;
    int x, y, h, v, junkbw;
    unsigned int junkMask;

    switch (evp->type) {
      case ButtonPress:
	x = evp->xbutton.x_root;
	y = evp->xbutton.y_root;
	break;
      case KeyPress:
	x = evp->xkey.x_root;
	y = evp->xkey.y_root;
	break;
      default:
	if (!XQueryPointer (dpy, Scr->Root, &junkRoot, &junkRoot,
			    &x, &y, &junkbw, &junkbw, &junkMask))
	  return;
    }

    h = ((x - dragx) / (dragWidth < 3 ? 1 : (dragWidth / 3)));
    v = ((y - dragy - tmp_win->title_height) /
	 (dragHeight < 3 ? 1 : (dragHeight / 3)));

    if (h <= 0) {
	clampLeft = 1;
	clampDX = (x - dragx);
    } else if (h >= 2) {
	clampRight = 1;
	clampDX = (x - dragx - dragWidth);
    }

    if (v <= 0) {
	clampTop = 1;
	clampDY = (y - dragy);
    } else if (v >= 2) {
	clampBottom = 1;
	clampDY = (y - dragy - dragHeight);
    }
}


/**
 * begin a window resize operation
 *  \param ev           the event structure (button press)
 *  \param tmp_win      the TwmWindow pointer
 *  \param fromtitlebar action invoked from titlebar button
 */
void
StartResize(XEvent *evp, TwmWindow *tmp_win, Bool fromtitlebar)
{
    Window      junkRoot;
    unsigned int junkbw, junkDepth;

    ResizeWindow = tmp_win->frame;
    XGrabServer(dpy);
    XGrabPointer(dpy, Scr->Root, True,
        ButtonPressMask | ButtonReleaseMask |
	ButtonMotionMask | PointerMotionHintMask,
        GrabModeAsync, GrabModeAsync,
        Scr->Root, Scr->ResizeCursor, CurrentTime);

    XGetGeometry(dpy, (Drawable) tmp_win->frame, &junkRoot,
        &dragx, &dragy, (unsigned int *)&dragWidth, (unsigned int *)&dragHeight, &junkbw,
                 &junkDepth);
    dragx += tmp_win->frame_bw;
    dragy += tmp_win->frame_bw;
    origx = dragx;
    origy = dragy;
    origWidth = dragWidth;
    origHeight = dragHeight;
    clampTop = clampBottom = clampLeft = clampRight = clampDX = clampDY = 0;

    if (Scr->AutoRelativeResize && !fromtitlebar)
      do_auto_clamp (tmp_win, evp);

    Scr->SizeStringOffset = SIZE_HINDENT;
    XResizeWindow (dpy, Scr->SizeWindow,
		   Scr->SizeStringWidth + SIZE_HINDENT * 2,
		   Scr->SizeFont.height + SIZE_VINDENT * 2);
    XMapRaised(dpy, Scr->SizeWindow);
    InstallRootColormap();
    last_width = 0;
    last_height = 0;
    DisplaySize(tmp_win, origWidth, origHeight);
    MoveOutline (Scr->Root, dragx - tmp_win->frame_bw,
		 dragy - tmp_win->frame_bw, dragWidth + 2 * tmp_win->frame_bw,
		 dragHeight + 2 * tmp_win->frame_bw,
		 tmp_win->frame_bw, tmp_win->title_height, tmp_win->title_pos);
}



void
MenuStartResize(TwmWindow *tmp_win, int x, int y, int w, int h)
{
    XGrabServer(dpy);
    XGrabPointer(dpy, Scr->Root, True,
        ButtonPressMask | ButtonMotionMask | PointerMotionMask,
        GrabModeAsync, GrabModeAsync,
        Scr->Root, Scr->ResizeCursor, CurrentTime);
    dragx = x + tmp_win->frame_bw;
    dragy = y + tmp_win->frame_bw;
    origx = dragx;
    origy = dragy;
    dragWidth = origWidth = w; /* - 2 * tmp_win->frame_bw; */
    dragHeight = origHeight = h; /* - 2 * tmp_win->frame_bw; */
    clampTop = clampBottom = clampLeft = clampRight = clampDX = clampDY = 0;
    last_width = 0;
    last_height = 0;
    Scr->SizeStringOffset = SIZE_HINDENT;
    XResizeWindow (dpy, Scr->SizeWindow,
		   Scr->SizeStringWidth + SIZE_HINDENT * 2,
		   Scr->SizeFont.height + SIZE_VINDENT * 2);
    XMapRaised(dpy, Scr->SizeWindow);
    DisplaySize(tmp_win, origWidth, origHeight);
    MoveOutline (Scr->Root, dragx - tmp_win->frame_bw,
		 dragy - tmp_win->frame_bw,
		 dragWidth + 2 * tmp_win->frame_bw,
		 dragHeight + 2 * tmp_win->frame_bw,
		 tmp_win->frame_bw, tmp_win->title_height, tmp_win->title_pos);
}

/**
 * begin a windorew resize operation from AddWindow
 *  \param tmp_win the TwmWindow pointer
 */

void
AddStartResize(TwmWindow *tmp_win, int x, int y, int w, int h)
{
    XGrabServer(dpy);
    XGrabPointer(dpy, Scr->Root, True,
        ButtonReleaseMask | ButtonMotionMask | PointerMotionHintMask,
        GrabModeAsync, GrabModeAsync,
        Scr->Root, Scr->ResizeCursor, CurrentTime);

    dragx = x + tmp_win->frame_bw;
    dragy = y + tmp_win->frame_bw;
    origx = dragx;
    origy = dragy;
    dragWidth = origWidth = w - 2 * tmp_win->frame_bw;
    dragHeight = origHeight = h - 2 * tmp_win->frame_bw;
    clampTop = clampBottom = clampLeft = clampRight = clampDX = clampDY = 0;
/*****
    if (Scr->AutoRelativeResize) {
	clampRight = clampBottom = 1;
    }
*****/
    last_width = 0;
    last_height = 0;
    DisplaySize(tmp_win, origWidth, origHeight);
}



void
MenuDoResize(int x_root, int y_root, TwmWindow *tmp_win)
{
    int action;

    action = 0;

    x_root -= clampDX;
    y_root -= clampDY;

    if (clampTop) {
        int         delta = y_root - dragy;
        if (dragHeight - delta < MINHEIGHT) {
            delta = dragHeight - MINHEIGHT;
            clampTop = 0;
        }
        dragy += delta;
        dragHeight -= delta;
        action = 1;
    }
    else if (y_root <= dragy/* ||
             y_root == findRootInfo(root)->rooty*/) {
        dragy = y_root;
        dragHeight = origy + origHeight -
            y_root;
        clampBottom = 0;
        clampTop = 1;
	clampDY = 0;
        action = 1;
    }
    if (clampLeft) {
        int         delta = x_root - dragx;
        if (dragWidth - delta < MINWIDTH) {
            delta = dragWidth - MINWIDTH;
            clampLeft = 0;
        }
        dragx += delta;
        dragWidth -= delta;
        action = 1;
    }
    else if (x_root <= dragx/* ||
             x_root == findRootInfo(root)->rootx*/) {
        dragx = x_root;
        dragWidth = origx + origWidth -
            x_root;
        clampRight = 0;
        clampLeft = 1;
	clampDX = 0;
        action = 1;
    }
    if (clampBottom) {
        int         delta = y_root - dragy - dragHeight;
        if (dragHeight + delta < MINHEIGHT) {
            delta = MINHEIGHT - dragHeight;
            clampBottom = 0;
        }
        dragHeight += delta;
        action = 1;
    }
    else if (y_root >= dragy + dragHeight) {
        dragy = origy;
        dragHeight = 1 + y_root - dragy;
        clampTop = 0;
        clampBottom = 1;
	clampDY = 0;
        action = 1;
    }
    if (clampRight) {
        int         delta = x_root - dragx - dragWidth;
        if (dragWidth + delta < MINWIDTH) {
            delta = MINWIDTH - dragWidth;
            clampRight = 0;
        }
        dragWidth += delta;
        action = 1;
    }
    else if (x_root >= dragx + dragWidth) {
        dragx = origx;
        dragWidth = 1 + x_root - origx;
        clampLeft = 0;
        clampRight = 1;
	clampDX = 0;
        action = 1;
    }

    if (action) {
        ConstrainSize (tmp_win, &dragWidth, &dragHeight);
        if (clampLeft)
            dragx = origx + origWidth - dragWidth;
        if (clampTop)
            dragy = origy + origHeight - dragHeight;
        MoveOutline(Scr->Root,
            dragx - tmp_win->frame_bw,
            dragy - tmp_win->frame_bw,
            dragWidth + 2 * tmp_win->frame_bw,
            dragHeight + 2 * tmp_win->frame_bw,
	    tmp_win->frame_bw, tmp_win->title_height, tmp_win->title_pos);
    }

    DisplaySize(tmp_win, dragWidth, dragHeight);
}


/**
 * move the rubberband around.  This is called for each motion event when
 * we are resizing
 *
 *  \param x_root  the X corrdinate in the root window
 *  \param y_root  the Y corrdinate in the root window
 *  \param tmp_win the current twm window
 */
void
DoResize(int x_root, int y_root, TwmWindow *tmp_win)
{
    int action;

    action = 0;

    x_root -= clampDX;
    y_root -= clampDY;

    if (clampTop) {
        int         delta = y_root - dragy;
        if (dragHeight - delta < MINHEIGHT) {
            delta = dragHeight - MINHEIGHT;
            clampTop = 0;
        }
        dragy += delta;
        dragHeight -= delta;
        action = 1;
    }
    else if (y_root <= dragy/* ||
             y_root == findRootInfo(root)->rooty*/) {
        dragy = y_root;
        dragHeight = origy + origHeight -
            y_root;
        clampBottom = 0;
        clampTop = 1;
	clampDY = 0;
        action = 1;
    }
    if (clampLeft) {
        int         delta = x_root - dragx;
        if (dragWidth - delta < MINWIDTH) {
            delta = dragWidth - MINWIDTH;
            clampLeft = 0;
        }
        dragx += delta;
        dragWidth -= delta;
        action = 1;
    }
    else if (x_root <= dragx/* ||
             x_root == findRootInfo(root)->rootx*/) {
        dragx = x_root;
        dragWidth = origx + origWidth -
            x_root;
        clampRight = 0;
        clampLeft = 1;
	clampDX = 0;
        action = 1;
    }
    if (clampBottom) {
        int         delta = y_root - dragy - dragHeight;
        if (dragHeight + delta < MINHEIGHT) {
            delta = MINHEIGHT - dragHeight;
            clampBottom = 0;
        }
        dragHeight += delta;
        action = 1;
    }
    else if (y_root >= dragy + dragHeight - 1/* ||
           y_root == findRootInfo(root)->rooty
           + findRootInfo(root)->rootheight - 1*/) {
        dragy = origy;
        dragHeight = 1 + y_root - dragy;
        clampTop = 0;
        clampBottom = 1;
	clampDY = 0;
        action = 1;
    }
    if (clampRight) {
        int         delta = x_root - dragx - dragWidth;
        if (dragWidth + delta < MINWIDTH) {
            delta = MINWIDTH - dragWidth;
            clampRight = 0;
        }
        dragWidth += delta;
        action = 1;
    }
    else if (x_root >= dragx + dragWidth - 1/* ||
             x_root == findRootInfo(root)->rootx +
             findRootInfo(root)->rootwidth - 1*/) {
        dragx = origx;
        dragWidth = 1 + x_root - origx;
        clampLeft = 0;
        clampRight = 1;
	clampDX = 0;
        action = 1;
    }

    if (action) {
        ConstrainSize (tmp_win, &dragWidth, &dragHeight);
        if (clampLeft)
            dragx = origx + origWidth - dragWidth;
        if (clampTop)
            dragy = origy + origHeight - dragHeight;
        MoveOutline(Scr->Root,
            dragx - tmp_win->frame_bw,
            dragy - tmp_win->frame_bw,
            dragWidth + 2 * tmp_win->frame_bw,
            dragHeight + 2 * tmp_win->frame_bw,
	    tmp_win->frame_bw, tmp_win->title_height, tmp_win->title_pos);
    }

    DisplaySize(tmp_win, dragWidth, dragHeight);
}

/**
 * display the size in the dimensions window.
 *
 *  \param tmp_win the current twm window
 *  \param width   the width of the rubber band
 *  \param height  the height of the rubber band
 */
static void
DisplaySize(TwmWindow *tmp_win, int width, int height)
{
    char str[100];
    int dwidth;
    int dheight;

    if (last_width == width && last_height == height)
        return;

    last_width = width;
    last_height = height;

  if (tmp_win->title_pos == TP_TOP || tmp_win->title_pos == TP_BOTTOM) {
    dheight = height - tmp_win->title_height;
    dwidth = width;
  } else {
    dheight = height;
    dwidth = width - tmp_win->title_height;
  }

    /*
     * ICCCM says that PMinSize is the default is no PBaseSize is given,
     * and vice-versa.
     */
    if (tmp_win->hints.flags&(PMinSize|PBaseSize) && tmp_win->hints.flags & PResizeInc)
    {
	if (tmp_win->hints.flags & PBaseSize) {
	    dwidth -= tmp_win->hints.base_width;
	    dheight -= tmp_win->hints.base_height;
	} else {
	    dwidth -= tmp_win->hints.min_width;
	    dheight -= tmp_win->hints.min_height;
	}
    }

    if (tmp_win->hints.flags & PResizeInc)
    {
        dwidth /= tmp_win->hints.width_inc;
        dheight /= tmp_win->hints.height_inc;
    }

    (void) snprintf (str, sizeof(str), " %4d x %-4d ", dwidth, dheight);
    XRaiseWindow(dpy, Scr->SizeWindow);
    MyFont_ChangeGC(Scr->DefaultC.fore, Scr->DefaultC.back, &Scr->SizeFont);
    MyFont_DrawImageString (dpy, Scr->SizeWindow, &Scr->SizeFont,
			    Scr->NormalGC, Scr->SizeStringOffset,
			    Scr->SizeFont.ascent + SIZE_VINDENT,
			    str, 13);
}

/**
 * finish the resize operation
 */
void
EndResize(void)
{
    TwmWindow *tmp_win;

#ifdef DEBUG
    fprintf(stderr, "EndResize\n");
#endif

    MoveOutline(Scr->Root, 0, 0, 0, 0, 0, 0, 0);
    XUnmapWindow(dpy, Scr->SizeWindow);

    XFindContext(dpy, ResizeWindow, TwmContext, (caddr_t *)&tmp_win);

    ConstrainSize (tmp_win, &dragWidth, &dragHeight);

    if (dragWidth != tmp_win->frame_width ||
        dragHeight != tmp_win->frame_height)
            tmp_win->zoomed = ZOOM_NONE;

    SetupWindow (tmp_win, dragx - tmp_win->frame_bw, dragy - tmp_win->frame_bw,
		 dragWidth, dragHeight, -1);

    if (tmp_win->iconmgr)
    {
	int ncols = tmp_win->iconmgrp->cur_columns;
	if (ncols == 0) ncols = 1;

	tmp_win->iconmgrp->width = (int) ((dragWidth *
					   (long) tmp_win->iconmgrp->columns)
					  / ncols);
        PackIconManager(tmp_win->iconmgrp);
    }

    if (!Scr->NoRaiseResize)
        XRaiseWindow(dpy, tmp_win->frame);

    UninstallRootColormap();

    ResizeWindow = None;
}

void
MenuEndResize(TwmWindow *tmp_win)
{
    MoveOutline(Scr->Root, 0, 0, 0, 0, 0, 0, 0);
    XUnmapWindow(dpy, Scr->SizeWindow);
    ConstrainSize (tmp_win, &dragWidth, &dragHeight);
    AddingX = dragx - tmp_win->frame_bw;
    AddingY = dragy - tmp_win->frame_bw;
    AddingW = dragWidth;/* + (2 * tmp_win->frame_bw);*/
    AddingH = dragHeight;/* + (2 * tmp_win->frame_bw);*/
    SetupWindow (tmp_win, AddingX, AddingY, AddingW, AddingH, -1);
}



/**
 * finish the resize operation for AddWindo<w
 */
void
AddEndResize(TwmWindow *tmp_win)
{

#ifdef DEBUG
    fprintf(stderr, "AddEndResize\n");
#endif

    ConstrainSize (tmp_win, &dragWidth, &dragHeight);
    AddingX = dragx - tmp_win->frame_bw;
    AddingY = dragy - tmp_win->frame_bw;
    AddingW = dragWidth + (2 * tmp_win->frame_bw);
    AddingH = dragHeight + (2 * tmp_win->frame_bw);
}

/**
 * adjust the given width and height to account for the constraints imposed
 * by size hints.
 *
 *      The general algorithm, especially the aspect ratio stuff, is
 *      borrowed from uwm's CheckConsistency routine.
 */
void
ConstrainSize (TwmWindow *tmp_win, int *widthp, int *heightp)
{
#define makemult(a,b) ((b==1) ? (a) : (((int)((a)/(b))) * (b)) )
#define _min(a,b) (((a) < (b)) ? (a) : (b))

    int minWidth, minHeight, maxWidth, maxHeight, xinc, yinc, delta;
    int baseWidth, baseHeight;
    int dwidth = *widthp, dheight = *heightp;


  if (tmp_win->title_pos == TP_LEFT || tmp_win->title_pos == TP_RIGHT)
    dwidth -= tmp_win->title_height;
  else
    dheight -= tmp_win->title_height;

    if (tmp_win->hints.flags & PMinSize) {
        minWidth = tmp_win->hints.min_width;
        minHeight = tmp_win->hints.min_height;
    } else if (tmp_win->hints.flags & PBaseSize) {
        minWidth = tmp_win->hints.base_width;
        minHeight = tmp_win->hints.base_height;
    } else
        minWidth = minHeight = 1;

    if (tmp_win->hints.flags & PBaseSize) {
	baseWidth = tmp_win->hints.base_width;
	baseHeight = tmp_win->hints.base_height;
    } else if (tmp_win->hints.flags & PMinSize) {
	baseWidth = tmp_win->hints.min_width;
	baseHeight = tmp_win->hints.min_height;
    } else
	baseWidth = baseHeight = 0;


    if (tmp_win->hints.flags & PMaxSize) {
        maxWidth = _min (Scr->MaxWindowWidth, tmp_win->hints.max_width);
        maxHeight = _min (Scr->MaxWindowHeight, tmp_win->hints.max_height);
    } else {
        maxWidth = Scr->MaxWindowWidth;
	maxHeight = Scr->MaxWindowHeight;
    }

    if (tmp_win->hints.flags & PResizeInc) {
        xinc = tmp_win->hints.width_inc;
        yinc = tmp_win->hints.height_inc;
    } else
        xinc = yinc = 1;

    /*
     * First, clamp to min and max values
     */
    if (dwidth < minWidth) dwidth = minWidth;
    if (dheight < minHeight) dheight = minHeight;

    if (dwidth > maxWidth) dwidth = maxWidth;
    if (dheight > maxHeight) dheight = maxHeight;


    /*
     * Second, fit to base + N * inc
     */
    dwidth = ((dwidth - baseWidth) / xinc * xinc) + baseWidth;
    dheight = ((dheight - baseHeight) / yinc * yinc) + baseHeight;


    /*
     * Third, adjust for aspect ratio
     */
#define maxAspectX tmp_win->hints.max_aspect.x
#define maxAspectY tmp_win->hints.max_aspect.y
#define minAspectX tmp_win->hints.min_aspect.x
#define minAspectY tmp_win->hints.min_aspect.y
    /*
     * The math looks like this:
     *
     * minAspectX    dwidth     maxAspectX
     * ---------- <= ------- <= ----------
     * minAspectY    dheight    maxAspectY
     *
     * If that is multiplied out, then the width and height are
     * invalid in the following situations:
     *
     * minAspectX * dheight > minAspectY * dwidth
     * maxAspectX * dheight < maxAspectY * dwidth
     *
     */

    if (tmp_win->hints.flags & PAspect)
    {
        if (minAspectX * dheight > minAspectY * dwidth)
        {
            delta = makemult(minAspectX * dheight / minAspectY - dwidth,
                             xinc);
            if (dwidth + delta <= maxWidth) dwidth += delta;
            else
            {
                delta = makemult(dheight - dwidth*minAspectY/minAspectX,
                                 yinc);
                if (dheight - delta >= minHeight) dheight -= delta;
            }
        }

        if (maxAspectX * dheight < maxAspectY * dwidth)
        {
            delta = makemult(dwidth * maxAspectY / maxAspectX - dheight,
                             yinc);
            if (dheight + delta <= maxHeight) dheight += delta;
            else
            {
                delta = makemult(dwidth - maxAspectX*dheight/maxAspectY,
                                 xinc);
                if (dwidth - delta >= minWidth) dwidth -= delta;
            }
        }
    }


    /*
     * Fourth, account for border width and title height
     */
  if (tmp_win->title_pos == TP_LEFT || tmp_win->title_pos == TP_RIGHT) {
    *widthp = dwidth + tmp_win->title_height;
    *heightp = dheight;
  } else {
    *widthp = dwidth;
    *heightp = dheight + tmp_win->title_height;
  }
}


/**
 * set window sizes, this was called from either AddWindow, EndResize, or
 * HandleConfigureNotify.
 *
 *  Special Considerations:
 * This routine will check to make sure the window is not completely off the
 * display, if it is, it'll bring some of it back on.
 *
 * The tmp_win->frame_XXX variables should NOT be updated with the values of
 * x,y,w,h prior to calling this routine, since the new values are compared
 * against the old to see whether a synthetic ConfigureNotify event should be
 * sent.  (It should be sent if the window was moved but not resized.)
 *
 *  \param tmp_win the TwmWindow pointer
 *  \param x       the x coordinate of the upper-left outer corner of the frame
 *  \param y       the y coordinate of the upper-left outer corner of the frame
 *  \param w       the width of the frame window w/o border
 *  \param h       the height of the frame window w/o border
 *  \param bw      the border width of the frame window or -1 not to change
 */
void SetupWindow (TwmWindow *tmp_win, int x, int y, int w, int h, int bw)
{
    SetupFrame (tmp_win, x, y, w, h, bw, False);
}

/**
 *  \param sendEvent whether or not to force a send
 */
void SetupFrame (TwmWindow *tmp_win, int x, int y, int w, int h, int bw, Bool sendEvent)
{
    XEvent client_event;
    XWindowChanges frame_wc, xwc;
    unsigned long frame_mask, xwcm;
    int title_width, title_height;
    int reShape;

#ifdef DEBUG
    fprintf (stderr, "SetupWindow: x=%d, y=%d, w=%d, h=%d, bw=%d\n",
	     x, y, w, h, bw);
#endif

    if (x >= Scr->MyDisplayWidth)
      x = Scr->MyDisplayWidth - 16;	/* one "average" cursor width */
    if (y >= Scr->MyDisplayHeight)
      y = Scr->MyDisplayHeight - 16;	/* one "average" cursor width */
    if (bw < 0)
      bw = tmp_win->frame_bw;		/* -1 means current frame width */

    if (tmp_win->iconmgr) {
	if (tmp_win->title_pos == TP_TOP ||
	    tmp_win->title_pos == TP_BOTTOM) {
	    tmp_win->iconmgrp->width = w;
	    h = tmp_win->iconmgrp->height + tmp_win->title_height;
	} else {
	    tmp_win->iconmgrp->width = w - tmp_win->title_height;
	    h = tmp_win->iconmgrp->height;
	}
    }

    /*
     * According to the July 27, 1988 ICCCM draft, we should send a
     * "synthetic" ConfigureNotify event to the client if the window
     * was moved but not resized.
     */
    if (((x != tmp_win->frame_x || y != tmp_win->frame_y) &&
	 (w == tmp_win->frame_width && h == tmp_win->frame_height)) ||
	(bw != tmp_win->frame_bw))
      sendEvent = TRUE;

    xwcm = CWWidth;
    title_width = xwc.width = w;
    title_height = Scr->TitleHeight + bw;

  if (tmp_win->title_pos == TP_LEFT || tmp_win->title_pos == TP_RIGHT) {
    /* title_width actually means height of title */
    title_width = h;
    ComputeWindowTitleOffsets (tmp_win, h, True);
  } else
    ComputeWindowTitleOffsets (tmp_win, xwc.width, True);

    reShape = (tmp_win->wShaped ? TRUE : FALSE);
    if (tmp_win->squeeze_info)		/* check for title shaping */
    {
	title_width = tmp_win->rightx + Scr->TBInfo.rightoff;
      if (tmp_win->title_pos == TP_LEFT || tmp_win->title_pos == TP_RIGHT) {
	if (title_width < h)
	{
	    xwc.width = title_width;
	    if (tmp_win->frame_height != h ||
		tmp_win->frame_width != w ||
		tmp_win->frame_bw != bw ||
		title_width != tmp_win->title_width)
		reShape = TRUE;
	}
	else
	{
	    if (!tmp_win->wShaped) reShape = TRUE;
	    title_width = h;
	}
      } else {
	if (title_width < xwc.width)
	{
	    xwc.width = title_width;
	    if (tmp_win->frame_height != h ||
	    	tmp_win->frame_width != w ||
		tmp_win->frame_bw != bw ||
	    	title_width != tmp_win->title_width)
	    	reShape = TRUE;
	}
	else
	{
	    if (!tmp_win->wShaped) reShape = TRUE;
	    title_width = xwc.width;
	}
      }
    }

    tmp_win->title_width = title_width;
    if (tmp_win->title_height) tmp_win->title_height = title_height;

    if (tmp_win->title_w) {
	if (bw != tmp_win->frame_bw) {
	    xwc.border_width = bw;
	    tmp_win->title_x = xwc.x = -bw;
	    tmp_win->title_y = xwc.y = -bw;
	    xwcm |= (CWX | CWY | CWBorderWidth);
	}
	if (tmp_win->squeeze_info == NULL && tmp_win->title_pos != TP_TOP) {
	    /* can't call ComputeTitleLocation() yet! */
	    tmp_win->title_x = -bw;
	    tmp_win->title_y = -bw;
	    if (tmp_win->title_pos == TP_BOTTOM)
		tmp_win->title_y += (h - tmp_win->title_height + bw);
	    if (tmp_win->title_pos == TP_RIGHT)
		tmp_win->title_x += (w - tmp_win->title_height + bw);
	    xwc.x = tmp_win->title_x;
	    xwc.y = tmp_win->title_y;
	    xwcm |= (CWX | CWY);
	}
	if (tmp_win->title_pos == TP_LEFT || tmp_win->title_pos == TP_RIGHT) {
	    xwcm |= CWHeight;
	    xwc.height = title_width;
	    xwcm &= ~CWWidth;
	    /* xwc.width = Scr->TitleHeight */;
	}

	XConfigureWindow(dpy, tmp_win->title_w, xwcm, &xwc);
    }

  if (tmp_win->title_pos == TP_TOP || tmp_win->title_pos == TP_BOTTOM) {
    if (tmp_win->attr.width != w)
	tmp_win->widthEverChangedByUser = True;

    if (tmp_win->attr.height != (h - tmp_win->title_height))
	tmp_win->heightEverChangedByUser = True;

    tmp_win->attr.width = w;
    tmp_win->attr.height = h - tmp_win->title_height;
  } else {
    if (tmp_win->attr.width != (w - tmp_win->title_height))
	tmp_win->widthEverChangedByUser = True;

    if (tmp_win->attr.height != h)
	tmp_win->heightEverChangedByUser = True;

    tmp_win->attr.width = w - tmp_win->title_height;
    tmp_win->attr.height = h;
  }

    switch (tmp_win->title_pos) {
    case TP_TOP:
        XMoveResizeWindow (dpy, tmp_win->w, 0, tmp_win->title_height,
			   w, h - tmp_win->title_height);
	break;
    case TP_BOTTOM:
	XMoveResizeWindow (dpy, tmp_win->w, 0, 0,
			   w, h - tmp_win->title_height);
	break;
    case TP_LEFT:
	XMoveResizeWindow (dpy, tmp_win->w, tmp_win->title_height, 0,
			   w - tmp_win->title_height, h);
	break;
    case TP_RIGHT:
	XMoveResizeWindow (dpy, tmp_win->w, 0, 0,
			   w - tmp_win->title_height, h);
	break;
    }

    /*
     * fix up frame and assign size/location values in tmp_win
     */
    frame_mask = 0;
    if (bw != tmp_win->frame_bw) {
	frame_wc.border_width = tmp_win->frame_bw = bw;
	frame_mask |= CWBorderWidth;
    }
    frame_wc.x = tmp_win->frame_x = x;
    frame_wc.y = tmp_win->frame_y = y;
    frame_wc.width = tmp_win->frame_width = w;
    frame_wc.height = tmp_win->frame_height = h;
    frame_mask |= (CWX | CWY | CWWidth | CWHeight);
    XConfigureWindow (dpy, tmp_win->frame, frame_mask, &frame_wc);

    /*
     * fix up highlight window
     */
    if (tmp_win->title_height && tmp_win->hilite_w)
    {
	xwc.width = (tmp_win->rightx - tmp_win->highlightx);
	if (Scr->TBInfo.nright > 0) xwc.width -= Scr->TitlePadding;
        if (xwc.width <= 0) {
            xwc.x = Scr->MyDisplayWidth;	/* move offscreen */
            xwc.width = 1;
        } else {
            xwc.x = tmp_win->highlightx;
        }

        xwcm = CWX | CWWidth;
	if (tmp_win->title_pos == TP_LEFT || tmp_win->title_pos == TP_RIGHT) {
	   xwcm &= ~CWX;
	   xwcm |= CWY | CWHeight;
	   xwc.y = xwc.x;
	   xwc.height = xwc.width;
	   xwc.width = (Scr->TitleHeight - 2 * Scr->FramePadding);
	}
        XConfigureWindow(dpy, tmp_win->hilite_w, xwcm, &xwc);
    }

    if (HasShape && reShape) {
	SetFrameShape (tmp_win);
    }

    if (sendEvent)
    {
        client_event.type = ConfigureNotify;
        client_event.xconfigure.display = dpy;
        client_event.xconfigure.event = tmp_win->w;
        client_event.xconfigure.window = tmp_win->w;
    switch (tmp_win->title_pos) {
    case TP_TOP:
        client_event.xconfigure.x = (x + tmp_win->frame_bw - tmp_win->old_bw);
        client_event.xconfigure.y = (y + tmp_win->frame_bw +
				     tmp_win->title_height - tmp_win->old_bw);
        client_event.xconfigure.width = tmp_win->frame_width;
        client_event.xconfigure.height = tmp_win->frame_height -
                tmp_win->title_height;
	break;
    case TP_BOTTOM:
        client_event.xconfigure.x = (x + tmp_win->frame_bw - tmp_win->old_bw);
        client_event.xconfigure.y = (y + tmp_win->frame_bw - tmp_win->old_bw);
        client_event.xconfigure.width = tmp_win->frame_width;
        client_event.xconfigure.height = tmp_win->frame_height -
                tmp_win->title_height;
	break;
    case TP_LEFT:
        client_event.xconfigure.x = (x + tmp_win->frame_bw +
				     tmp_win->title_height - tmp_win->old_bw);
        client_event.xconfigure.y = (y + tmp_win->frame_bw - tmp_win->old_bw);
        client_event.xconfigure.width = tmp_win->frame_width -
                tmp_win->title_height;
        client_event.xconfigure.height = tmp_win->frame_height;
	break;
    case TP_RIGHT:
        client_event.xconfigure.x = (x + tmp_win->frame_bw - tmp_win->old_bw);
        client_event.xconfigure.y = (y + tmp_win->frame_bw - tmp_win->old_bw);
        client_event.xconfigure.width = tmp_win->frame_width -
		tmp_win->title_height;
        client_event.xconfigure.height = tmp_win->frame_height;
	break;
    }
        client_event.xconfigure.border_width = tmp_win->old_bw;
        /* Real ConfigureNotify events say we're above title window, so ... */
	/* what if we don't have a title ????? */
        client_event.xconfigure.above = tmp_win->frame;
        client_event.xconfigure.override_redirect = False;
        XSendEvent(dpy, tmp_win->w, False, StructureNotifyMask, &client_event);
    }
}


/**
 * zooms window to full height of screen or to full height and width of screen.
 * (Toggles so that it can undo the zoom - even when switching between fullzoom
 * and vertical zoom.)
 *
 *  \param tmp_win  the TwmWindow pointer
 */
void
fullzoom(TwmWindow *tmp_win, int flag)
{
    Window      junkRoot;
    unsigned int junkbw, junkDepth;
    int basex, basey;
    int frame_bw_times_2;

	XGetGeometry(dpy, (Drawable) tmp_win->frame, &junkRoot,
	        &dragx, &dragy, (unsigned int *)&dragWidth, (unsigned int *)&dragHeight, &junkbw,
	        &junkDepth);

	basex = 0;
	basey = 0;

        if (tmp_win->zoomed == flag)
        {
            dragHeight = tmp_win->save_frame_height;
            dragWidth = tmp_win->save_frame_width;
            dragx = tmp_win->save_frame_x;
            dragy = tmp_win->save_frame_y;
            tmp_win->zoomed = ZOOM_NONE;
        }
        else
        {
                if (tmp_win->zoomed == ZOOM_NONE)
                {
                        tmp_win->save_frame_x = dragx;
                        tmp_win->save_frame_y = dragy;
                        tmp_win->save_frame_width = dragWidth;
                        tmp_win->save_frame_height = dragHeight;
                        tmp_win->zoomed = flag;
                 }
                  else
                            tmp_win->zoomed = flag;


	frame_bw_times_2 = 2*tmp_win->frame_bw;

        switch (flag)
        {
        case ZOOM_NONE:
            break;
        case F_ZOOM:
            dragHeight = Scr->MyDisplayHeight - frame_bw_times_2;
            dragy=basey;
            break;
        case F_HORIZOOM:
            dragx = basex;
            dragWidth = Scr->MyDisplayWidth - frame_bw_times_2;
            break;
        case F_FULLZOOM:
            dragx = basex;
            dragy = basey;
            dragHeight = Scr->MyDisplayHeight - frame_bw_times_2;
            dragWidth = Scr->MyDisplayWidth - frame_bw_times_2;
            break;
        case F_LEFTZOOM:
            dragx = basex;
            dragy = basey;
            dragHeight = Scr->MyDisplayHeight - frame_bw_times_2;
            dragWidth = Scr->MyDisplayWidth/2 - frame_bw_times_2;
            break;
        case F_RIGHTZOOM:
            dragx = basex + Scr->MyDisplayWidth/2;
            dragy = basey;
            dragHeight = Scr->MyDisplayHeight - frame_bw_times_2;
            dragWidth = Scr->MyDisplayWidth/2 - frame_bw_times_2;
            break;
        case F_TOPZOOM:
            dragx = basex;
            dragy = basey;
            dragHeight = Scr->MyDisplayHeight/2 - frame_bw_times_2;
            dragWidth = Scr->MyDisplayWidth - frame_bw_times_2;
            break;
        case F_BOTTOMZOOM:
            dragx = basex;
            dragy = basey + Scr->MyDisplayHeight/2;
            dragHeight = Scr->MyDisplayHeight/2 - frame_bw_times_2;
            dragWidth = Scr->MyDisplayWidth - frame_bw_times_2;
            break;
         }
      }

    if (!Scr->NoRaiseResize)
        XRaiseWindow(dpy, tmp_win->frame);

    ConstrainSize(tmp_win, &dragWidth, &dragHeight);

    SetupWindow (tmp_win, dragx , dragy , dragWidth, dragHeight, -1);
    XUngrabPointer (dpy, CurrentTime);
    XUngrabServer (dpy);
}

void
SetFrameShape (TwmWindow *tmp)
{
    /*
     * see if the titlebar needs to move
     */
    if (tmp->title_w) {
	int oldx = tmp->title_x, oldy = tmp->title_y;
	ComputeTitleLocation (tmp);
	if (oldx != tmp->title_x || oldy != tmp->title_y)
	  XMoveWindow (dpy, tmp->title_w, tmp->title_x, tmp->title_y);
    }

    /*
     * The frame consists of the shape of the contents window offset by
     * title_height or'ed with the shape of title_w (which is always
     * rectangular).
     */
    if (tmp->wShaped) {
	/*
	 * need to do general case
	 */
	if (tmp->title_pos == TP_TOP || tmp->title_pos == TP_BOTTOM) {
	    XShapeCombineShape (dpy, tmp->frame, ShapeBounding,
				0,
			    (tmp->title_pos == TP_TOP)?tmp->title_height:0,
				tmp->w,
				ShapeBounding, ShapeSet);
	    if (tmp->title_w) {
		XShapeCombineShape (dpy, tmp->frame, ShapeBounding,
				    tmp->title_x + tmp->frame_bw,
				    tmp->title_y + tmp->frame_bw,
				    tmp->title_w, ShapeBounding,
				    ShapeUnion);
	    }
	} else {
	    XShapeCombineShape (dpy, tmp->frame, ShapeBounding,
			    (tmp->title_pos == TP_LEFT)?tmp->title_height:0,
				0, tmp->w,
				ShapeBounding, ShapeSet);
	    if (tmp->title_w) {
		XShapeCombineShape (dpy, tmp->frame, ShapeBounding,
				    tmp->title_x + tmp->frame_bw,
				    tmp->title_y + tmp->frame_bw,
				    tmp->title_w, ShapeBounding,
				    ShapeUnion);
	    }
	}
    } else {
	/*
	 * can optimize rectangular contents window
	 */
	if (tmp->squeeze_info) {
	    XRectangle  newBounding[2];
	    XRectangle  newClip[2];
	    int fbw2 = 2 * tmp->frame_bw;

	    /*
	     * Build the border clipping rectangles; one around title, one
	     * around window.  The title_[xy] field already have had frame_bw
	     * subtracted off them so that they line up properly in the frame.
	     *
	     * The frame_width and frame_height do *not* include borders.
	     */
	  if (tmp->title_pos == TP_TOP || tmp->title_pos == TP_BOTTOM) {
	    int order = (tmp->title_pos == TP_TOP)?YXBanded:Unsorted;
	    /* border */
	    newBounding[0].x = tmp->title_x;
	    newBounding[0].y = tmp->title_y;
	    if (tmp->title_pos == TP_BOTTOM)
		newBounding[0].y += tmp->frame_bw;
	    newBounding[0].width = tmp->title_width + fbw2;
	    newBounding[0].height = tmp->title_height;
	    newBounding[1].x = -tmp->frame_bw;
	    newBounding[1].y = (tmp->title_pos == TP_TOP)?
				Scr->TitleHeight : -tmp->frame_bw;
	    newBounding[1].width = tmp->attr.width + fbw2;
	    newBounding[1].height = tmp->attr.height + fbw2;
	    XShapeCombineRectangles (dpy, tmp->frame, ShapeBounding, 0, 0,
				     newBounding, 2, ShapeSet, order);
	    /* insides */
	    newClip[0].x = tmp->title_x + tmp->frame_bw;
	    newClip[0].y = (tmp->title_pos == TP_TOP)?
				0 : tmp->attr.height + tmp->frame_bw;
	    newClip[0].width = tmp->title_width;
	    newClip[0].height = Scr->TitleHeight;
	    newClip[1].x = 0;
	    newClip[1].y = (tmp->title_pos == TP_TOP)?
				tmp->title_height : 0;
	    newClip[1].width = tmp->attr.width;
	    newClip[1].height = tmp->attr.height;
	    XShapeCombineRectangles (dpy, tmp->frame, ShapeClip, 0, 0,
				     newClip, 2, ShapeSet, order);
	  } else {
	    int order = Unsorted;
	    /* border */
	    newBounding[0].x = tmp->title_x;
	    if (tmp->title_pos == TP_RIGHT)
		newBounding[0].x += tmp->frame_bw;
	    newBounding[0].y = tmp->title_y;
	    newBounding[0].width = tmp->title_height;
	    newBounding[0].height = tmp->title_width + fbw2;
	    newBounding[1].x = (tmp->title_pos == TP_LEFT)?
				Scr->TitleHeight : -tmp->frame_bw;
	    newBounding[1].y = -tmp->frame_bw;
	    newBounding[1].width = tmp->attr.width + fbw2;
	    newBounding[1].height = tmp->attr.height + fbw2;
	    XShapeCombineRectangles (dpy, tmp->frame, ShapeBounding, 0, 0,
				     newBounding, 2, ShapeSet, order);
	    /* insides */
	    newClip[0].x = (tmp->title_pos == TP_LEFT)?
				0 : tmp->attr.width + tmp->frame_bw;
	    newClip[0].y = tmp->title_y + tmp->frame_bw;
	    newClip[0].width = Scr->TitleHeight;
	    newClip[0].height = tmp->title_width;
	    newClip[1].x = (tmp->title_pos == TP_LEFT)?  tmp->title_height : 0;
	    newClip[1].y = 0;
	    newClip[1].width = tmp->attr.width;
	    newClip[1].height = tmp->attr.height;
	    XShapeCombineRectangles (dpy, tmp->frame, ShapeClip, 0, 0,
				     newClip, 2, ShapeSet, order);
	  }
	} else {
	    (void) XShapeCombineMask (dpy, tmp->frame, ShapeBounding, 0, 0,
 				      None, ShapeSet);
	    (void) XShapeCombineMask (dpy, tmp->frame, ShapeClip, 0, 0,
				      None, ShapeSet);
	}
    }
}

/*
 * Squeezed Title:
 *
 *                         tmp->title_x
 *                   0     |
 *  tmp->title_y   ........+--------------+.........  -+,- tmp->frame_bw
 *             0   : ......| +----------+ |....... :  -++
 *                 : :     | |          | |      : :   ||-Scr->TitleHeight
 *                 : :     | |          | |      : :   ||
 *                 +-------+ +----------+ +--------+  -+|-tmp->title_height
 *                 | +---------------------------+ |  --+
 *                 | |                           | |
 *                 | |                           | |
 *                 | |                           | |
 *                 | |                           | |
 *                 | |                           | |
 *                 | +---------------------------+ |
 *                 +-------------------------------+
 *
 *
 * Unsqueezed Title:
 *
 *                 tmp->title_x
 *                 | 0
 *  tmp->title_y   +-------------------------------+  -+,tmp->frame_bw
 *             0   | +---------------------------+ |  -+'
 *                 | |                           | |   |-Scr->TitleHeight
 *                 | |                           | |   |
 *                 + +---------------------------+ +  -+
 *                 |-+---------------------------+-|
 *                 | |                           | |
 *                 | |                           | |
 *                 | |                           | |
 *                 | |                           | |
 *                 | |                           | |
 *                 | +---------------------------+ |
 *                 +-------------------------------+
 *
 *
 *
 * Dimensions and Positions:
 *
 *     frame orgin                 (0, 0)
 *     frame upper left border     (-tmp->frame_bw, -tmp->frame_bw)
 *     frame size w/o border       tmp->frame_width , tmp->frame_height
 *     frame/title border width    tmp->frame_bw
 *     extra title height w/o bdr  tmp->title_height = TitleHeight + frame_bw
 *     title window height         Scr->TitleHeight
 *     title origin w/o border     (tmp->title_x, tmp->title_y)
 *     client origin               (0, Scr->TitleHeight + tmp->frame_bw)
 *     client size                 tmp->attr.width , tmp->attr.height
 *
 * When shaping, need to remember that the width and height of rectangles
 * are really deltax and deltay to lower right handle corner, so they need
 * to have -1 subtracted from would normally be the actual extents.
 */


/***********************************************************************
 *
 *  Procedure:
 *      RelocateBtns - relocate buttons
 *
 *  Inputs:
 *      tmp_win - the TwmWindow pointer
 *      topbtm - TRUE when top or bottom
 *
 ***********************************************************************
 */
static void
RelocateBtns (tmp_win, topbtm)
    TwmWindow *tmp_win;
    int topbtm;
{
    TBWindow *tbw;
    TitleButton *tb;
    XSetWindowAttributes attributes;
    XWindowChanges xwc;
    int y, leftx, rightx, boxwidth;

    if (tmp_win->titlebuttons == NULL || topbtm == -1)
	return;

    leftx = y = Scr->TBInfo.leftx;
    rightx = tmp_win->rightx;
    boxwidth = (Scr->TBInfo.width + Scr->TBInfo.pad);

    for (tb = Scr->TBInfo.head, tbw = tmp_win->titlebuttons; tb;
	 tb = tb->next, tbw++) {
	int x;

	XConfigureWindow(dpy, tbw->window, (CWX|CWY), &xwc);
	if (tb->rightside) {
	    x = rightx;
	    rightx += boxwidth;
	    if (topbtm == TRUE) {
		attributes.win_gravity = NorthEastGravity;
		xwc.x = x;
		xwc.y = y;
	    } else {
		attributes.win_gravity = SouthWestGravity;
		xwc.x = y;
		xwc.y = x;
	    }
	} else {
	    x = leftx;
	    leftx += boxwidth;
	    if (topbtm == TRUE) {
		xwc.x = x;
		xwc.y = y;
	    } else {
		xwc.x = y;
		xwc.y = x;
	    }
	    attributes.win_gravity = NorthWestGravity;
	}
	XChangeWindowAttributes(dpy, tbw->window, CWWinGravity, &attributes);
	XConfigureWindow(dpy, tbw->window, (CWX|CWY), &xwc);
    }
}

/***********************************************************************
 *
 *  Procedure:
 *      ChangeTitlePos - change the position of titlebar
 *
 *  Inputs:
 *      tmp_win - the TwmWindow pointer
 *      pos - position of titlebar
 *
 ***********************************************************************
 */
void
ChangeTitlePos (tmp_win, pos)
    TwmWindow *tmp_win;
    int pos;
{
    int h, w;
    int x, y;
    int reloc_btns = -1;


    if (pos == -1 || tmp_win->title_pos == pos)
	return; /* nothing to do */

    h = tmp_win->frame_height;
    w = tmp_win->frame_width;

    /* reconfigure title window if necessary */
    if (tmp_win->title_w) {
	XWindowChanges xwc_w, xwc_h;
	unsigned long xwcm_w, xwcm_h;
	xwcm_w = xwcm_h = 0;
	if (tmp_win->title_pos != TP_TOP && pos == TP_TOP) {
	    xwcm_w |= CWX | CWY;
	    xwc_w.x = - tmp_win->frame_bw;
	    xwc_w.y = - tmp_win->frame_bw;
	}
	if (tmp_win->title_pos == TP_TOP || tmp_win->title_pos == TP_BOTTOM) {
	    if (pos == TP_LEFT || pos == TP_RIGHT) {
		xwcm_w |= CWHeight | CWWidth;
		xwc_w.height = tmp_win->attr.height;
		xwc_w.width = Scr->TitleHeight;
		if (tmp_win->title_height && tmp_win->hilite_w) {
		    /* SetupWindow() do the real work */
		    xwcm_h |= CWHeight | CWWidth | CWX | CWY;
		    xwc_h.height = tmp_win->attr.height;
		    xwc_h.width = (Scr->TitleHeight - 2 * Scr->FramePadding);
		    xwc_h.x = Scr->FramePadding;
		    xwc_h.y = 0;
		}
		reloc_btns = FALSE;
		h -= tmp_win->title_height;
		w += tmp_win->title_height;
	    }
	} else {
	    if (pos == TP_TOP || pos == TP_BOTTOM) {
		xwcm_w |= CWHeight | CWWidth;
		xwc_w.height = Scr->TitleHeight;
		xwc_w.width = tmp_win->attr.width;
		if (tmp_win->title_height && tmp_win->hilite_w) {
		    /* SetupWindow() do the real work */
		    xwcm_h |= CWHeight | CWWidth | CWX | CWY;
		    xwc_h.height = (Scr->TitleHeight - 2 * Scr->FramePadding);
		    xwc_h.width = tmp_win->attr.width;
		    xwc_h.x = 0;
		    xwc_h.y = Scr->FramePadding;
		}
		reloc_btns = TRUE;
		h += tmp_win->title_height;
		w -= tmp_win->title_height;
	    }
	}
	if (xwcm_w)
	    XConfigureWindow(dpy, tmp_win->title_w, xwcm_w, &xwc_w);
	if (xwcm_h)
	    XConfigureWindow(dpy, tmp_win->hilite_w, xwcm_h, &xwc_h);
    }

    x = tmp_win->frame_x;
    y = tmp_win->frame_y;

    if (tmp_win->title_pos == TP_TOP)
	y += tmp_win->title_height;
    else if (tmp_win->title_pos == TP_LEFT)
	x += tmp_win->title_height;

    if (pos == TP_TOP)
	y -= tmp_win->title_height;
    else if (pos == TP_LEFT)
	x -= tmp_win->title_height;

    tmp_win->title_pos = pos;
    tmp_win->title_width = -1; /* force SetFrameShape() */


    SetupWindow(tmp_win, x, y, w, h, tmp_win->frame_bw);

    RelocateBtns (tmp_win, reloc_btns);
}

/***********************************************************************
 *
 *  Procedure:
 *      ChangeSqueeze - change the squeeze status of titlebar
 *
 *  Inputs:
 *      tmp_win - the TwmWindow pointer
 *      justify - squeeze justify (J_LEFT, J_CENTER, J_RIGHT)
 *
 ***********************************************************************
 */
void
ChangeSqueeze (tmp_win, justify)
    TwmWindow *tmp_win;
    int justify;
{
    static SqueezeInfo default_squeeze[] = {
	{J_LEFT, 0, 0 },
	{J_CENTER, 0, 0 },
	{J_RIGHT, 0, 0 }
    };
    int i;

    if (tmp_win->squeeze_info == NULL && justify == -1)
	return; /* should not happen */

    if (tmp_win->squeeze_info && tmp_win->squeeze_info->justify == justify)
	return;

    if (justify == -1) {
	    /* need to resize title bar window */
	    XWindowChanges xwc;
	    unsigned long xwcm;

	    if (tmp_win->title_pos == TP_TOP ||
		tmp_win->title_pos == TP_BOTTOM) {
		xwcm = CWWidth;
		xwc.width = tmp_win->attr.width;
	    } else {
		xwcm = CWHeight;
		xwc.height = tmp_win->attr.height;
	    }
	    XConfigureWindow(dpy, tmp_win->title_w, xwcm, &xwc);
	    XConfigureWindow(dpy, tmp_win->hilite_w, xwcm, &xwc);

	    tmp_win->squeeze_info = NULL;
    } else {
	for (i=0; i < sizeof(default_squeeze)/sizeof(default_squeeze[0]); i++)
	    if (default_squeeze[i].justify == justify) {
		tmp_win->squeeze_info = &default_squeeze[i];
		break;
	    }
	if (i >= sizeof(default_squeeze)/sizeof(default_squeeze[0]))
	    return;
    }

    tmp_win->title_width = -1; /* force SetFrameShape() */

    SetupWindow(tmp_win, tmp_win->frame_x, tmp_win->frame_y,
		tmp_win->frame_width, tmp_win->frame_height,
		tmp_win->frame_bw);

    if (justify == -1)
	SetFrameShape(tmp_win);
}
