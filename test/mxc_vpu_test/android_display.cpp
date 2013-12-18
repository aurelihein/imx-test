/*
 *  Copyright (C) 2013 Freescale Semiconductor, Inc.
 *  All Rights Reserved.
 *
 *  The following programs are the sole property of Freescale Semiconductor Inc.,
 *  and contain its proprietary and confidential information.
 *
 */

/*
 *	android_display.cpp
 *	implement video display based on g2d renderer
 *	History :
 *	Date(y.m.d)        Author            Version        Description
 *	2013-04-10         Li Xianzhong      0.1            Created
*/
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <sys/types.h>
#include <ui/FramebufferNativeWindow.h>
#include <hardware/gralloc.h>

#include <utils/Log.h>
#include <gui/Surface.h>
#include <gui/SurfaceComposerClient.h>

#ifndef LOGE
#define LOGE ALOGE
#endif

extern "C" {
#include "vpu_test.h"
    extern int vpu_v4l_performance_test;
    struct vpu_display *android_display_open(struct decode *dec, int nframes, struct rot rotation, Rect cropRect);
    int android_put_data(struct vpu_display *disp, int index, int field, int fps);
    void android_display_close(struct vpu_display *disp);
}

struct private_handle_t {

    native_handle_t nativeHandle;

    /* file-descriptors */
    int     fd;
    /* ints */
    int     magic;
    int     flags;
    int     size;
    int     offset;

    int     base;

    int     phys;
    int     width;
    int     height;
    int     format;

    int     pid;
};

static __inline int queue_size(struct buf_queue * q)
{
    if (q->tail >= q->head)
        return (q->tail - q->head);
    else
        return ((q->tail + QUEUE_SIZE) - q->head);
}

static __inline int queue_buf(struct buf_queue * q, int idx)
{
    if (((q->tail + 1) % QUEUE_SIZE) == q->head)
        return -1;      /* queue full */
    q->list[q->tail] = idx;
    q->tail = (q->tail + 1) % QUEUE_SIZE;
    return 0;
}

static __inline int dequeue_buf(struct buf_queue * q)
{
    int ret;
    if (q->tail == q->head)
         return -1;      /* queue empty */
    ret = q->list[q->head];
    q->head = (q->head + 1) % QUEUE_SIZE;
    return ret;
}

static __inline int peek_next_buf(struct buf_queue * q)
{
    if (q->tail == q->head)
        return -1;      /* queue empty */
    return q->list[q->head];
}

extern int quitflag;
static int g2d_waiting = 0;
static int g2d_running = 0;
static pthread_cond_t g2d_cond;
static pthread_mutex_t g2d_mutex;
static inline void wait_queue()
{
    pthread_mutex_lock(&g2d_mutex);
    g2d_waiting = 1;
    pthread_cond_wait(&g2d_cond, &g2d_mutex);
    g2d_waiting = 0;
    pthread_mutex_unlock(&g2d_mutex);
}

static inline void wakeup_queue()
{
    pthread_cond_signal(&g2d_cond);
}

struct android_display_context {
    android::sp<android::SurfaceComposerClient> g2d_client;
    android::sp<android::SurfaceControl> g2d_control;
    android::sp<android::Surface> g2d_surface;
};

