/*
 * Copyright (C) 2008 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <sys/mman.h>

#include <dlfcn.h>

#include <cutils/ashmem.h>
#include <cutils/log.h>
#include <cutils/properties.h>

#include <hardware/hardware.h>
#include <hardware/gralloc.h>

#include <fcntl.h>
#include <errno.h>
#include <pthread.h>
#include <sys/ioctl.h>
#include <sys/resource.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include <cutils/log.h>
#include <cutils/atomic.h>

#include <linux/fb.h>
#include <linux/msm_mdp.h>

#include <GLES/gl.h>

#include "gralloc_priv.h"
#include "gr.h"

/*****************************************************************************/

// numbers of buffers for page flipping
#define NUM_BUFFERS 2


enum {
    PAGE_FLIP = 0x00000001,
    LOCKED = 0x00000002
};

struct fb_context_t {
    framebuffer_device_t  device;
};

/*****************************************************************************/

static void
msm_copy_buffer(buffer_handle_t handle, int fd,
                int width, int height, int format,
                int x, int y, int w, int h);

static int fb_setSwapInterval(struct framebuffer_device_t* dev,
            int interval)
{
    fb_context_t* ctx = (fb_context_t*)dev;
    if (interval < dev->minSwapInterval || interval > dev->maxSwapInterval)
        return -EINVAL;
    // FIXME: implement fb_setSwapInterval
    return 0;
}

static int fb_setUpdateRect(struct framebuffer_device_t* dev,
        int l, int t, int w, int h)
{
    if (((w|h) <= 0) || ((l|t)<0))
        return -EINVAL;
        
    fb_context_t* ctx = (fb_context_t*)dev;
    private_module_t* m = reinterpret_cast<private_module_t*>(
            dev->common.module);
    m->info.reserved[0] = 0x54445055; // "UPDT";
    m->info.reserved[1] = (uint16_t)l | ((uint32_t)t << 16);
    m->info.reserved[2] = (uint16_t)(l+w) | ((uint32_t)(t+h) << 16);
    return 0;
}

/*
 * Y210: posting asincrono del framebuffer (como el gralloc CAF del stock).
 *
 * El panel es MIPI DSI en modo comando: FBIOPUT_VSCREENINFO no vuelve hasta
 * que la DMA del MDP y el motor de comandos DSI terminaron de mandar el frame
 * al panel (~15 ms, mdp_dma2_update en el kernel). Hacerlo en el hilo de
 * SurfaceFlinger lo bloqueaba todo ese tiempo. Aqui fb_post solo encola el
 * buffer y un hilo propio hace el pan, asi SF compone el siguiente frame
 * mientras el panel recibe el actual.
 *
 * Seguridad: fb_post espera a que el pan anterior haya vuelto antes de
 * soltar el buffer anterior (mismo orden unlock/lock que el camino
 * sincrono). Como el pan solo vuelve con la DMA terminada, el buffer que SF
 * vuelve a dibujar ya esta en la GRAM del panel. Hay un solo pan en vuelo.
 *
 * debug.gr.async_post=0 vuelve al posting sincrono (se lee al abrir fb0).
 */
struct fb_post_queue_t {
    pthread_mutex_t lock;
    pthread_cond_t cond_post;   /* hay un buffer encolado */
    pthread_cond_t cond_idle;   /* el pan en curso termino */
    int fd;
    int pending;                /* 1 = encolado o en pan */
    int quit;
    int enabled;
    pthread_t thread;
    struct fb_var_screeninfo info;
};

static fb_post_queue_t sPost;   /* mutex/conds se inicializan al abrir fb0 */

static void* fb_post_thread(void*)
{
    /* misma prioridad que el hilo de composicion de SF */
    setpriority(PRIO_PROCESS, gettid(), -8 /* ANDROID_PRIORITY_URGENT_DISPLAY */);

    pthread_mutex_lock(&sPost.lock);
    for (;;) {
        while (!sPost.pending && !sPost.quit)
            pthread_cond_wait(&sPost.cond_post, &sPost.lock);
        if (sPost.quit)
            break;
        struct fb_var_screeninfo info = sPost.info;
        pthread_mutex_unlock(&sPost.lock);

        if (ioctl(sPost.fd, FBIOPUT_VSCREENINFO, &info) == -1)
            LOGE("FBIOPUT_VSCREENINFO failed (%s)", strerror(errno));

        pthread_mutex_lock(&sPost.lock);
        sPost.pending = 0;
        pthread_cond_broadcast(&sPost.cond_idle);
    }
    pthread_mutex_unlock(&sPost.lock);
    return NULL;
}

