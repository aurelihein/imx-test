
/*
 * Copyright 2004-2007 Freescale Semiconductor, Inc. All Rights Reserved.
 *
 * Copyright (c) 2006, Chips & Media.  All rights reserved.
 */

/*
 * The code contained herein is licensed under the GNU General Public
 * License. You may obtain a copy of the GNU General Public License
 * Version 2 or later at the following locations:
 *
 * http://www.opensource.org/licenses/gpl-license.html
 * http://www.gnu.org/copyleft/gpl.html
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "vpu_test.h"

void v4l_free_bufs(int n, struct vpu_display *disp)
{
	int i;
	struct v4l_buf *buf;
	for (i = 0; i < n; i++) {
		if (disp->buffers[i] != NULL) {
			buf = disp->buffers[i];
			if (buf->start > 0)
				munmap(buf->start, buf->length);

			free(buf);
			disp->buffers[i] = NULL;
		}
	}
}

static int
calculate_ratio(int width, int height, int maxwidth, int maxheight)
{
	int i, tmp, ratio, compare;

	i = ratio = 1;
	if (width >= height) {
		tmp = width;
		compare = maxwidth;
	} else {
		tmp = height;
		compare = maxheight;
	}

	if (width <= maxwidth && height <= maxheight) {
		ratio = 1;
	} else {
		while (tmp > compare) {
			ratio = (1 << i);
			tmp /= ratio;
			i++;
		}
	}

	return ratio;
}

struct vpu_display * 
v4l_display_open(int width, int height, int nframes, int rot, int stride)
{
	int fd, err, out, i;
	char v4l_device[32] = "/dev/video16";
	struct v4l2_cropcap cropcap = {0};
	struct v4l2_crop crop = {0};
	struct v4l2_framebuffer fb = {0};
	struct v4l2_format fmt = {0};
	struct v4l2_requestbuffers reqbuf = {0};
	struct vpu_display *disp;
	int ratio;

	if (platform_is_mx27()) {
		out = 0;
	} else {
		out = 3;
	}
	
	if (rot) {
		i = width;
		width = height;
		height = i;
	}

	disp = (struct vpu_display *)calloc(1, sizeof(struct vpu_display));
       	if (disp == NULL) {
		printf("falied to allocate vpu_display\n");
		return NULL;
	}

	fd = open(v4l_device, O_RDWR, 0);
	if (fd < 0) {
		printf("unable to open %s\n", v4l_device);
		return NULL;
	}

	err = ioctl(fd, VIDIOC_S_OUTPUT, &out);
	if (err < 0) {
		printf("VIDIOC_S_OUTPUT failed\n");
		goto err;
	}

	cropcap.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
	err = ioctl(fd, VIDIOC_CROPCAP, &cropcap);
	if (err < 0) {
		printf("VIDIOC_CROPCAP failed\n");
		goto err;
	}

	ratio = calculate_ratio(width, height, cropcap.bounds.width, 
				cropcap.bounds.height);

	crop.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
	crop.c.top = 0;
	crop.c.left = 0;
	crop.c.width = width / ratio;
	crop.c.height = height / ratio;

	err = ioctl(fd, VIDIOC_S_CROP, &crop);
	if (err < 0) {
		printf("VIDIOC_S_CROP failed\n");
		goto err;
	}

	if (platform_is_mx27()) {
		fb.capability = V4L2_FBUF_CAP_EXTERNOVERLAY;
		fb.flags = V4L2_FBUF_FLAG_PRIMARY;
	
		err = ioctl(fd, VIDIOC_S_FBUF, &fb);
		if (err < 0) {
			printf("VIDIOC_S_FBUF failed\n");
			goto err;
		}
	}

	fmt.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
	fmt.fmt.pix.width = width;
	fmt.fmt.pix.height = height;
	fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUV420;
	fmt.fmt.pix.bytesperline = stride;

	err = ioctl(fd, VIDIOC_S_FMT, &fmt);
	if (err < 0) {
		printf("VIDIOC_S_FMT failed\n");
		goto err;
	}

	err = ioctl(fd, VIDIOC_G_FMT, &fmt);
	if (err < 0) {
		printf("VIDIOC_G_FMT failed\n");
		goto err;
	}

	reqbuf.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
	reqbuf.memory = V4L2_MEMORY_MMAP;
	reqbuf.count = nframes;

	err = ioctl(fd, VIDIOC_REQBUFS, &reqbuf);
	if (err < 0) {
		printf("VIDIOC_REQBUFS failed\n");
		goto err;
	}

	if (reqbuf.count < nframes) {
		printf("VIDIOC_REQBUFS: not enough buffers\n");
		goto err;
	}

	for (i = 0; i < nframes; i++) {
		struct v4l2_buffer buffer = {0};
		struct v4l_buf *buf;

		buf = calloc(1, sizeof(struct v4l_buf));
		if (buf == NULL) {
			v4l_free_bufs(i, disp);
			goto err;
		}
	        	
		disp->buffers[i] = buf;
		
		buffer.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
		buffer.memory = V4L2_MEMORY_MMAP;
		buffer.index = i;

		err = ioctl(fd, VIDIOC_QUERYBUF, &buffer);
		if (err < 0) {
			printf("VIDIOC_QUERYBUF: not enough buffers\n");
			v4l_free_bufs(i, disp);
			goto err;
		}

		buf->offset = buffer.m.offset;
		buf->length = buffer.length;
		buf->start = mmap(NULL, buffer.length, PROT_READ | PROT_WRITE,
				MAP_SHARED, fd, buffer.m.offset);
		
		if (buf->start == MAP_FAILED) {
			printf("mmap failed\n");
			v4l_free_bufs(i, disp);
			goto err;
		}

	}

	disp->fd = fd;
	disp->nframes = nframes;

	return disp;
err:
	close(fd);
	free(disp);
	return NULL;
}

void v4l_display_close(struct vpu_display *disp)
{
	int type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
	ioctl(disp->fd, VIDIOC_STREAMOFF, &type);
	v4l_free_bufs(disp->nframes, disp);
	close(disp->fd);	
	free(disp);
}

int v4l_put_data(struct vpu_display *disp)
{
	struct timeval tv;
	int err, type;

	if (disp->ncount == 0) {
		gettimeofday(&tv, 0);
		disp->buf.timestamp.tv_sec = tv.tv_sec;
		disp->buf.timestamp.tv_usec = tv.tv_usec;

		disp->sec = tv.tv_sec;
		disp->usec = tv.tv_usec;
	}

	disp->buf.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
	disp->buf.memory = V4L2_MEMORY_MMAP;
	if (disp->ncount < disp->nframes) {
		disp->buf.index = disp->ncount;
		err = ioctl(disp->fd, VIDIOC_QUERYBUF, &disp->buf);
		if (err < 0) {
			printf("VIDIOC_QUERYBUF failed\n");
			goto err;
		}
	} else {
		disp->buf.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
		disp->buf.memory = V4L2_MEMORY_MMAP;
		err = ioctl(disp->fd, VIDIOC_DQBUF, &disp->buf);
		if (err < 0) {
			printf("VIDIOC_DQBUF failed\n");
			goto err;
		}
	}

	if (disp->ncount > 0) {
		disp->usec += (1000000 / 30);
		if (disp->usec >= 1000000) {
			disp->sec += 1;
			disp->usec -= 1000000;
		}
		
		disp->buf.timestamp.tv_sec = disp->sec;
		disp->buf.timestamp.tv_usec = disp->usec;

	}

	err = ioctl(disp->fd, VIDIOC_QBUF, &disp->buf);
	if (err < 0) {
		printf("VIDIOC_QBUF failed\n");
		goto err;
	}

	if (disp->ncount == 1) {
		type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
		err = ioctl(disp->fd, VIDIOC_STREAMON, &type);
		if (err < 0) {
			printf("VIDIOC_STREAMON failed\n");
			goto err;
		}
	}

	disp->ncount++;
	return 0;

err:
	return -1;	
}