extern "C" struct ANativeWindow *android_get_display_surface(struct android_display_context *dispctx, int disp_left, int disp_top, int disp_width, int disp_height);
void *android_disp_loop_thread(void *arg)
{
    int fence = -1;
    void *vaddr = NULL;
    pthread_attr_t attr;
    void *g2d_handle = NULL;
    struct g2d_surface src,dst;
    hw_module_t const* pModule = NULL;
    struct private_handle_t *hnd = NULL;
    gralloc_module_t *gralloc_module = NULL;
    struct ANativeWindowBuffer *native_buf = NULL;
    struct decode *dec = (struct decode *)arg;
    DecHandle handle = dec->handle;
    struct vpu_display *disp = dec->disp;
    int width = dec->picwidth;
    int height = dec->picheight;
    int left = dec->picCropRect.left;
    int top = dec->picCropRect.top;
    int right = dec->picCropRect.right;
    int bottom = dec->picCropRect.bottom;
    int disp_width = dec->cmdl->width;
    int disp_height = dec->cmdl->height;
    int disp_left =  dec->cmdl->loff;
    int disp_top =  dec->cmdl->toff;
    int index = -1, err, mode, disp_format = 0;
    struct ANativeWindow *native_win = NULL;
    struct android_display_context dispctx;

    if(disp_width == 0 || disp_height == 0)
    {
         native_win = (struct ANativeWindow *)android_createDisplaySurface();
    }
    else
    {
         if(dec->cmdl->rot_en && (dec->cmdl->rot_angle == 90 || dec->cmdl->rot_angle == 270))
         {
            int temp    = disp_width;
            disp_width  = disp_height;
            disp_height = temp;
         }

         native_win = android_get_display_surface(&dispctx, disp_left, disp_top, disp_width, disp_height);
    }

    if (native_win == NULL) {
        err_msg("failed to create display surface window \n");
        return NULL;
    }

    if(g2d_open(&g2d_handle))
    {
         err_msg("gpu_dev_open fail. \n");
         return NULL;
    }

    g2d_running = 1;

    if (!disp_width || !disp_height) {
         native_win->query(native_win, NATIVE_WINDOW_WIDTH, &disp_width);
         native_win->query(native_win, NATIVE_WINDOW_HEIGHT, &disp_height);
    }

    native_win->query(native_win, NATIVE_WINDOW_FORMAT, &disp_format);

    hw_get_module(GRALLOC_HARDWARE_MODULE_ID, &pModule);
    gralloc_module = (gralloc_module_t *)(pModule);

    pthread_attr_init(&attr);
    pthread_attr_setschedpolicy(&attr, SCHED_RR);
    while(1) {

        while(sem_trywait(&disp->avaiable_dequeue_frame) != 0) {
           usleep(1000);
           if(quitflag) break;
        }

        if(quitflag) break;

        index = dequeue_buf(&(disp->g2d_buf_q));
        if (index < 0) {
            info_msg("thread is going to finish\n");
            break;
        }

        native_win->dequeueBuffer(native_win, &native_buf, &fence);
        native_buf->common.incRef(&native_buf->common);

        if (gralloc_module->lock(gralloc_module, native_buf->handle,
                                    GRALLOC_USAGE_HW_RENDER  |
                                    GRALLOC_USAGE_SW_WRITE_OFTEN,
                                    0, 0, native_buf->width, native_buf->height, &vaddr))
        {
            err_msg("gralloc failed to lock the buffer \n");
            break;
        }

        hnd = (struct private_handle_t *)native_buf->handle;

        if (dec->cmdl->chromaInterleave == 0) {
            src.format = G2D_I420;
            src.planes[0] = disp->g2d_bufs[index]->buf_paddr;
            src.planes[1] = disp->g2d_bufs[index]->buf_paddr + width * height;
            src.planes[2] = src.planes[1] + width * height / 4;
        }
        else {
            src.format = G2D_NV12;
            src.planes[0] = disp->g2d_bufs[index]->buf_paddr;
            src.planes[1] = disp->g2d_bufs[index]->buf_paddr + width * height;
            src.planes[2] = 0;
        }
        src.left = 0;
        src.top = 0;
        src.right = width;
        src.bottom = height;
        src.stride = width;
        src.rot    = G2D_ROTATION_0;

        switch(disp_format)
        {
        case HAL_PIXEL_FORMAT_RGB_565:
            dst.format = G2D_RGB565;
            break;
        case HAL_PIXEL_FORMAT_RGBA_8888:
        case HAL_PIXEL_FORMAT_RGBX_8888:
            dst.format = G2D_RGBX8888;
            break;
        case HAL_PIXEL_FORMAT_BGRA_8888:
            dst.format = G2D_BGRX8888;
            break;
        default:
            dst.format = G2D_RGB565;
            break;
        }
        dst.planes[0] = hnd->phys;
        dst.left = 0;
        dst.top = 0;
        dst.right = disp_width;
        dst.bottom = disp_height;
        dst.stride = native_buf->width;
        dst.rot    = G2D_ROTATION_0;
        if(dec->cmdl->rot_en)
        {
         switch(dec->cmdl->rot_angle)
         {
         case 90:
            dst.rot    = G2D_ROTATION_90;
            break;
         case 180:
            dst.rot    = G2D_ROTATION_180;
            break;
         case 270:
            dst.rot    = G2D_ROTATION_270;
            break;
         default:
            dst.rot    = G2D_ROTATION_0;
          }
        }

        g2d_blit(g2d_handle, &src, &dst);
        g2d_finish(g2d_handle);
        usleep(1000);

        if (gralloc_module->unlock(gralloc_module, native_buf->handle))
        {
            err_msg("gralloc failed to unlock the buffer \n");
            break;
        }

        native_win->queueBuffer(native_win, native_buf, fence);
        native_buf->common.decRef(&native_buf->common);

        if(quitflag == 0)
        {
            /* Clear the flag after showing */
            err = vpu_DecClrDispFlag(dec->handle, index);
            if (err) {
                err_msg("vpu_DecClrDispFlag failed Error code %d\n", err);
                quitflag = 1;
                break;
            }
        }
        sem_post(&disp->avaiable_decoding_frame);
    }

    g2d_close(g2d_handle);
    g2d_running = 0;

    return NULL;
}

