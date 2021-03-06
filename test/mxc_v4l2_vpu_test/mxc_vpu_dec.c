/*
 *  Copyright 2018 NXP
 */

/*
 * The code contained herein is licensed under the GNU General Public
 * License. You may obtain a copy of the GNU General Public License
 * Version 2 or late.
 *
 */

/*
 * @file mxc_vpu_dec.c
 * Description: V4L2 driver decoder utility
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>
#include <time.h>
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#include <ctype.h>
#include <string.h>
#include <sys/ioctl.h>
#include <mntent.h>
#include <signal.h>
#include <asm/types.h>
#include <errno.h>
#include <poll.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>
#include <linux/version.h>
#include <linux/types.h>
#include <linux/videodev2.h>

#include "mxc_v4l2.h"

#define _TEST_MMAP

#define MODULE_NAME			"mxc_vpu_dec.out"
#define MODULE_VERSION		"0.1"

#ifndef max
#define max(a,b)        (((a) < (b)) ? (b) : (a))
#endif //!max

#define DQEVENT

volatile int ret_err = 0;
volatile unsigned int g_unCtrlCReceived = 0;
unsigned  long time_total =0;
unsigned int num_total =0;
struct  timeval start;
struct  timeval end;
volatile int loopTimes = 1;
volatile int preLoopTimes = 1;
volatile int initLoopTimes = 1;
int frame_done = 0; 

static __u32  formats_compressed[] = 
{
    VPU_PIX_FMT_LOGO,   //VPU_VIDEO_UNDEFINED = 0,
    V4L2_PIX_FMT_H264,  //VPU_VIDEO_AVC = 1,     ///< https://en.wikipedia.org/wiki/H.264/MPEG-4_AVC
    V4L2_PIX_FMT_VC1_ANNEX_G,   //VPU_VIDEO_VC1_ANNEX_G = 2,     ///< https://en.wikipedia.org/wiki/VC-1
    V4L2_PIX_FMT_MPEG2, //VPU_VIDEO_MPEG2 = 3,   ///< https://en.wikipedia.org/wiki/H.262/MPEG-2_Part_2
    VPU_PIX_FMT_AVS,    //VPU_VIDEO_AVS = 4,     ///< https://en.wikipedia.org/wiki/Audio_Video_Standard
    V4L2_PIX_FMT_MPEG4,    //VPU_VIDEO_ASP = 5,     ///< https://en.wikipedia.org/wiki/MPEG-4_Part_2
    V4L2_PIX_FMT_JPEG,  //VPU_VIDEO_JPEG = 6,    ///< https://en.wikipedia.org/wiki/JPEG
    VPU_PIX_FMT_RV8,    //VPU_VIDEO_RV8 = 7,     ///< https://en.wikipedia.org/wiki/RealVideo
    VPU_PIX_FMT_RV9,    //VPU_VIDEO_RV9 = 8,     ///< https://en.wikipedia.org/wiki/RealVideo
    VPU_PIX_FMT_VP6,    //VPU_VIDEO_VP6 = 9,     ///< https://en.wikipedia.org/wiki/VP6
    VPU_PIX_FMT_SPK,    //VPU_VIDEO_SPK = 10,    ///< https://en.wikipedia.org/wiki/Sorenson_Media#Sorenson_Spark
    V4L2_PIX_FMT_VP8,   //VPU_VIDEO_VP8 = 11,    ///< https://en.wikipedia.org/wiki/VP8
    V4L2_PIX_FMT_H264_MVC,  //VPU_VIDEO_AVC_MVC = 12,///< https://en.wikipedia.org/wiki/Multiview_Video_Coding
    VPU_PIX_FMT_HEVC,   //VPU_VIDEO_HEVC = 13,   ///< https://en.wikipedia.org/wiki/High_Efficiency_Video_Coding
    VPU_PIX_FMT_DIVX,   //VPU_VIDEO_VC1_ANNEX_L = 14,     ///< https://en.wikipedia.org/wiki/VC-1
    V4L2_PIX_FMT_VC1_ANNEX_L,   //VPU_VIDEO_VC1_ANNEX_L = 15,     ///< https://en.wikipedia.org/wiki/VC-1
};
#define ZPU_NUM_FORMATS_COMPRESSED  SIZEOF_ARRAY(formats_compressed)


static __u32  formats_yuv[] = 
{
    V4L2_PIX_FMT_NV12,
    V4L2_PIX_FMT_YUV420M,
    VPU_PIX_FMT_TILED_8,
    VPU_PIX_FMT_TILED_10
};
#define ZPU_NUM_FORMATS_YUV SIZEOF_ARRAY(formats_yuv)

static void SigIntHanlder(int Signal)
{
    /*signal(SIGALRM, Signal);*/
    g_unCtrlCReceived = 1;
    return;
}

static void SigStopHanlder(int Signal)
{
    printf("%s()\n", __FUNCTION__);
    return;
}

static void SigContHanlder(int Signal)
{
    printf("%s()\n", __FUNCTION__);
    return;
}

void changemode(int dir)
{
	static struct termios	oldt, newt;

	if (dir == 1)
	{
		tcgetattr(STDIN_FILENO, &oldt);
		newt = oldt;
		newt.c_lflag &= ~(ICANON | ECHO);
		tcsetattr(STDIN_FILENO, TCSANOW, &newt);
	}
	else
	{
		tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
	}
}

int kbhit(void)
{
	struct timeval	tv;
	fd_set			rdfs;
	int ret;

	tv.tv_sec = 0;
	tv.tv_usec = 0;

	FD_ZERO(&rdfs);
	FD_SET(STDIN_FILENO, &rdfs);

	ret = select(STDIN_FILENO + 1, &rdfs, NULL, NULL, &tv);
	if(ret == -1)
	{
		printf("warnning: select stdin failed.\n");
	}
	else if(ret == 0)
	{
	}
	else
	{
		int size = 1024;
		char *buff = (char *)malloc(size);
		memset(buff, 0, size);
		if(NULL != fgets(buff,size,stdin))
		{
			if(buff[strlen(buff)-1] == '\n')
				buff[strlen(buff)-1] = '\0';
			if(!strcasecmp(buff, "x") || !strcasecmp(buff, "stop"))
			{
				g_unCtrlCReceived = 1;
			}
		}
		free(buff);
	}
	return FD_ISSET(STDIN_FILENO, &rdfs);
}

int check_video_device(uint32_t devInstance,
					   uint32_t *p_busType,
                       uint32_t *p_devType,
					   char *pszDeviceName
					  )

{
	int			            hDev;
	int			            lErr;
	struct v4l2_capability  cap;
    char                    card[64];
	uint32_t                busType = -1;
    uint32_t                devType = -1;


	hDev = open(pszDeviceName, O_RDWR);
    if (hDev >= 0) 
	{
		// query capability
		lErr = ioctl(hDev, 
					 VIDIOC_QUERYCAP, 
					 &cap
					 );
        // close the driver now
		close(hDev);
        if (0 == lErr)
        {
            if (0 == strcmp((const char*)cap.bus_info, "PCIe:"))
            {
                busType = 0;
            }
            if (0 == strcmp((const char*)cap.bus_info, "platform:"))
            {
                busType = 1;
            }

            if (0 == strcmp((const char*)cap.driver, "MX8 codec"))
            {
                devType = COMPONENT_TYPE_CODEC;
            }
            if (0 == strcmp((const char*)cap.driver, "vpu B0"))
            {
                devType = COMPONENT_TYPE_DECODER;
            }
            if (0 == strcmp((const char*)cap.driver, "vpu encoder"))
            {
                devType = COMPONENT_TYPE_ENCODER;
            }

            // find the matching device
			if (-1 != (int)*p_busType)
			{
				if (*p_busType != busType)
				{
					return -1;
				}
			}

            if (-1 != (int)*p_devType)
            {
                if (*p_devType != devType)
                {
					return -1;
                }
            }

            // instance
            snprintf(card, sizeof(cap.card) - 1, "%s", cap.driver);
            if (0 == strcmp((const char*)cap.card, card))
            {
                *p_busType = busType;
                *p_devType = devType;
				return 0;
            }
        }
        else
        {
		return -1;
        }
	}
	else
    {
		return -1;
    }
	return -1;
}

int lookup_video_device_node(uint32_t devInstance,
							 uint32_t *p_busType,
                             uint32_t *p_devType,
							 char *pszDeviceName
							 )
{
   	int			            nCnt = 0;
    ZVDEV_INFO	            devInfo;
   
    printf("%s()-> Requesting %d:%d\n", __FUNCTION__, devInstance, *p_busType);

	memset(&devInfo, 
           0xAA, 
           sizeof(ZVDEV_INFO)
           );

    while (nCnt < 64) 
	{
		sprintf(pszDeviceName, 
				"/dev/video%d", 
				nCnt
				);
		if (!check_video_device(devInstance, p_busType, p_devType, pszDeviceName))
			break;
		else
			nCnt++;
	}

	if (64 == nCnt) 
	{
		// return empty device name
		*pszDeviceName = 0;
		return (-1);
	}
	else
	{
		return (0);
	}
}

int zvconf(component_t *pComponent,
		   char *scrfilename,
           uint32_t type
		   )
{
    // setup port type and open format
    switch (type)
    {
        case COMPONENT_TYPE_ENCODER:
	        pComponent->ports[STREAM_DIR_IN].portType = COMPONENT_PORT_YUV_IN;
	        pComponent->ports[STREAM_DIR_IN].openFormat.yuv.nWidth = 1920;
	        pComponent->ports[STREAM_DIR_IN].openFormat.yuv.nHeight = 1088;
	        pComponent->ports[STREAM_DIR_IN].openFormat.yuv.nBitCount = 12;
	        pComponent->ports[STREAM_DIR_IN].openFormat.yuv.nDataType = ZV_YUV_DATA_TYPE_NV12;
	        pComponent->ports[STREAM_DIR_IN].openFormat.yuv.nFrameRate = 30;
	        pComponent->ports[STREAM_DIR_IN].frame_size = (1920 * 1088 * 3) / 2;
	        pComponent->ports[STREAM_DIR_IN].buf_count = 4;
	        pComponent->ports[STREAM_DIR_OUT].portType = COMPONENT_PORT_COMP_OUT;
	        pComponent->ports[STREAM_DIR_OUT].frame_size = 256 * 1024;
	        pComponent->ports[STREAM_DIR_OUT].buf_count = 16;
            break;
        case COMPONENT_TYPE_CODEC:
        case COMPONENT_TYPE_DECODER:
        default:
	        pComponent->ports[STREAM_DIR_IN].portType = COMPONENT_PORT_COMP_IN;
			if(pComponent->ports[STREAM_DIR_IN].frame_size <= 0)
				pComponent->ports[STREAM_DIR_IN].frame_size = 256 * 1024 * 3 +1;
			if(pComponent->ports[STREAM_DIR_IN].buf_count <= 0)
				pComponent->ports[STREAM_DIR_IN].buf_count = 6;
	        pComponent->ports[STREAM_DIR_OUT].portType = COMPONENT_PORT_YUV_OUT;
	        pComponent->ports[STREAM_DIR_OUT].openFormat.yuv.nWidth = 1920;
	        pComponent->ports[STREAM_DIR_OUT].openFormat.yuv.nHeight = 1088;
	        pComponent->ports[STREAM_DIR_OUT].openFormat.yuv.nBitCount = 12;
	        pComponent->ports[STREAM_DIR_OUT].openFormat.yuv.nDataType = ZV_YUV_DATA_TYPE_NV12;
	        pComponent->ports[STREAM_DIR_OUT].openFormat.yuv.nFrameRate = 30;
	        pComponent->ports[STREAM_DIR_OUT].frame_size = (1920 * 1088 * 3) / 2;
			if(pComponent->ports[STREAM_DIR_OUT].buf_count <= 0)
				pComponent->ports[STREAM_DIR_OUT].buf_count = 4;
            break;
    }
	pComponent->ulWidth = 1920;
	pComponent->ulHeight = 1088;
	return (0);
}

