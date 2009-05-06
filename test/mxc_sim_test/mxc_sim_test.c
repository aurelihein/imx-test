/*
 * Copyright 2008-2009 Freescale Semiconductor, Inc. All Rights Reserved.
 */

/*
 * The code contained herein is licensed under the GNU General Public
 * License. You may obtain a copy of the GNU General Public License
 * Version 2 or later at the following locations:
 *
 * http://www.opensource.org/licenses/gpl-license.html
 * http://www.gnu.org/copyleft/gpl.html
 */

/*!
 * @file mxc_sim_test.c
 *
 * @brief Test program for Freescale IMX SIM Linux driver
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <signal.h>
#include <string.h>
#include <strings.h>
#include <errno.h>
#include <linux/mxc_sim_interface.h>

#include "iso7816-3.h"

typedef unsigned short word;
typedef unsigned char byte;

/* ---------- ISO7816-4 ---------- */

int sim_verify(byte chv, word pin)
{
	byte cmd[] = { 0xa0, 0x20, 0x00, chv, 0x08, '0', '0', '0',
		'0', 0xff, 0xff, 0xff, 0xff
	};
	cmd[5] = '0' + ((pin >> 12) & 0x0f);
	cmd[6] = '0' + ((pin >> 8) & 0x0f);
	cmd[7] = '0' + ((pin >> 4) & 0x0f);
	cmd[8] = '0' + ((pin >> 0) & 0x0f);
	return SendReceiveAPDU(cmd, 13, 0, 0);
};

int sim_select_file_byid(word file)
{
	byte cmd[] = {
		0xa0, 0xa4, 0x00, 0x00, 0x02, 0x00, 0x00
	};
	cmd[5] = (file >> 8) & 0xff;
	cmd[6] = (file >> 0) & 0xff;
	return SendReceiveAPDU(cmd, 7, 0, 0);
};

int sim_read_binary(int len, byte *buf)
{
	byte cmd[] = { 0xa0, 0xb0, 0, 0, len };
	return SendReceiveAPDU(cmd, 5, buf, len);
};

int sim_read_record(int id, int len, byte *buf)
{
	byte cmd[] = { 0xa0, 0xb2, id, 0x04, len };
	return SendReceiveAPDU(cmd, 5, buf, len);
};

int sim_get_response(int len, byte *buf)
{
	byte cmd[] = { 0xa0, 0xc0, 0x00, 0x00, len };
	return SendReceiveAPDU(cmd, 5, buf, len);
};

/* ---------- GSM11.11 ---------- */

typedef struct {
	char *name;
	word df, ef;
	byte structure;
	byte length;
	int available;
} gsm_file_t;

#define GSM_MF         0x3f00
#define GSM_DF_TELECOM 0x7f10
#define GSM_DF_GSM     0x7f20

#define GSM_STRUCTURE_TR 0
#define GSM_STRUCTURE_LF 1
#define GSM_STRUCTURE_CY 1

gsm_file_t gsm_file[] = {
	{"ICCID", 0x0000, 0x2fe2, GSM_STRUCTURE_TR, 10, 1},
	/* DF_TELECOM */
	{"ADN", GSM_DF_TELECOM, 0x6f3a, GSM_STRUCTURE_LF, 0, 1},
	{"FDN", GSM_DF_TELECOM, 0x6f3b, GSM_STRUCTURE_LF, 0, 1},
	{"SMS", GSM_DF_TELECOM, 0x6f3c, GSM_STRUCTURE_LF, 0, 1},
	{"CCP", GSM_DF_TELECOM, 0x6f3d, GSM_STRUCTURE_LF, 0, 1},
	{"MSISDN", GSM_DF_TELECOM, 0x6f40, GSM_STRUCTURE_LF, 0, 1},
	{"SMSP", GSM_DF_TELECOM, 0x6f42, GSM_STRUCTURE_LF, 0, 1},
	{"SMSS", GSM_DF_TELECOM, 0x6f43, GSM_STRUCTURE_TR, 0, 1},
	{"LND", GSM_DF_TELECOM, 0x6f44, GSM_STRUCTURE_CY, 0, 0},
	{"EXT1", GSM_DF_TELECOM, 0x6f4a, GSM_STRUCTURE_LF, 0, 1},
	{"EXT2", GSM_DF_TELECOM, 0x6f4b, GSM_STRUCTURE_LF, 0, 1},
	/* DF_GSM */
	{"LP", GSM_DF_GSM, 0x6f05, GSM_STRUCTURE_TR, 0, 1},
	{"IMSI", GSM_DF_GSM, 0x6f07, GSM_STRUCTURE_TR, 9, 1},
	{"KC", GSM_DF_GSM, 0x6f20, GSM_STRUCTURE_TR, 9, 1},
	{"PLMNsel", GSM_DF_GSM, 0x6f30, GSM_STRUCTURE_TR, 0, 1},
	{"HPLMN", GSM_DF_GSM, 0x6f31, GSM_STRUCTURE_TR, 1, 1},
	{"ACMax", GSM_DF_GSM, 0x6f37, GSM_STRUCTURE_TR, 3, 1},
	{"SST", GSM_DF_GSM, 0x6f38, GSM_STRUCTURE_TR, 5, 1},
	{"ACM", GSM_DF_GSM, 0x6f39, GSM_STRUCTURE_TR, 3, 0},
	{"GID1", GSM_DF_GSM, 0x6f3e, GSM_STRUCTURE_TR, 0, 0},
	{"GID2", GSM_DF_GSM, 0x6f3f, GSM_STRUCTURE_TR, 0, 0},
	{"PUCT", GSM_DF_GSM, 0x6f41, GSM_STRUCTURE_TR, 5, 1},
	{"CBMI", GSM_DF_GSM, 0x6f45, GSM_STRUCTURE_TR, 0, 1},
	{"SPN", GSM_DF_GSM, 0x6f46, GSM_STRUCTURE_TR, 16, 0},
	{"BCCH", GSM_DF_GSM, 0x6f74, GSM_STRUCTURE_TR, 16, 1},
	{"ACC", GSM_DF_GSM, 0x6f78, GSM_STRUCTURE_TR, 2, 1},
	{"FPLMN", GSM_DF_GSM, 0x6f7b, GSM_STRUCTURE_TR, 12, 1},
	{"LOCI", GSM_DF_GSM, 0x6f7e, GSM_STRUCTURE_TR, 11, 1},
	{"AD", GSM_DF_GSM, 0x6fad, GSM_STRUCTURE_TR, 0, 1},
	{"PHASE", GSM_DF_GSM, 0x6fae, GSM_STRUCTURE_TR, 1, 1}
};