struct vpu_display *
android_display_open(struct decode *dec, int nframes, struct rot rotation, Rect cropRect)
{
    int err = 0, i;
    int width = dec->picwidth;
    int height = dec->picheight;
    struct vpu_display *disp = NULL;

    vpu_v4l_performance_test = 1;

    disp = (struct vpu_display *)calloc(1, sizeof(struct vpu_display));
    if (disp == NULL) {
        err_msg("falied to allocate vpu_display\n");
        return NULL;
    }

    disp->nframes = nframes;
    disp->frame_size = width*height*3/2;

    for (i=0;i<nframes;i++) {
        disp->g2d_bufs[i] = g2d_alloc(disp->frame_size, 0);
        if (disp->g2d_bufs[i] == NULL) {
            err_msg("g2d memory alloc failed\n");
            free(disp);
            return NULL;
        }
    }

    disp->g2d_buf_q.tail = disp->g2d_buf_q.head = 0;
    disp->stopping = 0;

    dec->disp = disp;
    pthread_mutex_init(&g2d_mutex, NULL);
    pthread_cond_init(&g2d_cond, NULL);
    sem_init(&disp->avaiable_decoding_frame, 0,
			    dec->regfbcount - dec->minfbcount);
    sem_init(&disp->avaiable_dequeue_frame, 0, 0);

 //   info_msg("%s %d, sem_init avaiable_decoding_frame count %d  \n", __FUNCTION__, __LINE__, dec->regfbcount - dec->minfbcount);
    /* start disp loop thread */
    pthread_create(&(disp->g2d_disp_loop_thread), NULL, android_disp_loop_thread, (void *)dec);

    return disp;
}

int android_put_data(struct vpu_display *disp, int index, int field, int fps)
{
    if (field == V4L2_FIELD_TOP || field == V4L2_FIELD_BOTTOM ||
       field == V4L2_FIELD_INTERLACED_TB ||
       field == V4L2_FIELD_INTERLACED_BT){
        disp->deinterlaced = 1;
    }

    queue_buf(&(disp->g2d_buf_q), index);
    sem_post(&disp->avaiable_dequeue_frame);

    while(sem_trywait(&disp->avaiable_decoding_frame) != 0) usleep(1000);

    return 0;
}

void android_display_close(struct vpu_display *disp)
{
    int i, err=0;

    quitflag = 1;
    disp->stopping = 1;
    disp->deinterlaced = 0;
    while(g2d_running && ((queue_size(&(disp->g2d_buf_q)) > 0) || !g2d_waiting)) usleep(10000);
    if (g2d_running) {
	wakeup_queue();
	info_msg("Join disp loop thread\n");
        pthread_join(disp->g2d_disp_loop_thread, NULL);
    }
    pthread_mutex_destroy(&g2d_mutex);
    pthread_cond_destroy(&g2d_cond);

    for (i=0;i<disp->nframes;i++) {
        err = g2d_free(disp->g2d_bufs[i]);
        if (err != 0) {
            err_msg("%s: g2d memory free failed\n", __FUNCTION__);
            free(disp);
            return;
        }
    }

    free(disp);
    return;
}

using namespace android;
struct ANativeWindow *android_get_display_surface(struct android_display_context *dispctx, int disp_left, int disp_top, int disp_width, int disp_height)
{
    // create a client to surfaceflinger
    dispctx->g2d_client = new SurfaceComposerClient();

    disp_width = (disp_width + 0xf) & ~0xf;

    dispctx->g2d_control = dispctx->g2d_client->createSurface(String8("VPU Renderer Surface"), disp_width, disp_height, PIXEL_FORMAT_RGB_565, 0);
    if(dispctx->g2d_control == NULL)
    {
        LOGE("%s %d error !!!", __FUNCTION__, __LINE__);
        return NULL;
    }

    dispctx->g2d_surface = dispctx->g2d_control->getSurface();
    if( dispctx->g2d_surface == NULL)
    {
        LOGE("%s %d error !!!", __FUNCTION__, __LINE__);
        return NULL;
    }

    SurfaceComposerClient::openGlobalTransaction();
    dispctx->g2d_control.get()->setPosition(disp_left, disp_top);
    dispctx->g2d_control.get()->setSize(disp_width, disp_height);
    dispctx->g2d_control.get()->setLayer(0x7fffffff);
    SurfaceComposerClient::closeGlobalTransaction();

    native_window_set_usage(dispctx->g2d_surface.get(), GRALLOC_USAGE_SW_READ_NEVER | GRALLOC_USAGE_SW_WRITE_NEVER | GRALLOC_USAGE_FORCE_CONTIGUOUS);
    native_window_set_buffers_geometry(dispctx->g2d_surface.get(),disp_width, disp_height, HAL_PIXEL_FORMAT_RGB_565);
    native_window_set_buffer_count(dispctx->g2d_surface.get(), 3);

    return (struct ANativeWindow *)dispctx->g2d_surface.get();
}