void usage(int nExit)
{
    printf("\n");
    printf("  [%s version %s]\n", MODULE_NAME, MODULE_VERSION);
    printf("  -------------------------------------------------\n");
    printf("  %s <command1> <arg1> <arg2> ... [+ <command2> <arg1> <arg2> ...] \n", MODULE_NAME);
    printf("\n");
    printf("  stream command,\n");
    printf("\n");
    printf("  arg for stream media,\n");
    printf("    ifile <file_name>       => input from file.\n");
    printf("    ifmt <fmt>              => input format 0 - 14.\n");
    printf("    ofmt <fmt>              => output format 0 - 4.\n");
    printf("    ofile <file_name>       => output to file.\n");
    printf("\n");
    printf("  examples, \n");
    printf("\n");

    if (nExit) {
        exit(nExit);
    }
}

static void convert_inter_2_prog_4_nv12(unsigned char *buffer,
					unsigned int width,
					unsigned int height,
					unsigned int luma_size,
					unsigned int chroma_size)
{
	unsigned char *src;
	unsigned char *dst;
	unsigned char *yTopSrc;
	unsigned char *yBotSrc;
	unsigned char *uvTopSrc;
	unsigned char *uvBotSrc;
	unsigned char *yDst;
	unsigned char *uvDst;
	int i;

	src = (unsigned char *)malloc(luma_size + chroma_size);
	memset(src, 0, luma_size + chroma_size);
	memcpy(src, buffer, luma_size + chroma_size);
	memset(buffer, 0, luma_size);

	 yTopSrc = src;
	 yBotSrc = src + luma_size / 2;
	 uvTopSrc = src + luma_size;
	 uvBotSrc = uvTopSrc + chroma_size / 2;
	 dst = buffer;
	 yDst = dst;
	 uvDst = dst + luma_size;

	/*convert luma */
	for (i = 0; i < height; i++)
	{
		if (i & 0x1)
		{
			memcpy(yDst, yBotSrc, width);
			yBotSrc += width;
		}
		else
		{
			memcpy(yDst, yTopSrc, width);
			yTopSrc += width;
		}
		yDst += width;
	}

	/*convert chroma */
	for (i = 0; i < height / 2; i++)
	{
		if (i & 0x1)
		{
			memcpy(uvDst, uvBotSrc, width);
			uvBotSrc += width;
		}
		else
		{
			memcpy(uvDst, uvTopSrc, width);
			uvTopSrc += width;
		}
		uvDst += width;
	}

	free(src);
}

//#define PRECISION_8BIT // 10 bits if not defined
// Read 8-bit FSL stored image
// Format is based on ratser array of tiles, where a tile is 1KB = 8x128
static unsigned int ReadYUVFrame_FSL_8b(unsigned int nPicWidth,
					unsigned int nPicHeight,
					unsigned int uVOffsetLuma,
					unsigned int uVOffsetChroma,
					unsigned int nFsWidth,
					uint8_t **nBaseAddr,
					uint8_t *pDstBuffer,
					unsigned int bInterlaced
				       )
{
	unsigned int i;
	unsigned int h_tiles, v_tiles, v_offset, nLines, vtile, htile;
	unsigned int nLinesLuma  = nPicHeight;
	unsigned int nLinesChroma = nPicHeight >> 1;
	uint8_t      *cur_addr;
	unsigned int *pBuffer = (unsigned int *)pDstBuffer;
	unsigned int *pTmpBuffer;

	pTmpBuffer = (unsigned int*)malloc(256 * 4);
	if (!pTmpBuffer) {
		printf("failed to alloc pTmpBuffer\r\n");
		return -1;
	}

	if (bInterlaced)
	{
		nLinesLuma   >>= 1;
		nLinesChroma >>= 1;
	}

	// Read Top Luma
	nLines    = nLinesLuma;
	v_offset  = uVOffsetLuma;
	h_tiles   = (nPicWidth + 7) >> 3;
	v_tiles   = (nLines + v_offset + 127) >> 7;

	for (vtile = 0; vtile < v_tiles; vtile++)
	{
		unsigned int v_base_offset = nFsWidth * 128 * vtile;

		for (htile = 0; htile < h_tiles; htile++)
		{
			cur_addr = (uint8_t *)(nBaseAddr[0] + htile * 1024 + v_base_offset);
			memcpy(pTmpBuffer, cur_addr, 256 * 4);

			for (i = 0; i < 128; i++)
			{
				int line_num  = (i + 128 * vtile) - v_offset;
				unsigned int line_base = (line_num * nPicWidth) >> 2;
				// Skip data that is off the bottom of the pic
				if (line_num == (int)nLines)
					break;
				// Skip data that is off the top of the pic
				if (line_num < 0)
					continue;
				pBuffer[line_base + (2 * htile) + 0] = pTmpBuffer[2 * i + 0];
				pBuffer[line_base + (2 * htile) + 1] = pTmpBuffer[2 * i + 1];
			}        
		}
	}

	if (bInterlaced)
	{
		pBuffer += (nPicWidth * nPicHeight) >> 3;

		// Read Bot Luma
		nLines = nLinesLuma;
		v_offset = uVOffsetLuma;
		h_tiles = (nPicWidth + 7) >> 3;
		v_tiles = (nLines + v_offset + 127) >> 7;

		for (vtile = 0; vtile < v_tiles; vtile++)
		{
			unsigned int v_base_offset = nFsWidth * 128 * vtile;

			for (htile = 0; htile < h_tiles; htile++)
			{
				cur_addr = (uint8_t *)(nBaseAddr[2] + htile * 1024 + v_base_offset);
				memcpy(pTmpBuffer, cur_addr, 256 * 4);

				for (i = 0; i < 128; i++)
				{
					int line_num = (i + 128 * vtile) - v_offset;
					unsigned int line_base = (line_num * nPicWidth) >> 2;
					// Skip data that is off the bottom of the pic
					if (line_num == (int)nLines)
						break;
					// Skip data that is off the top of the pic
					if (line_num < 0)
						continue;
					pBuffer[line_base + (2 * htile) + 0] = pTmpBuffer[2 * i + 0];
					pBuffer[line_base + (2 * htile) + 1] = pTmpBuffer[2 * i + 1];
				}
			}
		}

		pBuffer += (nPicWidth * nPicHeight) >> 3;
	}
	else
	{
		pBuffer += (nPicWidth * nPicHeight) >> 2;
	}

	// Read Top Chroma
	nLines    = nLinesChroma;
	v_offset  = uVOffsetChroma;
	h_tiles = (nPicWidth + 7) >> 3;
	v_tiles = (nLines + v_offset + 127) >> 7;
	for (vtile = 0; vtile < v_tiles; vtile++)
	{
		unsigned int v_base_offset = nFsWidth * 128 * vtile;

		for (htile = 0; htile < h_tiles; htile++)
		{
			cur_addr = (uint8_t *)(nBaseAddr[1] + htile * 1024 + v_base_offset);
			memcpy(pTmpBuffer, cur_addr, 256 * 4);

			for (i = 0; i < 128; i++)
			{
				int line_num = (i + 128 * vtile) - v_offset;
				unsigned int line_base = (line_num * nPicWidth) >> 2;
				// Skip data that is off the bottom of the pic
				if (line_num == (int)nLines)
					break;
				// Skip data that is off the top of the pic
				if (line_num < 0)
					continue;
				pBuffer[line_base + (2 * htile) + 0] = pTmpBuffer[2 * i + 0];
				pBuffer[line_base + (2 * htile) + 1] = pTmpBuffer[2 * i + 1];
			}
		}
	}

	if (bInterlaced)
	{
		pBuffer += (nPicWidth * nPicHeight) >> 4;

		// Read Bot Chroma
		nLines = nLinesChroma;
		v_offset = uVOffsetChroma;
		h_tiles = (nPicWidth + 7) >> 3;
		v_tiles = (nLines + v_offset + 127) >> 7;
		for (vtile = 0; vtile < v_tiles; vtile++)
		{
			unsigned int v_base_offset = nFsWidth * 128 * vtile;

			for (htile = 0; htile < h_tiles; htile++)
			{
				cur_addr = (uint8_t *)(nBaseAddr[3] + htile * 1024 + v_base_offset);
				memcpy(pTmpBuffer, cur_addr, 256 * 4);

				for (i = 0; i < 128; i++)
				{
					int line_num = (i + 128 * vtile) - v_offset;
					unsigned int line_base = (line_num * nPicWidth) >> 2;
					// Skip data that is off the bottom of the pic
					if (line_num == (int)nLines)
						break;
					// Skip data that is off the top of the pic
					if (line_num < 0)
						continue;
					pBuffer[line_base + (2 * htile) + 0] = pTmpBuffer[2 * i + 0];
					pBuffer[line_base + (2 * htile) + 1] = pTmpBuffer[2 * i + 1];
				}
			}
		}
	}

	if (bInterlaced)
		convert_inter_2_prog_4_nv12(pDstBuffer, nPicWidth, nPicHeight, nPicWidth * nPicHeight, nPicWidth * nPicHeight / 2);

	free(pTmpBuffer);
	return(0);
}

