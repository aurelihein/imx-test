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

/*
 * @file mxc_isl29023.c
 *
 */

#ifdef __cplusplus
extern "C"{
#endif

/* Standard Include Files */
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <poll.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <linux/isl29023.h>
#include <linux/input.h>

#define TFAIL -1
#define TPASS 0
#define PATH_SIZE 1024

static const char *INPUT_DIR_BASE = "/sys/devices/virtual/input/";

int main(int argc, char **argv)
{
	FILE *fd_mode, *fd_lux, *fd_int_ht_lux, *fd_int_lt_lux;
	FILE *fd_phys;
	int fd_event;
	int lux, int_ht_lux, int_lt_lux;
	int ret, dev_found = 0;
	char buf[6];
	char fd_mode_dev[] = "/sys/class/i2c-dev/i2c-2/device/2-0044/mode";
	char fd_lux_dev[] = "/sys/class/i2c-dev/i2c-2/device/2-0044/lux";
	char fd_int_ht_lux_dev[] = "/sys/class/i2c-dev/i2c-2/device/2-0044/int_ht_lux";
	char fd_int_lt_lux_dev[] = "/sys/class/i2c-dev/i2c-2/device/2-0044/int_lt_lux";
	char fd_event_dev[] = "/dev/input/event2";
	struct pollfd fds;
	struct dirent *dent;
	DIR *inputdir = NULL;
	char *path;
	char phys[6];
	struct input_event event;

	inputdir = opendir(INPUT_DIR_BASE);
	if (inputdir == NULL)
		return TFAIL;

	path = malloc(PATH_SIZE);
	if (!path)
		return TFAIL;
	memset(path, 0, PATH_SIZE);

	while ((dent = readdir(inputdir)) != NULL) {
		if (strcmp(dent->d_name, ".") == 0 ||
		    strcmp(dent->d_name, "..") == 0)
			continue;

		path = strcpy(path, INPUT_DIR_BASE);
		path = strcat(path, dent->d_name);
		path = strcat(path, "/phys");
		if ((fd_phys = fopen(path, "r")) == NULL)
			continue;

		ret = fread(phys, 1, 6, fd_phys);
		if (ret <= 0) {
			fclose(fd_phys);
			continue;
		}

		fclose(fd_phys);

		ret = strncmp("0044", &phys[2], 4);
		if (ret == 0) {
			dev_found = 1;
			break;
		}
	}

	if (!dev_found) {
		printf("Unable to find light sensor device\n");
		free(path);
		return TFAIL;
	}

	/* phys[0] is i2c num */
	fd_mode_dev[23] = fd_mode_dev[32] = phys[0];
	fd_lux_dev[23] = fd_lux_dev[32] = phys[0];
	fd_int_ht_lux_dev[23] = fd_int_ht_lux_dev[32] = phys[0];
	fd_int_lt_lux_dev[23] = fd_int_lt_lux_dev[32] = phys[0];

	/* dent->d_name[5] is event num */
	fd_event_dev[16] = dent->d_name[5];

        if ((fd_mode = fopen(fd_mode_dev, "r+")) == NULL)
        {
                printf("Unable to open %s\n", fd_mode_dev);
		free(path);
                return TFAIL;
        }

	memset(buf, 0, 6);
	snprintf(buf, 6, "%d", ISL29023_ALS_CONT_MODE);
	if ((ret = fwrite(buf, 1, 1, fd_mode)) < 0)
	{
		printf("Unable to write %s\n", fd_mode_dev);
		free(path);
		return TFAIL;
	}

	fclose(fd_mode);

	usleep(1000000);

	while (1) {
		if ((fd_lux = fopen(fd_lux_dev, "r")) == NULL)
		{
			printf("Unable to open %s\n", fd_lux_dev);
			free(path);
			return TFAIL;
		}

		memset(buf, 0, 6);
		if ((ret = fread(buf, 1, 6, fd_lux)) < 0)
		{
			free(path);
			printf("Unable to read %s\n", fd_lux_dev);
		}
		lux = atoi(buf);
		printf("Current light is %d lux\n", lux);

		fclose(fd_lux);

		if ((fd_int_ht_lux = fopen(fd_int_ht_lux_dev, "r+")) == NULL)
		{
			printf("Unable to open %s\n", fd_int_ht_lux_dev);
			free(path);
			return TFAIL;
		}

		if ((fd_int_lt_lux = fopen(fd_int_lt_lux_dev, "r+")) == NULL)
		{
			printf("Unable to open %s\n", fd_int_lt_lux_dev);
			free(path);
			return TFAIL;
		}

		memset(buf, 0, 6);
		if ((ret = fread(buf, 1, 6, fd_int_ht_lux)) < 0)
		{
			free(path);
			printf("Unable to read %s\n", fd_int_ht_lux_dev);
			return TFAIL;
		}
		int_ht_lux = atoi(buf);
		printf("Current int ht is %d lux\n", int_ht_lux);

		memset(buf, 0, 6);
		if ((ret = fread(buf, 1, 6, fd_int_lt_lux)) < 0)
		{
			printf("Unable to read %s\n", fd_int_lt_lux_dev);
			free(path);
			return TFAIL;
		}
		int_lt_lux = atoi(buf);
		printf("Current int lt is %d lux\n", int_lt_lux);

		fclose(fd_int_ht_lux);
		fclose(fd_int_lt_lux);

		int_ht_lux = lux + 100;
		int_lt_lux = lux - 100;
		if (int_lt_lux < 0)
			int_lt_lux = 0;
		printf("Change int ht to %d lux\n", int_ht_lux);
		printf("Change int lt to %d lux\n", int_lt_lux);

		if ((fd_int_ht_lux = fopen(fd_int_ht_lux_dev, "r+")) == NULL)
		{
			printf("Unable to open %s\n", fd_int_ht_lux_dev);
			free(path);
			return TFAIL;
		}

		if ((fd_int_lt_lux = fopen(fd_int_lt_lux_dev, "r+")) == NULL)
		{
			printf("Unable to open %s\n", fd_int_lt_lux_dev);
			free(path);
			return TFAIL;
		}

		memset(buf, 0, 6);
		snprintf(buf, 6, "%d", int_ht_lux);
		if ((ret = fwrite(buf, 1, 6, fd_int_ht_lux)) < 0)
		{
			printf("Unable to write %s\n", fd_int_ht_lux_dev);
			free(path);
			return TFAIL;
		}

		memset(buf, 0, 6);
		snprintf(buf, 6, "%d", int_lt_lux);
		if ((ret = fwrite(buf, 1, 6, fd_int_lt_lux)) < 0)
		{
			printf("Unable to write %s\n", fd_int_lt_lux_dev);
			free(path);
			return TFAIL;
		}

		fclose(fd_int_ht_lux);
		fclose(fd_int_lt_lux);

		if ((fd_event = open(fd_event_dev, O_RDONLY, 0)) < 0)
		{
			printf("Unable to open %s\n", fd_event_dev);
			free(path);
			return TFAIL;
		}

		fds.fd = fd_event;
		fds.events = POLLIN;

		ret = poll(&fds, 1, -1);

		switch (ret) {
		case 0:
			printf("poll timeout\n");
			continue;
		case -1:
			printf("poll error\n");
			break;
		default:
			if (fds.revents & POLLIN) {
				struct input_absinfo absinfo;

				if (read(fd_event, &event, sizeof(event)) < 0) {
					printf("Unable to read %s\n", fd_event_dev);
					free(path);
					return TFAIL;
				}

				if (ioctl(fd_event, EVIOCGABS(ABS_MISC), &absinfo) < 0) {
					printf("Unable to read lux which is out of range\n");
					free(path);
					return TFAIL;
				}
				printf("Lux %d is out of threshold\n\n", event.value);
			}
		}
		close(fd_event);
	}
	free(path);
	return TPASS;
}