static void fb_post_wait_idle()
{
    pthread_mutex_lock(&sPost.lock);
    while (sPost.pending)
        pthread_cond_wait(&sPost.cond_idle, &sPost.lock);
    pthread_mutex_unlock(&sPost.lock);
}

static int fb_post(struct framebuffer_device_t* dev, buffer_handle_t buffer)
{
    if (private_handle_t::validate(buffer) < 0)
        return -EINVAL;

    fb_context_t* ctx = (fb_context_t*)dev;

    private_handle_t const* hnd = reinterpret_cast<private_handle_t const*>(buffer);
    private_module_t* m = reinterpret_cast<private_module_t*>(
            dev->common.module);

    /* el buffer anterior sigue en pan hasta que el hilo termine */
    if (sPost.enabled)
        fb_post_wait_idle();

    if (m->currentBuffer) {
        m->base.unlock(&m->base, m->currentBuffer);
        m->currentBuffer = 0;
    }

    if (hnd->flags & private_handle_t::PRIV_FLAGS_FRAMEBUFFER) {

        m->base.lock(&m->base, buffer, 
                private_module_t::PRIV_USAGE_LOCKED_FOR_POST, 
                0, 0, m->info.xres, m->info.yres, NULL);

        const size_t offset = hnd->base - m->framebuffer->base;
        /* Y210: el panel es MIPI DSI en modo comando (hx8357c). Con
         * FB_ACTIVATE_VBL, msm_fb_pan_display() sincroniza ademas con vsync
         * y eglSwapBuffers tardaba ~29.5 ms (2 periodos, ~33 fps). Con NOW
         * baja a ~15.4 ms. El stock evita la espera con un hilo de posting
         * asincrono (gralloc CAF) y queda en ~4.7 ms. */
        m->info.activate = FB_ACTIVATE_NOW;
        m->info.yoffset = offset / m->finfo.line_length;
        if (sPost.enabled) {
            /* copia de m->info: setUpdateRect la modifica desde SF */
            pthread_mutex_lock(&sPost.lock);
            sPost.info = m->info;
            sPost.pending = 1;
            pthread_cond_signal(&sPost.cond_post);
            pthread_mutex_unlock(&sPost.lock);
        } else if (ioctl(m->framebuffer->fd, FBIOPUT_VSCREENINFO, &m->info) == -1) {
            LOGE("FBIOPUT_VSCREENINFO failed");
            m->base.unlock(&m->base, buffer); 
            return -errno;
        }
        m->currentBuffer = buffer;
        
    } else {
        void* fb_vaddr;
        void* buffer_vaddr;
        
        m->base.lock(&m->base, m->framebuffer, 
                GRALLOC_USAGE_SW_WRITE_RARELY, 
                0, 0, m->info.xres, m->info.yres,
                &fb_vaddr);

        m->base.lock(&m->base, buffer, 
                GRALLOC_USAGE_SW_READ_RARELY, 
                0, 0, m->info.xres, m->info.yres,
                &buffer_vaddr);

        //memcpy(fb_vaddr, buffer_vaddr, m->finfo.line_length * m->info.yres);

        msm_copy_buffer(
                m->framebuffer, m->framebuffer->fd,
                m->info.xres, m->info.yres, m->fbFormat,
                m->info.xoffset, m->info.yoffset,
                m->info.width, m->info.height);

        m->base.unlock(&m->base, buffer); 
        m->base.unlock(&m->base, m->framebuffer); 
    }

    return 0;
}

static int fb_compositionComplete(struct framebuffer_device_t* dev)
{
    // TODO: Properly implement composition complete callback
    glFinish();

    return 0;
}

/*****************************************************************************/