// Read 10bit packed FSL stored image
// Format is based on ratser array of tiles, where a tile is 1KB = 8x128
static unsigned int ReadYUVFrame_FSL_10b(unsigned int nPicWidth,
					 unsigned int nPicHeight,
					 unsigned int uVOffsetLuma,
					 unsigned int uVOffsetChroma,
					 unsigned int nFsWidth,
					 unsigned char **nBaseAddr,
					 unsigned char *pDstBuffer,
					 unsigned int bInterlaced
					)
{
#define RB_WIDTH (4096*5/4/4)
	unsigned int (*pRowBuffer)[RB_WIDTH];
	unsigned int i;
	unsigned int h_tiles,v_tiles,v_offset,nLines,vtile,htile;
	unsigned int nLinesLuma  = nPicHeight;
	unsigned int nLinesChroma = nPicHeight>>1;
	unsigned char *cur_addr;
#ifdef PRECISION_8BIT
	unsigned char *pBuffer = (unsigned char *)pDstBuffer;
#else
	uint16_t *pBuffer = (uint16_t *)pDstBuffer;
#endif
	unsigned int *pTmpBuffer;
	unsigned int pix;

	pRowBuffer = malloc(128 * RB_WIDTH * 4);
	if (!pRowBuffer) {
		printf("failed to alloc pRowBuffer\n");
		return -1;
	}

	pTmpBuffer = malloc(256 * 4);
	if (!pTmpBuffer) {
		printf("failed to alloc pTmpBuffer\n");
		return -1;
	}

	// Read Luma
	nLines    = nLinesLuma;
	v_offset  = uVOffsetLuma;
	h_tiles   = ((nPicWidth*5/4) + 7) >>3;  // basically number of 8-byte words across the pic
	v_tiles   = (nLines+v_offset+127) >>7;  // basically number of 128 line groups

	for (vtile=0; vtile<v_tiles; vtile++)
	{
		unsigned int v_base_offset = nFsWidth*128*vtile;

		// Read Row of tiles as 10-bit
		for (htile=0; htile<h_tiles;htile++)
		{
			cur_addr = (unsigned char *)(nBaseAddr[0] + htile*1024 + v_base_offset);
			memcpy(pTmpBuffer, cur_addr, 256 * 4);

			// Expand data into Rows
			for (i = 0; i < 128; i++)
			{
				pRowBuffer[i][(2 * htile) + 0] = pTmpBuffer[2 * i + 0];
				pRowBuffer[i][(2 * htile) + 1] = pTmpBuffer[2 * i + 1];
			}
		}

		// Convert the 10-bit data to 8-bit, by dropping the LSBs
		for (i = 0; i < 128; i++)
		{
			unsigned char * pRow = (unsigned char *)&pRowBuffer[i];    // Pointer to start of row
			int line_num = (i + 128 * vtile) - v_offset;  // line number in the potentially vertically offset image
			unsigned int line_base = (line_num * nPicWidth);       // location of first pixel in the line in byte units

			// Skip data that is off the bottom of the pic
			if (line_num == (int)nLines)
				break;
			// Skip data that is off the top of the pic
			if (line_num < 0)
				continue;

			// Convert and store 1 pixel at a time across the row
			for (pix = 0; pix<nPicWidth; pix++)
			{
				unsigned int bit_pos         = 10 * pix;                                   // First bit of pixel across the packed line
				unsigned int byte_loc        = bit_pos / 8;                                // Byte containing the first bit        
				unsigned int bit_loc         = bit_pos % 8;                                // First bit location in the firstbyte counting down from MSB!
				uint16_t two_bytes  = (pRow[byte_loc] << 8) | pRow[byte_loc + 1]; // The 2 bytes conaining the pixel
#ifdef PRECISION_8BIT
				unsigned char  the_pix    = two_bytes >> (8 - bit_loc);                 // Align the 8 MSBs to the LSB ans store in a char
				// Store the converted pixel
				pBuffer[line_base + pix] = the_pix;
#else
				pBuffer[line_base + pix] = (two_bytes >> (6 - bit_loc)) & 0x3FF;
#endif
				if ((line_num == 0) && (pix < 4))
					printf("10b 0x%x\n", (two_bytes >> (6 - bit_loc)) & 0x3FF);
			}
		}
	}

	pBuffer += (nPicWidth * nPicHeight);

	// Read Chroma
	nLines    = nLinesChroma;
	v_offset  = uVOffsetChroma;
	h_tiles   = ((nPicWidth*5/4) + 7) >>3;  // basically number of 8-byte words across the pic
	v_tiles   = (nLines+v_offset+127) >>7;  // basically number of 128 line groups
	for (vtile = 0; vtile<v_tiles; vtile++)
	{
		unsigned int v_base_offset = nFsWidth*128*vtile;

		for (htile=0; htile<h_tiles;htile++)
		{
			cur_addr = (unsigned char *)(nBaseAddr[1] + htile * 1024 + v_base_offset);
			memcpy(pTmpBuffer, cur_addr, 256 * 4);

			// Expand data into Rows
			for (i = 0; i < 128; i++)
			{
				pRowBuffer[i][(2 * htile) + 0] = pTmpBuffer[2 * i + 0];
				pRowBuffer[i][(2 * htile) + 1] = pTmpBuffer[2 * i + 1];
			}
		}

		// Convert the 10-bit data to 8-bit, by dropping the LSBs
		for (i = 0; i < 128; i++)
		{
			unsigned char * pRow = (unsigned char *)&pRowBuffer[i];    // Pointer to start of row
			int line_num = (i + 128 * vtile) - v_offset;  // line number in the potentially vertically offset image
			unsigned int line_base = (line_num * nPicWidth);       // location of first pixel in the line in byte units

			// Skip data that is off the bottom of the pic
			if (line_num == (int)nLines)
				break;
			// Skip data that is off the top of the pic
			if (line_num < 0)
				continue;

			// Convert and store 1 pixel at a time across the row
			for (pix = 0; pix<nPicWidth; pix++)
			{
				unsigned int bit_pos         = 10 * pix;                                   // First bit of pixel across the packed line
				unsigned int byte_loc        = bit_pos / 8;                                // Byte containing the first bit        
				unsigned int bit_loc         = bit_pos % 8;                                // First bit location in the firstbyte counting down from MSB!
				uint16_t two_bytes  = (pRow[byte_loc] << 8) | pRow[byte_loc + 1]; // The 2 bytes conaining the pixel
#ifdef PRECISION_8BIT
				unsigned char  the_pix    = two_bytes >> (8 - bit_loc);                 // Align the 8 MSBs to the LSB ans store in a char
				// Store the converted pixel
				pBuffer[line_base + pix] = the_pix;
#else
				pBuffer[line_base + pix] = (two_bytes >> (6 - bit_loc)) & 0x3FF;
#endif
			}
		}
	}
	free(pRowBuffer);
	free(pTmpBuffer);
	return(0);
}  

static void LoadFrameNV12 (unsigned char *pFrameBuffer, unsigned char *pYuvBuffer, unsigned int nFrameWidth, unsigned int nFrameHeight, unsigned int nSizeLuma, unsigned int bMonochrome, unsigned int bInterlaced)
{
	unsigned int nSizeUorV    = nSizeLuma >> 2;
	unsigned char *pYSrc  = pFrameBuffer;
	unsigned char *pUVSrc = pYSrc + nSizeLuma;

	unsigned char *pYDst  = pYuvBuffer;
	unsigned char *pUDst  = pYuvBuffer + nSizeLuma;
	unsigned char *pVDst  = pYuvBuffer + nSizeLuma + nSizeUorV;

	memcpy(pYDst, pYSrc, nSizeLuma);
	if (bMonochrome)
	{
		memset(pUDst, 128, nSizeUorV);
		memset(pVDst, 128, nSizeUorV);
	}
	else
	{
		unsigned char *pLast = pVDst;

		while (pUDst < pLast)
		{
			*pUDst++ = *pUVSrc++;
			*pVDst++ = *pUVSrc++;
		}
	}
}

static void LoadFrameNV12_10b (unsigned char *pFrameBuffer, unsigned char *pYuvBuffer, unsigned int nFrameWidth, unsigned int nFrameHeight, unsigned int nSizeLuma, unsigned int bMonochrome, unsigned int bInterlaced)
{
	unsigned int nSizeUorV    = nSizeLuma >> 2;
#ifdef PRECISION_8BIT
	unsigned char *pYSrc  = pFrameBuffer;
	unsigned char *pUVSrc = pYSrc + nSizeLuma;

	unsigned char *pYDst  = pYuvBuffer;
	unsigned char *pUDst  = pYuvBuffer + nSizeLuma;
	unsigned char *pVDst  = pYuvBuffer + nSizeLuma + nSizeUorV;

	unsigned char *pLast = pVDst;
#else
	uint16_t *pYSrc  = (uint16_t *)pFrameBuffer;
	uint16_t *pUVSrc = pYSrc + nSizeLuma;

	uint16_t *pYDst  = (uint16_t *)pYuvBuffer;
	uint16_t *pUDst  = (uint16_t *)pYuvBuffer + nSizeLuma;
	uint16_t *pVDst  = (uint16_t *)pYuvBuffer + nSizeLuma + nSizeUorV;

	uint16_t *pLast = pVDst;
#endif

#ifdef PRECISION_8BIT
	memcpy (pYDst, pYSrc, nSizeLuma);
#else
	memcpy (pYDst, pYSrc, nSizeLuma * 2);
#endif

	while (pUDst < pLast)
	{
		*pUDst++ = *pUVSrc++;
		*pVDst++ = *pUVSrc++;
	}
}

int isNumber(char *str)
{
	int ret = 1;
	int len = strlen(str);
	int i;
	for(i = 0; i < len; i++)
	{
		if(str[i]<'0' || str[i]>'9')
		{
			ret = 0;
			break;
		}
	}
	return ret;	
}

void release_buffer(stream_media_t *port)
{
	int i, j;

	if (V4L2_MEMORY_MMAP == port->memory)
	{
		if (port->stAppV4lBuf)
		{
			for (i = 0; i < port->buf_count; i++)
			{
				for (j = 0; j < 2; j++)
				{
					if (port->stAppV4lBuf[i].addr[j])
						munmap(port->stAppV4lBuf[i].addr[j], port->stAppV4lBuf[i].size[j]);
				}
			}
			free((void *)port->stAppV4lBuf);
			port->stAppV4lBuf = NULL;
		}
	}
}

int get_fmt(component_t *pComponent, stream_media_t *port, struct v4l2_format *format)
{
	int lErr;

	lErr = ioctl(pComponent->hDev, VIDIOC_G_FMT, format);
	if (lErr)
	{
		printf("%s() VIDIOC_G_FMT ioctl failed %d %s\n", __FUNCTION__, errno, strerror(errno));
		return -1;
	}
	else
	{
		printf("%s() VIDIOC_G_FMT w(%d) h(%d)\n", __FUNCTION__, format->fmt.pix_mp.width, format->fmt.pix_mp.height);
		printf("%s() VIDIOC_G_FMT: sizeimage_0(%d) bytesperline_0(%d) sizeimage_1(%d) bytesperline_1(%d) \n", __FUNCTION__, format->fmt.pix_mp.plane_fmt[0].sizeimage, format->fmt.pix_mp.plane_fmt[0].bytesperline, format->fmt.pix_mp.plane_fmt[1].sizeimage, format->fmt.pix_mp.plane_fmt[1].bytesperline);
	}
	port->openFormat.yuv.nWidth = format->fmt.pix_mp.width;
	port->openFormat.yuv.nHeight = format->fmt.pix_mp.height;
	port->openFormat.yuv.nBitCount = 12;
	port->openFormat.yuv.nDataType = ZV_YUV_DATA_TYPE_NV12;
	port->openFormat.yuv.nFrameRate = 30;
	port->frame_size = (format->fmt.pix_mp.height * format->fmt.pix_mp.height * 3) / 2;
	pComponent->ulWidth = format->fmt.pix_mp.width;
	pComponent->ulHeight = format->fmt.pix_mp.height;

	return 0;
}

int set_fmt(component_t *pComponent, struct v4l2_format *format)
{
	int lErr;

	lErr = ioctl(pComponent->hDev, VIDIOC_S_FMT, format);
	if (lErr)
	{
		printf("%s() VIDIOC_S_FMT ioctl failed %d %s\n", __FUNCTION__, errno, strerror(errno));
		return -1;
	}

	return 0;
}