#define GSM_FILE_ID_MAX (sizeof(gsm_file)/sizeof(gsm_file_t))

typedef struct {
	byte rfu1[2];
	byte filesize[2];
	byte fileid[2];
	byte filetype;
	byte increase;
	byte accessconditions[3];
	byte filestatus;
	byte numbytesfollow;
	byte efstruct;
	byte recordsize;
} gsm_fci_ef_t;

word df_current = 0x3f00;
word ef_current = 0x0000;
gsm_fci_ef_t fci_current;

int gsm_read_transparent(word df, word ef, byte len, byte *buf)
{
	int errval;

	if (df != df_current) {
		errval = sim_select_file_byid(GSM_MF);
		printf("SELECT FILE MF: SW = %04x (%i)\n", errval, errval);
		if (errval < 0)
			return errval;

		if (df) {
			errval = sim_select_file_byid(df);
			printf("SELECT FILE DF %04x: SW = %04x (%i)\n",
			       df, errval, errval);
			if (errval < 0)
				return errval;
		};

		df_current = df;
	};

	if (ef != ef_current) {
		errval = sim_select_file_byid(ef);
		printf("SELECT FILE EF %04x: SW = %04x (%i)\n",
		       ef, errval, errval);
		if (errval < 0)
			return errval;

		ef_current = ef;
	};

	errval = sim_read_binary(len, buf);
	printf("READ BINARY: SW = %04x (%i)\n", errval, errval);
	if (errval < 0)
		return errval;

	return 0;
};

int gsm_read_linearfixed(word df, word ef, byte len, byte *buf)
{
	int i;
	int errval;
	int recordsize;
	int entries;

	if (df != df_current) {
		errval = sim_select_file_byid(GSM_MF);
		printf("SELECT FILE MF: SW = %04x (%i)\n", errval, errval);
		if (errval < 0)
			return errval;

		if (df) {
			errval = sim_select_file_byid(df);
			printf("SELECT FILE DF %04x: SW = %04x (%i)\n",
			       df, errval, errval);
			if (errval < 0)
				return errval;
		};

		df_current = df;
	};

	if (ef != ef_current) {
		errval = sim_select_file_byid(ef);
		printf("SELECT FILE EF %04x: SW = %04x (%i)\n",
		       ef, errval, errval);
		if (errval < 0)
			return errval;

		if ((errval & 0xff) != sizeof(gsm_fci_ef_t))
			return -1;

		errval = sim_get_response(sizeof(gsm_fci_ef_t),
					  (byte *) &fci_current);
		printf("GET RESPONSE: err = %i SW = %04x\n", errval, errval);
		if (errval < 0)
			return errval;

		ef_current = ef;
	};

	recordsize = fci_current.recordsize;
	entries = ((fci_current.filesize[0] << 8) + fci_current.filesize[1])
	    / fci_current.recordsize;

	for (i = 1; i < entries; i++) {
		int j;
		byte recbuf[recordsize];

		errval = sim_read_record(i, recordsize, recbuf);
		printf("READ RECORD: SW = %04x (%i)\n", errval, errval);
		if (errval < 0)
			return errval;
		for (j = 0; j < recordsize; j++) {
			char c = (char)recbuf[j];

			if ((c >= 32) && (c < 128))
				printf("%c", c);
			else
				printf(".");
		};
		printf("\n");
	};

	return 0;
};

