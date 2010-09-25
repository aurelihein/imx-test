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

#include <stdarg.h>
#include <termio.h>
#include <sys/timeb.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>
#include <stdlib.h>


#define DEFAULT_RATE 115200;

unsigned int get_time(void)
{
    struct timeb tb;
    unsigned int realNow;

    ( void ) ftime( &tb );

    realNow = (unsigned int )(((tb.time%4290) * 1000000 ) + ( tb.millitm * 1000 ));

    return(realNow);
}

static speed_t baudrate_map(unsigned long b)
{
    speed_t retval;

    switch(b)
    {
        case 110:
            retval = B110;
            break;

        case 300:
            retval = B300;
            break;

        case 1200:
            retval = B1200;
            break;

        case 2400:
            retval = B2400;
            break;

        case 4800:
            retval = B4800;
            break;

        case 9600:
            retval = B9600;
            break;

        case 19200:
            retval = B19200;
            break;

        case 38400:
            retval = B38400;
            break;

        case 57600:
            retval = B57600;
            break;

        case 115200:
            retval = B115200;
            break;

#ifdef B230400
        case 230400:
            retval = B230400;
            break;
#endif

#ifdef B460800
        case 460800:
            retval = B460800;
            break;
#endif

#ifdef B500000
        case 500000:
            retval = B500000;
            break;
#endif

#ifdef B576000
        case 576000:
            retval = B576000;
            break;
#endif

#ifdef B921600
        case 921600:
            retval = B921600;
            break;
#endif

#ifdef B1000000
        case 1000000:
            retval = B1000000;
            break;
#endif

#ifdef B1152000
        case 1152000:
            retval = B1152000;
            break;
#endif

#ifdef B1500000
        case 1500000:
            retval = B1500000;
            break;
#endif

#ifdef B2000000
        case 2000000:
            retval = B2000000;
            break;
#endif

#ifdef B2500000
        case 2500000:
            retval = B2500000;
            break;
#endif

#ifdef B3000000
        case 3000000:
            retval = B3000000;
            break;
#endif

#ifdef B3500000
        case 3500000:
            retval = B3500000;
            break;
#endif

#ifdef B4000000
        case 4000000:
            retval = B4000000;
            break;
#endif

        default:
            retval = 0;
            break;
    }

    return(retval);
}

int trun, rrun;
int fd;
int size = 10000;
unsigned int tcount, rcount;
FILE *furead;

void *Uartsend(void * threadParameter)
{
	int *tx;
	double speed;
	double time;
	tcount = 0;

	tx = malloc(size);
	memset(tx, 0x0f, size);
	while(trun) {
		sleep(1);
		tx[0] = 0x55;
		time = get_time();
		write(fd, tx, size);
		time = get_time() - time;
		speed = size * 8 / (time/1000000);
		tcount += size;
		printf("sent %d bytes with speed %fbps\n", size, speed);
	}
	free(tx);
	return 0;
}

void *Uartread(void * threadParameter)
{
	char *rx;
	int iores, iocount;
	rcount = 0;

	while(rrun) {
		iocount = 0;
		iores = ioctl(fd, FIONREAD, &iocount);
		if(!iocount)
			continue;
                rx = malloc(iocount);
		/* Read in and wrap around the list */
                iores = read(fd, rx, iocount);
		rcount += iores;
		fwrite(rx, 1, iores, furead);
		free(rx);
	}
	return 0;
}

static void print_usage(const char *pname)
{
	printf("Usage: %s device [-S] [-O] [-E] [-HW] [-B baudrate]"
	       "\n\t'-S' for 2 stop bit"
	       "\n\t'-O' for PARODD "
	       "\n\t'-E' for PARENB"
	       "\n\t'-HW' for HW flow control enable"
	       "\n\t'-B baudrate' for different baudrate\n", pname);

}

int main(int argc, char *argv[])
{
	int i, ret;
	struct termios options;
	unsigned long baudrate = DEFAULT_RATE;
	char c = 0;
	pthread_t p_Uartsend, p_Uartread;
	void *thread_res;

	if (argc < 2 || strncmp(argv[1], "/dev/ttymxc", 11)) {
		print_usage(argv[0]);
		return -1;
	}

	fd = open(argv[1], O_RDWR | O_NOCTTY);

	if (fd == -1) {
		printf("open_port: Unable to open serial port - %s", argv[1]);
		return -1;
	}

	fcntl(fd, F_SETFL, 0);

	tcgetattr(fd, &options);
	options.c_cflag &= ~CSTOPB;
	options.c_cflag &= ~CSIZE;
	options.c_cflag |= PARENB;
	options.c_cflag &= ~PARODD;
	options.c_cflag |= CS8;
	options.c_cflag &= ~CRTSCTS;

	options.c_lflag &= ~(ICANON | IEXTEN | ISIG | ECHO);
	options.c_oflag &= ~OPOST;
	options.c_iflag &= ~(ICRNL | INPCK | ISTRIP | IXON | BRKINT );

	options.c_cc[VMIN] = 1;
	options.c_cc[VTIME] = 0;

	options.c_cflag |= (CLOCAL | CREAD);
	for(i = 2; i < argc; i++) {
		if (!strcmp(argv[i], "-S")) {
			options.c_cflag |= CSTOPB;
			continue;
		}
		if (!strcmp(argv[i], "-O")) {
			options.c_cflag |= PARODD;
			options.c_cflag &= ~PARENB;
			continue;
		}
		if (!strcmp(argv[i], "-E")) {
			options.c_cflag &= ~PARODD;
			options.c_cflag |= PARENB;
			continue;
		}
		if (!strcmp(argv[i], "-HW")) {
			options.c_cflag |= CRTSCTS;
			continue;
		}
		if (!strcmp(argv[i], "-B")) {
			i++;
			baudrate = atoi(argv[i]);
			if(!baudrate_map(baudrate))
				baudrate = DEFAULT_RATE;
			continue;
		}
	}
	if(baudrate) {
		cfsetispeed(&options, baudrate_map(baudrate));
		cfsetospeed(&options, baudrate_map(baudrate));
	}
	tcsetattr(fd, TCSANOW, &options);
	printf("UART %lu, %dbit, %dstop, %s, HW flow %s\n", baudrate, 8,
	       (options.c_cflag & CSTOPB) ? 2 : 1,
	       (options.c_cflag & PARODD) ? "PARODD" : "PARENB",
	       (options.c_cflag & CRTSCTS) ? "enabled" : "disabled");

	trun = 1;
	rrun = 1;
	furead = fopen("uart_read.txt","wb");

	ret = pthread_create(&p_Uartsend, NULL, Uartsend, NULL);
	if(ret < 0)
		goto error;
	ret = pthread_create(&p_Uartread, NULL, Uartread, NULL);
	if(ret < 0) {
		ret = pthread_join(p_Uartsend, &thread_res);
		goto error;
	}

	printf("test begin, press 'c' to exit\n");

	while(c != 'c'){
		c = getchar();
	}

	trun = 0;
	ret = pthread_join(p_Uartsend, &thread_res);
	if(ret < 0)
		printf("fail to stop Uartsend thread\n");
	printf("tcount=%d\n",tcount);
	i = 5;
	while((tcount > rcount) && i) {
		i--;
		sleep(1);
	}
	rrun = 0;
	ret = pthread_join(p_Uartread, &thread_res);
	if(ret < 0)
		printf("fail to stop Uartread thread\n");
	printf("rcount=%d\n",rcount);
error:
	fclose(furead);
	close(fd);
	printf("test exit\n");

	return 0;
}