int get_crop(component_t *pComponent)
{
	int lErr;

	lErr = ioctl(pComponent->hDev, VIDIOC_G_CROP, &pComponent->crop);
	if (lErr)
	{
		printf("%s() VIDIOC_G_CROP ioctl failed %d %s\n", __FUNCTION__, errno, strerror(errno));
		return -1;
	}
	printf("%s() VIDIOC_G_CROP left(%d) top(%d) w(%d) h(%d)\n", __FUNCTION__, pComponent->crop.c.left, pComponent->crop.c.top, pComponent->crop.c.width, pComponent->crop.c.height);

	return 0;
}

int get_mini_cap_buffer(component_t *pComponent)
{
	struct v4l2_control		ctl;
	int 					lErr;

	memset(&ctl, 0, sizeof(struct v4l2_control));
	ctl.id = V4L2_CID_MIN_BUFFERS_FOR_CAPTURE;
	lErr = ioctl(pComponent->hDev, VIDIOC_G_CTRL, &ctl);
	if (lErr)
	{
		printf("%s() VIDIOC_G_CTRL ioctl failed %d %s\n", __FUNCTION__, errno, strerror(errno));
		return -1;
	}
	printf("%s() VIDIOC_G_CTRL ioctl val=%d\n", __FUNCTION__, ctl.value);
	if(pComponent->ports[STREAM_DIR_OUT].buf_count < ctl.value)
		pComponent->ports[STREAM_DIR_OUT].buf_count = ctl.value;

	return 0;
}

int req_buffer(component_t *pComponent, stream_media_t *port, struct v4l2_requestbuffers  *req_bufs)
{
	int lErr;

	lErr = ioctl(pComponent->hDev, VIDIOC_REQBUFS, req_bufs);
	if (lErr)
	{
		printf("%s() VIDIOC_REQBUFS ioctl failed %d %s\n", __FUNCTION__, errno, strerror(errno));
		return -1;
	}

	port->memory = req_bufs->memory;
	port->buf_count = req_bufs->count;

	return 0;
}

int query_buffer(component_t *pComponent, stream_media_t *port, enum v4l2_buf_type type, struct v4l2_plane *stV4lPlanes)
{
	struct zvapp_v4l_buf_info	*stAppV4lBuf;
	struct v4l2_buffer			stV4lBuf;
	int							i, j;
	int							lErr;


	stAppV4lBuf = port->stAppV4lBuf;
	memset(stAppV4lBuf, 0, port->buf_count * sizeof(struct zvapp_v4l_buf_info));

	if (V4L2_MEMORY_MMAP == port->memory)
	{
		for (i = 0; i < port->buf_count; i++)
		{
			stAppV4lBuf[i].stV4lBuf.index = i;
			stAppV4lBuf[i].stV4lBuf.type = type;
			stAppV4lBuf[i].stV4lBuf.bytesused = 0;
			stAppV4lBuf[i].stV4lBuf.memory = port->memory;
			stAppV4lBuf[i].stV4lBuf.m.planes = stAppV4lBuf[i].stV4lPlanes;
			if (type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE
				|| type == V4L2_BUF_TYPE_VIDEO_CAPTURE)
				stAppV4lBuf[i].stV4lBuf.length = 2;
			else
				stAppV4lBuf[i].stV4lBuf.length = 1;
			stV4lBuf.type = type;
			stV4lBuf.memory = V4L2_MEMORY_MMAP;
			stV4lBuf.index = i;
			stV4lBuf.m.planes = stV4lPlanes;
			if (type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE
				|| type == V4L2_BUF_TYPE_VIDEO_CAPTURE)
				stV4lBuf.length = 2;
			else
				stV4lBuf.length = 1;

			lErr = ioctl(pComponent->hDev, VIDIOC_QUERYBUF, &stV4lBuf);
			if (!lErr)
			{
				for (j = 0; j < stV4lBuf.length; j++)
				{
					stAppV4lBuf[i].size[j] = stV4lBuf.m.planes[j].length;
					stAppV4lBuf[i].addr[j] = mmap(0, stV4lBuf.m.planes[j].length, PROT_READ | PROT_WRITE, MAP_SHARED, pComponent->hDev, stV4lBuf.m.planes[j].m.mem_offset);
					if (stAppV4lBuf[i].addr[j] <= 0)
					{
						printf("%s() V4L mmap failed index=%d \n", __FUNCTION__, i);
						return -1;
					}
					stAppV4lBuf[i].stV4lBuf.m.planes[j].m.mem_offset = stV4lBuf.m.planes[j].m.mem_offset;
					stAppV4lBuf[i].stV4lBuf.m.planes[j].bytesused = 0;
					stAppV4lBuf[i].stV4lBuf.m.planes[j].length = stV4lBuf.m.planes[j].length;
					stAppV4lBuf[i].stV4lBuf.m.planes[j].data_offset = 0;
				}
			}
			else
			{
				printf("%s() VIDIOC_QUERYBUF failed index=%d \n", __FUNCTION__, i);
				return -1;
			}
		}
	}

	return 0;
}
void showUsage(void)
{
	printf("\n\
Usage: ./mxc_v4l2_vpu_dec.out ifile [PATH] ifmt [IFMT] ofmt [OFMT] [OPTIONS]\n\n\
OPTIONS:\n\
    --help          Show usage manual.\n\n\
    PATH            Specify the input file path.\n\n\
    IFMT            Specify input file encode format number. Format comparsion table:\n\
                        VPU_VIDEO_UNDEFINED           0\n\
                        VPU_VIDEO_AVC                 1\n\
                        VPU_VIDEO_VC1                 2\n\
                        VPU_VIDEO_MPEG2               3\n\
                        VPU_VIDEO_AVS                 4\n\
                        VPU_VIDEO_ASP(MPEG4/H263)     5\n\
                        VPU_VIDEO_JPEG(MJPEG)         6\n\
                        VPU_VIDEO_RV8                 7\n\
                        VPU_VIDEO_RV9                 8\n\
                        VPU_VIDEO_VP6                 9\n\
                        VPU_VIDEO_SPK                 10\n\
                        VPU_VIDEO_VP8                 11\n\
                        VPU_VIDEO_AVC_MVC             12\n\
                        VPU_VIDEO_HEVC                13\n\
                        VPU_PIX_FMT_DIVX              14\n\n\
    OFMT            Specify decode format number. Format comparsion table:\n\
                        V4L2_PIX_FMT_NV12             0\n\
                        V4L2_PIX_FMT_YUV420           1\n\
                        VPU_PIX_FMT_TILED_8           2\n\
                        VPU_PIX_FMT_TILED_10          3\n\n\
    ofile path      Specify the output file path.\n\n\
    loop times      Specify loop decode times to the same file. If the times not set, the loop continues.\n\n\
    frames count    Specify the count of decode frames. Default total decode.\n\n\
    bs count        Specify the count of input buffer block size, the unit is Kb.\n\n\
    iqc count       Specify the count of input reqbuf.\n\n\
    oqc count       Specify the count of output reqbuf.\n\n\
    dev device     Specify the VPU decoder device node(generally /dev/video12).\n\n\n\
EXAMPLES:\n\
    ./mxc_v4l2_vpu_dec.out ifile decode.264 ifmt 1 ofmt 1 ofile test.yuv\n\n\
    ./mxc_v4l2_vpu_dec.out ifile decode.264 ifmt 1 bs 500 ofmt 1 ofile test.yuv\n\n\
    ./mxc_v4l2_vpu_dec.out ifile decode.bit ifmt 13 ofmt 1 ofile test.yuv frames 100 loop 10 dev /dev/video12\n\n\
    ./mxc_v4l2_vpu_dec.out ifile decode.bit ifmt 13 ofmt 1 loop\n\n");

}

