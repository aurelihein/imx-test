/*
 * Copyright 2009 Freescale Semiconductor, Inc. All rights reserved.
 */

/*
 * The code contained herein is licensed under the GNU General Public
 * License. You may obtain a copy of the GNU General Public License
 * Version 2 or later at the following locations:
 *
 * http://www.opensource.org/licenses/gpl-license.html
 * http://www.gnu.org/copyleft/gpl.html
 */

/*
 * @file csi_v4l2_capture.c
 *
 * @brief Mx25 Video For Linux 2 driver test application
 *
 */

#ifdef __cplusplus
extern "C"{
#endif

/*=======================================================================
                                        INCLUDE FILES
=======================================================================*/
/* Standard Include Files */
#include <errno.h>

/* Verification Test Environment Include Files */
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <asm/types.h>
#include <linux/videodev.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <string.h>
#include <malloc.h>

#define TEST_BUFFER_NUM 3

struct testbuffer
{
        unsigned char *start;
        size_t offset;
        unsigned int length;
};

struct testbuffer buffers[TEST_BUFFER_NUM];
int g_width = 640;
int g_height = 480;
int g_cap_fmt = V4L2_PIX_FMT_RGB565;
int g_capture_mode = 0;
int g_timeout = 10;

int start_capturing(int fd_v4l)
{
        unsigned int i;
        struct v4l2_buffer buf;
        enum v4l2_buf_type type;

        for (i = 0; i < TEST_BUFFER_NUM; i++)
        {
                memset(&buf, 0, sizeof (buf));
                buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
                buf.index = i;
                if (ioctl(fd_v4l, VIDIOC_QUERYBUF, &buf) < 0)
                {
                        printf("VIDIOC_QUERYBUF error\n");
                        return -1;
                }

                buffers[i].length = buf.length;
                buffers[i].offset = (size_t) buf.m.offset;
                buffers[i].start = mmap (NULL, buffers[i].length,
                    			 PROT_READ | PROT_WRITE, MAP_SHARED,
                    			 fd_v4l, buffers[i].offset);
		memset(buffers[i].start, 0xFF, buffers[i].length);
        }

        for (i = 0; i < TEST_BUFFER_NUM; i++)
        {
                memset(&buf, 0, sizeof (buf));
                buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
                buf.memory = V4L2_MEMORY_MMAP;
                buf.index = i;
		buf.m.offset = buffers[i].offset;

                if (ioctl (fd_v4l, VIDIOC_QBUF, &buf) < 0) {
                        printf("VIDIOC_QBUF error\n");
                        return -1;
                }
        }

        type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        if (ioctl (fd_v4l, VIDIOC_STREAMON, &type) < 0) {
                printf("VIDIOC_STREAMON error\n");
                return -1;
        }

        return 0;
}

int stop_capturing(int fd_v4l)
{
        enum v4l2_buf_type type;

        type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        return ioctl (fd_v4l, VIDIOC_STREAMOFF, &type);
}

int v4l_capture_setup(void)
{
        char v4l_device[100] = "/dev/video0";
        struct v4l2_format fmt;
        struct v4l2_streamparm parm;
        int fd_v4l = 0;

        if ((fd_v4l = open(v4l_device, O_RDWR, 0)) < 0)
        {
                printf("Unable to open %s\n", v4l_device);
                return 0;
        }

	parm.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	parm.parm.capture.capturemode = g_capture_mode;
	if (ioctl(fd_v4l, VIDIOC_S_PARM, &parm) < 0)
	{
	        printf("VIDIOC_S_PARM failed\n");
	        return -1;
	}

	memset(&fmt, 0, sizeof(fmt));
        fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	fmt.fmt.pix.pixelformat = g_cap_fmt;
        fmt.fmt.pix.width = g_width;
        fmt.fmt.pix.height = g_height;
        if (ioctl(fd_v4l, VIDIOC_S_FMT, &fmt) < 0)
        {
                printf("set format failed\n");
                return 0;
        }

        struct v4l2_requestbuffers req;
        memset(&req, 0, sizeof (req));
        req.count = TEST_BUFFER_NUM;
        req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        req.memory = V4L2_MEMORY_MMAP;

        if (ioctl(fd_v4l, VIDIOC_REQBUFS, &req) < 0)
        {
                printf("v4l_capture_setup: VIDIOC_REQBUFS failed\n");
                return 0;
        }

        return fd_v4l;
}

int v4l_capture_test(int fd_v4l)
{
        struct v4l2_buffer buf;
        struct v4l2_format fmt;
	char fb_device[100] = "/dev/fb0";
	int fd_fb = 0;
	int frame_num = 0, i;
	unsigned short * fb0;
	struct timeval tv1, tv2;

	if ((fd_fb = open(fb_device, O_RDWR )) < 0) {
		printf("Unable to open frame buffer\n");
	}

        fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        if (ioctl(fd_v4l, VIDIOC_G_FMT, &fmt) < 0)
        {
                printf("get format failed\n");
                return -1;
        } else {
		printf("\t Width = %d", fmt.fmt.pix.width);
		printf("\t Height = %d", fmt.fmt.pix.height);
		printf("\t Image size = %d\n", fmt.fmt.pix.sizeimage);
        }

        if (start_capturing(fd_v4l) < 0)
        {
                printf("start_capturing failed\n");
                return -1;
        }

	 /* Map the device to memory*/
	fb0 = (unsigned short *)mmap(0, buffers[1].length,
					PROT_READ | PROT_WRITE, MAP_SHARED, fd_fb, 0);
	if ((int)fb0 == -1)
	{
		printf("Error: failed to map framebuffer device 0 to memory.\n");
		close(fd_fb);
	}

	gettimeofday(&tv1, NULL);
	do {
                memset(&buf, 0, sizeof (buf));
                buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
                buf.memory = V4L2_MEMORY_MMAP;
                if (ioctl (fd_v4l, VIDIOC_DQBUF, &buf) < 0) {
                        printf("VIDIOC_DQBUF failed.\n");
                }

        	for (i = 0; i < TEST_BUFFER_NUM; i++) {
			if (buf.m.userptr == buffers[i].offset)
				memcpy(fb0, buffers[i].start, buffers[i].length);
		}

		if (ioctl (fd_v4l, VIDIOC_QBUF, &buf) < 0) {
			printf("VIDIOC_QBUF failed\n");
		}

		frame_num += 1;
		gettimeofday(&tv2, NULL);
	} while(tv2.tv_sec - tv1.tv_sec < g_timeout);

        if (stop_capturing(fd_v4l) < 0)
        {
                printf("stop_capturing failed\n");
                return -1;
        }

        close(fd_v4l);
        return 0;
}


int process_cmdline(int argc, char **argv)
{
        int i;

        for (i = 1; i < argc; i++) {
		if (strcmp(argv[i], "-t") == 0) {
			g_timeout = atoi(argv[++i]);
		} else if (strcmp(argv[i], "-help") == 0) {
                        printf("CSI Video4Linux capture Device Test\n" \
                               "Syntax: ./csi_v4l2_capture -t <time>\n");
                        return -1;
               } else {
                        return -1;
	       }
        }

        return 0;
}

int main(int argc, char **argv)
{
        int fd_v4l;

        if (process_cmdline(argc, argv) < 0) {
                return -1;
        }

        fd_v4l = v4l_capture_setup();
	v4l_capture_test(fd_v4l);

	return 0;
}
