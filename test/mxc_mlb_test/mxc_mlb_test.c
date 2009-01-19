/*
 * Copyright 2008-2009 Freescale Semiconductor, Inc. All rights reserved.
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

#include <asm/arch/mxc_mlb.h>

#define MLB_ASYNC_DEV	"/dev/async"
#define MLB_CTRL_DEV	"/dev/ctrl"

#define vprintf(fmt, args...) \
do {	\
	if (verbose)	\
		printf(fmt, ##args);	\
} while (0);

static int afd, cfd;
static int verbose = 0;
static int tcase = 0;
static unsigned int fps = 512;
static int blocked = 0;

int do_basic_test(void);
int do_txrx_test(int fd);

void print_help(void)
{
	printf("Usage: mxc_mlb_test [-v] [-h] [-b] [-f fps] [-t casetype]\n"
	"\t-v verbose\n\t-h help\n\t-b block io test\n"
	"\t-f FPS, 256/512/1024\n\t-t CASE"
	"\t   CASE can be 'async' or 'ctrl'\n");
}

int main(int argc, char *argv[])
{
	int ret, flags;

	while (1) {
		ret = getopt(argc, argv, "vf:t:hb");

		if (ret == -1)
			break;

		switch (ret) {
		case 'v':
			verbose = 1;
			break;
		case 'b':
			blocked = 1;
			break;
		case 't':
			if (strcmp(optarg, "async") == 0) {
				tcase = 1;
			} else if (strcmp(optarg, "ctrl") == 0) {
				tcase = 0;
			} else {
				fprintf(stderr, "No such case\n");
				print_help();
				return 0;
			}
			break;
		case 'f':
			if (1 == sscanf(optarg, "%d", &fps)) {
				if (fps == 256 || fps == 512 || fps == 1024)
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

	/* open two devices */
	afd = open(MLB_ASYNC_DEV, flags);
	cfd = open(MLB_CTRL_DEV, flags);

	if (cfd <= 0 || afd <= 0) {
		fprintf(stderr, "Failed to open device\n");
		return -1;
	}

	if (do_basic_test()) {
		printf("Basic test failed\n");
		return -1;
	}

	if (tcase) {
		printf("=======================\n");
		printf("Async tx/rx test start\n");
		if (do_txrx_test(afd)) {
			printf("Async tx/rx test failed\n");
			return -1;
		}
		printf("Async tx/rx test PASS\n");
	} else {
		printf("=======================\n");
		printf("Control tx/rx test start\n");
		if (do_txrx_test(cfd)) {
			printf("Control tx/rx test failed\n");
			return -1;
		}
		printf("Control tx/rx test PASS\n\n");
	}

	return 0;
}

int do_basic_test(void)
{
	int ret;
	unsigned long ver;
	unsigned int tfps;
	unsigned char addr = 0xC0;

	/* ioctl check */
	/* get mlb device version */
	ret = ioctl(afd, MLB_GET_VER, &ver);
	if (ret) {
		printf("Get version failed: %d\n", ret);
		return -1;
	}
	printf("MLB device version: 0x%08x\n", (unsigned int)ver);

	/* set mlb device address */
	ret = ioctl(cfd, MLB_SET_DEVADDR, &addr);
	if (ret) {
		printf("Set device address failed %d\n", ret);
		return -1;
	}
	printf("MLB device address: 0x%x\n", addr);

	/* set tfps */
	tfps = 222;
	if (0 == ioctl(cfd, MLB_SET_FPS, &tfps)) {
		printf("Wrong fps setting ??\n");
		return -1;
	}
	tfps = 256;
	ret = ioctl(cfd, MLB_SET_FPS, &tfps);
	if (ret) {
		printf("Set fps to 256 failed: %d\n", ret);
		return -1;
	}
	tfps = 1024;
	ret = ioctl(cfd, MLB_SET_FPS, &tfps);
	if (ret) {
		printf("Set fps 1024 failed: %d\n", ret);
		return -1;
	}
	tfps = 512;
	ret = ioctl(cfd, MLB_SET_FPS, &tfps);
	if (ret) {
		printf("Set fps to 512 failed: %d\n", ret);
		return -1;
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
	}

	ret = ioctl(fd, MLB_CHAN_SETADDR, &evt);
	if (ret) {
		printf("set control channel address failed: %d\n", ret);
		return -1;
	}

	/* channel startup */
	ret = ioctl(fd, MLB_CHAN_STARTUP);
	if (ret) {
		printf("Failed to startup async channel\n");
		return -1;
	}

	if (blocked) {

		while (1) {

			ret = read(fd, buf, 2048);
			if (ret <= 0) {
				printf("Failed to read MLB packet: %s\n", strerror(errno));
			} else {
				vprintf(">> Read MLB packet:\n(length)\n%d\n(data)\n", ret);
				dump_hex(buf, ret);
			}

			ret = write(fd, buf, ret);
			if (ret <= 0) {
				printf("Write MLB packet failed\n");
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