void test_streamout(component_t *pComponent)
{
	int					lErr = 0;
	FILE					*fpOutput = 0;
	struct zvapp_v4l_buf_info		*stAppV4lBuf;
	struct v4l2_buffer			stV4lBuf;
	struct v4l2_plane           		stV4lPlanes[3];
	unsigned int				ulWidth;
	unsigned int				ulHeight;
	fd_set					rd_fds;
	fd_set 					evt_fds;
	struct timeval 				tv;
	int 					r;
	struct v4l2_event 			evt;
	unsigned int 				i;
	unsigned int                		j;
	zoe_bool_t                  		seek_flag;
	unsigned int                		outFrameNum = 0;
	unsigned int                		stream_type;
	float                       		used_time = 0.01;
	struct v4l2_format          		format;
	struct v4l2_requestbuffers  		req_bufs;

STREAMOUT_INFO:

	memset(&format, 0, sizeof(struct v4l2_format));
	format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
	lErr = get_fmt(pComponent, &pComponent->ports[STREAM_DIR_OUT], &format);
	if (lErr < 0)
	{
		ret_err = 40;
		g_unCtrlCReceived = 1;
		goto FUNC_EXIT;
	}

	format.fmt.pix_mp.pixelformat = pComponent->ports[STREAM_DIR_OUT].fmt;
	lErr = set_fmt(pComponent, &format);
	if (lErr < 0)
	{
		ret_err = 41;
		g_unCtrlCReceived = 1;
		goto FUNC_EXIT;
	}
	ulWidth = pComponent->ulWidth;
	ulHeight = pComponent->ulHeight;


	pComponent->crop.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
	lErr = get_crop(pComponent);
	if (lErr < 0)
	{
		ret_err = 42;
		g_unCtrlCReceived = 1;
		goto FUNC_EXIT;
	}

	lErr = get_mini_cap_buffer(pComponent);
	if (lErr < 0)
	{
		ret_err = 43;
		g_unCtrlCReceived = 1;
		goto FUNC_EXIT;
	}

	memset(&req_bufs, 0, sizeof(struct v4l2_requestbuffers));
	req_bufs.count = pComponent->ports[STREAM_DIR_OUT].buf_count;
	req_bufs.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
	req_bufs.memory = V4L2_MEMORY_MMAP;
	lErr = req_buffer(pComponent, &pComponent->ports[STREAM_DIR_OUT], &req_bufs);
	if (lErr < 0)
	{
		ret_err = 44;
		g_unCtrlCReceived = 1;
		goto FUNC_EXIT;
	}

	pComponent->ports[STREAM_DIR_OUT].stAppV4lBuf = malloc(pComponent->ports[STREAM_DIR_OUT].buf_count * sizeof(struct zvapp_v4l_buf_info));
	if (!pComponent->ports[STREAM_DIR_OUT].stAppV4lBuf)
	{
		printf("warning: %s() alloc stream buffer failed.\n", __FUNCTION__);
		ret_err = 44;
		g_unCtrlCReceived = 1;
		goto FUNC_EXIT;
	}
	stAppV4lBuf = pComponent->ports[STREAM_DIR_OUT].stAppV4lBuf;

	lErr = query_buffer(pComponent, &pComponent->ports[STREAM_DIR_OUT], V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE, stV4lPlanes);
	if (lErr < 0)
	{
		ret_err = 45;
		g_unCtrlCReceived = 1;
		goto FUNC_EXIT;
	}

	pComponent->ports[STREAM_DIR_OUT].opened = 1;

STREAMOUT_START:
	printf("%s() [\n", __FUNCTION__);
	seek_flag = 1;
	pComponent->ports[STREAM_DIR_OUT].done_flag = 0;
	frame_done = 0;
	outFrameNum = 0;
    
	/***********************************************
    ** 1> Open output file descriptor
    ***********************************************/
		
	if (pComponent->ports[STREAM_DIR_OUT].eMediaType == MEDIA_FILE_OUT)
	{
		fpOutput = fopen(pComponent->ports[STREAM_DIR_OUT].pszNameOrAddr, "w+");
		if (fpOutput == NULL)
		{
			printf("%s() error: Unable to open file %s.\n", __FUNCTION__, pComponent->ports[STREAM_DIR_OUT].pszNameOrAddr);
			g_unCtrlCReceived = 1;
			ret_err = 46;
			goto FUNC_EXIT;
		}
	}
	else if (pComponent->ports[STREAM_DIR_OUT].eMediaType == MEDIA_NULL_OUT)
	{
		// output to null
	}
	else
	{
		printf("%s() Unknown media type %d.\n", __FUNCTION__, pComponent->ports[STREAM_DIR_OUT].eMediaType);
		g_unCtrlCReceived = 1;
		ret_err = 47;
		goto FUNC_EXIT;
	}

    // stream on v4l2 capture	
    stream_type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
	lErr = ioctl(pComponent->hDev, VIDIOC_STREAMON, &stream_type);
	if (!lErr)
	{
		pComponent->ports[STREAM_DIR_OUT].unCtrlCReceived = 0;
	}
    else
    {
		printf("%s() VIDIOC_STREAMON V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE failed errno(%d) %s\n", __FUNCTION__, errno, strerror(errno));
		g_unCtrlCReceived = 1;
		ret_err = 48;
		goto FUNC_END;
    }

	/***********************************************
	** 2> Stream on
	***********************************************/
	while (!g_unCtrlCReceived && !pComponent->ports[STREAM_DIR_OUT].unCtrlCReceived)
	{
		/***********************************************
		** QBUF, send all the buffers to driver
		***********************************************/
		if(seek_flag==1){
			for (i = 0; i < pComponent->ports[STREAM_DIR_OUT].buf_count; i++)
			{
				stAppV4lBuf[i].sent = 0;
			}
			seek_flag = 0;
		}
		for (i = 0; i < pComponent->ports[STREAM_DIR_OUT].buf_count; i++)
		{
			if (!stAppV4lBuf[i].sent)
			{
				for (j = 0; j < stAppV4lBuf[i].stV4lBuf.length; j++)
				{
					stAppV4lBuf[i].stV4lBuf.m.planes[j].bytesused = 0;
					stAppV4lBuf[i].stV4lBuf.m.planes[j].data_offset = 0;
				}
				lErr = ioctl(pComponent->hDev, VIDIOC_QBUF, &stAppV4lBuf[i].stV4lBuf);
				if (lErr)
				{
					printf("%s() QBUF ioctl failed %d %s\n", __FUNCTION__, errno, strerror(errno));
					if (errno == EAGAIN)
					{
						lErr = 0;
					}
					break;
				}
				else
				{
					stAppV4lBuf[i].sent = 1;
				}
			}
		}

		/***********************************************
		** DQBUF, get buffer from driver
		***********************************************/
		FD_ZERO(&rd_fds);
		FD_ZERO(&evt_fds);
		FD_SET(pComponent->hDev, &rd_fds);
		FD_SET(pComponent->hDev, &evt_fds);

		// Timeout
		tv.tv_sec = 1;
		tv.tv_usec = 0;

		r = select(pComponent->hDev + 1, &rd_fds, NULL, &evt_fds, &tv);

		if (-1 == r)
		{
			fprintf(stderr, "%s() select errno(%d)\n", __FUNCTION__, errno);
			continue;
		}
		else if (0 == r)
		{
			printf("\nstream out: select readable dev timeout.\n");
			continue;
		}
		else
		{
			if(FD_ISSET(pComponent->hDev, &evt_fds))
			{
				memset(&evt, 0, sizeof(struct v4l2_event));
				lErr = ioctl(pComponent->hDev, VIDIOC_DQEVENT, &evt);
				if (lErr)
				{
					 printf("%s() VIDIOC_DQEVENT ioctl failed %d %s\n", __FUNCTION__, errno, strerror(errno));
					 break;
				}

				if(evt.type == V4L2_EVENT_EOS)
				{
					printf("V4L2_EVENT_EOS is called\n");
					break;
				}
				else if(evt.type == V4L2_EVENT_SOURCE_CHANGE)
				{
					pComponent->res_change_flag = 1;
					break;
				}
				else if(evt.type == V4L2_EVENT_DECODE_ERROR)
				{
					g_unCtrlCReceived = 1;
					goto FUNC_END;
				}
				else
				{
					printf("%s() VIDIOC_DQEVENT type=%d\n", __FUNCTION__, evt.type);
				}
			}

			if(!FD_ISSET(pComponent->hDev, &rd_fds))
				continue;
		}

		stV4lBuf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
		stV4lBuf.memory = V4L2_MEMORY_MMAP;
		stV4lBuf.m.planes = stV4lPlanes;
		stV4lBuf.length = 2;
		
		lErr = ioctl(pComponent->hDev, VIDIOC_DQBUF, &stV4lBuf);
		if (!lErr)
		{
			// clear sent flag
			stAppV4lBuf[stV4lBuf.index].sent = 0;

			if(pComponent->ports[STREAM_DIR_OUT].outFrameCount > 0 &&
				outFrameNum >= pComponent->ports[STREAM_DIR_OUT].outFrameCount)
			{
				if(!frame_done)
					frame_done = 1;
				break;
			}
			else
			{
				outFrameNum++;
				gettimeofday(&end, NULL);
				used_time = (float)(end.tv_sec - start.tv_sec + (end.tv_usec - start.tv_usec)/1000000.0);
				printf("\rframes = %d, fps = %.2f, used_time = %.2f\t\t", outFrameNum, outFrameNum / used_time, used_time);
				if (pComponent->ports[STREAM_DIR_OUT].eMediaType == MEDIA_FILE_OUT)
				{
					{
						unsigned char *nBaseAddr[4];
						unsigned int horiAlign = V4L2_NXP_FRAME_HORIZONTAL_ALIGN - 1;
						unsigned int stride = ((ulWidth + horiAlign) & ~horiAlign);
						unsigned int b10format = (stV4lBuf.reserved == 1) ? 1 : 0;
						unsigned int bInterLace = (stV4lBuf.field == 4) ? 1 : 0;
						unsigned int byteused = ulWidth * ulHeight * 3 / 2;
						unsigned int totalSizeImage = stV4lBuf.m.planes[0].length + stV4lBuf.m.planes[1].length;
						unsigned char *dstbuf, *yuvbuf;

						if (b10format)
						{
							printf("warning: unsupport 10bit stream.\n");
							g_unCtrlCReceived = 1;
							goto FUNC_END;
						}

						dstbuf = (unsigned char *)malloc(totalSizeImage);
						if(!dstbuf)
						{
							printf("error: dstbuf alloc failed\n");
							goto FUNC_END;
						}
						yuvbuf = (unsigned char *)malloc(totalSizeImage);
						if(!yuvbuf)
						{
							printf("error: yuvbuf alloc failed\n");
							goto FUNC_END;
						}

						nBaseAddr[0] = (unsigned char *)(stAppV4lBuf[stV4lBuf.index].addr[0] + stV4lBuf.m.planes[0].data_offset);
						nBaseAddr[1] = (unsigned char *)(stAppV4lBuf[stV4lBuf.index].addr[1] + stV4lBuf.m.planes[1].data_offset);
						nBaseAddr[2] = nBaseAddr[0]  + stV4lBuf.m.planes[0].length / 2;
						nBaseAddr[3] = nBaseAddr[1] + stV4lBuf.m.planes[1].length / 2;

						/*because hardware currently only support NY12_TILED format*/
						/*so need to complete the subsequent conversion*/
						switch (pComponent->ports[STREAM_DIR_OUT].fmt)
 						{
						case V4L2_PIX_FMT_NV12:
							if (b10format)
							{
								ReadYUVFrame_FSL_10b(ulWidth, ulHeight, 0, 0, stride, nBaseAddr, yuvbuf, 0);
							}
							else
							{
								ReadYUVFrame_FSL_8b(ulWidth, ulHeight, 0, 0, stride, nBaseAddr, yuvbuf, bInterLace);
							}
							fwrite((void *)yuvbuf, 1, byteused, fpOutput);
							break;
						case V4L2_PIX_FMT_YUV420M:
							if (b10format)
							{
								ReadYUVFrame_FSL_10b(ulWidth, ulHeight, 0, 0, stride, nBaseAddr, yuvbuf, 0);
								LoadFrameNV12_10b (yuvbuf, dstbuf, ulWidth, ulHeight, ulWidth * ulHeight, 0, 0);
							}
							else
							{
								ReadYUVFrame_FSL_8b(ulWidth, ulHeight, 0, 0, stride, nBaseAddr, yuvbuf, bInterLace);
								LoadFrameNV12(yuvbuf, dstbuf, ulWidth, ulHeight, ulWidth * ulHeight, 0, bInterLace);
							}
							fwrite((void *)dstbuf, 1, byteused, fpOutput);
							break;
						case VPU_PIX_FMT_TILED_8:
						case VPU_PIX_FMT_TILED_10:
							fwrite((void *)nBaseAddr[0], 1, stV4lBuf.m.planes[0].bytesused, fpOutput);
							fwrite((void *)nBaseAddr[1], 1, stV4lBuf.m.planes[1].bytesused, fpOutput);
							break;
						default:
							printf("warning: %s() please specify output format, or the format you specified is not standard. \n", __FUNCTION__);
							break;
						}
						free(dstbuf);
						free(yuvbuf);
					}
				}
				else if (pComponent->ports[STREAM_DIR_OUT].eMediaType == MEDIA_NULL_OUT)
				{
				}
				fflush(fpOutput);
			}
		}
		else	
		{
			if (errno == EAGAIN)
			{
				lErr = 0;
			}
			else
			{
				printf("\r%s()  DQBUF failed(%d) errno(%d)\n", __FUNCTION__, lErr, errno);
			}
		}
		usleep(10);
	}

FUNC_END:
	printf("%s() ]\n", __FUNCTION__);
	fflush(fpOutput);
	if (fpOutput)
	{
		fclose(fpOutput);
	}
	
	stream_type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
	lErr = ioctl(pComponent->hDev, VIDIOC_STREAMOFF, &stream_type);
	if (lErr)
	{
		printf("warning: %s() VIDIOC_STREAMOFF has error, errno(%d), %s \n", __FUNCTION__, errno, strerror(errno));
		g_unCtrlCReceived = 1;
	}
    else
	{
		printf("%s() sent cmd: VIDIOC_STREAMOFF\n", __FUNCTION__);
	}

	if(pComponent->res_change_flag)
	{
		release_buffer(&pComponent->ports[STREAM_DIR_OUT]);
		pComponent->res_change_flag = 0;
		goto STREAMOUT_INFO;
	}

	pComponent->ports[STREAM_DIR_OUT].unCtrlCReceived = 1;
	pComponent->ports[STREAM_DIR_OUT].done_flag = 1;
	printf("Total: frames = %d, fps = %.2f, used_time = %.2f \n", outFrameNum, outFrameNum / used_time, used_time);

	if(!g_unCtrlCReceived)
	{
		while(preLoopTimes == loopTimes && !g_unCtrlCReceived); //sync stream in loopTimes
		
		if(abs(preLoopTimes - loopTimes) > 1)
		{
			printf("warn: test_streamin thread too fast, test_streamout cannot sync\n");
			loopTimes = preLoopTimes - 1;  //sync actual loopTimes
			g_unCtrlCReceived = 1;
		}
		else
		{
			if(loopTimes <= 0)
			{
				g_unCtrlCReceived = 1;
			}
			else
			{
				while(pComponent->ports[STREAM_DIR_IN].done_flag && !g_unCtrlCReceived)
				{
					usleep(10);
				}
				
				preLoopTimes = loopTimes;
				goto STREAMOUT_START;
			}
		}
	}

FUNC_EXIT:

	release_buffer(&pComponent->ports[STREAM_DIR_OUT]);
	stAppV4lBuf = NULL;
	pComponent->ports[STREAM_DIR_OUT].opened = ZOE_FALSE;
}

