/*
 * Copyright (C) 2010 Freescale Semiconductor, Inc. All rights reserved.
 * Author: Shrek Wu <b16972@freescale.com>
 *
 * Testing code for Modelo and i.MX  L2 switch driver.
 *
 * This is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <sys/ioctl.h>
#include <net/if.h>
#include "mxc_l2switch_test.h"

#define SIOCGMIIPHY 0x8947
#define CMD_MAX_LEN 128
#define PORT_MAX_LEN 1
#define PORTDATA_MAX_LEN 1
static void usage(void)
{
	printf("usage : ipconfig interface \n");
	exit(0);
}

/*--------------------------------------------------------*/
/*parm 0  dev
 *parm 1  cmd
 *parm 2  real parm 1
 */
int main(int argc, char **argv)
{
	struct ifreq ifr;
	char *name = NULL;
	unsigned long parm1 = 0, parm1_len = 0;
	unsigned long parm2 = 0, parm2_len = 0;
	unsigned long parm3 = 0, parm3_len = 0;
	unsigned long parm4 = 0, parm4_len = 0;
	unsigned long parm5 = 0;
	unsigned long parm6 = 0, parm6_len = 0;
	unsigned long parm7 = 0;
	unsigned long parm8 = 0;
	unsigned long parm9 = 0;
	unsigned long parm10_len = 0;
	unsigned long parm11_len = 0;
	unsigned long parm12 = 0;

	unsigned long configData;
	eswIoctlPortConfig PortConfig;
	eswIoctlPortEnableConfig PortEnableConfig;
	eswIoctlIpsnoopConfig IpSnoopConfig;
	eswIoctlPortsnoopConfig PortsnoopConfig;
	eswIoctlPortMirrorConfig PortMirrorConfig;
	eswIoctlP0ForcedForwardConfig P0ForcedForwardConfig;
	unsigned char mac0[6];
	unsigned char mac1[6];
	unsigned long ESW_IPSNP[8];
	unsigned long ESW_PSNP[8];
	esw_statistics_status Statistics_status;
	esw_port_statistics_status Port_statistics_status;
	esw_output_queue_status Output_queue_status;
	eswIoctlPortMirrorStatus PortMirrorStatus;
	eswIoctlVlanOutputConfig VlanOutputConfig;
	eswIoctlVlanInputConfig VlanInputConfig;
	eswIoctlVlanVerificationConfig VlanVerificationConfig;
	eswIoctlVlanResoultionTable VlanResoultionTable;
	eswIoctlVlanInputStatus VlanInputStatus;
	eswIoctlUpdateStaticMACtable UpdateStaticMACtable;

	int sockfd;

	int i, k;

	if (argc < 3)
		usage();
	else
		name = argv[1];

	/*========================================================*/
	/* get the length of the command */
	for (i = 0; i < (CMD_MAX_LEN + 1); i++) {
		if (argv[2][i] == 0) {
			parm1_len = i;
			if (parm1_len == 0) {
				printf("input no the second parm\n");
				exit(0);
			}
			break;
		}
	}

	if (i == CMD_MAX_LEN + 1) {
		printf("the second parm  is too long\n");
		exit(0);
	}

#if 0
	for (i = 0; i < parm1_len ; i++) {
		if (((argv[2][i] > '0') || (argv[2][i] == '0'))
			&& ((argv[2][i] < '9') ||
				(argv[2][i] == '9'))) {
			parm1 += (argv[2][i] - '0') <<
				((parm1_len - 1 - i) * 4);

		} else if (((argv[2][i] > 'a') || (argv[2][i] == 'a'))
			&& ((argv[2][i] < 'f') ||
				(argv[2][i] == 'f'))) {
			parm1 += (argv[2][i] - 'a' + 10) <<
				((parm1_len - 1 - i) * 4);
		} else if (((argv[2][i] > 'A') || (argv[2][i] == 'A'))
			&& ((argv[2][i] < 'F') ||
				(argv[2][i] == 'F'))) {
			parm1 += (argv[2][i] - 'A' + 10) <<
				((parm1_len - 1 - i) * 4);
		} else {
			printf("the second parm is input error\n");
			exit(0);
		}
	}
#endif
	if (strcmp(argv[2], "setlearning") == 0)
		parm1 = ESW_SET_LEARNING_CONF;
	else if (strcmp(argv[2], "getlearning") == 0)
		parm1 = ESW_GET_LEARNING_CONF;
	else if (strcmp(argv[2], "setblocking") == 0)
		parm1 = ESW_SET_BLOCKING_CONF;
	else if (strcmp(argv[2], "getblocking") == 0)
		parm1 = ESW_GET_BLOCKING_CONF;
	else if (strcmp(argv[2], "setmutilcast") == 0)
		parm1 = ESW_SET_MULTICAST_CONF;
	else if (strcmp(argv[2], "getmutilcast") == 0)
		parm1 = ESW_GET_MULTICAST_CONF;
	else if (strcmp(argv[2], "setbroadcast") == 0)
		parm1 = ESW_SET_BROADCAST_CONF;
	else if (strcmp(argv[2], "getbroadcast") == 0)
		parm1 = ESW_GET_BROADCAST_CONF;
	else if (strcmp(argv[2], "setswtichmode") == 0)
		parm1 = ESW_SET_SWITCH_MODE;
	else if (strcmp(argv[2], "getswtichmode") == 0)
		parm1 = ESW_GET_SWITCH_MODE;
	else if (strcmp(argv[2], "setbridgecfg") == 0)
		parm1 = ESW_SET_BRIDGE_CONFIG;
	else if (strcmp(argv[2], "getbridgecfg") == 0)
		parm1 = ESW_GET_BRIDGE_CONFIG;
	else if (strcmp(argv[2], "setportenable") == 0)
		parm1 = ESW_SET_PORTENABLE_CONF;
	else if (strcmp(argv[2], "getportenable") == 0)
		parm1 = ESW_GET_PORTENABLE_CONF;
	else if (strcmp(argv[2], "setp0forward") == 0)
		parm1 = ESW_SET_P0_FORCED_FORWARD;
	else if (strcmp(argv[2], "getp0forward") == 0)
		parm1 = ESW_GET_P0_FORCED_FORWARD;
	else if (strcmp(argv[2], "setipsnoop") == 0)
		parm1 = ESW_SET_IP_SNOOP_CONF;
	else if (strcmp(argv[2], "getipsnoop") == 0)
		parm1 = ESW_GET_IP_SNOOP_CONF;
	else if (strcmp(argv[2], "setportsnoop") == 0)
		parm1 = ESW_SET_PORT_SNOOP_CONF;
	else if (strcmp(argv[2], "getportsnoop") == 0)
		parm1 = ESW_GET_PORT_SNOOP_CONF;
	else if (strcmp(argv[2], "setportmirror") == 0)
		parm1 = ESW_SET_PORT_MIRROR_CONF;
	else if (strcmp(argv[2], "getportmirror") == 0)
		parm1 = ESW_GET_PORT_MIRROR_CONF;
	else if (strcmp(argv[2], "setvlanoutput") == 0)
		parm1 = ESW_SET_VLAN_OUTPUT_PROCESS;
	else if (strcmp(argv[2], "getvlanoutput") == 0)
		parm1 = ESW_GET_VLAN_OUTPUT_PROCESS;
	else if (strcmp(argv[2], "setvlaninput") == 0)
		parm1 = ESW_SET_VLAN_INPUT_PROCESS;
	else if (strcmp(argv[2], "getvlaninput") == 0)
		parm1 = ESW_GET_VLAN_INPUT_PROCESS;
	else if (strcmp(argv[2], "setresoutionvlan") == 0)
		parm1 = ESW_SET_VLAN_RESOLUTION_TABLE;
	else if (strcmp(argv[2], "getresoutionvlan") == 0)
		parm1 = ESW_GET_VLAN_RESOLUTION_TABLE;
	else if (strcmp(argv[2], "setdomainvlanverify") == 0)
		parm1 = ESW_SET_VLAN_DOMAIN_VERIFICATION;
	else if (strcmp(argv[2], "getdomainvlanverify") == 0)
		parm1 = ESW_GET_VLAN_DOMAIN_VERIFICATION;
	else if (strcmp(argv[2], "setmacstatictable") == 0)
		parm1 = ESW_UPDATE_STATIC_MACTABLE;
	else if (strcmp(argv[2], "getstatistics") == 0)
		parm1 = ESW_GET_STATISTICS_STATUS;
	else if (strcmp(argv[2], "getstatusp0") == 0)
		parm1 = ESW_GET_PORT0_STATISTICS_STATUS;
	else if (strcmp(argv[2], "getstatusp1") == 0)
		parm1 = ESW_GET_PORT1_STATISTICS_STATUS;
	else if (strcmp(argv[2], "getstatusp2") == 0)
		parm1 = ESW_GET_PORT2_STATISTICS_STATUS;
	else if (strcmp(argv[2], "getstatusoutputqueue") == 0)
		parm1 = ESW_GET_OUTPUT_QUEUE_STATUS;

	printf("cmd %lx %s\n", parm1, argv[2]);
	/*----command---there are 1 parm(unsigned long 4 char)-----*/
	if ((parm1 == ESW_SET_SWITCH_MODE) ||
		(parm1 == ESW_SET_BRIDGE_CONFIG)) {
		if (argc < 4)
			usage();
		/* get the lengeth of the real parm 1*/
		/* the value is 32 bit*/
		for (i = 0; i < (8 + 1); i++) {
			if (argv[3][i] == 0) {
				parm2_len = i;
				if (parm2_len == 0) {
					printf("input no the 2th parm\n");
					exit(0);
				}
				break;
			}
		}
		printf("%c parm2_len %lx\n", argv[3][8], parm2_len);
		if (i == 0x8 + 1) {
			printf("the 2th parm  is too long\n");
			exit(0);
		}

		for (i = 0; i < parm2_len; i++) {
			if (((argv[3][i] > '0') || (argv[3][i] == '0'))
				&& ((argv[3][i] < '9') ||
					(argv[3][i] == '9'))) {

				parm2 += (argv[3][i] - '0') <<
					((parm2_len - 1 - i) * 4);

			} else if (((argv[3][i] > 'a') || (argv[3][i] == 'a'))
				&& ((argv[3][i] < 'f') ||
					(argv[3][i] == 'f'))) {

				parm2 += (argv[3][i] - 'a' + 10) <<
					((parm2_len - 1 - i) * 4);

			} else if (((argv[3][i] > 'A') || (argv[3][i] == 'A'))
				&& ((argv[3][i] < 'F') ||
					(argv[3][i] == 'F'))) {

				parm2 += (argv[3][i] - 'A' + 10) <<
					((parm2_len - 1 - i) * 4);

			} else {
				printf("the 3th parm is input error\n");
				exit(0);
			}
		}
			configData = parm2;
			ifr.ifr_data = (void *)&configData;
	/*----command-----------there are 2 parm(port, enable)(int, int)--*/
	} else if ((parm1 == ESW_SET_LEARNING_CONF) ||
		(parm1 == ESW_SET_BLOCKING_CONF) ||
		(parm1 == ESW_SET_MULTICAST_CONF) ||
		(parm1 == ESW_SET_BROADCAST_CONF)) {
		if (argc < 5)
			usage();
		/* get the lengeth of the real parm 1(port)*/
		if (argv[3][1] != 0) {
			printf("the third parm  is too long\n");
			exit(0);
		}

		if (((argv[3][0] > '0') || (argv[3][0] == '0'))
		&& ((argv[3][0] < '2') || (argv[3][0] == '2'))) {
			parm2 = (argv[3][0] - '0');

		} else {
			printf("the third parm is input error\n");
			exit(0);
		}

		/* get the lengeth of the real parm 2(enable)*/
		if (argv[4][1] != 0) {
			printf("the 4th parm  is too long\n");
			exit(0);
		}

		if (((argv[4][0] > '0') || (argv[4][0] == '0'))
			&& ((argv[4][0] < '1') || (argv[4][0] == '1'))) {
			parm3 = (argv[4][0] - '0');

		} else {
			printf("the 4th parm is input error\n");
			exit(0);
		}

		PortConfig.port = parm2;
		PortConfig.enable = parm3;
		ifr.ifr_data = (void *)&PortConfig;
	/*----command----------
	 * there are 3 parm(port, tx_en, rx_en)
	 * (int, int, int)--*/
	} else if (parm1 == ESW_SET_PORTENABLE_CONF) {
		if (argc < 6)
			usage();

		/* get the lengeth of the real parm 1(port)*/
		if (argv[3][1] != 0) {
			printf("the third parm  is too long\n");
			exit(0);
		}
		printf("parm3 %c \n", argv[3][0]);
		if (((argv[3][0] > '0') || (argv[3][0] == '0'))
		&& ((argv[3][0] < '2') || (argv[3][0] == '2'))) {
			parm2 = (argv[3][0] - '0');
		} else {
			printf("the third parm is input error\n");
			exit(0);
		}

		/* get the lengeth of the real parm 2(tx_enable)*/
		if (argv[4][1] != 0) {
			printf("the 4th parm  is too long\n");
			exit(0);
		}
		printf("parm4 %c \n", argv[4][0]);
		if (((argv[4][0] > '0') || (argv[4][0] == '0'))
		&& ((argv[4][0] < '1') || (argv[4][0] == '1'))) {
			parm3 = (argv[4][0] - '0');
		} else {
			printf("the 4th parm is input error\n");
			exit(0);
		}

		/* get the lengeth of the real parm 3(rx_enable)*/
		if (argv[5][1] != 0) {
			printf("the 5th parm  is too long\n");
			exit(0);
		}
		printf("parm5 %c \n", argv[5][0]);
		if (((argv[5][0] > '0') || (argv[5][0] == '0'))
		&& ((argv[5][0] < '1') || (argv[5][0] == '1'))) {
			parm4 = (argv[5][0] - '0');
		} else {
			printf("the 5th parm is input error\n");
			exit(0);
		}

		PortEnableConfig.port      = parm2;
		PortEnableConfig.tx_enable = parm3;
		PortEnableConfig.rx_enable = parm4;
		ifr.ifr_data = (void *)&PortEnableConfig;
		printf("\n%lx   %lx  %lx  %lx  %lx\n",
			parm1, parm2, parm3, parm4, parm5);
		/*
		 **----command-----------there are 3 parm---------------------
		 *---(port1, port2, enable)(0-1, 0-1, 0-1)-------------------
		 */
	} else if (parm1 == ESW_SET_P0_FORCED_FORWARD) {
		if (argc < 6)
			usage();
		/* get the lengeth of the real parm 1(port1) 0-1*/
		if (argv[3][1] != 0) {
			printf("the third parm  is too long\n");
			exit(0);
		}

		printf("parm3 %c \n", argv[3][0]);
		if ((argv[3][0] == '0') || (argv[3][0] == '1')) {
			parm2 = (argv[3][0] - '0');
		} else {
			printf("the third parm is input error\n");
			exit(0);
		}

		/* get the lengeth of the real parm 2(port2) 0-1*/
		if (argv[4][1] != 0) {
			printf("the 4th parm  is too long\n");
			exit(0);
		}
		printf("parm4 %c \n", argv[4][0]);
		if ((argv[4][0] == '0') || (argv[4][0] == '1')) {
			parm3 = (argv[4][0] - '0');
		} else {
			printf("the 4th parm is input error\n");
			exit(0);
		}

		/* get the lengeth of the real parm 3(enable) 0-1*/
		if (argv[5][1] != 0) {
			printf("the 4th parm  is too long\n");
			exit(0);
		}

		printf("parm5 %c \n", argv[5][0]);
		if ((argv[5][0] == '0') || (argv[5][0] == '1')) {
			parm4 = (argv[5][0] - '0');
		} else {
			printf("the 5th parm is input error\n");
			exit(0);
		}

		P0ForcedForwardConfig.port1      = parm2;
		P0ForcedForwardConfig.port2      = parm3;
		P0ForcedForwardConfig.enable     = parm4;
		ifr.ifr_data = (void *)&P0ForcedForwardConfig;

	/*
	 *----command-----------there are 3 parm---------------------
	 *---(num, mode, ip_header_protocol)(int, int, unsigned long)--
	 */
	} else if (parm1 == ESW_SET_IP_SNOOP_CONF) {
		if (argc < 6)
			usage();
		/* get the lengeth of the real parm 1(num) 0-7*/
		if (argv[3][1] != 0) {
			printf("the third parm  is too long\n");
			exit(0);
		}
		printf("parm3 %c \n", argv[3][0]);
		if (((argv[3][0] > '0') || (argv[3][0] == '0'))
		&& ((argv[3][0] < '7') || (argv[3][0] == '7'))) {
			parm2 = (argv[3][0] - '0');
		} else {
			printf("the third parm is input error\n");
			exit(0);
		}

		/* get the lengeth of the real parm 2(mode) 0-3*/
		if (argv[4][1] != 0) {
			printf("the 4th parm  is too long\n");
			exit(0);
		}
		printf("parm4 %c \n", argv[4][0]);
		if (((argv[4][0] > '0') || (argv[4][0] == '0'))
		&& ((argv[4][0] < '3') || (argv[4][0] == '3'))) {
			parm3 = (argv[4][0] - '0');
		} else {
			printf("the 4th parm is input error\n");
			exit(0);
		}

		/* get the lengeth of the real parm 3(ip type 8bit) 0-2^8*/
		for (i = 0; i < (2 + 1); i++) {
			if (argv[5][i] == 0) {
				parm4_len = i;
				if (parm4_len == 0) {
					printf("input no the 5th parm\n");
					exit(0);
				}
				break;
			}
		}
		printf("%c parm5_len %lx\n", argv[5][2], parm4_len);
		if (i == 0x2 + 1) {
			printf("the 5th parm  is too long\n");
			exit(0);
		}

		for (i = 0; i < parm4_len; i++) {
			printf("the %d is %c\n", i, argv[5][i]);
			if (((argv[5][i] > '0') || (argv[5][i] == '0'))
				&& ((argv[5][i] < '9') ||
					(argv[5][i] == '9'))) {
				parm4 += (argv[5][i] - '0') <<
					((parm4_len - 1 - i) * 4);

			} else if (((argv[5][i] > 'a') || (argv[5][i] == 'a'))
				&& ((argv[5][i] < 'f') ||
					(argv[5][i] == 'f'))) {
				parm4 += (argv[5][i] - 'a' + 10) <<
					((parm4_len - 1 - i) * 4);
			} else if (((argv[5][i] > 'A') || (argv[5][i] == 'A'))
				&& ((argv[5][i] < 'F') ||
					(argv[5][i] == 'F'))) {
				parm4 += (argv[5][i] - 'A' + 10) <<
					((parm4_len - 1 - i) * 4);
			} else {
				printf("the 5th parm is input error\n");
				exit(0);
			}
		}


		IpSnoopConfig.num     = parm2;
		IpSnoopConfig.mode    = parm3;
		IpSnoopConfig.ip_header_protocol = parm4;
		ifr.ifr_data = (void *)&IpSnoopConfig;
	/*
	*----command-----------there are 4 parm---------------------
	*---(num, mode, compare port, compare num)
	* (int, int, unsigned short, int)--
	*---(0-7   0-3   2^16(16bit    0-3)
	*/
	} else if (parm1 == ESW_SET_PORT_SNOOP_CONF) {
		if (argc < 7)
			usage();

		/* get the lengeth of the real parm 1(num) 0-7*/
		if (argv[3][1] != 0) {
			printf("the third parm  is too long\n");
			exit(0);
		}
		printf("parm3 %c \n", argv[3][0]);
		if (((argv[3][0] > '0') || (argv[3][0] == '0'))
		&& ((argv[3][0] < '7') || (argv[3][0] == '7'))) {
			parm2 = (argv[3][0] - '0');
		} else {
			printf("the third parm is input error\n");
			exit(0);
		}

		/* get the lengeth of the real parm 2(mode) 0-3*/
		if (argv[4][1] != 0) {
			printf("the 4th parm  is too long\n");
			exit(0);
		}
		printf("parm4 %c \n", argv[4][0]);
		if (((argv[4][0] > '0') || (argv[4][0] == '0'))
			&& ((argv[4][0] < '3') || (argv[4][0] == '3'))) {
			parm3 = (argv[4][0] - '0');
		} else {
			printf("the 4th parm is input error\n");
			exit(0);
		}

		/* get the lengeth of the real parm 3(port type 8bit) 0-2^16*/
		/* the port is 16 bit*/
		for (i = 0; i < (4 + 1); i++) {
			if (argv[5][i] == 0) {
				parm4_len = i;
				if (parm4_len == 0) {
					printf("input no the 5th parm\n");
					exit(0);
				}
				break;
			}
		}
		printf("%c parm5_len %lx\n", argv[5][2], parm4_len);
		if (i == 0x4 + 1) {
			printf("the 5th parm  is too long\n");
			exit(0);
		}

		for (i = 0; i < parm4_len; i++) {
			if (((argv[5][i] > '0') || (argv[5][i] == '0'))
			&& ((argv[5][i] < '9') || (argv[5][i] == '9'))) {
				parm4 += (argv[5][i] - '0') <<
					((parm4_len - 1 - i) * 4);
			} else if (((argv[5][i] > 'a') || (argv[5][i] == 'a'))
			&& ((argv[5][i] < 'f') || (argv[5][i] == 'f'))) {
				parm4 += (argv[5][i] - 'a' + 10) <<
					((parm4_len - 1 - i) * 4);
			} else if (((argv[5][i] > 'A') || (argv[5][i] == 'A'))
			&& ((argv[5][i] < 'F') || (argv[5][i] == 'F'))) {
				parm4 += (argv[5][i] - 'A' + 10) <<
					((parm4_len - 1 - i) * 4);
			} else {
				printf("the 5th parm is input error\n");
				exit(0);
			}
		}

		/* get the lengeth of the real parm 4(mode) 0-3*/
		if (argv[6][1] != 0) {
			printf("the 6th parm  is too long\n");
			exit(0);
		}
		printf("parm6 %c \n", argv[6][0]);
		if (((argv[6][0] > '0') || (argv[6][0] == '0'))
		&& ((argv[4][0] < '3') || (argv[4][0] == '3'))) {
			parm5 = (argv[4][0] - '0');
		} else {
			printf("the 6th parm is input error\n");
			exit(0);
		}

		PortsnoopConfig.num    = parm2;
		PortsnoopConfig.mode   = parm3;
		PortsnoopConfig.compare_port =  parm4;
		PortsnoopConfig.compare_num  =  parm5;
		ifr.ifr_data = (void *)&PortsnoopConfig;

		/*
		 *----command-----------there are 10 parm---------------------
		 *---(mirror_port, port, compare port,   compare num)
		 *    (int,   int, int   unsigned short,   int)-
		 *---(0-2          0-2   2^16(16bit    0-3)
		 */
	} else if (parm1 == ESW_SET_PORT_MIRROR_CONF) {
		if (argc < 13)
			usage();

		/* get the lengeth of the real parm 1(mirror port) 0-2*/
		if (argv[3][1] != 0) {
			printf("the third parm  is too long\n");
			exit(0);
		}

		printf("parm3 %c \n", argv[3][0]);
		if (((argv[3][0] > '0') || (argv[3][0] == '0'))
			&& ((argv[3][0] < '2') || (argv[3][0] == '2'))) {
			parm2 = (argv[3][0] - '0');
		} else {
			printf("the third parm is input error\n");
			exit(0);
		}

		/* get the lengeth of the real parm 2( port) 0-2*/
		if (argv[4][1] != 0) {
			printf("the 4th parm  is too long\n");
			exit(0);
		}

		printf("parm4 %c \n", argv[4][0]);
		if (((argv[4][0] > '0') || (argv[4][0] == '0'))
			&& ((argv[4][0] < '2') || (argv[4][0] == '2'))) {
			parm3 = (argv[4][0] - '0');
		} else {
			printf("the 4th parm is input error\n");
			exit(0);
		}

		/* get the lengeth of the (egress_en) 0-1*/
		if (argv[5][1] != 0) {
			printf("the 5th parm  is too long\n");
			exit(0);
		}

		printf("parm5 %c \n", argv[5][0]);
		if (((argv[5][0] > '0') || (argv[5][0] == '0'))
			&& ((argv[5][0] < '1') || (argv[5][0] == '1'))) {
			parm4 = (argv[5][0] - '0');
		} else {
			printf("the 5th parm is input error\n");
			exit(0);
		}

		/* get the lengeth of the (inegress_en) 0-1*/
		if (argv[6][1] != 0) {
			printf("the 6th parm  is too long\n");
			exit(0);
		}

		printf("parm6 %c \n", argv[6][0]);
		if (((argv[6][0] > '0') || (argv[6][0] == '0'))
			&& ((argv[6][0] < '1') || (argv[6][0] == '1'))) {
			parm5 = (argv[6][0] - '0');
		} else {
			printf("the 6th parm is input error\n");
			exit(0);
		}

		/* get the lengeth of the (egress_mac_src_en) 0-1*/
		if (argv[7][1] != 0) {
			printf("the 7th parm  is too long\n");
			exit(0);
		}

		printf("parm7 %c \n", argv[7][0]);
		if (((argv[7][0] > '0') || (argv[7][0] == '0'))
			&& ((argv[7][0] < '1') || (argv[7][0] == '1'))) {
			parm6 = (argv[7][0] - '0');
		} else {
			printf("the 7th parm is input error\n");
			exit(0);
		}
		/* get the lengeth of the (egress_mac_des_en) 0-1*/
		if (argv[8][1] != 0) {
			printf("the 8th parm  is too long\n");
			exit(0);
		}

		printf("parm8 %c \n", argv[8][0]);
		if (((argv[8][0] > '0') || (argv[8][0] == '0'))
			&& ((argv[8][0] < '1') || (argv[8][0] == '1'))) {
			parm7 = (argv[8][0] - '0');
		} else {
			printf("the 8th parm is input error\n");
			exit(0);
		}

		/* get the lengeth of the (inegress_mac_src_en) 0-1*/
		if (argv[9][1] != 0) {
			printf("the 9th parm  is too long\n");
			exit(0);
		}

		printf("parm9 %c \n", argv[9][0]);
		if (((argv[9][0] > '0') || (argv[9][0] == '0'))
			&& ((argv[9][0] < '1') || (argv[9][0] == '1'))) {
			parm8 = (argv[9][0] - '0');
		} else {
			printf("the 9th parm is input error\n");
			exit(0);
		}

		/* get the lengeth of the (inegress_mac_des_en) 0-1*/
		if (argv[10][1] != 0) {
			printf("the 10th parm  is too long\n");
			exit(0);
		}

		printf("parm10 %c \n", argv[9][0]);
		if (((argv[10][0] > '0') || (argv[10][0] == '0'))
			&& ((argv[10][0] < '1') || (argv[10][0] == '1'))) {
			parm9 = (argv[10][0] - '0');
		} else {
			printf("the 9th parm is input error\n");
			exit(0);
		}

		/* get the lengeth of the real parm (mac address  48bit) */
		/* the port is 6 char X2 = (6X2)  4 bit*/
		for (i = 0; i < (12 + 1); i++) {
			if (argv[11][i] == 0) {
				parm10_len = i;
				if (parm10_len == 0) {
					printf("input no the 10th parm\n");
					exit(0);
				}
				break;
			}
		}

		printf("parm10_len %lx parm10 %s\n", parm10_len, argv[11]);
		if (i == 12 + 1) {
			printf("the 10th parm  is too long\n");
			exit(0);
		}

		for (i = 0, k = 0; i < parm10_len; i = i + 2, k++) {
			printf("the %d is %c\n", i, argv[11][i]);
			if (((argv[11][i] > '0') ||
					(argv[11][i] == '0'))
				&& ((argv[11][i] < '9') ||
					(argv[11][i] == '9'))) {
				mac0[k]  = (argv[11][i] - '0') << 4;
			} else if (((argv[11][i] > 'a') ||
					(argv[11][i] == 'a'))
				&& ((argv[11][i] < 'f') ||
					(argv[11][i] == 'f'))) {
				mac0[k]  = (argv[11][i] - 'a' + 10) << 4;
			} else if (((argv[11][i] > 'A') ||
					(argv[11][i] == 'A'))
				&& ((argv[11][i] < 'F') ||
					(argv[11][i] == 'F'))) {
				mac0[k]  = (argv[11][i] - 'A' + 10) << 4;
			} else {
				printf("the 11th parm is input error\n");
				exit(0);
			}

			if (((argv[11][i + 1] > '0') ||
					(argv[11][i + 1] == '0'))
				&& ((argv[11][i + 1] < '9') ||
					(argv[11][i + 1] == '9'))) {
				mac0[k] += (argv[11][i + 1] - '0');
			} else if (((argv[11][i + 1] > 'a') ||
					(argv[11][i + 1] == 'a'))
				&& ((argv[11][i + 1] < 'f') ||
					(argv[11][i + 1] == 'f'))) {
				mac0[k] += (argv[11][i + 1] - 'a' + 10);
			} else if (((argv[11][i + 1] > 'A') ||
					(argv[11][i + 1] == 'A'))
				&& ((argv[11][i + 1] < 'F') ||
					(argv[11][i + 1] == 'F'))) {
				mac0[k] += (argv[11][i + 1] - 'A' + 10);
			} else {
				printf("the 11th parm is input error\n");
				exit(0);
			}
		}
		printf("mac0 %x %x %x %x %x %x\n",
			mac0[0], mac0[1], mac0[2], mac0[3],
			mac0[4], mac0[5]);
		/* get the lengeth of the real parm (mac address  48bit) */
		/* the port is 6 X char X2 = (6X2)  4 bit*/
		for (i = 0; i < (12 + 1); i++) {
			if (argv[12][i] == 0) {
				parm11_len = i;
				if (parm11_len == 0) {
					printf("input no the 11th parm\n");
					exit(0);
				}
				break;
			}
		}

		printf("parm11_len %lx parm11 %s\n", parm11_len, argv[12]);
		if (i == 12 + 1) {
			printf("the 11th parm  is too long\n");
			exit(0);
		}

		for (i = 0, k = 0; i < parm11_len; i = i + 2, k++) {
			printf("the %d is %c\n", i, argv[12][i]);
			if (((argv[12][i] > '0') ||
					(argv[12][i] == '0'))
				&& ((argv[12][i] < '9') ||
					(argv[12][i] == '9'))) {
				mac1[k]  = (argv[12][i] - '0') << 4;
			} else if (((argv[12][i] > 'a') ||
					(argv[12][i] == 'a'))
				&& ((argv[12][i] < 'f') ||
					(argv[12][i] == 'f'))) {
				mac1[k]  = (argv[12][i] - 'a' + 10) << 4;
			} else if (((argv[12][i] > 'A') ||
					(argv[12][i] == 'A'))
				&& ((argv[12][i] < 'F') ||
					(argv[12][i] == 'F'))) {
				mac1[k]  = (argv[12][i] - 'A' + 10) << 4;
			} else {
				printf("the 12th parm is input error\n");
				exit(0);
			}

			if (((argv[12][i + 1] > '0') ||
					(argv[12][i + 1] == '0'))
				&& ((argv[12][i + 1] < '9') ||
					(argv[12][i + 1] == '9'))) {
				mac1[k] |= (argv[12][i + 1] - '0');
			} else if (((argv[12][i + 1] > 'a') ||
					(argv[12][i + 1] == 'a'))
				&& ((argv[12][i + 1] < 'f') ||
					(argv[12][i + 1] == 'f'))) {
				mac1[k] |= (argv[12][i + 1] - 'a' + 10);
			} else if (((argv[12][i + 1] > 'A') ||
					(argv[12][i + 1] == 'A'))
				&& ((argv[12][i + 1] < 'F') ||
					(argv[12][i + 1] == 'F'))) {
				mac1[k] |= (argv[12][i + 1] - 'A' + 10);
			} else {
				printf("the 12th parm is input error\n");
				exit(0);
			}
		}

		printf("mac1 %x %x %x %x %x %x\n",
			mac1[0], mac1[1], mac1[2], mac1[3],
			mac1[4], mac1[5]);


		/* get the lengeth of the (mirror_en) 0-1*/
		if (argv[13][1] != 0) {
			printf("the 13th parm  is too long\n");
			exit(0);
		}

		printf("parm13 %c \n", argv[13][0]);
		if ((argv[13][0] == '1') || (argv[13][0] == '0')) {
			parm12 = (argv[13][0] - '0');
		} else {
			printf("the 13th parm is input error\n");
			exit(0);
		}

		PortMirrorConfig.mirror_port = parm2;
		PortMirrorConfig.port = parm3;
		PortMirrorConfig.egress_en = parm4;
		PortMirrorConfig.ingress_en = parm5;
		PortMirrorConfig.egress_mac_src_en = parm6;
		PortMirrorConfig.egress_mac_des_en = parm7;
		PortMirrorConfig.ingress_mac_src_en = parm8;
		PortMirrorConfig.ingress_mac_des_en = parm9;
		PortMirrorConfig.src_mac = mac0;
		PortMirrorConfig.des_mac = mac1;
		PortMirrorConfig.mirror_enable = parm12;
		ifr.ifr_data = (void *)&PortMirrorConfig;
	/*----command-----------there are 2 parm(port, mode)(int, int)--*/
	/*   port: 0/1/2; mode: 0/1/2/3*/
	} else if (parm1 == ESW_SET_VLAN_OUTPUT_PROCESS) {
		/* get the lengeth of the real parm 1(port)*/
		if (argv[3][1] != 0) {
			printf("the third parm  is too long\n");
			exit(0);
		}

		if (((argv[3][0] > '0') || (argv[3][0] == '0'))
			&& ((argv[3][0] < '2') || (argv[3][0] == '2'))) {
			parm2 = (argv[3][0] - '0');
		} else {
			printf("the third parm is input error\n");
			exit(0);
		}

		/* get the lengeth of the real parm 2(enable)*/
		if (argv[4][1] != 0) {
			printf("the 4th parm  is too long\n");
			exit(0);
		}

		if (((argv[4][0] > '0') || (argv[4][0] == '0'))
			&& ((argv[4][0] < '3') || (argv[4][0] == '3'))) {
			parm3 = (argv[4][0] - '0');
		} else {
			printf("the 4th parm is input error\n");
			exit(0);
		}

		VlanOutputConfig.port = parm2;
		VlanOutputConfig.mode = parm3;
		ifr.ifr_data = (void *)&VlanOutputConfig;

	/*
	*----command-----------there are 6 parm---------------------
	*(port, mode, port vlan id,  vlan verifition en,
	* int, int,  unsigned short,   int,
	* (0-2   0-3    2^12(12bit)    0/1
	* vlan domain num, vlan domain port)
	*     int,             int)
	*      0-31          0-7
	*/
	} else if (parm1 == ESW_SET_VLAN_INPUT_PROCESS) {
		if (argc < 9)
			usage();

		/* get the lengeth of the real parm 1(num) 0-2*/
		if (argv[3][1] != 0) {
			printf("the third parm  is too long\n");
			exit(0);
		}

		if (((argv[3][0] > '0') || (argv[3][0] == '0'))
			&& ((argv[3][0] < '2') || (argv[3][0] == '2'))) {
			parm2 = (argv[3][0] - '0');
		} else {
			printf("the third parm is input error\n");
			exit(0);
		}

		/* get the lengeth of the real parm 2(mode) 0-3*/
		if (argv[4][1] != 0) {
			printf("the 4th parm  is too long\n");
			exit(0);
		}

		if (((argv[4][0] > '0') || (argv[4][0] == '0'))
			&& ((argv[4][0] < '3') || (argv[4][0] == '3'))) {
			parm3 = (argv[4][0] - '0');
		} else {
			printf("the 4th parm is input error\n");
			exit(0);
		}

		/* get the lengeth of the real parm 3(port type 12bit) 0-2^12*/
		/* the port is 16 bit*/
		for (i = 0; i < (4 + 1); i++) {
			if (argv[5][i] == 0) {
				parm4_len = i;
				if (parm4_len == 0) {
					printf("input no the 5th parm\n");
					exit(0);
				}
				break;
			}
		}

		if (i == 0x4 + 1) {
			printf("the 5th parm  is too long\n");
			exit(0);
		}

		for (i = 0; i < parm4_len; i++) {
			printf("the %d is %c\n", i, argv[5][i]);
			if (((argv[5][i] > '0') || (argv[5][i] == '0'))
				&& ((argv[5][i] < '9') ||
					(argv[5][i] == '9'))) {
				parm4 += (argv[5][i] - '0') <<
					((parm4_len - 1 - i) * 4);
			} else if (((argv[5][i] > 'a') || (argv[5][i] == 'a'))
				&& ((argv[5][i] < 'f') ||
					(argv[5][i] == 'f'))) {
				parm4 += (argv[5][i] - 'a' + 10) <<
					((parm4_len - 1 - i) * 4);
			} else if (((argv[5][i] > 'A') || (argv[5][i] == 'A'))
				&& ((argv[5][i] < 'F') ||
					(argv[5][i] == 'F'))) {
				parm4 += (argv[5][i] - 'A' + 10) <<
					((parm4_len - 1 - i) * 4);
			} else {
				printf("the 5th parm is input error\n");
				exit(0);
			}
		}

		/* get the lengeth of the parm 4(num) 0-1*/
		if (argv[6][1] != 0) {
			printf("the 6th parm  is too long\n");
			exit(0);
		}

		if (((argv[6][0] > '0') || (argv[6][0] == '0'))
			&& ((argv[6][0] < '1') || (argv[6][0] == '1'))) {
			parm5 = (argv[6][0] - '0');
		} else {
			printf("the 6th parm is input error\n");
			exit(0);
		}

		/* get the lengeth of the real parm 5( 0-31)*/
		/* the port is 5 bit*/
		for (i = 0; i < (2 + 1); i++) {
			if (argv[7][i] == 0) {
				parm6_len = i;
				if (parm6_len == 0) {
					printf("input no the 7th parm\n");
					exit(0);
				}
				break;
			}
		}

		if (i == 0x2 + 1) {
			printf("the 7th parm  is too long\n");
			exit(0);
		}

		for (i = 0; i < parm6_len; i++) {
			printf("the %d is %c\n", i, argv[7][i]);
			if (((argv[7][i] > '0') || (argv[7][i] == '0'))
				&& ((argv[7][i] < '9') ||
					(argv[7][i] == '9'))) {
				parm6 += (argv[7][i] - '0') <<
					((parm6_len - 1 - i) * 4);
			} else if (((argv[7][i] > 'a') || (argv[7][i] == 'a'))
				&& ((argv[7][i] < 'f') ||
					(argv[7][i] == 'f'))) {
				parm6 += (argv[7][i] - 'a' + 10) <<
					((parm6_len - 1 - i) * 4);
			} else if (((argv[7][i] > 'A') || (argv[7][i] == 'A'))
				&& ((argv[7][i] < 'F') ||
					(argv[7][i] == 'F'))) {
				parm6 += (argv[7][i] - 'A' + 10) <<
					((parm6_len - 1 - i) * 4);
			} else {
				printf("the 7th parm is input error\n");
				exit(0);
			}

			if (parm6 > 31) {
				printf("the 7th parm input too large\n");
				exit(0);
			}
		}

		 /* get the lengeth of the real parm 6(num) 0-7*/
		if (argv[8][1] != 0) {
			printf("the 8th parm  is too long\n");
			exit(0);
		}

		if (((argv[8][0] > '0') || (argv[8][0] == '0'))
			&& ((argv[8][0] < '7') || (argv[8][0] == '7'))) {
			parm7 = (argv[8][0] - '0');
		} else {
			printf("the 8th parm is input error\n");
			exit(0);
		}

		VlanInputConfig.port             = parm2;
		VlanInputConfig.mode 	         = parm3;
		VlanInputConfig.port_vlanid      = parm4;
		VlanInputConfig.vlan_verify_en   = parm5;
		VlanInputConfig.vlan_domain_num  = parm6;
		VlanInputConfig.vlan_domain_port = parm7;
		ifr.ifr_data = (void *)&VlanInputConfig;

		/*
		*----command-----------there are 3 parm---------------------
		*(port vlan id,    vlan domain num, vlan domain port)
		* unsigned short,      int,                  int
		*       2^12(12bit)     0-31          0-7
		*/
	} else if (parm1 == ESW_SET_VLAN_RESOLUTION_TABLE) {
		if (argc < 6)
			usage();

		/* get the lengeth of the real parm 1(port type 12bit) 0-2^12*/
		/* the port is 16 bit*/
		for (i = 0; i < (4 + 1); i++) {
			if (argv[3][i] == 0) {
				parm2_len = i;
				if (parm2_len == 0) {
					printf("input no the 3th parm\n");
					exit(0);
				}
				break;
			}
		}

		if (i == 0x4 + 1) {
			printf("the 3th parm  is too long\n");
			exit(0);
		}

		for (i = 0; i < parm2_len; i++) {
			printf("the %d is %c\n", i, argv[3][i]);
			if (((argv[3][i] > '0') || (argv[3][i] == '0'))
				&& ((argv[3][i] < '9') ||
					(argv[3][i] == '9'))) {
				parm2 += (argv[3][i] - '0') <<
					((parm2_len - 1 - i) * 4);
			} else if (((argv[3][i] > 'a') || (argv[3][i] == 'a'))
				&& ((argv[3][i] < 'f') ||
					(argv[3][i] == 'f'))) {
				parm2 += (argv[3][i] - 'a' + 10) <<
					((parm2_len - 1 - i) * 4);
			} else if (((argv[3][i] > 'A') || (argv[3][i] == 'A'))
				&& ((argv[3][i] < 'F') ||
					(argv[3][i] == 'F'))) {
				parm2 += (argv[3][i] - 'A' + 10) <<
					((parm2_len - 1 - i) * 4);
			} else {
				printf("the 3th parm is input error\n");
				exit(0);
			}
		}

		/* get the lengeth of the real parm 2( 0-31)*/
		/* the port is 5 bit*/
		for (i = 0; i < (2 + 1); i++) {
			if (argv[4][i] == 0) {
				parm3_len = i;
				if (parm3_len == 0) {
					printf("input no the 4th parm\n");
					exit(0);
				}
				break;
			}
		}

		if (i == 0x2 + 1) {
			printf("the 4th parm  is too long\n");
			exit(0);
		}

		for (i = 0; i < parm3_len; i++) {
			if (((argv[4][i] > '0') || (argv[4][i] == '0'))
				&& ((argv[4][i] < '9') ||
					(argv[4][i] == '9'))) {
				parm3 += (argv[4][i] - '0') <<
					((parm3_len - 1 - i) * 4);
			} else if (((argv[4][i] > 'a') || (argv[4][i] == 'a'))
				&& ((argv[4][i] < 'f') ||
					(argv[4][i] == 'f'))) {
				parm3 += (argv[4][i] - 'a' + 10) <<
					((parm3_len - 1 - i) * 4);
			} else if (((argv[4][i] > 'A') || (argv[4][i] == 'A'))
				&& ((argv[4][i] < 'F') ||
					(argv[4][i] == 'F'))) {
				parm3 += (argv[4][i] - 'A' + 10) <<
					((parm3_len - 1 - i) * 4);
			} else {
				printf("the 4th parm is input error\n");
				exit(0);
			}

			if (parm3 > 31) {
				printf("the 4th parm input too large\n");
				exit(0);
			}
		}

		/* get the lengeth of the real parm 3(num) 0-7*/
		if (argv[5][1] != 0) {
			printf("the 5th parm  is too long\n");
			exit(0);
		}

		if (((argv[5][0] > '0') || (argv[5][0] == '0'))
			&& ((argv[5][0] < '7') || (argv[5][0] == '7'))) {
			parm4 = (argv[5][0] - '0');
		} else {
			printf("the 5th parm is input error\n");
			exit(0);
		}

		VlanResoultionTable.vlan_domain_port = parm4;
		VlanResoultionTable.vlan_domain_num  = parm3;
		VlanResoultionTable.port_vlanid      = parm2;
		ifr.ifr_data = (void *)&VlanResoultionTable;

	/*command--there are 3 parm(port, verification en, discard en)*/
	/*   port: 0/1/2; verifition en: 0/1, discard en: 0/1 */
	} else if (parm1 == ESW_SET_VLAN_DOMAIN_VERIFICATION) {
		if (argc < 6)
			usage();
		/* get the lengeth of the real parm 1(port)*/
		if (argv[3][1] != 0) {
			printf("the third parm  is too long\n");
			exit(0);
		}

		if (((argv[3][0] > '0') || (argv[3][0] == '0'))
			&& ((argv[3][0] < '2') || (argv[3][0] == '2'))) {
			parm2 = (argv[3][0] - '0');
		} else {
			printf("the third parm is input error\n");
			exit(0);
		}

		/* get the lengeth of the real parm 2(enable)*/
		if (argv[4][1] != 0) {
			printf("the 4th parm  is too long\n");
			exit(0);
		}

		if ((argv[4][0] == '1') || (argv[4][0] == '0')) {
			parm3 = (argv[4][0] - '0');
		} else {
			printf("the 4th parm is input error\n");
			exit(0);
		}

		/* get the lengeth of the real parm 3(enable)*/
		if (argv[5][1] != 0) {
			printf("the 5th parm  is too long\n");
			exit(0);
		}

		if ((argv[5][0] == '1') || (argv[5][0] == '0')) {
			parm4 = (argv[5][0] - '0');
		} else {
			printf("the 5th parm is input error\n");
			exit(0);
		}

		VlanVerificationConfig.port                    = parm2;
		VlanVerificationConfig.vlan_domain_verify_en   = parm3;
		VlanVerificationConfig.vlan_discard_unknown_en = parm4;
		ifr.ifr_data = (void *)&VlanVerificationConfig;
	/*command--there are 1 parm(vlan domain num  0-31)*/
	} else if (parm1 == ESW_GET_VLAN_RESOLUTION_TABLE) {
		if (argc < 4)
			usage();
		/* get the lengeth of the real parm 1( 0-31)*/
		/* the port is 5 bit*/
		for (i = 0; i < (2 + 1); i++) {
			if (argv[3][i] == 0) {
				parm2_len = i;
				if (parm2_len == 0) {
					printf("input no the 3th parm\n");
					exit(0);
				}
				break;
			}
		}

		if (i == 0x2 + 1) {
			printf("the 3th parm  is too long\n");
			exit(0);
		}

		for (i = 0; i < parm2_len; i++) {
			printf("the %d is %c\n", i, argv[3][i]);
			if (((argv[3][i] > '0') || (argv[3][i] == '0'))
				&& ((argv[3][i] < '9') ||
					(argv[3][i] == '9'))) {
				parm2 += (argv[3][i] - '0') <<
						((parm2_len - 1 - i) * 4);
			} else if (((argv[3][i] > 'a') ||
					(argv[3][i] == 'a'))
				&& ((argv[3][i] < 'f') ||
					(argv[3][i] == 'f'))) {
				parm2 += (argv[3][i] - 'a' + 10) <<
						((parm2_len - 1 - i) * 4);
			} else if (((argv[3][i] > 'A') ||
					(argv[3][i] == 'A'))
				&& ((argv[3][i] < 'F') ||
					(argv[3][i] == 'F'))) {
				parm2 += (argv[3][i] - 'A' + 10) <<
					((parm2_len - 1 - i) * 4);
			} else {
				printf("the 3th parm is input error\n");
				exit(0);
			}

			if (parm2 > 31) {
				printf("the 3th parm input too large\n");
				exit(0);
			}
		}

		configData = parm2;
		ifr.ifr_data = (void *)&configData;
	/*command--there are 3 parm(mac address, port, priority)*/
	/* 6 char,  port: 0-7, priority: 0-7 */
	} else if (parm1 == ESW_UPDATE_STATIC_MACTABLE) {
		if (argc < 6)
			usage();
		/* get the lengeth of the real parm (mac address  48bit) */
		/* the port is 6 X char X2 = (6X2)  4 bit*/
		for (i = 0; i < (12 + 1); i++) {
			if (argv[3][i] == 0) {
				parm2_len = i;
				if (parm2_len == 0) {
					printf("input no the 3th parm\n");
					exit(0);
				}
				break;
			}
		}

		printf("parm2_len %lx parm2 %s\n", parm3_len, argv[3]);
		if (i == 12 + 1) {
			printf("the 3th parm  is too long\n");
			exit(0);
		}

		for (i = 0, k = 0; i < parm2_len; i = i + 2, k++) {
			printf("the %d is %c\n", i, argv[3][i]);
			if (((argv[3][i] > '0') || (argv[3][i] == '0'))
				&& ((argv[3][i] < '9') ||
					(argv[3][i] == '9'))) {
				mac1[k]  = (argv[3][i] - '0') << 4;
			} else if (((argv[3][i] > 'a') ||
					(argv[3][i] == 'a'))
				&& ((argv[3][i] < 'f') ||
					(argv[3][i] == 'f'))) {
				mac1[k]  = (argv[3][i] - 'a' + 10) << 4;
			} else if (((argv[3][i] > 'A') ||
					(argv[3][i] == 'A'))
				&& ((argv[3][i] < 'F') ||
					(argv[3][i] == 'F'))) {
				mac1[k]  = (argv[3][i] - 'A' + 10) << 4;
			} else {
				printf("the 3th parm is input error\n");
				exit(0);
			}

			if (((argv[3][i + 1] > '0') || (argv[3][i + 1] == '0'))
				&& ((argv[3][i + 1] < '9') ||
					(argv[3][i + 1] == '9'))) {
				mac1[k]  |= (argv[3][i + 1] - '0');
			} else if (((argv[3][i + 1] > 'a') ||
					(argv[3][i + 1] == 'a'))
				&& ((argv[3][i + 1] < 'f') ||
					(argv[3][i + 1] == 'f'))) {
				mac1[k] |= (argv[3][i + 1] - 'a' + 10);
			} else if (((argv[3][i + 1] > 'A') ||
					(argv[3][i + 1] == 'A'))
				&& ((argv[3][i + 1] < 'F') ||
					(argv[3][i + 1] == 'F'))) {
				mac1[k] |= (argv[3][i + 1] - 'A' + 10);
			} else {
				printf("the 3th parm is input error\n");
				exit(0);
			}
		}
		printf("mac1 %x %x %x %x %x %x\n",
			mac1[0], mac1[1], mac1[2], mac1[3],
			mac1[4], mac1[5]);

		/* get the lengeth of the real parm 2(num) 0-7*/
		if (argv[4][1] != 0) {
			printf("the 4th parm  is too long\n");
			exit(0);
		}

		if (((argv[4][0] > '0') || (argv[4][0] == '0'))
			&& ((argv[4][0] < '7') || (argv[4][0] == '7'))) {
			parm3 = (argv[4][0] - '0');
		} else {
			printf("the 4th parm is input error\n");
			exit(0);
		}

		/* get the lengeth of the real parm 3(num) 0-7*/
		if (argv[5][1] != 0) {
			printf("the 5th parm  is too long\n");
			exit(0);
		}

		if (((argv[5][0] > '0') || (argv[5][0] == '0'))
			&& ((argv[5][0] < '7') || (argv[5][0] == '7'))) {
			parm4 = (argv[5][0] - '0');
		} else {
			printf("the 5th parm is input error\n");
			exit(0);
		}

		UpdateStaticMACtable.mac_addr = mac1;
		UpdateStaticMACtable.port     = parm3;
		UpdateStaticMACtable.priority = parm4;
		ifr.ifr_data = (void *)&UpdateStaticMACtable;

	} else if (parm1 == ESW_GET_STATISTICS_STATUS) {
		ifr.ifr_data = (void *)&Statistics_status;
	} else if ((parm1 == ESW_GET_PORT0_STATISTICS_STATUS) ||
		   (parm1 == ESW_GET_PORT1_STATISTICS_STATUS) ||
		   (parm1 == ESW_GET_PORT2_STATISTICS_STATUS)) {
		ifr.ifr_data = (void *)&Port_statistics_status;
	} else if (parm1 == ESW_GET_OUTPUT_QUEUE_STATUS) {
		ifr.ifr_data = (void *)&Output_queue_status;
	} else if (parm1 == ESW_GET_PORT_MIRROR_CONF) {
		ifr.ifr_data = (void *)&PortMirrorStatus;
	} else if ((parm1 == ESW_GET_LEARNING_CONF) ||
		   (parm1 == ESW_GET_BLOCKING_CONF) ||
		   (parm1 == ESW_GET_MULTICAST_CONF) ||
		   (parm1 == ESW_GET_BROADCAST_CONF) ||
		   (parm1 == ESW_GET_P0_FORCED_FORWARD) ||
		   (parm1 == ESW_GET_PORTENABLE_CONF) ||
		   (parm1 == ESW_GET_SWITCH_MODE) ||
		   (parm1 == ESW_GET_BRIDGE_CONFIG) ||
		   (parm1 == ESW_GET_VLAN_DOMAIN_VERIFICATION) ||
		   (parm1 == ESW_GET_VLAN_OUTPUT_PROCESS)) {
		ifr.ifr_data = (void *)&configData;
	} else if (parm1 == ESW_GET_IP_SNOOP_CONF) {
		ifr.ifr_data = (void *)&ESW_IPSNP;
	} else if (parm1 == ESW_GET_PORT_SNOOP_CONF) {
		ifr.ifr_data = (void *)&ESW_PSNP;
	} else if (parm1 == ESW_GET_VLAN_INPUT_PROCESS) {
		ifr.ifr_data = (void *)&VlanInputStatus;
	} else {
		printf("we do not support the command %lx\n", parm1);
	}
	/***********************************************************/
	sockfd = socket(AF_INET, SOCK_DGRAM, 0);
	strncpy(ifr.ifr_name, name, IFNAMSIZ-1);

	if (ioctl(sockfd, parm1, &ifr) == -1) {
		printf("ioctl error");
		exit(1);
	}
	printf("\n");
	/*--------------------GET--------------------------------*/
	if (parm1 == ESW_GET_STATISTICS_STATUS) {
		/*show result*/
		printf("GET STATISTICS STATUS :\n");
		printf("   ESW_DISCN:  %lx\n",
				Statistics_status.ESW_DISCN);
		printf("   ESW_DISCB:  %lx\n",
				Statistics_status.ESW_DISCB);
		printf("   ESW_NDISCN: %lx\n",
				Statistics_status.ESW_NDISCN);
		printf("   ESW_NDISCB: %lx\n",
				Statistics_status.ESW_NDISCB);
	} else if (parm1 == ESW_GET_PORT0_STATISTICS_STATUS) {
		/*show result*/
		printf("GET port 0 STATISTICS_STATUS :\n");
		printf("   MCF_ESW_POQC:   %lx\n",
				Port_statistics_status.MCF_ESW_POQC);
		printf("   MCF_ESW_PMVID:  %lx\n",
				Port_statistics_status.MCF_ESW_PMVID);
		printf("   MCF_ESW_PMVTAG: %lx\n",
				Port_statistics_status.MCF_ESW_PMVTAG);
		printf("   MCF_ESW_PBL:    %lx\n",
				Port_statistics_status.MCF_ESW_PBL);
	} else if (parm1 == ESW_GET_PORT1_STATISTICS_STATUS) {
		/*show result*/
		printf("GET port 1 STATISTICS_STATUS :\n");
		printf("   MCF_ESW_POQC:    %lx\n",
				Port_statistics_status.MCF_ESW_POQC);
		printf("   MCF_ESW_PMVID:   %lx\n",
				Port_statistics_status.MCF_ESW_PMVID);
		printf("   MCF_ESW_PMVTAG:  %lx\n",
				Port_statistics_status.MCF_ESW_PMVTAG);
		printf("   MCF_ESW_PBL:     %lx\n",
				Port_statistics_status.MCF_ESW_PBL);
	} else if (parm1 == ESW_GET_PORT2_STATISTICS_STATUS) {
		/*show result*/
		printf("GET port 2 STATISTICS_STATUS :\n");
		printf("   MCF_ESW_POQC:   %lx\n",
				Port_statistics_status.MCF_ESW_POQC);
		printf("   MCF_ESW_PMVID:  %lx\n",
				Port_statistics_status.MCF_ESW_PMVID);
		printf("   MCF_ESW_PMVTAG: %lx\n",
				Port_statistics_status.MCF_ESW_PMVTAG);
		printf("   MCF_ESW_PBL:    %lx\n",
				Port_statistics_status.MCF_ESW_PBL);
	} else if (parm1 == ESW_GET_OUTPUT_QUEUE_STATUS) {
		printf("GET OUTPUT QUEUE STATUS :\n");
		printf("   ESW_MMSR:  %lx\n", Output_queue_status.ESW_MMSR);
		printf("   ESW_LMT:   %lx\n", Output_queue_status.ESW_LMT);
		printf("   ESW_LFC:   %lx\n", Output_queue_status.ESW_LFC);
		printf("   ESW_IOSR:  %lx\n", Output_queue_status.ESW_IOSR);
		printf("   ESW_QWT:   %lx\n", Output_queue_status.ESW_QWT);
		printf("   ESW_P0BCT: %lx\n", Output_queue_status.ESW_P0BCT);
	} else if (parm1 == ESW_GET_PORT_MIRROR_CONF) {
		printf("GET PORT MIRROR Config :\n");
		printf("   ESW_MCR:    %lx\n", PortMirrorStatus.ESW_MCR);
		printf("   ESW_EGMAP:  %lx\n", PortMirrorStatus.ESW_EGMAP);
		printf("   ESW_INGMAP: %lx\n", PortMirrorStatus.ESW_INGMAP);
		printf("   ESW_INGSAL: %lx\n", PortMirrorStatus.ESW_INGSAL);
		printf("   ESW_INGSAH: %lx\n", PortMirrorStatus.ESW_INGSAH);
		printf("   ESW_INGDAL  %lx\n", PortMirrorStatus.ESW_INGDAL);
		printf("   ESW_INGDAH: %lx\n", PortMirrorStatus.ESW_INGDAH);
		printf("   ESW_ENGSAL: %lx\n", PortMirrorStatus.ESW_ENGSAL);
		printf("   ESW_ENGSAH: %lx\n", PortMirrorStatus.ESW_ENGSAH);
		printf("   ESW_ENGDAL: %lx\n", PortMirrorStatus.ESW_ENGDAL);
		printf("   ESW_ENGDAH: %lx\n", PortMirrorStatus.ESW_ENGDAH);
		printf("   ESW_MCVAL:  %lx\n", PortMirrorStatus.ESW_MCVAL);
	} else if (parm1 == ESW_GET_LEARNING_CONF) {
		printf("GET learning Config : %lx\n", configData);
	} else if (parm1 == ESW_GET_BLOCKING_CONF) {
		printf("GET blocking Config : %lx\n", configData);
	} else if (parm1 == ESW_GET_MULTICAST_CONF) {
		printf("GET multicast Config : %lx\n", configData);
	} else if (parm1 == ESW_GET_BROADCAST_CONF) {
		printf("GET broadcast Config : %lx\n", configData);
	} else if (parm1 == ESW_GET_PORTENABLE_CONF) {
		printf("GET port enable Config : %lx\n", configData);
	} else if (parm1 == ESW_GET_P0_FORCED_FORWARD) {
		printf("GET p0 forced forward Config : %lx\n",
				configData);
	} else if (parm1 == ESW_GET_SWITCH_MODE) {
		printf("GET switch mode Config : %lx\n", configData);
	} else if (parm1 == ESW_GET_BRIDGE_CONFIG) {
		printf("GET bridge mode Config : %lx\n", configData);
	} else if (parm1 == ESW_GET_VLAN_OUTPUT_PROCESS) {
		printf("GET vlan output Config : %lx\n", configData);
	} else if (parm1 == ESW_GET_VLAN_DOMAIN_VERIFICATION) {
		printf("GET vlan domain verification Config : %lx\n",
				configData);
	} else if (parm1 == ESW_GET_IP_SNOOP_CONF) {
		printf("GET ip snoop Config : \n");
		printf("   %lx : %lx : %lx : %lx :",
			ESW_IPSNP[0], ESW_IPSNP[1], ESW_IPSNP[2], ESW_IPSNP[3]);
		printf(" %lx : %lx : %lx : %lx\n",
			ESW_IPSNP[4], ESW_IPSNP[5], ESW_IPSNP[6], ESW_IPSNP[7]);
	} else if (parm1 == ESW_GET_PORT_SNOOP_CONF) {
		printf("GET tcp/udp port snoop Config : \n");
		printf("   %lx : %lx : %lx : %lx :",
			ESW_PSNP[0], ESW_PSNP[1], ESW_PSNP[2], ESW_PSNP[3]);
		printf(" %lx : %lx : %lx : %lx\n",
			ESW_PSNP[4], ESW_PSNP[5], ESW_PSNP[6], ESW_PSNP[7]);
	} else if (parm1 == ESW_GET_VLAN_INPUT_PROCESS) {
		printf("GET vlan input process status : \n");
		printf("   %lx : %lx : %lx : %lx :",
			VlanInputStatus.ESW_VLANV, VlanInputStatus.ESW_PID[0],
			VlanInputStatus.ESW_PID[1], VlanInputStatus.ESW_PID[2]);
		printf("   %lx : %lx : %lx : %lx \n",
			VlanInputStatus.ESW_VIMSEL,
			VlanInputStatus.ESW_VIMEN,
			VlanInputStatus.ESW_VRES[0],
			VlanInputStatus.ESW_VRES[1]);

	} else if (parm1 == ESW_GET_VLAN_RESOLUTION_TABLE) {
		printf("GET vlan resolution table : %lx\n", configData);
	}

	exit(0);
}
