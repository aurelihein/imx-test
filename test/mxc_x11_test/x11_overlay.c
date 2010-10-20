/*
 * Copyright (C) 2010 Freescale Semiconductor, Inc. All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 *
 */

/*
 * @file x11_overlay.c
 *
 * @brief X11 unit test applicationi to demonstratate rendering of
 * a moving overlay on top of a window.
 *
 */

/*
 * usage: x11_overlay <num-iterations>
 *
 * Description:
 *
 * - Create 512x512 X window with visual and color format matching the screen.
 * This will be the target drawable.
 * - Create an Xrender picture to wrap the target drawable for use with Xrender.
 *
 * - Create a 512x512 X pixmap with depth matching the window.
 * This will be the background source drawable.
 * - Create an Xrender picture to wrap the background source drawable for use
 * with Xrender using the same visual as the screen.
 * - Fill background source with grayscale pattern which is concentric bands
 * of a gray gradient.
 * - Copy the background source drawable to the window.
 *
 * - Create 256x256 X pixmap with depth matching the screen.
 * This will be the overlay source drawable.
 * - Create an Xrender picture to wrap the overlay source drawable for use with
 * XRender using the same visual as the window.
 * - Fill overlay source with solid black color.
 *
 * - Create 256x256 X pixmap with 8-bit depth.  This will be the mask drawable.
 * - Create an XRender picture to wrap the mask drawable for use with XRender.
 * - Fill mask with 0's corresponding to 0% opacity or full transparency.
 * - Using 25% opacity value, draw an outline of the pixmap and also draw a
 * circle somewhere in the miiddle of the pixmap.
 *
 * - The overlay source and mask drawables are assumed to align.  When drawn
 * together using XRenderComposite with the "over" operation, then the overlay
 * source is blended on top of the target using the opacity value stored in the
 * mask.  So, only places where the mask has a non-zero value is something from
 * the corresponding pixel in the source overlay drawn on top of the target.
 * Together, the overlay source and mask create an overlay.
 *
 * - Since the target is twice the size of the source and mask, the idea is to
 * pan the overlay over the top of target moving back-and-forth left-to-right
 * and top-to-bottom.  Each time the overlay is moved, the 256x256 area
 * where the overlay had been needs to be redrawn by copying just that portion
 * of the background source drawable to the window.  Then XRenderComposite
 * is used to draw overlay source drawable using the mask drawable to a new
 * location over the window.  When X-acceleration is enabled, this will use
 * the GPU to perform the blended rendering of the overlay source on top of
 * the bacground source in the window (the target drawable).
 *
 * - This panning operation is repeated a number of times as specified on
 * the command line.  The total time to perform the panning operationi
 * for the specified number of operations is measured and an average update
 * time is reported.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xcms.h>
#include <X11/extensions/Xrender.h>

#define	IMAGE_WIDTH	256
#define	IMAGE_HEIGHT	256

static unsigned long
getElapsedMicroseconds(struct timeval* pTimeStart, struct timeval* pTimeStop)
{
	/* If either time is missing, then return 0. */
	if ((NULL == pTimeStart) || (NULL == pTimeStop)) {
		return 0;
	}

	/* Start time after stop time (looking at seconds field only). */
	if (pTimeStart->tv_sec > pTimeStop->tv_sec) {

		return 0;

	/* Start time and stop time have same seconds field. */
	} else if (pTimeStart->tv_sec == pTimeStop->tv_sec) {

		/* Start time after stop time (looking at usec field only). */
		if (pTimeStart->tv_usec > pTimeStop->tv_usec) {

			return 0;

		} else {

			return pTimeStop->tv_usec - pTimeStart->tv_usec;
		}

	/* Start time is before stop time, but the seconds are different. */
	} else {

		unsigned long elapsedMicroseconds =
			(pTimeStop->tv_sec - pTimeStart->tv_sec) * 1000000U;

		elapsedMicroseconds +=
			(pTimeStop->tv_usec - pTimeStart->tv_usec);

		return elapsedMicroseconds;
	}
}