void test_streamin(component_t *pComponent)
{
	int							lErr = 0;
	FILE						*fpInput = 0;

	struct zvapp_v4l_buf_info	*stAppV4lBuf;
	struct v4l2_buffer			stV4lBuf;
	struct v4l2_plane           stV4lPlanes[3];
	struct v4l2_buffer			*pstV4lBuf = NULL;
	struct v4l2_decoder_cmd     v4l2cmd;

	fd_set                      fds;
	struct timeval              tv;
	int                         r;

	unsigned int                i;
	unsigned int                total;
	long                        file_size;
	int                         stream_type;
	int                         seek_flag;
	int                         qbuf_times;
	struct v4l2_requestbuffers  req_bufs;
	struct v4l2_format          format;
	
	unsigned int				ulIOBlockSize;

	// set v4l2 output format (compressed data input)
	memset(&format, 0, sizeof(struct v4l2_format));
	format.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
	if (pComponent->ports[STREAM_DIR_IN].fmt < ZPU_NUM_FORMATS_COMPRESSED)
	{
		format.fmt.pix_mp.pixelformat = formats_compressed[pComponent->ports[STREAM_DIR_IN].fmt];
	}
	else
	{
		format.fmt.pix_mp.pixelformat = VPU_PIX_FMT_LOGO;
	}
	format.fmt.pix_mp.num_planes = 1;
	format.fmt.pix_mp.plane_fmt[0].sizeimage = pComponent->ports[STREAM_DIR_IN].frame_size;
	lErr = set_fmt(pComponent, &format);
	if (lErr < 0)
	{
		ret_err = 30;
		g_unCtrlCReceived = 1;
		goto FUNC_EXIT;
	}

	ulIOBlockSize = pComponent->ports[STREAM_DIR_IN].frame_size;

	memset(&req_bufs, 0, sizeof(struct v4l2_requestbuffers));
	req_bufs.count = pComponent->ports[STREAM_DIR_IN].buf_count;
	req_bufs.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
	req_bufs.memory = V4L2_MEMORY_MMAP;
	lErr = req_buffer(pComponent, &pComponent->ports[STREAM_DIR_IN], &req_bufs);
	if (lErr < 0)
	{
		ret_err = 31;
		g_unCtrlCReceived = 1;
		goto FUNC_EXIT;
	}

	pComponent->ports[STREAM_DIR_IN].stAppV4lBuf = malloc(pComponent->ports[STREAM_DIR_IN].buf_count * sizeof(struct zvapp_v4l_buf_info));
	if (!pComponent->ports[STREAM_DIR_IN].stAppV4lBuf)
	{
		printf("warning: %s() alloc stream buffer failed.\n", __FUNCTION__);
		ret_err = 31;
		g_unCtrlCReceived = 1;
		goto FUNC_EXIT;
	}
	stAppV4lBuf = pComponent->ports[STREAM_DIR_IN].stAppV4lBuf;

	lErr = query_buffer(pComponent, &pComponent->ports[STREAM_DIR_IN], V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE, stV4lPlanes);
	if (lErr < 0)
	{
		ret_err = 32;
		g_unCtrlCReceived = 1;
		goto FUNC_EXIT;
	}

	pComponent->ports[STREAM_DIR_IN].opened = 1;
	
STREAMIN_START:	
	printf("%s() [\n", __FUNCTION__);
	pComponent->ports[STREAM_DIR_IN].done_flag = 0;
	seek_flag = 1;
	qbuf_times = 0;
	gettimeofday(&start,NULL);

	/***********************************************
	** 1> Open output file descriptor
	***********************************************/
	if (pComponent->ports[STREAM_DIR_IN].eMediaType == MEDIA_FILE_IN)
	{
		fpInput = fopen(pComponent->ports[STREAM_DIR_IN].pszNameOrAddr, "r");
		if (fpInput == NULL)
		{
			printf("%s() error: Unable to open file %s.\n", __FUNCTION__, pComponent->ports[STREAM_DIR_IN].pszNameOrAddr);
			g_unCtrlCReceived = 1;
			ret_err = 33;
			goto FUNC_EXIT;
		}
			else
			{
				printf("Testing stream: %s\n",pComponent->ports[STREAM_DIR_IN].pszNameOrAddr);
				fseek(fpInput, 0, SEEK_END);
				file_size = ftell(fpInput);
				fseek(fpInput, 0, SEEK_SET);
			}
	}
	else
	{
		printf("%s() Unknown media type %d.\n", __FUNCTION__, pComponent->ports[STREAM_DIR_IN].eMediaType);
		g_unCtrlCReceived = 1;
		ret_err = 34;
		goto FUNC_EXIT;
	}

	//  stream on v4l2 output
    stream_type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
    lErr = ioctl(pComponent->hDev, VIDIOC_STREAMON, &stream_type);
    if (!lErr)
    {
		pComponent->ports[STREAM_DIR_IN].unCtrlCReceived = 0;
    }
    else
    {
    	printf("%s() VIDIOC_STREAMON V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE failed errno(%d) %s\n", __FUNCTION__, errno, strerror(errno));
		g_unCtrlCReceived = 1;
		ret_err = 35;
    	goto FUNC_END;
    }

	while(pComponent->ports[STREAM_DIR_OUT].done_flag && !g_unCtrlCReceived)
	{
		usleep(10);
	}

	/***********************************************
	** 2> Stream on
	***********************************************/
	while (!g_unCtrlCReceived && !pComponent->ports[STREAM_DIR_IN].unCtrlCReceived)
	{
		if(frame_done || pComponent->ports[STREAM_DIR_OUT].done_flag)
		{
			break;
		}
		
		if(seek_flag)
		{
			for(i = 0; i < pComponent->ports[STREAM_DIR_IN].buf_count; i++)
			{
				stAppV4lBuf[i].sent = 0;
			}
			seek_flag = 0;
		}

		int buf_avail = 0;

		/***********************************************
		** DQBUF, get buffer from driver
		***********************************************/
		for (i = 0; i < pComponent->ports[STREAM_DIR_IN].buf_count; i++)
		{
			if (!stAppV4lBuf[i].sent)
			{
                buf_avail = 1;
                break;
            }
		}

        if (!buf_avail)
        {
            FD_ZERO(&fds);
            FD_SET(pComponent->hDev, &fds);

            // Timeout
            tv.tv_sec = 2;
            tv.tv_usec = 0;

            r = select(pComponent->hDev + 1, NULL, &fds, NULL, &tv);

            if (-1 == r) 
            {
                fprintf(stderr, "%s() select errno(%d)\n", __FUNCTION__, errno);
                continue;
            }
            if (0 == r) 
            {
                continue;
            }

			stV4lBuf.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
			stV4lBuf.memory = V4L2_MEMORY_MMAP;
            stV4lBuf.m.planes = stV4lPlanes;
	        stV4lBuf.length = 1;
			lErr = ioctl(pComponent->hDev, VIDIOC_DQBUF, &stV4lBuf);
			if (!lErr)
			{
				stAppV4lBuf[stV4lBuf.index].sent = 0;
			}
        }

		if (lErr)
		{
			if (errno == EAGAIN)
			{
				lErr = 0;
			}
		}

		/***********************************************
		** get empty buffer and read data
		***********************************************/
		pstV4lBuf = NULL;

		for (i = 0; i < pComponent->ports[STREAM_DIR_IN].buf_count; i++)
		{
			if (!stAppV4lBuf[i].sent)
			{
				pstV4lBuf = &stAppV4lBuf[i].stV4lBuf;
				break;
			}
		}

		if (pstV4lBuf)
		{
			char            *pBuf;
            unsigned int    block_size;

RETRY:
			if (pComponent->ports[STREAM_DIR_IN].eMediaType == MEDIA_FILE_IN)
			{ 
				for (i = 0; i < pstV4lBuf->length; i++)
            	{
					pBuf = stAppV4lBuf[pstV4lBuf->index].addr[i];
					block_size = stAppV4lBuf[pstV4lBuf->index].size[i];
					pstV4lBuf->m.planes[i].bytesused = fread((void*)pBuf, 1, block_size, fpInput);
					pstV4lBuf->m.planes[i].data_offset = 0;
					file_size -= pstV4lBuf->m.planes[i].bytesused;
					if (V4L2_MEMORY_MMAP == pComponent->ports[STREAM_DIR_IN].memory)
					{
						msync((void*)pBuf, block_size, MS_SYNC);
					}
				}					
			}

            total = 0;
            ulIOBlockSize = 0;
            for (i = 0; i < pstV4lBuf->length; i++)
            {
                total += pstV4lBuf->m.planes[i].bytesused;
                ulIOBlockSize += stAppV4lBuf[pstV4lBuf->index].size[i];
            }

			if ((pComponent->ports[STREAM_DIR_IN].eMediaType == MEDIA_FILE_IN) &&
				(total != ulIOBlockSize)
				)
			{
				if ((pComponent->ports[STREAM_DIR_IN].portType == COMPONENT_PORT_COMP_IN) ||
					(pComponent->ports[STREAM_DIR_IN].portType == COMPONENT_PORT_YUV_IN)
					)
				{
					pComponent->ports[STREAM_DIR_IN].unCtrlCReceived = 1;
				}
			}

			if (total != 0)
			{
				/***********************************************
				** QBUF, put data to driver
				***********************************************/
				lErr = ioctl(pComponent->hDev, VIDIOC_QBUF, pstV4lBuf);
				if (lErr)
				{
					if (errno == EAGAIN)
					{
					    printf("\n");
						lErr = 0;
					}
                    else
                    {
					    printf("v4l2_buf index(%d) type(%d) memory(%d) sequence(%d) length(%d) planes(%p)\n",
                            pstV4lBuf->index,
                            pstV4lBuf->type,
                            pstV4lBuf->memory,
                            pstV4lBuf->sequence,
                            pstV4lBuf->length,
                            pstV4lBuf->m.planes
                            );
					}
				}
				else
				{
					stAppV4lBuf[pstV4lBuf->index].sent = 1;
					qbuf_times++;
				}
			}
			else
			{
				if (pComponent->ports[STREAM_DIR_IN].unCtrlCReceived)
				{
					printf("\n\n%s() CTRL+C received.\n", __FUNCTION__);
					break;
				}
				if (file_size == 0)
				{
					file_size = -1;
					break;													
				}
				usleep(10);
				goto RETRY;
			}
		}		
		usleep(10);
	}

FUNC_END:
	printf("\n%s() ]\n", __FUNCTION__);
	if (fpInput)
	{
		fclose(fpInput);
	}
    
	pComponent->ports[STREAM_DIR_IN].unCtrlCReceived = 1;

	printf("stream in: qbuf_times= %d\n",qbuf_times);
	if (!g_unCtrlCReceived && !frame_done)
	{
		v4l2cmd.cmd = V4L2_DEC_CMD_STOP;
		lErr = ioctl(pComponent->hDev, VIDIOC_DECODER_CMD, &v4l2cmd);
		if (lErr)
		{
			printf("warning: %s() VIDIOC_DECODER_CMD has error, errno(%d), %s \n", __FUNCTION__, errno, strerror(errno));
			g_unCtrlCReceived = 1;
			ret_err = 36;
		}
	    else
		{
			printf("%s() sent cmd: V4L2_DEC_CMD_STOP\n", __FUNCTION__);
		}
	}
	pComponent->ports[STREAM_DIR_IN].done_flag = 1;
	while(!pComponent->ports[STREAM_DIR_OUT].done_flag && !g_unCtrlCReceived)
	{
		usleep(1000);
	}
	
	//Cannot streamoff unitl current stream out thread is done.
	stream_type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
	lErr = ioctl(pComponent->hDev, VIDIOC_STREAMOFF, &stream_type);
	if (lErr)
	{
		printf("warning: %s() VIDIOC_STREAMOFF has error, errno(%d), %s \n", __FUNCTION__, errno, strerror(errno));
		g_unCtrlCReceived = 1;
	}
    else
	{
		printf("%s() sent cmd: VIDIOC_STREAMOFF\n", __FUNCTION__);
	}

	loopTimes--;	
	if(!g_unCtrlCReceived)
	{
		if((loopTimes) > 0)
		{			
			goto STREAMIN_START;
		}
	}

FUNC_EXIT:

	release_buffer(&pComponent->ports[STREAM_DIR_IN]);
	stAppV4lBuf = NULL;
	pComponent->ports[STREAM_DIR_IN].opened = ZOE_FALSE;
}

