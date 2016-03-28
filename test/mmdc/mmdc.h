/*
 * Copyright (C) 2013-2015 Freescale Semiconductor, Inc. All Rights Reserved.
 */

/*
 * The code contained herein is licensed under the GNU General Public
 * License. You may obtain a copy of the GNU General Public License
 * Version 2 or later at the following locations:
 *
 * http://www.opensource.org/licenses/gpl-license.html
 * http://www.gnu.org/copyleft/gpl.html
 */

#ifndef MMDC_H_
#define MMDC_H_

typedef struct
{
	unsigned int mdctl;
	unsigned int mdpdc;
	unsigned int mdotc;
	unsigned int mdcfg0;
	unsigned int mdcfg1;
	unsigned int mdcfg2;
	unsigned int mdmisc;
	unsigned int mdscr;
	unsigned int mdref;
	unsigned int mdwcc;
	unsigned int mdrcc;
	unsigned int mdrwd;
	unsigned int mdor;
	unsigned int mdmrr;
	unsigned int mdcfg3lp;
	unsigned int mdmr4;
	unsigned int mdasp;

	unsigned int adopt_base_offset_fill[239];
	unsigned int maarcr;
	unsigned int mapsr;
	unsigned int maexidr0;
	unsigned int maexidr1;
	unsigned int madpcr0;
	unsigned int madpcr1;
	unsigned int madpsr0;
	unsigned int madpsr1;
	unsigned int madpsr2;
	unsigned int madpsr3;
	unsigned int madpsr4;
	unsigned int madpsr5;
	unsigned int masbs0;
	unsigned int masbs1;
	unsigned int ma_reserved1;
	unsigned int ma_reserved2;
	unsigned int magenp;

	unsigned int phy_base_offset_fill[239];
	unsigned int mpzqhwctrl;
	unsigned int mpzqswctrl;
	unsigned int mpwlgcr;
	unsigned int mpwldectrl0;
	unsigned int mpwldectrl1;
	unsigned int mpwldlst;
	unsigned int mpodtctrl;
	unsigned int mpredqby0dl;
	unsigned int mpredqby1dl;
	unsigned int mpredqby2dl;
	unsigned int mpredqby3dl;
	unsigned int mpwrdqby0dl;
	unsigned int mpwrdqby1dl;
	unsigned int mpwrdqby2dl;
	unsigned int mpwrdqby3dl;
	unsigned int mpdgctrl0;
	unsigned int mpdgctrl1;
	unsigned int mpdgdlst;
	unsigned int mprddlctl;
	unsigned int mprddlst;
	unsigned int mpwrdlctl;
	unsigned int mpwrdlst;
	unsigned int mpsdctrl;
	unsigned int mpzqlp2ctl;
	unsigned int mprddlhwctl;
	unsigned int mpwrdlhwctl;
	unsigned int mprddlhwst0;
	unsigned int mprddlhwst1;
	unsigned int mpwrdlhwst0;
	unsigned int mpwrdlhwst1;
	unsigned int mpwlhwerr;
	unsigned int mpdghwst0;
	unsigned int mpdghwst1;
	unsigned int mpdghwst2;
	unsigned int mpdghwst3;
	unsigned int mppdcmpr1;
	unsigned int mppdcmpr2;
	unsigned int mpswdar;
	unsigned int mpswdrdr0;
	unsigned int mpswdrdr1;
	unsigned int mpswdrdr2;
	unsigned int mpswdrdr3;
	unsigned int mpswdrdr4;
	unsigned int mpswdrdr5;
	unsigned int mpswdrdr6;
	unsigned int mpswdrdr7;
	unsigned int mpmur;
	unsigned int mpwrcadl;
	unsigned int mpdccr;
	unsigned int mpbc;
} MMDC_t;

typedef MMDC_t *pMMDC_t;

/********************* Profiler Types & Functions ************************/
typedef struct
{
	unsigned int total_cycles;
	unsigned int busy_cycles;
	unsigned int read_accesses;
	unsigned int write_accesses;
	unsigned int read_bytes;
	unsigned int write_bytes;
	unsigned int data_load;
	unsigned int utilization;
	unsigned int access_utilization;
	unsigned int avg_write_burstsize;
	unsigned int avg_read_burstsize;
} MMDC_PROFILE_RES_t;

