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
 * @file mxc_epdc_x11_fb_test.c
 *
 * @brief MXC EPDC unit test applicationi for framebuffer updates
 * based on X11 changes to its root window
 *
 */

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <poll.h>
#include <X11/Xlib.h>
#include <X11/extensions/Xdamage.h>
#include <linux/fb.h>
#include <linux/mxcfb.h>
#include <sys/ioctl.h>

#define WAVEFORM_MODE_DU	0x1	/* Grey->white/grey->black */
#define WAVEFORM_MODE_GC16	0x2	/* High fidelity (flashing) */
#define WAVEFORM_MODE_GC4	0x3	/* Lower fidelity */

static int kbhit(void)
{
	struct pollfd pollfd;
	pollfd.fd = 0;
	pollfd.events = POLLIN;
	pollfd.revents = 0;

	return poll(&pollfd, 1, 0);
}

int main (int argc, char* argv[])
{
	/* Declare and init variables used after cleanup: goto. */
	Display* xDisplay = NULL;
	Damage xDamageScreen = 0;
	int fd = -1;

	/* Open the X display */
	xDisplay = XOpenDisplay(NULL);
	if (NULL == xDisplay)
	{
		printf("\nError: unable to open X display\n");
		goto cleanup;
	}

	/* Acess the root window for the display. */
	Window xRootWindow = XDefaultRootWindow(xDisplay);
	if (0 == xRootWindow)
	{
		printf("\nError: unable to access root window for screen\n");
		goto cleanup;
	}

	/* Query the damage extension associated with this display. */
	int xDamageEventBase;
	int xDamageErrorBase;
	if (!XDamageQueryExtension(xDisplay, &xDamageEventBase, &xDamageErrorBase))
	{
		printf("\nError: unable to query XDamage extension\n");
		goto cleanup;
	}

	/* Setup to receive damage notification on the main screen */
	/* each time the bounding box increases until it is reset. */
	xDamageScreen =
		XDamageCreate(xDisplay, xRootWindow,
			XDamageReportBoundingBox);
	if (0 == xDamageScreen)
	{
		printf("\nError: unable to setup X damage on display screen\n");
		goto cleanup;
	}

	/* Find the EPDC FB device */
	char fbDevName[10] = "/dev/fb";
	int fbDevNum = 0;
	struct fb_fix_screeninfo fbFixScreenInfo;
	do {
		/* Close previously opened fbdev */
		if (fd >= 0)
		{
			close(fd);
			fd = -1;
		}

		/* Open next fbdev */
		fbDevName[7] = '0' + (fbDevNum++);
		fd = open(fbDevName, O_RDWR, 0);
		if (fd < 0)
		{
			printf("Error in opening fb device\n");
			perror(fbDevName);
			goto cleanup;
		}

		/* Query fbdev fixed screen info. */
		if (0 > ioctl(fd, FBIOGET_FSCREENINFO, &fbFixScreenInfo))
		{
			printf("Error in ioctl(FBIOGET_FSCREENINFFO) call\n");
			perror(fbDevName);
			goto cleanup;
		}

	} while (0 != strcmp(fbFixScreenInfo.id, "mxc_epdc_fb"));
	printf("EPDC fbdev is %s\n", fbDevName);

	/* Query fbdev var screen info. */
	struct fb_var_screeninfo fbVarScreenInfo;
	if (0 > ioctl(fd, FBIOGET_VSCREENINFO, &fbVarScreenInfo))
	{
		printf("Error in ioctl(FBIOGET_VSCREENINFO) call\n");
		perror(fbDevName);
		goto cleanup;
	}

	/* Force EPDC to initialize */
	fbVarScreenInfo.activate = FB_ACTIVATE_FORCE;
	if (0 > ioctl(fd, FBIOPUT_VSCREENINFO, &fbVarScreenInfo))
	{
		printf("Error in ioctl(FBIOPUT_VSCREENINFO) call\n");
		perror(fbDevName);
		goto cleanup;
	}

	/* Put EPDC into region update mode. */
	int mxcfbSetAutoUpdateMode = AUTO_UPDATE_MODE_REGION_MODE;
	if (0 > ioctl(fd, MXCFB_SET_AUTO_UPDATE_MODE, &mxcfbSetAutoUpdateMode))
	{
		printf("Error in ioctl(MXCFB_SET_AUTO_UPDATE_MODE) call\n");
		perror(fbDevName);
		goto cleanup;
	}

	/* Setup waveform modes. */
	struct mxcfb_waveform_modes mxcfbWaveformModes;
	mxcfbWaveformModes.mode_init = 0;
	mxcfbWaveformModes.mode_du = 1;
	mxcfbWaveformModes.mode_gc4 = 3;
	mxcfbWaveformModes.mode_gc8 = 2;
	mxcfbWaveformModes.mode_gc16 = 2;
	mxcfbWaveformModes.mode_gc32 = 2;
	if (0 > ioctl(fd, MXCFB_SET_WAVEFORM_MODES, &mxcfbWaveformModes))
	{
		printf("Error in ioctl(MXCFB_SET_WAVEFORM_MODES) call\n");
		perror(fbDevName);
		goto cleanup;
	}

	printf ("Press ENTER to exit...\n");

	/* Common properties for EPDC screen update */
	struct mxcfb_update_data mxcfbUpdateData;
	mxcfbUpdateData.update_mode = UPDATE_MODE_FULL;
	mxcfbUpdateData.waveform_mode = WAVEFORM_MODE_AUTO;
	mxcfbUpdateData.temp = TEMP_USE_AMBIENT;
	mxcfbUpdateData.use_alt_buffer = 0;
	mxcfbUpdateData.update_marker = 0;

	int numPanelUpdates = 0;
	while (!kbhit())
	{
		int numDamageUpdates = 0;
		struct mxcfb_rect updateRect;
		updateRect.left = 0;
		updateRect.top = 0;
		updateRect.width = 0;
		updateRect.height = 0;

		while (XPending(xDisplay))
		{
			XEvent xEvent;
			XNextEvent(xDisplay, &xEvent);
			if ((XDamageNotify+xDamageEventBase) == xEvent.type)
			{
				XDamageNotifyEvent* xDamageNotifyEvent =
					(XDamageNotifyEvent*)&xEvent;

				updateRect.left = xDamageNotifyEvent->area.x;
				updateRect.top = xDamageNotifyEvent->area.y;
				updateRect.width = xDamageNotifyEvent->area.width;
				updateRect.height = xDamageNotifyEvent->area.height;

				++numDamageUpdates;
			}
		}

		if (numDamageUpdates > 0)
		{
			/* Send accum bound rect updates to EPDC */
			mxcfbUpdateData.update_marker = ++numPanelUpdates;
#if 0
			mxcfbUpdateData.update_region = updateRect;
#else
			mxcfbUpdateData.update_region.left = 0;
			mxcfbUpdateData.update_region.top = 0;
			mxcfbUpdateData.update_region.width = updateRect.width + updateRect.left;
			mxcfbUpdateData.update_region.height = updateRect.height + updateRect.top;
#endif
			if (0 > ioctl(fd, MXCFB_SEND_UPDATE, &mxcfbUpdateData))
			{
				printf("Error in ioctl(MXCFB_SEND_UPDATE) call\n");
				perror(fbDevName);
				goto cleanup;
			}

			/* Display what got updated */
			printf("%d: (x,y)=(%d,%d) (w,h)=(%d,%d)\n",
				numPanelUpdates,
				(int)updateRect.left,
				(int)updateRect.top,
				(int)updateRect.width,
				(int)updateRect.height);

			/* Clear the damage update bounding box */
			XDamageSubtract(xDisplay, xDamageScreen, None, None);

			/* Wait for previous EPDC update to finish */
			if (0 > ioctl(fd, MXCFB_WAIT_FOR_UPDATE_COMPLETE, &mxcfbUpdateData.update_marker))
			{
				printf("Error in ioctl(MXCFB_WAIT_FOR_UPDATE_COMPLETE) call\n");
				perror(fbDevName);
				goto cleanup;
			}
		}

	}

cleanup:
	if (fd >= 0)
	{
		close(fd);
		fd = -1;
	}

	if (0 != xDamageScreen)
	{
		XDamageDestroy(xDisplay, xDamageScreen);
		xDamageScreen = 0;
	}

	if (NULL != xDisplay)
	{
		XCloseDisplay(xDisplay);
		xDisplay = NULL;
	}

	return 0;
}