static int set_ctrl(int fd, int id, int value)
{
	struct v4l2_queryctrl qctrl;
	struct v4l2_control ctrl;
	int ret;

	memset(&qctrl, 0, sizeof(qctrl));
	qctrl.id = id;
	ret = ioctl(fd, VIDIOC_QUERYCTRL, &qctrl);
	if (ret) {
		printf("query ctrl(%d) fail, %s\n", id, strerror(errno));
		return ret;
	}

	memset(&ctrl, 0, sizeof(ctrl));
	ctrl.id = id;
	ctrl.value = value;
	ret = ioctl(fd, VIDIOC_S_CTRL, &ctrl);
	if (ret) {
		printf("VIDIOC_S_CTRL(%s : %d) fail, %s\n",
		       qctrl.name, value, strerror(errno));
		return ret;
	}

	return 0;
}
#define MAX_SUPPORTED_COMPONENTS	5

#define HAS_ARG_NUM(argc, argnow, need) ((nArgNow + need) < argc)

int main(int argc,
		 char* argv[]
		 )
{
	int			                lErr = 0;
	int			                nArgNow = 1;
	int			                nCmdIdx = 0;
	int			                nHas2ndCmd;
	component_t	                component[MAX_SUPPORTED_COMPONENTS];
    component_t                 *pComponent;
	unsigned int			    i, j;
    uint32_t                    type = COMPONENT_TYPE_DECODER;  //COMPOENT_TYPE

    struct v4l2_event_subscription  sub;
    struct v4l2_event           evt;

    int                         r;
	struct pollfd               p_fds;
	int                         wait_pollpri_times;
	int                         int_tmp;
	signal(SIGINT, SigIntHanlder);
	signal(SIGSTOP, SigStopHanlder);
	signal(SIGCONT, SigContHanlder);

	memset(&component[0],
		   0,
		   MAX_SUPPORTED_COMPONENTS * sizeof(component_t)
		   );

HAS_2ND_CMD:

	nHas2ndCmd = 0;

	component[nCmdIdx].busType = -1;
	component[nCmdIdx].ports[STREAM_DIR_OUT].eMediaType = MEDIA_NULL_OUT;
	component[nCmdIdx].ports[STREAM_DIR_IN].fmt = 0xFFFFFFFF;
	component[nCmdIdx].ports[STREAM_DIR_OUT].fmt = 0xFFFFFFFF;
	component[nCmdIdx].ports[STREAM_DIR_IN].pszNameOrAddr = NULL;
	pComponent = &component[nCmdIdx];

	if(argc >= 2 && strstr(argv[1],"help"))
	{
		showUsage();
		return 0;
	}
	
    while (nArgNow < argc)
	{
		if (!strcasecmp(argv[nArgNow],"IFILE"))
		{
			if (!HAS_ARG_NUM(argc, nArgNow, 1))
            {
				break;
            }
			nArgNow++;
			component[nCmdIdx].ports[STREAM_DIR_IN].eMediaType = MEDIA_FILE_IN;
			component[nCmdIdx].ports[STREAM_DIR_IN].pszNameOrAddr = malloc(sizeof(char)*(strlen(argv[nArgNow])+1));
			memset(component[nCmdIdx].ports[STREAM_DIR_IN].pszNameOrAddr, 0x00, sizeof(char)*(strlen(argv[nArgNow])+1));
			memcpy(component[nCmdIdx].ports[STREAM_DIR_IN].pszNameOrAddr, argv[nArgNow], strlen(argv[nArgNow]));
			if (!HAS_ARG_NUM(argc, nArgNow, 1))
            {
				break;
            }
			nArgNow++;
		}
		else if (!strcasecmp(argv[nArgNow],"OFILE"))
		{
			if (!HAS_ARG_NUM(argc, nArgNow, 1))
            {
				break;
            }
			nArgNow++;
			if(strcasecmp(argv[nArgNow], "NONE"))
			{
				component[nCmdIdx].ports[STREAM_DIR_OUT].eMediaType = MEDIA_FILE_OUT;
				component[nCmdIdx].ports[STREAM_DIR_OUT].pszNameOrAddr = malloc(sizeof(char)*(strlen(argv[nArgNow])+1));
				memset(component[nCmdIdx].ports[STREAM_DIR_OUT].pszNameOrAddr, 0x00, sizeof(char)*(strlen(argv[nArgNow])+1));
				memcpy(component[nCmdIdx].ports[STREAM_DIR_OUT].pszNameOrAddr, argv[nArgNow], strlen(argv[nArgNow]));
			}
			if (!HAS_ARG_NUM(argc, nArgNow, 1))
            {
				break;
            }
			nArgNow++;
		}
		else if (!strcasecmp(argv[nArgNow],"NULL"))
		{
			if (!HAS_ARG_NUM(argc, nArgNow, 1))
            {
				break;
            }
			nArgNow++;
			component[nCmdIdx].ports[STREAM_DIR_OUT].eMediaType = MEDIA_NULL_OUT;
		}
		else if (!strcasecmp(argv[nArgNow],"IFMT"))
		{
			if (!HAS_ARG_NUM(argc, nArgNow, 1))
            {
				break;
            }
			nArgNow++;
			if(isNumber(argv[nArgNow]))
				component[nCmdIdx].ports[STREAM_DIR_IN].fmt = atoi(argv[nArgNow++]);   //uint32_t
        }
		else if (!strcasecmp(argv[nArgNow],"OFMT"))
		{
			if (!HAS_ARG_NUM(argc, nArgNow, 1))
            {
				break;
            }
			nArgNow++;
			if(isNumber(argv[nArgNow]))
			{
				int_tmp = atoi(argv[nArgNow]);
				if (int_tmp >=  0 && int_tmp <= (ZPU_NUM_FORMATS_YUV - 1))
				{
					component[nCmdIdx].ports[STREAM_DIR_OUT].fmt = formats_yuv[int_tmp];
				}
				else
				{
					printf("error: %s() the specified output format is not support yet. \n", __FUNCTION__);
					ret_err = 1;
					goto FUNC_END;
				}
				if (!HAS_ARG_NUM(argc, nArgNow, 1))
				{
					break;
				}
				nArgNow++;
			}
        }
		else if(!strcasecmp(argv[nArgNow],"BS"))
		{
			if(!HAS_ARG_NUM(argc,nArgNow,1))
			{
				break;
			}
			nArgNow++;
			if(isNumber(argv[nArgNow]))
			{
				unsigned int fs = abs(atoi(argv[nArgNow++]));
				if(fs > 7900)
				{
					fs = 7900;
					printf("The maximum input buffer block size is 7900.\n");
				}
				component[nCmdIdx].ports[STREAM_DIR_IN].frame_size = fs*1024+1;				
			}
		}
		else if(!strcasecmp(argv[nArgNow],"IQC"))
		{
			if(!HAS_ARG_NUM(argc,nArgNow,1))
			{
				break;
			}
			nArgNow++;
			if(isNumber(argv[nArgNow]))
			{
				unsigned int iqc = abs(atoi(argv[nArgNow++]));
				if(iqc < 2)
				{
					iqc = 2;
					printf("The minimum qbuf count is 2.\n");
				}
				else if(iqc > 32)
				{
					iqc = 32;
					printf("The maximum qbuf count is 32.\n");
				}
				component[nCmdIdx].ports[STREAM_DIR_IN].buf_count = iqc;
			}
		}
		else if(!strcasecmp(argv[nArgNow],"OQC"))
		{
			if(!HAS_ARG_NUM(argc,nArgNow,1))
			{
				break;
			}
			nArgNow++;
			if(isNumber(argv[nArgNow]))
			{
				unsigned int oqc = abs(atoi(argv[nArgNow++]));
				if(oqc < 2)
				{
					oqc = 2;
					printf("The minimum qbuf count is 2.\n");
				}
				else if(oqc > 32)
				{
					oqc = 32;
					printf("The maximum qbuf count is 32.\n");
				}
				component[nCmdIdx].ports[STREAM_DIR_OUT].buf_count = oqc;
			}
		}
		else if(!strcasecmp(argv[nArgNow],"FRAMES"))
		{
			if(!HAS_ARG_NUM(argc,nArgNow,1))
			{
				break;
			}
			nArgNow++;
			if(isNumber(argv[nArgNow]))
				component[nCmdIdx].ports[STREAM_DIR_OUT].outFrameCount = atoi(argv[nArgNow++]);
		}
		else if(!strcasecmp(argv[nArgNow],"LOOP"))
		{
			initLoopTimes = preLoopTimes = loopTimes = 10000;
			if(!HAS_ARG_NUM(argc,nArgNow,1))
			{
				break;
			}
			nArgNow++;
			if(isNumber(argv[nArgNow]))
				loopTimes = atoi(argv[nArgNow++]);
			initLoopTimes = preLoopTimes = loopTimes;
		}
		else if (!strcasecmp(argv[nArgNow], "DEV"))
		{
			if(!HAS_ARG_NUM(argc,nArgNow,1))
			{
				break;			
			}
			nArgNow++;
			memset(component[nCmdIdx].szDevName, 0x00, sizeof(component[nCmdIdx].szDevName));
			strcpy(component[nCmdIdx].szDevName, argv[nArgNow++]);
		}
		else if (!strcasecmp(argv[nArgNow],"+"))
		{
            if (!HAS_ARG_NUM(argc, nArgNow, 1))
            {
				break;
            }
			nArgNow++;
            nHas2ndCmd = 1;
            break;
        }
		else
		{
            if (!HAS_ARG_NUM(argc, nArgNow, 1))
            {
				break;
            }
			nArgNow++;
        }    
	}
	if(component[nCmdIdx].ports[STREAM_DIR_IN].pszNameOrAddr == NULL
			|| component[nCmdIdx].ports[STREAM_DIR_IN].fmt == 0xFFFFFFFF
			|| component[nCmdIdx].ports[STREAM_DIR_OUT].fmt == 0xFFFFFFFF
	  )
	{
		printf("INPUT ERROR.\n\
Usage:\n\
	./mxc_v4l2_vpu_dec.out ifile decode.264 ifmt 1 ofmt 1 ofile test.yuv\n\n\
	./mxc_v4l2_vpu_dec.out ifile decode.264 ifmt 1 ofmt 1 ofile test.yuv\n\n\
	./mxc_v4l2_vpu_dec.out ifile decode.bit ifmt 13 ofmt 1 loop\n\n\
Or reference the usage manual.\n\
	./mxc_v4l2_vpu_dec.out --help \n\
	");
		ret_err = 1;
		goto FUNC_END;
	}

	if (strstr(component[nCmdIdx].szDevName, "/dev/video"))
	{
		printf("=====  select device =====\n");
		// check the select device 
		lErr = check_video_device(component[nCmdIdx].devInstance,
				           &component[nCmdIdx].busType,
				           &type,
				           component[nCmdIdx].szDevName
				          );
		if (lErr)
		{
			printf("%s(): error: The selected device(%s) does not match VPU decoder!\nplease select: /dev/video12\n", 
					__FUNCTION__, component[nCmdIdx].szDevName);
			ret_err = 2;
			goto FUNC_END;
		}
	}
	else 
	{
		// lookup and open the device
		lErr = lookup_video_device_node(component[nCmdIdx].devInstance,
										&component[nCmdIdx].busType,
	                                    &type,
										component[nCmdIdx].szDevName
										);
		if (lErr)
		{
			printf("%s() error: Unable to find device.\n", __FUNCTION__);
			ret_err = 3;
			goto FUNC_END;
		}
	}
	component[nCmdIdx].hDev = open(component[nCmdIdx].szDevName,
								   O_RDWR
								   );
	if (component[nCmdIdx].hDev <= 0)
	{
		printf("%s() error: Unable to Open %s.\n", __FUNCTION__, component[nCmdIdx].szDevName);
		ret_err = 4;
		goto FUNC_END;
	}


	// get the configuration
	lErr = zvconf(&component[nCmdIdx],
				  component[nCmdIdx].pszScriptName,
                  type
				  );
	if (lErr)
	{
		printf("%s() error: Unable to config device.\n", __FUNCTION__);
		ret_err = 5;
		goto FUNC_END;
	}

    // subsribe v4l2 events
    memset(&sub, 0, sizeof(struct v4l2_event_subscription));
    sub.type = V4L2_EVENT_SOURCE_CHANGE;
    lErr = ioctl(pComponent->hDev, VIDIOC_SUBSCRIBE_EVENT, &sub);
	if (lErr)
	{
		printf("%s() VIDIOC_SUBSCRIBE_EVENT(V4L2_EVENT_SOURCE_CHANGE) ioctl failed %d %s\n", __FUNCTION__, errno, strerror(errno));
		ret_err = 6;
		goto FUNC_END;
	}
	memset(&sub, 0, sizeof(struct v4l2_event_subscription));
	sub.type = V4L2_EVENT_EOS;
	lErr = ioctl(pComponent->hDev, VIDIOC_SUBSCRIBE_EVENT, &sub);
	if (lErr)
	{
		printf("%s() VIDIOC_SUBSCRIBE_EVENT(V4L2_EVENT_EOS) ioctl failed %d %s\n", __FUNCTION__, errno, strerror(errno));
		ret_err = 7;
		goto FUNC_END;
	}
	memset(&sub, 0, sizeof(struct v4l2_event_subscription));
	sub.type = V4L2_EVENT_DECODE_ERROR;
	lErr = ioctl(pComponent->hDev, VIDIOC_SUBSCRIBE_EVENT, &sub);
	if (lErr)
	{
		printf("%s() VIDIOC_SUBSCRIBE_EVENT(V4L2_EVENT_DECODE_ERROR) ioctl failed %d %s\n",
			__FUNCTION__, errno, strerror(errno));
		ret_err = 7;
		goto FUNC_END;
	}

	set_ctrl(pComponent->hDev, V4L2_CID_USER_RAW_BASE, 1);
	set_ctrl(pComponent->hDev, V4L2_CID_USER_STREAM_INPUT_MODE, NON_FRAME_LVL);

	lErr = pthread_create(&pComponent->ports[STREAM_DIR_IN].threadId,
			NULL,
			(void *)test_streamin,
			pComponent
			);
	if (!lErr)
	{
		pComponent->ports[STREAM_DIR_IN].ulThreadCreated = 1;
	}
	else
	{
		printf("%s() pthread create failed, threadId: %lu \n", __FUNCTION__, pComponent->ports[STREAM_DIR_IN].threadId);
		ret_err = 8;
		goto FUNC_END;
	}

	// wait for 10 msec
	usleep(10000);

    // wait for resoltion change
    p_fds.fd = pComponent->hDev;
    p_fds.events = POLLPRI;

	wait_pollpri_times = 0;
	if (!g_unCtrlCReceived)
	{
		while (!g_unCtrlCReceived)
		{
			r = poll(&p_fds, 1, 2000);
			if (-1 == r)
			{
				fprintf(stderr, "%s() select errno(%d)\n", __FUNCTION__, errno);
				g_unCtrlCReceived = 1;
				ret_err = 9;
				goto FUNC_END;
			}
			else if (0 == r)
			{
				wait_pollpri_times++;
				}
			else
			{
				if (p_fds.revents & POLLPRI)
				{
					break;
				}
				else
				{
					sleep(1);
					wait_pollpri_times++;
				}
			}
			if (wait_pollpri_times >= 30)
			{
				printf("error: %s(), waiting for the POLLPRI event response timeout.\n", __FUNCTION__);
				g_unCtrlCReceived = 1;
				ret_err = 10;
				goto FUNC_END;
			}
		}
	}

	if (g_unCtrlCReceived)
	{
		goto FUNC_END;
	}

#ifdef DQEVENT
    memset(&evt, 0, sizeof(struct v4l2_event));
    lErr = ioctl(pComponent->hDev, VIDIOC_DQEVENT, &evt);
	if (lErr)
	{
		printf("%s() VIDIOC_DQEVENT ioctl failed %d %s\n", __FUNCTION__, errno, strerror(errno));
		ret_err = 11;
		g_unCtrlCReceived = 1;
		goto FUNC_END;
	}
    else
    {
        switch (evt.type)
        {
            case V4L2_EVENT_SOURCE_CHANGE:
		        printf("%s() VIDIOC_DQEVENT V4L2_EVENT_SOURCE_CHANGE changes(0x%x) pending(%d) seq(%d)\n", __FUNCTION__, evt.u.src_change.changes, evt.pending, evt.sequence);
                break;
            case V4L2_EVENT_EOS:
		        printf("%s() VIDIOC_DQEVENT V4L2_EVENT_EOS pending(%d) seq(%d)\n", __FUNCTION__, evt.pending, evt.sequence);
                break;
            case V4L2_EVENT_DECODE_ERROR:
		        printf("%s() VIDIOC_DQEVENT V4L2_EVENT_DECODE_ERROR pending(%d) seq(%d)\n", __FUNCTION__, evt.pending, evt.sequence);
			g_unCtrlCReceived = 1;
			goto FUNC_END;
                break;
            default:
		        printf("%s() VIDIOC_DQEVENT unknown event(%d)\n", __FUNCTION__, evt.type);
                break;
        }
    }
#endif

	lErr = pthread_create(&pComponent->ports[STREAM_DIR_OUT].threadId,
			NULL,
			(void *)test_streamout,
			pComponent
			);
	if (!lErr)
	{
		pComponent->ports[STREAM_DIR_OUT].ulThreadCreated = 1;
	}
	else
	{
		printf("%s() pthread create failed, threadId: %lu \n", __FUNCTION__, pComponent->ports[STREAM_DIR_OUT].threadId);
		ret_err = 12;
		g_unCtrlCReceived = 1;
		goto FUNC_END;
	}
	// wait for 10 msec
	usleep(10000);

	if (nHas2ndCmd)
	{
		nCmdIdx++;
		goto HAS_2ND_CMD;
	}

	// wait for user input
CHECK_USER_INPUT:
	while ((g_unCtrlCReceived == 0) && !kbhit())
	{
		usleep(1000);
	}

	if (!g_unCtrlCReceived) {
		usleep(1000);
		goto CHECK_USER_INPUT;
	}

FUNC_END:
	// wait for 100 msec
	usleep(100000);
	for (i = 0; i < MAX_SUPPORTED_COMPONENTS; i++)
	{
		for (j = 0; j < MAX_STREAM_DIR; j++)
		{
			if (component[i].ports[j].ulThreadCreated)
			{
				lErr = pthread_join(component[i].ports[j].threadId,
							 NULL
							 );
				if(lErr)
				{
					printf("warning: %s() pthread_join failed: errno(%d)  \n", __FUNCTION__, lErr);
					ret_err = 13;
				}

				component[i].ports[j].ulThreadCreated = 0;
			}
		}
	}

	// unsubscribe v4l2 events
	memset(&sub, 0, sizeof(struct v4l2_event_subscription));
	for (i = 0; i < MAX_SUPPORTED_COMPONENTS; i++)
	{
		if (component[i].hDev > 0)
		{
            sub.type = V4L2_EVENT_ALL;
            lErr = ioctl(component[i].hDev, VIDIOC_UNSUBSCRIBE_EVENT, &sub);
	        if (lErr)
	        {
		        printf("%s() VIDIOC_UNSUBSCRIBE_EVENT ioctl failed %d %s\n", __FUNCTION__, errno, strerror(errno));
	        }
        }
    }

	for (i = 0; i < MAX_SUPPORTED_COMPONENTS; i++)
	{
		if(component[i].ports[STREAM_DIR_IN].pszNameOrAddr)
		{
			free(component[i].ports[STREAM_DIR_IN].pszNameOrAddr);
		}
		if(component[i].ports[STREAM_DIR_OUT].pszNameOrAddr)
		{
			free(component[i].ports[STREAM_DIR_OUT].pszNameOrAddr);
		}

		// close device
		if (component[i].hDev > 0)
		{
			close(component[i].hDev);
			component[i].hDev = 0;
		}

	}

	printf("\nEND.\t loop_times=%d\n",(initLoopTimes - loopTimes));

	return (ret_err);
}