typedef enum
{
    RES_FULL,
    RES_UTILIZATION
} MMDC_RES_TYPE_t;

void start_mmdc_profiling(pMMDC_t mmdc);
void stop_mmdc_profiling(pMMDC_t mmdc);
void pause_mmdc_profiling(pMMDC_t mmdc);
void resume_mmdc_profiling(pMMDC_t mmdc);
void get_mmdc_profiling_results(pMMDC_t mmdc, MMDC_PROFILE_RES_t *results);
void print_mmdc_profiling_results(MMDC_PROFILE_RES_t results, MMDC_RES_TYPE_t print_type,int time);

extern unsigned int system_rev;

#define CHIP_REV_1_0            	0x1
#define CHIP_REV_2_0			0x2

#define cpu_is_mx6qp()		mxc_is_cpu(0x65)
#define cpu_is_mx6q()		mxc_is_cpu(0x63)
#define cpu_is_mx6dl()		mxc_is_cpu(0x61)
#define cpu_is_mx6sl()		mxc_is_cpu(0x60)
#define cpu_is_mx6sx()		mxc_is_cpu(0x62)
#define cpu_is_mx6ul()		mxc_is_cpu(0x64)
#define mxc_is_cpu(part)        ((mxc_cpu() == (unsigned int)part) ? 1 : 0)
#define mxc_cpu()               (system_rev >> 12)
#define mxc_cpu_rev()           (system_rev & 0xFF)
#define mxc_cpu_is_rev(rev)     \
        ((mxc_cpu_rev() == (unsigned int)rev) ? 1 : ((mxc_cpu_rev() < (unsigned int)rev) ? -1 : 2))
#define MXC_REV(type)                           \
static inline int type## _rev (int rev)         \
{                                               \
        return (type() ? mxc_cpu_is_rev(rev) : 0);      \
}
MXC_REV(cpu_is_mx6q);

#define axi_ipu1                0x00070004
#define axi_ipu2_6q             0x00070005
#define axi_gpu3d_6dl           0x003F0002
#define axi_gpu3d_6q            0x003E0002
#define axi_gpu2d2_6dl          0x003F0003
#define axi_gpu2d1_6dl          0x003F000A
#define axi_gpu2d_6q            0x003E000B
#define axi_gpu2d_6sl           0x0017000F
#define axi_vpu_6dl             0x003F000B
#define axi_vpu_6q              0x003F0013
#define axi_openvg_6q           0x003F0022
#define axi_openvg_6sl          0x001F0017
#define axi_arm                 0x00060000
#define axi_arm_6sx             0x000E0000
#define axi_arm_6ul             0x000E0000
#define axi_m4_6sx              0x000F0002
#define axi_gpu3d_6sx           0x000F0005
#define axi_pxp_6sx             0x043F0404
#define axi_pxp_6ul             0x043F0003
#define axi_lcd1_6sl            0x0e870005
#define axi_lcd1_6sx            0x043F0013
#define axi_lcd2_6sx            0x043F0023
#define axi_usb                 0x03FF005A
#define axi_usb_6sl             0x003F0014
#define axi_usb_6sx             0x007F0046
#define axi_usb_6ul             0x007F0021
#define axi_default             0x00000000
#define axi_enet1_6ul           0x04FF0422
#define axi_enet2_6ul           0x007F0031
#define axi_lcdif_6ul           0x08010801

#define axi_gpu3d_6qp		0x00170006
#define axi_gpu2d_6qp		0x001F0016
#define axi_pre_6qp		0x00070007  /* for all PRE */
#define axi_pre0_6qp		0x001F0007
#define axi_pre1_6qp		0x001F000F
#define axi_pre2_6qp		0x001F0017
#define axi_pre3_6qp		0x001F001F
#define axi_vpu_6qp		0x003F0002


#endif /*MMDC_H_*/