int main (int argc, char* argv[])
{
	/* Determine number of iterations. */
	if (2 != argc)
	{
		printf("usage: %s <num-iterations>\n", argv[0]);
		exit(1);
	}
	int numIterations = atoi(argv[1]);

	int returnCode = 0;
	Display* xDisplay = NULL;
	Window xWindow = 0;
	Picture xPictureWindow = 0;
	Pixmap xPixmapMain = 0;
	Picture xPictureMain = 0;
	Pixmap xPixmapOverlay = 0;
	Picture xPictureOverlay = 0;
	Pixmap xPixmapMask = 0;
	Picture xPictureMask = 0;
	GC gcMask = 0;

	/* Access X display. */
	if (NULL == (xDisplay = XOpenDisplay(NULL)))
	{
		printf("XOpenDisplay(NULL) failed\n");
		returnCode = 1;
		goto error;
	}

	/* Access info about the screen. */
	Screen* xScreen = XDefaultScreenOfDisplay(xDisplay);
	GC gc = XDefaultGCOfScreen(xScreen);
	Colormap xColormap = XDefaultColormapOfScreen(xScreen);

	/* Create main X window */
	xWindow = XCreateSimpleWindow(
			xDisplay,
			RootWindow(xDisplay, 0),
			0, 0,
			IMAGE_WIDTH*2,
			IMAGE_HEIGHT*2,
			0,
			BlackPixel(xDisplay, 0),
			BlackPixel(xDisplay, 0));
	if (0 == xWindow)
	{
		printf("XCreateSimpleWindow failed\n");
		returnCode = 1;
		goto error;
	}
	XMapWindow(xDisplay, xWindow);
	XSync(xDisplay, False);

	/* Get the attributes associated with the main window. */
	XWindowAttributes xWindowAttr;
	if (!XGetWindowAttributes(xDisplay, xWindow, &xWindowAttr))
	{
		printf("XGetWindowAttributes failed\n");
		returnCode = 1;
		goto error;
	}

	/* Find the X render picture format associated with the visual */
	/* for the main window */
	XRenderPictFormat* xRenderPictFormatWindow =
		XRenderFindVisualFormat(xDisplay, xWindowAttr.visual);
	if (NULL == xRenderPictFormatWindow)
	{
		printf("XRenderFindVisualFormat failed\n");
		returnCode = 1;
		goto error;
	}

	/* Find the X render picture format associated with 8 bit alpha. */
	XRenderPictFormat xRenderPictFormatTemplate;
	xRenderPictFormatTemplate.depth = 8;
	xRenderPictFormatTemplate.type = PictTypeDirect;
	xRenderPictFormatTemplate.direct.alphaMask = 0x0FF;
	unsigned long xRenderPictFormatTemplateMask =
			PictFormatDepth | PictFormatType | PictFormatAlphaMask;
	XRenderPictFormat* xRenderPictFormatMask =
		XRenderFindFormat(
			xDisplay,
			xRenderPictFormatTemplateMask,
			&xRenderPictFormatTemplate,
			0);
	if (NULL == xRenderPictFormatMask)
	{
		printf("XRenderFindFormat failed\n");
		returnCode = 1;
		goto error;
	}

	/* Create X render picture associated with the screen. */
	/* Having the same visual format as the window. */
	xPictureWindow = XRenderCreatePicture(
				xDisplay,
				xWindow,
				xRenderPictFormatWindow,
				0,
				NULL);
	if (0 == xPictureWindow)
	{
		printf("XRenderCreatePicture (window) failed\n");
		returnCode = 1;
		goto error;
	}

	/* Create backing pixmap for the main window. */
	xPixmapMain = XCreatePixmap(
			xDisplay,
			xWindow,
			xWindowAttr.width,
			xWindowAttr.height,
			xWindowAttr.depth);
	if (0 == xPixmapMain)
	{
		printf("XCreatePixmap (main) failed\n");
		returnCode = 1;
		goto error;
	}

	/* Create X render picture associated with the backing pixmap. */
	/* Having the same visual format as the window. */
	xPictureMain = XRenderCreatePicture(
			xDisplay,
			xPixmapMain,
			xRenderPictFormatWindow,
			0,
			NULL);
	if (0 == xPictureMain)
	{
		printf("XRenderCreatePicture (main) failed\n");
		returnCode = 1;
		goto error;
	}

	/* Draw concentric rectangles of different gray. */
	unsigned i;
	for (i = 0; i < 256; ++i)
	{
		float fGray = i / 255.0;

		/* Find the color gray. */
		XcmsColor xColorGray;
		xColorGray.spec.RGBi.red = fGray;
		xColorGray.spec.RGBi.green = fGray;
		xColorGray.spec.RGBi.blue = fGray;
		xColorGray.format = XcmsRGBiFormat;
		if (0 == XcmsAllocColor(
				xDisplay,
				xColormap,
				&xColorGray,
				XcmsRGBFormat))
		{
			printf("XcmsAllocColor failed\n");
			returnCode = 1;
			goto error;
		}

		/* Change the drawing color for the main window. */
		XSetForeground(xDisplay, gc, xColorGray.pixel);

		XDrawRectangle(
			xDisplay,
			xPixmapMain,
			gc,
			i, i,
			(IMAGE_WIDTH - i) * 2 - 1,
			(IMAGE_HEIGHT - i) * 2 - 1);
	}
	XRenderComposite(
		xDisplay,
		PictOpSrc,
		xPictureMain,	/* src */
		0,		/* mask */
		xPictureWindow,	/* dst */
		0, 0,		/* src (x,y) */
		0, 0,		/* mask (x,y) */
		0, 0,		/* dst (x,y) */
		xWindowAttr.width,
		xWindowAttr.height);
	XSync(xDisplay, False);

	/* Create pixmap for the overlay content. */
	xPixmapOverlay = XCreatePixmap(
				xDisplay,
				xWindow,
				IMAGE_WIDTH,
				IMAGE_HEIGHT,
				xWindowAttr.depth);
	if (0 == xPixmapOverlay)
	{
		printf("XCreatePixmap (overlay) failed\n");
		returnCode = 1;
		goto error;
	}

	/* Create X render picture assocaited with the overlay pixmap. */
	/* Having the same visual format as the window. */
	xPictureOverlay = XRenderCreatePicture(
				xDisplay,
				xPixmapOverlay,
				xRenderPictFormatWindow,
				0,
				NULL);
	if (0 == xPictureOverlay)
	{
		printf("XRenderCreatePicture (overlay) failed\n");
		returnCode = 1;
		goto error;
	}

	/* Fill the overlay with black to be used for overlay color. */
	XSetForeground(xDisplay, gc, XBlackPixelOfScreen(xScreen));
	XFillRectangle(
		xDisplay,
		xPixmapOverlay,
		gc,
		0, 0,
		IMAGE_WIDTH,
		IMAGE_HEIGHT);

	/* Create pixmap for the mask content. */
	xPixmapMask = XCreatePixmap(
				xDisplay,
				xWindow,
				IMAGE_WIDTH,
				IMAGE_HEIGHT,
				8);
	if (0 == xPixmapMask)
	{
		printf("XCreatePixmap (mask) failed\n");
		returnCode = 1;
		goto error;
	}

	/* Create X render picture assocaited with the mask pixmap. */
	xPictureMask = XRenderCreatePicture(
				xDisplay,
				xPixmapMask,
				xRenderPictFormatMask,
				0,
				NULL);
	if (0 == xPictureMask)
	{
		printf("XRenderCreatePicture (mask) failed\n");
		returnCode = 1;
		goto error;
	}

	/* Create a GC to go with mask */
	gcMask = XCreateGC(xDisplay, xPixmapMask, 0, NULL);
	XSetForeground(xDisplay, gcMask, 0x00000000);
	XFillRectangle(
		xDisplay,
		xPixmapMask,
		gcMask,
		0, 0,
		IMAGE_WIDTH,
		IMAGE_HEIGHT);
	XSetForeground(xDisplay, gcMask, 0x40404040);
	XDrawRectangle(
		xDisplay,
		xPixmapMask,
		gcMask,
		0, 0,
		IMAGE_WIDTH-1,
		IMAGE_HEIGHT-1);
	XFillArc(
		xDisplay,
		xPixmapMask,
		gcMask,
		100, 100,
		100, 100,
		0,		/* start angle-degrees * 64 */
		360 * 64);	/* extent angle-degrees * 64 */

	Bool bIncX = True;
	Bool bIncY = True;
	Bool bNextRow = False;
	int x = 0;
	int y = 0;
	struct timeval timeStart;
	gettimeofday(&timeStart, NULL);
	int iter;
	for (iter = 0; iter < numIterations; ++iter)
	{
		XRenderComposite(
			xDisplay,
			PictOpSrc,
			xPictureMain,	/* src */
			0,		/* mask */
			xPictureWindow,	/* dst */
			x, y,		/* src (x,y) */
			0, 0,		/* mask (x,y) */
			x,		/* dst x */
			y,		/* dst y */
			IMAGE_WIDTH,
			IMAGE_HEIGHT);

		if (bNextRow)
		{
			if (bIncY)
			{
				if ((y += 10) >= IMAGE_HEIGHT)
				{
					y = IMAGE_HEIGHT - 1;
					bIncY = False;
				}
			}
			else
			{
				if ((y -= 10) < 0)
				{
					y = 0;
					bIncY = True;
				}
			}

			bNextRow = False;
		}
		else
		{
			if (bIncX)
			{
				if (++x >= IMAGE_WIDTH)
				{
					x = IMAGE_WIDTH - 1;
					bIncX = False;
					bNextRow = True;
				}
			}
			else
			{
				if (--x < 0)
				{
					x = 0;
					bIncX = True;
					bNextRow = True;
				}
			}
		}

		XRenderComposite(
			xDisplay,
			PictOpOver,
			xPictureOverlay,/* src */
			xPictureMask,	/* mask */
			xPictureWindow,	/* dst */
			0, 0,		/* src (x,y) */
			0, 0,		/* mask (x,y) */
			x,		/* dst x */
			y,		/* dst y */
			IMAGE_WIDTH,
			IMAGE_HEIGHT);
	}
	XSync(xDisplay, False);

	struct timeval timeEnd;
	gettimeofday(&timeEnd, NULL);
	double elapsedSec =
		getElapsedMicroseconds(&timeStart, &timeEnd) / 1000000L;
	double fps = numIterations / elapsedSec;
	printf("average update rate = %.1lf FPS\n", fps);

error:

	if (0 != gcMask)
	{
		XFreeGC(xDisplay, gcMask);
		gcMask = 0;
	}

	if (0 != xPictureMask)
	{
		XRenderFreePicture(xDisplay, xPictureMask);
		xPictureMask = 0;
	}

	if (0 != xPixmapMask)
	{
		XFreePixmap(xDisplay, xPixmapMask);
		xPixmapMask = 0;
	}

	if (0 != xPictureOverlay)
	{
		XRenderFreePicture(xDisplay, xPictureOverlay);
		xPictureOverlay = 0;
	}

	if (0 != xPixmapOverlay)
	{
		XFreePixmap(xDisplay, xPixmapOverlay);
		xPixmapOverlay = 0;
	}

	if (0 != xPictureMain)
	{
		XRenderFreePicture(xDisplay, xPictureMain);
		xPictureMain = 0;
	}

	if (0 != xPixmapMain)
	{
		XFreePixmap(xDisplay, xPixmapMain);
		xPixmapMain = 0;
	}

	if (0 != xPictureWindow)
	{
		XRenderFreePicture(xDisplay, xPictureWindow);
		xPictureWindow = 0;
	}

	if (0 != xWindow)
	{
		XDestroyWindow(xDisplay, xWindow);
		xWindow = 0;
	}

	if (NULL != xDisplay)
	{
		XCloseDisplay(xDisplay);
		xDisplay = NULL;
	}

	return returnCode;
}