int gsm_verify(word pin)
{
	int errval;

	errval = sim_verify(1, pin);
	printf("VERIFY PIN: SW = %04x (%i)\n", errval, errval);
	return errval;
};

void sim_pts(void)
{
	byte pts_xmt[3] = { 0xff, 0x00, 0xff };
	byte pts_rcv[3];
	SendReceivePTS(pts_xmt, pts_rcv, 3);
	printf("PTS response: %02x %02x %02x\n",
	       pts_rcv[0], pts_rcv[1], pts_rcv[2]);
};

void sim_dump_atr(void)
{
	int i;
	int size = 32;
	byte buf[size];

	GetAtr(buf, &size);
	printf("ATR:");
	for (i = 0; i < size; i++)
		printf(" %02x", buf[i]);
	printf("\n");
};

#define SIM_INTERNAL_CLK  0
#define SIM_RFU          -1

void sim_dump_param(void)
{
	int sim_param_F[] = {
		SIM_INTERNAL_CLK, 372, 558, 744, 1116, 1488, 1860, SIM_RFU,
		SIM_RFU, 512, 768, 1024, 1536, 2048, SIM_RFU, SIM_RFU
	};
	int sim_param_D[] = {
		SIM_RFU, 64 * 1, 64 * 2, 64 * 4, 64 * 8, 64 * 16, SIM_RFU,
		SIM_RFU,
		SIM_RFU, SIM_RFU, 64 * 1 / 2, 64 * 1 / 4, 64 * 1 / 8,
		64 * 1 / 16, 64 * 1 / 32, 64 * 1 / 64
	};
	int fi, di, n, t;

	GetParamAtr(&fi, &di, &n, &t);
	printf("COM parameters: ");
	printf("F = %i ", sim_param_F[fi]);
	printf("D = %i/64 ", sim_param_D[di]);
	printf("N = %i ", n);
	printf("T = %i\n", t);
};

int cardstatechanged()
{
	char *cardstatestr[] = { "Removed", "Inserted", "ValidATR" };
	printf("%s\n", cardstatestr[GetCardState()]);
	return 0;
};

int main(int argc, char *argv[])
{
	int i;
	int errval;
	int errcnt = 0;

	int pin;

	if (argc != 2) {
		printf("usage: gsmdemo <pin>\n");
		exit(0);
	};

	pin = strtol(argv[1], 0, 16);
	printf("Using pin %04x\n", pin);

	RegisterCardStateCallbackFunc(cardstatechanged);

	errval = ReaderStart();
	if (errval < 0) {
		printf("ReaderStart %i %i\n", errval, errno);
		exit(1);
	};

	while (GetCardState() != SIM_PRESENT_OPERATIONAL)
		sleep(1);

	LockCard();

	sim_dump_atr();
	sim_dump_param();

#if 0
	usleep(200000);
	sim_pts();
	usleep(200000);
#endif

	gsm_verify(0x2000);

	for (i = 0; i < GSM_FILE_ID_MAX; i++) {

		gsm_file_t *ef = &gsm_file[i];

		if ((ef->available)) {
			if ((ef->structure == GSM_STRUCTURE_TR)
			    && (ef->length != 0)) {
				int j;
				byte buf[256];

				printf("\nReading %s\n", ef->name);
				errval = gsm_read_transparent(ef->df, ef->ef,
							      ef->length, buf);

				if (errval >= 0) {
					printf("%s:", ef->name);
					for (j = 0; j < ef->length; j++)
						printf(" %02x", buf[j]);
					printf("\n");
				};
				if (errval < 0)
					errcnt++;
			};
#if 0
			if ((ef->structure == GSM_STRUCTURE_LF)) {
				int j;
				byte buf[256];

				printf("\nReading %s\n", ef->name);
				errval = gsm_read_linearfixed(ef->df, ef->ef,
							      ef->length, buf);
				if (errval >= 0) {
					printf("%s:", ef->name);
					for (j = 0; j < ef->length; j++)
						printf(" %02x", buf[j]);
					printf("\n");
				};
				if (errval < 0)
					errcnt++;
			};
#endif
		};
	};

	ColdReset();

	while (GetCardState() != SIM_PRESENT_OPERATIONAL)
		sleep(1);

	sim_dump_atr();
	gsm_verify(0x2000);

#if 0
	WarmReset();
	while (GetCardState() != SIM_PRESENT_OPERATIONAL)
		sleep(1);
	sim_dump_atr();
	gsm_verify(0x2000);
#endif

	while (GetCardState() != SIM_PRESENT_REMOVED)
		sleep(1);

	EjectCard();

	ReaderStop();

	printf("\nERRORS=%i\n", errcnt);

	return 0;
};