int mapFrameBufferLocked(struct private_module_t* module)
{
    // already initialized...
    if (module->framebuffer) {
        return 0;
    }
        
    char const * const device_template[] = {
            "/dev/graphics/fb%u",
            "/dev/fb%u",
            0 };

    int fd = -1;
    int saved_errno = ENOENT;
    int attempt;
    char name[64];

    /*
     * El kernel stock rechaza open() con EPERM si el panel sigue en
     * early_suspend: msm_fb_open() llama blank UNBLANK y op_enable es
     * false ("can't turn on display"). En stop/start con la pantalla
     * apagada, SurfaceFlinger abre el fb antes de que PowerManager
     * pida el resume. Sin el write a /sys/power/state, fbDev queda
     * NULL y system_server muere en DisplayHardware::init (fps, 0x5c).
     *
     * /dev/fb0 no existe aqui. Si fb0 grafico falla con otro errno y se
     * prueba igual, el ENOENT del fallback tapa el EPERM real.
     */
    for (attempt = 0; attempt < 30 && fd < 0; attempt++) {
        snprintf(name, 64, device_template[0], 0);
        fd = open(name, O_RDWR, 0);
        if (fd < 0 && errno == ENOENT) {
            snprintf(name, 64, device_template[1], 0);
            fd = open(name, O_RDWR, 0);
        }
        if (fd >= 0)
            break;
        saved_errno = errno;
        if (saved_errno != EPERM)
            break;
        if (attempt == 0) {
            /*
             * PowerManager aun no pidio el resume (pasa en stop/start
             * con la pantalla apagada). El mismo write que
             * set_screen_state(1): "on" -> request_suspend_state(ON)
             * -> late_resume, y msm_fb_open vuelve a aceptar el fd.
             * system_server es group system; /sys/power/state es 0660.
             */
            int pfd = open("/sys/power/state", O_WRONLY);
            if (pfd >= 0) {
                if (write(pfd, "on", 2) == 2)
                    LOGW("fb0 open EPERM (panel suspended), requested resume");
                else
                    LOGW("fb0 open EPERM, write state=on failed (%s)",
                            strerror(errno));
                close(pfd);
            } else {
                LOGW("fb0 open EPERM, cannot open /sys/power/state (%s)",
                        strerror(errno));
            }
        }
        usleep(100 * 1000);
    }
    if (fd < 0)
        return -saved_errno;

    struct fb_fix_screeninfo finfo;
    if (ioctl(fd, FBIOGET_FSCREENINFO, &finfo) == -1)
        return -errno;

    struct fb_var_screeninfo info;
    if (ioctl(fd, FBIOGET_VSCREENINFO, &info) == -1)
        return -errno;

    info.reserved[0] = 0;
    info.reserved[1] = 0;
    info.reserved[2] = 0;
    info.xoffset = 0;
    info.yoffset = 0;
    info.activate = FB_ACTIVATE_NOW;

    /* Interpretation of offset for color fields: All offsets are from the right,
    * inside a "pixel" value, which is exactly 'bits_per_pixel' wide (means: you
    * can use the offset as right argument to <<). A pixel afterwards is a bit
    * stream and is written to video memory as that unmodified. This implies
    * big-endian byte order if bits_per_pixel is greater than 8.
    */

    /*
     * Explicitly request RGBA_8888
     */
    info.bits_per_pixel = 32;
    info.red.offset     = 24;
    info.red.length     = 8;
    info.green.offset   = 16;
    info.green.length   = 8;
    info.blue.offset    = 8;
    info.blue.length    = 8;
    info.transp.offset  = 0;
#ifdef USE_FRAMEBUFFER_ALPHA_CHANNEL
    info.transp.length  = 8;
#else
    info.transp.length  = 0;
#endif

    /* Note: the GL driver does not have a r=8 g=8 b=8 a=0 config, so if we do
     * not use the MDP for composition (i.e. hw composition == 0), ask for
     * RGBA instead of RGBX. */
    char property[PROPERTY_VALUE_MAX];
    if (property_get("debug.sf.hw", property, NULL) > 0 && atoi(property) == 0)
        module->fbFormat = HAL_PIXEL_FORMAT_RGBX_8888;
    else
        module->fbFormat = HAL_PIXEL_FORMAT_RGBA_8888;

    /*
     * Request NUM_BUFFERS screens (at lest 2 for page flipping)
     */
    info.yres_virtual = info.yres * NUM_BUFFERS;


    uint32_t flags = PAGE_FLIP;
    if (ioctl(fd, FBIOPUT_VSCREENINFO, &info) == -1) {
        info.yres_virtual = info.yres;
        flags &= ~PAGE_FLIP;
        LOGW("FBIOPUT_VSCREENINFO failed, page flipping not supported");
    }

    if (info.yres_virtual < info.yres * 2) {
        // we need at least 2 for page-flipping
        info.yres_virtual = info.yres;
        flags &= ~PAGE_FLIP;
        LOGW("page flipping not supported (yres_virtual=%d, requested=%d)",
                info.yres_virtual, info.yres*2);
    }

    if (ioctl(fd, FBIOGET_VSCREENINFO, &info) == -1)
        return -errno;

    int refreshRate = 1000000000000000LLU /
    (
            uint64_t( info.upper_margin + info.lower_margin + info.yres )
            * ( info.left_margin  + info.right_margin + info.xres )
            * info.pixclock
    );

    if (refreshRate == 0) {
        // bleagh, bad info from the driver
        refreshRate = 60*1000;  // 60 Hz
    }

    if (int(info.width) <= 0 || int(info.height) <= 0) {
        // the driver doesn't return that information
        // default to 160 dpi
        info.width  = ((info.xres * 25.4f)/160.0f + 0.5f);
        info.height = ((info.yres * 25.4f)/160.0f + 0.5f);
    }

    float xdpi = (info.xres * 25.4f) / info.width;
    float ydpi = (info.yres * 25.4f) / info.height;
    float fps  = refreshRate / 1000.0f;

    LOGI(   "using (fd=%d)\n"
            "id           = %s\n"
            "xres         = %d px\n"
            "yres         = %d px\n"
            "xres_virtual = %d px\n"
            "yres_virtual = %d px\n"
            "bpp          = %d\n"
            "r            = %2u:%u\n"
            "g            = %2u:%u\n"
            "b            = %2u:%u\n",
            fd,
            finfo.id,
            info.xres,
            info.yres,
            info.xres_virtual,
            info.yres_virtual,
            info.bits_per_pixel,
            info.red.offset, info.red.length,
            info.green.offset, info.green.length,
            info.blue.offset, info.blue.length
    );

    LOGI(   "width        = %d mm (%f dpi)\n"
            "height       = %d mm (%f dpi)\n"
            "refresh rate = %.2f Hz\n",
            info.width,  xdpi,
            info.height, ydpi,
            fps
    );


    if (ioctl(fd, FBIOGET_FSCREENINFO, &finfo) == -1)
        return -errno;

    if (finfo.smem_len <= 0)
        return -errno;


    module->flags = flags;
    module->info = info;
    module->finfo = finfo;
    module->xdpi = xdpi;
    module->ydpi = ydpi;
    module->fps = fps;

    /*
     * map the framebuffer
     */

    int err;
    size_t fbSize = roundUpToPageSize(finfo.line_length * info.yres_virtual);
    module->framebuffer = new private_handle_t(dup(fd), fbSize,
            private_handle_t::PRIV_FLAGS_USES_PMEM);

    module->numBuffers = info.yres_virtual / info.yres;
    module->bufferMask = 0;

    void* vaddr = mmap(0, fbSize, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
    if (vaddr == MAP_FAILED) {
        LOGE("Error mapping the framebuffer (%s)", strerror(errno));
        return -errno;
    }
    module->framebuffer->base = intptr_t(vaddr);
    memset(vaddr, 0, fbSize);
    return 0;
}

static int mapFrameBuffer(struct private_module_t* module)
{
    pthread_mutex_lock(&module->lock);
    int err = mapFrameBufferLocked(module);
    pthread_mutex_unlock(&module->lock);
    return err;
}

/*****************************************************************************/

static int fb_close(struct hw_device_t *dev)
{
    fb_context_t* ctx = (fb_context_t*)dev;
    if (sPost.enabled) {
        fb_post_wait_idle();
        pthread_mutex_lock(&sPost.lock);
        sPost.quit = 1;
        pthread_cond_signal(&sPost.cond_post);
        pthread_mutex_unlock(&sPost.lock);
        pthread_join(sPost.thread, NULL);
        sPost.enabled = 0;
        sPost.quit = 0;
    }
    if (ctx) {
        free(ctx);
    }
    return 0;
}

int fb_device_open(hw_module_t const* module, const char* name,
        hw_device_t** device)
{
    int status = -EINVAL;
    if (!strcmp(name, GRALLOC_HARDWARE_FB0)) {
        alloc_device_t* gralloc_device;
        status = gralloc_open(module, &gralloc_device);
        if (status < 0)
            return status;

        /* initialize our state here */
        fb_context_t *dev = (fb_context_t*)malloc(sizeof(*dev));
        memset(dev, 0, sizeof(*dev));

        /* initialize the procs */
        dev->device.common.tag = HARDWARE_DEVICE_TAG;
        dev->device.common.version = 0;
        dev->device.common.module = const_cast<hw_module_t*>(module);
        dev->device.common.close = fb_close;
        dev->device.setSwapInterval = fb_setSwapInterval;
        dev->device.post            = fb_post;
        dev->device.setUpdateRect = 0;
        dev->device.compositionComplete = fb_compositionComplete;

        private_module_t* m = (private_module_t*)module;
        status = mapFrameBuffer(m);
        if (status >= 0) {
            int stride = m->finfo.line_length / (m->info.bits_per_pixel >> 3);
            const_cast<uint32_t&>(dev->device.flags) = 0;
            const_cast<uint32_t&>(dev->device.width) = m->info.xres;
            const_cast<uint32_t&>(dev->device.height) = m->info.yres;
            const_cast<int&>(dev->device.stride) = stride;
            const_cast<int&>(dev->device.format) = m->fbFormat;
            const_cast<float&>(dev->device.xdpi) = m->xdpi;
            const_cast<float&>(dev->device.ydpi) = m->ydpi;
            const_cast<float&>(dev->device.fps) = m->fps;
            const_cast<int&>(dev->device.minSwapInterval) = 1;
            const_cast<int&>(dev->device.maxSwapInterval) = 1;

            if (m->finfo.reserved[0] == 0x5444 &&
                    m->finfo.reserved[1] == 0x5055) {
                dev->device.setUpdateRect = fb_setUpdateRect;
                LOGD("UPDATE_ON_DEMAND supported");
            }

            char value[PROPERTY_VALUE_MAX];
            property_get("debug.gr.async_post", value, "1");
            if (atoi(value) != 0 && m->numBuffers > 1 && !sPost.enabled) {
                pthread_mutex_init(&sPost.lock, NULL);
                pthread_cond_init(&sPost.cond_post, NULL);
                pthread_cond_init(&sPost.cond_idle, NULL);
                sPost.fd = m->framebuffer->fd;
                sPost.pending = 0;
                sPost.quit = 0;
                if (pthread_create(&sPost.thread, NULL, fb_post_thread, NULL) == 0) {
                    sPost.enabled = 1;
                    LOGI("async framebuffer posting enabled (%d buffers)",
                            m->numBuffers);
                } else {
                    LOGE("async framebuffer posting disabled: pthread_create failed");
                }
            }

            *device = &dev->device.common;
        }
    }
    return status;
}

/* Copy a pmem buffer to the framebuffer */

static void
msm_copy_buffer(buffer_handle_t handle, int fd,
                int width, int height, int format,
                int x, int y, int w, int h)
{
    struct {
        unsigned int count;
        mdp_blit_req req;
    } blit;
    private_handle_t *priv = (private_handle_t*) handle;

    memset(&blit, 0, sizeof(blit));
    blit.count = 1;

    blit.req.flags = 0;
    blit.req.alpha = 0xff;
    blit.req.transp_mask = 0xffffffff;

    blit.req.src.width = width;
    blit.req.src.height = height;
    blit.req.src.offset = 0;
    blit.req.src.memory_id = priv->fd;

    blit.req.dst.width = width;
    blit.req.dst.height = height;
    blit.req.dst.offset = 0;
    blit.req.dst.memory_id = fd; 
    blit.req.dst.format = format;

    blit.req.src_rect.x = blit.req.dst_rect.x = x;
    blit.req.src_rect.y = blit.req.dst_rect.y = y;
    blit.req.src_rect.w = blit.req.dst_rect.w = w;
    blit.req.src_rect.h = blit.req.dst_rect.h = h;

    if (ioctl(fd, MSMFB_BLIT, &blit))
        LOGE("MSMFB_BLIT failed = %d", -errno);
}
