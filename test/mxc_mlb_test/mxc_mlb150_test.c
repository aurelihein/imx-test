/*
 * Copyright (C) 2011 Freescale Semiconductor, Inc. All rights reserved.
 */

/*
 * The code contained herein is licensed under the GNU General Public
 * License. You may obtain a copy of the GNU General Public License
 * Version 2 or later at the following locations:
 *
 * http://www.opensource.org/licenses/gpl-license.html
 * http://www.gnu.org/copyleft/gpl.html
 */
/**
 * MLB driver unit test file
 */
#include <stdlib.h>
#include <stdio.h>
#include <sys/select.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

//#include <linux/mxc_mlb.h>
#include "mxc_mlb.h"

#define MLB_SYNC_DEV	"/dev/sync"
#define MLB_CTRL_DEV	"/dev/ctrl"
#define MLB_ASYNC_DEV	"/dev/async"
#define MLB_ISOC_DEV	"/dev/isoc"

#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))

#define vprintf(fmt, args...) \
do {	\
	if (verbose)	\
		printf(fmt, ##args);	\
} while (0);

static int m_fd;
static int verbose = 0;
static unsigned int fps = 512;
static int blocked = 0;

static unsigned int fps_set[] = {
	256,
	512,
	1024,
	2048,
	3072,
	4096,
	6144,
};

int do_basic_test(int fd);
int do_txrx_test(int fd);

void print_help(void)
{
	printf("Usage: mxc_mlb150_test [-v] [-h] [-b] [-f fps] [-t casetype]\n"
	"\t-v verbose\n\t-h help\n\t-b block io test\n"
	"\t-f FPS, 256/512/1024/2048/3072/4096/6144\n\t-t CASE"
	"\t   CASE can be 'sync', 'ctrl', 'async', 'isoc'\n");
}

int main(int argc, char *argv[])
{
	int ret, flags, t_case;
	int fd_set[4] = { 0 };
	char test_case_str[10] = { 0 };

	while (1) {
		ret = getopt(argc, argv, "vf:t:hb");

		if (1 == argc) {
			print_help();
			return 0;
		}

		if (-1 == ret)
			break;

		switch (ret) {
		case 'v':
			verbose = 1;
			break;
		case 'b':
			blocked = 1;
			break;
		case 't':
			if (strcmp(optarg, "sync") == 0) {
				t_case = 0;
			} else if (strcmp(optarg, "ctrl") == 0) {
				t_case = 1;
			} else if (strcmp(optarg, "async") == 0) {
				t_case = 2;
			}else if (strcmp(optarg, "isoc") == 0) {
				t_case = 3;
			} else {
				fprintf(stderr, "No such case\n");
				print_help();
				return 0;
			}
			strncpy(test_case_str, optarg, 5);
			break;
		case 'f':
			if (1 == sscanf(optarg, "%d", &fps)) {
				if (fps == 256 || fps == 512 || fps == 1024 ||
					fps == 2048 || fps == 3072 || fps == 4096 ||
					fps == 6144)
					break;
			}
			fprintf(stderr, "invalid input frame rate\n");
			return -1;
		case 'h':
			print_help();
			return 0;
		default:
			fprintf(stderr, "invalid command line\n");
			return -1;
		}
	}

	flags = O_RDWR;
	if (!blocked)
		flags |= O_NONBLOCK;

	switch (t_case) {
	case 0:
		m_fd = open(MLB_SYNC_DEV, flags);
		break;
	case 1:
		m_fd = open(MLB_CTRL_DEV, flags);
		break;
	case 2:
		m_fd = open(MLB_ASYNC_DEV, flags);
		break;
	case 3:
		m_fd = open(MLB_ISOC_DEV, flags);
		break;
	default:
		break;
	}

	if (m_fd <= 0) {
		fprintf(stderr, "Failed to open device\n");
		return -1;
	}

	if (do_basic_test(m_fd)) {
		printf("Basic test failed\n");
		return -1;
	}

	printf("=======================\n");
	ret = 0;
	printf("%s tx/rx test start\n", test_case_str);
	if (do_txrx_test(m_fd))
		ret = -1;
	printf("%s tx/rx test %s\n", test_case_str, (ret ? "failed" : "PASS"));

	return ret;
}

int do_basic_test(int fd)
{
	int i, ret;
	unsigned long ver;
	unsigned int tfps;
	unsigned char addr = 0xC0;

	/* ioctl check */
	/* get mlb device version */
	ret = ioctl(fd, MLB_GET_VER, &ver);
	if (ret) {
		printf("Get version failed: %d\n", ret);
		return -1;
	}
	printf("MLB device version: 0x%08x\n", (unsigned int)ver);

	/* set mlb device address */
	ret = ioctl(fd, MLB_SET_DEVADDR, &addr);
	if (ret) {
		printf("Set device address failed %d\n", ret);
		return -1;
	}
	printf("MLB device address: 0x%x\n", addr);

	/* set tfps */
	tfps = 222;
	if (0 == ioctl(fd, MLB_SET_FPS, &tfps)) {
		printf("Wrong fps setting ?\n");
		return -1;
	}

	for (i = 0; i < ARRAY_SIZE(fps_set); ++i) {
		tfps = fps_set[i];

		ret = ioctl(fd, MLB_SET_FPS, &tfps);
		if (ret) {
			printf("Set fps to %d failed: %d\n", tfps, ret);
			return -1;
		}
	}

	return 0;
}

void dump_hex(const char *buf, int len)
{
	int i;
	for (i = 0; i < len; i++) {
		vprintf("%02x ", *(buf + i));
		if ((i+1) % 8 == 0)
			vprintf("\n");
	}
	if (i % 8 != 0)
		vprintf("\n");
}

int do_txrx_test(int fd)
{
	int ret, packets = 200;
	fd_set rdfds, wtfds;
	unsigned long evt;
	char buf[2048];
	int gotlen = 0;

	/**
	 * set channel address
	 * MSB is for tx, LSB is for rx
	 * the normal address range [0x02, 0x3E]
	 */

	ret = ioctl(fd, MLB_SET_FPS, &fps);
	if (ret) {
		printf("Set fps to 512 failed: %d\n", ret);
		return -1;
	}
	printf("Set fps to %d\n", fps);

	switch (fps) {

	case 256:
		evt = 0x02 << 16 | 0x01;
		break;
	case 512:
		evt = 0x04 << 16 | 0x03;
		break;
	case 1024:
		evt = 0x06 << 16 | 0x05;
		break;
	case 2048:
		evt = 0x08 << 16 | 0x07;
		break;
	case 3072:
		evt = 0x10 << 16 | 0x09;
		break;
	case 4096:
		evt = 0x12 << 16 | 0x11;
		break;
	case 6144:
		evt = 0x14 << 16 | 0x13;
		break;
	default:
		break;
	}

	ret = ioctl(fd, MLB_CHAN_SETADDR, &evt);
	if (ret) {
		printf("set control channel address failed: %d\n", ret);
		return -1;
	}

	/* channel startup */
	ret = ioctl(fd, MLB_CHAN_STARTUP);
	if (ret) {
		printf("Failed to startup channel\n");
		return -1;
	}

	if (blocked) {

		while (1) {

			ret = read(fd, buf, 2048);
			ret = 64;
			if (ret <= 0) {
				printf("Failed to read MLB packet: %s\n", strerror(errno));
			} else {
				vprintf(">> Read MLB packet:\n(length)\n%d\n(data)\n", ret);
				dump_hex(buf, ret);
			}

			ret = write(fd, buf, ret);
			if (ret <= 0) {
				printf("Write MLB packet failed %d: %s\n", ret, strerror(errno));
			} else {
				vprintf("<< Send this received MLB packet back\n");
				packets --;
			}

			if (!packets)
				break;
		}

		goto shutdown;
	}

	while (1) {

		FD_ZERO(&rdfds);
		FD_ZERO(&wtfds);

		FD_SET(fd, &rdfds);
		FD_SET(fd, &wtfds);

		ret = select(fd + 1, &rdfds, &wtfds, NULL, NULL);

		if (ret < 0) {
			printf("Select failed: %d\n", ret);
			return -1;
		} else if (ret == 0) {
			continue;
		}

		if (FD_ISSET(fd, &rdfds)) {
			/* check event */
			ret = ioctl(fd, MLB_CHAN_GETEVENT, &evt);
			if (ret == 0) {
				vprintf("## Get event: %x\n", (unsigned int)evt);
			} else {
				if (errno != EAGAIN) {
					printf("Failed to get event:%s\n", strerror(errno));
					return -1;
				}
			}

			/* check read */
			ret = read(fd, buf, 2048);
			if (ret <= 0) {
				if (errno != EAGAIN)
					printf("Failed to read MLB packet: %s\n", strerror(errno));
			} else {
				vprintf(">> Read MLB packet:\n(length)\n%d\n(data)\n", ret);
				dump_hex(buf, ret);
				gotlen = ret;
			}
		}

		/* check write */
		if (FD_ISSET(fd, &wtfds) && gotlen) {
			ret = write(fd, buf, gotlen);
			if (ret <= 0) {
				printf("Write MLB packet failed\n");
			} else {
				vprintf("<< Send this received MLB packet back\n");
				packets --;
			}
			gotlen = 0;
		}

		if (!packets)
			break;

	}

shutdown:

	ret = ioctl(fd, MLB_CHAN_SHUTDOWN);
	if (ret) {
		printf("Failed to shutdown async channel\n");
		return -1;
	}

	return 0;
}
