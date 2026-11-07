#include "vice.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>

#include <audio/audio.h>
#include <sys/event_queue.h>
#include <sys/mutex.h>
#include <sys/systime.h>
#include <sys/thread.h>

#include "log.h"
#include "sound.h"
#include "ps3_gfx.h"

#define PS3_SND_HW_RATE       48000u
#define PS3_SND_HW_CHANNELS   2u
#define PS3_SND_BLOCK_SAMPLES 256u
#define PS3_SND_HW_BLOCKS     8u
#define PS3_SND_FRAG_FRAMES   512u
#define PS3_SND_FRAG_COUNT    16u
#define PS3_SND_RING_FRAMES   (PS3_SND_FRAG_FRAMES * PS3_SND_FRAG_COUNT)
#define PS3_SND_TARGET_FRAMES (PS3_SND_RING_FRAMES / 2u)

static u32 snd_port = 0;
static audioPortConfig snd_cfg;
static sys_event_queue_t snd_queue = 0;
static u64 snd_queue_key = 0;
static sys_ppu_thread_t snd_thread = 0;
static sys_lwmutex_t snd_lock;
static int snd_lock_ready = 0;

static volatile int snd_running = 0;
static volatile int snd_paused = 0;
static int snd_opened = 0;
static unsigned int snd_src_channels = 1;
static unsigned int snd_hw_block = 0;

static int16_t snd_ring[PS3_SND_RING_FRAMES * PS3_SND_HW_CHANNELS];
static int16_t snd_stage[PS3_SND_RING_FRAMES * PS3_SND_HW_CHANNELS];
static unsigned int snd_read_pos = 0;
static unsigned int snd_write_pos = 0;
static unsigned int snd_used_frames = 0;
static float snd_last_l = 0.0f;
static float snd_last_r = 0.0f;
static unsigned int snd_stat_blocks = 0;
static unsigned int snd_stat_underruns = 0;
static unsigned int snd_stat_drops = 0;

static void snd_ensure_lock(void)
{
    sys_lwmutex_attr_t attr;

    if (snd_lock_ready) {
        return;
    }

    memset(&attr, 0, sizeof(attr));
    attr.attr_protocol = SYS_MUTEX_PROTOCOL_FIFO;
    attr.attr_recursive = SYS_MUTEX_ATTR_NOT_RECURSIVE;
    strcpy(attr.name, "vicesnd");
    sysLwMutexCreate(&snd_lock, &attr);
    snd_lock_ready = 1;
}

static void snd_reset_fifo_locked(int prefill)
{
    memset(snd_ring, 0, sizeof(snd_ring));
    snd_read_pos = 0;
    if (prefill) {
        snd_write_pos = PS3_SND_TARGET_FRAMES % PS3_SND_RING_FRAMES;
        snd_used_frames = PS3_SND_TARGET_FRAMES;
    } else {
        snd_write_pos = 0;
        snd_used_frames = 0;
    }
    snd_last_l = 0.0f;
    snd_last_r = 0.0f;
}

static float *snd_block_ptr(unsigned int block_index)
{
    uintptr_t addr = (uintptr_t)(uint32_t)snd_cfg.audioDataStart;
    if (addr == 0) {
        return NULL;
    }
    return (float *)addr + ((size_t)block_index * PS3_SND_BLOCK_SAMPLES * PS3_SND_HW_CHANNELS);
}

static void snd_clear_hw_ring(void)
{
    unsigned int blocks = snd_cfg.numBlocks ? (unsigned int)snd_cfg.numBlocks : PS3_SND_HW_BLOCKS;
    float *base = snd_block_ptr(0);

    if (base != NULL) {
        memset(base, 0, (size_t)blocks * PS3_SND_BLOCK_SAMPLES * PS3_SND_HW_CHANNELS * sizeof(float));
    }
}

static void snd_fill_block(float *dst, int paused)
{
    unsigned int i;
    unsigned int rpos;
    unsigned int avail;
    unsigned int take;
    float last_l;
    float last_r;

    if (dst == NULL) {
        return;
    }

    if (paused) {
        for (i = 0; i < PS3_SND_BLOCK_SAMPLES * PS3_SND_HW_CHANNELS; i++) {
            dst[i] = 0.0f;
        }
        return;
    }

    sysLwMutexLock(&snd_lock, 0);

    rpos = snd_read_pos;
    avail = snd_used_frames;
    last_l = snd_last_l;
    last_r = snd_last_r;
    take = (avail < PS3_SND_BLOCK_SAMPLES) ? avail : PS3_SND_BLOCK_SAMPLES;

    for (i = 0; i < take; i++) {
        unsigned int idx = rpos * PS3_SND_HW_CHANNELS;
        last_l = (float)snd_ring[idx] * (1.0f / 32768.0f);
        last_r = (float)snd_ring[idx + 1u] * (1.0f / 32768.0f);
        dst[i * 2u] = last_l;
        dst[i * 2u + 1u] = last_r;
        rpos++;
        if (rpos >= PS3_SND_RING_FRAMES) {
            rpos = 0;
        }
    }

    if (take < PS3_SND_BLOCK_SAMPLES) {
        snd_stat_underruns++;
        for (; i < PS3_SND_BLOCK_SAMPLES; i++) {
            last_l *= 0.985f;
            last_r *= 0.985f;
            dst[i * 2u] = last_l;
            dst[i * 2u + 1u] = last_r;
        }
    }

    snd_read_pos = rpos;
    snd_used_frames = avail - take;
    snd_last_l = last_l;
    snd_last_r = last_r;
    snd_stat_blocks++;

    if ((snd_stat_blocks % 1875u) == 0u) {
        char msg[128];
        snprintf(msg, sizeof(msg), "audio: blocks=%u underruns=%u drops=%u fill=%u/%u",
                 snd_stat_blocks, snd_stat_underruns, snd_stat_drops,
                 snd_used_frames, (unsigned int)PS3_SND_RING_FRAMES);
        ps3_gfx_log_write(msg);
    }

    sysLwMutexUnlock(&snd_lock);
}

static void snd_thread_entry(void *arg)
{
    unsigned int blocks;
    (void)arg;

    blocks = snd_cfg.numBlocks ? (unsigned int)snd_cfg.numBlocks : PS3_SND_HW_BLOCKS;
    if (blocks == 0) {
        blocks = PS3_SND_HW_BLOCKS;
    }

    while (snd_running) {
        sys_event_t event;
        s32 rc = sysEventQueueReceive(snd_queue, &event, 20000);
        float *dst;

        if (!snd_running) {
            break;
        }
        if (rc != 0) {
            continue;
        }

        dst = snd_block_ptr(snd_hw_block);
        snd_fill_block(dst, snd_paused);
        snd_hw_block = (snd_hw_block + 1u) % blocks;
    }

    sysThreadExit(0);
}

static void snd_shutdown_port(void)
{
    u64 retval = 0;

    if (!snd_opened) {
        return;
    }

    snd_running = 0;
    snd_paused = 1;

    if (snd_thread != 0) {
        sysThreadJoin(snd_thread, &retval);
        snd_thread = 0;
    }

    if (snd_port != 0) {
        audioPortStop(snd_port);
    }

    if (snd_queue_key != 0) {
        audioRemoveNotifyEventQueue(snd_queue_key);
        snd_queue_key = 0;
    }

    if (snd_queue != 0) {
        sysEventQueueDestroy(snd_queue, 0);
        snd_queue = 0;
    }

    if (snd_port != 0) {
        audioPortClose(snd_port);
        snd_port = 0;
    }

    audioQuit();
    memset(&snd_cfg, 0, sizeof(snd_cfg));
    snd_opened = 0;
}

static int ps3_snd_init(const char *param, int *speed, int *fragsize, int *fragnr, int *channels)
{
    audioPortParam port_param;
    char msg[160];
    int rc;

    (void)param;

    snd_ensure_lock();

    if (snd_opened) {
        snd_shutdown_port();
    }

    if (channels != NULL && *channels >= 2) {
        snd_src_channels = 2;
        *channels = 2;
    } else {
        snd_src_channels = 1;
        if (channels != NULL) {
            *channels = 1;
        }
    }

    if (speed != NULL) {
        *speed = (int)PS3_SND_HW_RATE;
    }
    if (fragsize != NULL) {
        *fragsize = (int)PS3_SND_FRAG_FRAMES;
    }
    if (fragnr != NULL) {
        *fragnr = (int)PS3_SND_FRAG_COUNT;
    }

    sysLwMutexLock(&snd_lock, 0);
    snd_reset_fifo_locked(1);
    snd_stat_blocks = 0;
    snd_stat_underruns = 0;
    snd_stat_drops = 0;
    sysLwMutexUnlock(&snd_lock);

    rc = audioInit();
    if (rc != 0) {
        snprintf(msg, sizeof(msg), "audio: audioInit failed rc=%d", rc);
        ps3_gfx_log_write(msg);
        return -1;
    }

    memset(&port_param, 0, sizeof(port_param));
    port_param.numChannels = AUDIO_PORT_2CH;
    port_param.numBlocks = AUDIO_BLOCK_8;
    port_param.attrib = AUDIO_PORT_INITLEVEL;
    port_param.level = 1.0f;

    rc = audioPortOpen(&port_param, &snd_port);
    if (rc != 0) {
        snprintf(msg, sizeof(msg), "audio: audioPortOpen failed rc=%d", rc);
        ps3_gfx_log_write(msg);
        audioQuit();
        return -1;
    }

    memset(&snd_cfg, 0, sizeof(snd_cfg));
    rc = audioGetPortConfig(snd_port, &snd_cfg);
    if (rc != 0 || snd_cfg.audioDataStart == 0) {
        snprintf(msg, sizeof(msg), "audio: audioGetPortConfig failed rc=%d start=0x%x",
                 rc, snd_cfg.audioDataStart);
        ps3_gfx_log_write(msg);
        audioPortClose(snd_port);
        snd_port = 0;
        audioQuit();
        return -1;
    }

    snd_clear_hw_ring();

    rc = audioCreateNotifyEventQueue(&snd_queue, &snd_queue_key);
    if (rc != 0) {
        snprintf(msg, sizeof(msg), "audio: audioCreateNotifyEventQueue failed rc=%d", rc);
        ps3_gfx_log_write(msg);
        audioPortClose(snd_port);
        snd_port = 0;
        audioQuit();
        return -1;
    }

    rc = audioSetNotifyEventQueue(snd_queue_key);
    if (rc != 0) {
        snprintf(msg, sizeof(msg), "audio: audioSetNotifyEventQueue failed rc=%d", rc);
        ps3_gfx_log_write(msg);
        sysEventQueueDestroy(snd_queue, 0);
        snd_queue = 0;
        snd_queue_key = 0;
        audioPortClose(snd_port);
        snd_port = 0;
        audioQuit();
        return -1;
    }

    sysEventQueueDrain(snd_queue);

    snd_hw_block = 0;
    snd_paused = 0;
    snd_running = 1;
    snd_opened = 1;

    rc = audioPortStart(snd_port);
    if (rc != 0) {
        snprintf(msg, sizeof(msg), "audio: audioPortStart failed rc=%d", rc);
        ps3_gfx_log_write(msg);
        snd_shutdown_port();
        return -1;
    }

    rc = sysThreadCreate(&snd_thread,
                         snd_thread_entry,
                         NULL,
                         900,
                         0x10000,
                         THREAD_JOINABLE,
                         (char *)"vice_ps3_audio");
    if (rc != 0) {
        snprintf(msg, sizeof(msg), "audio: sysThreadCreate failed rc=%d", rc);
        ps3_gfx_log_write(msg);
        snd_shutdown_port();
        return -1;
    }

    snprintf(msg, sizeof(msg),
             "audio: open %u Hz ring=%u frames (%u ms) src_channels=%u blocks=%u",
             PS3_SND_HW_RATE,
             (unsigned int)PS3_SND_RING_FRAMES,
             (unsigned int)((PS3_SND_RING_FRAMES * 1000u) / PS3_SND_HW_RATE),
             snd_src_channels,
             (unsigned int)snd_cfg.numBlocks);
    ps3_gfx_log_write(msg);

    return 0;
}

static int ps3_snd_write(int16_t *pbuf, size_t nr)
{
    size_t in_frames;
    unsigned int avail;
    unsigned int out_frames;
    unsigned int i;
    unsigned int wpos;
    unsigned int free_frames;
    int wait_loops = 0;

    if (!snd_opened || !snd_running || pbuf == NULL || nr == 0) {
        return 0;
    }

    in_frames = (snd_src_channels >= 2) ? (nr / 2u) : nr;
    if (in_frames == 0) {
        return 0;
    }
    if (in_frames > PS3_SND_RING_FRAMES / 2u) {
        in_frames = PS3_SND_RING_FRAMES / 2u;
    }

    while (snd_running && !snd_paused && wait_loops < 40) {
        sysLwMutexLock(&snd_lock, 0);
        avail = snd_used_frames;
        sysLwMutexUnlock(&snd_lock);
        if (avail + (unsigned int)in_frames <= PS3_SND_RING_FRAMES - PS3_SND_FRAG_FRAMES) {
            break;
        }
        sysUsleep(400);
        wait_loops++;
    }

    sysLwMutexLock(&snd_lock, 0);
    avail = snd_used_frames;
    sysLwMutexUnlock(&snd_lock);

    if (avail < 1500u) {
        int extra = (int)((in_frames * (1500u - avail)) / 65536u);
        int max_extra = (int)(in_frames / 128u);
        if (max_extra < 1) {
            max_extra = 1;
        }
        if (extra > max_extra) {
            extra = max_extra;
        }
        if (extra < 1) {
            extra = 1;
        }
        out_frames = (unsigned int)((int)in_frames + extra);
    } else if (avail > 6500u) {
        int less = (int)((in_frames * (avail - 6500u)) / 65536u);
        int max_less = (int)(in_frames / 128u);
        if (max_less < 1) {
            max_less = 1;
        }
        if (less > max_less) {
            less = max_less;
        }
        if (less < 1) {
            less = 1;
        }
        out_frames = (unsigned int)((int)in_frames - less);
    } else {
        out_frames = (unsigned int)in_frames;
    }

    if (out_frames < 1u) {
        out_frames = 1u;
    }
    if (out_frames > PS3_SND_RING_FRAMES) {
        out_frames = PS3_SND_RING_FRAMES;
    }

    if (out_frames == (unsigned int)in_frames) {
        if (snd_src_channels >= 2) {
            memcpy(snd_stage, pbuf, in_frames * 2u * sizeof(int16_t));
        } else {
            for (i = 0; i < (unsigned int)in_frames; i++) {
                int16_t s = pbuf[i];
                snd_stage[i * 2u] = s;
                snd_stage[i * 2u + 1u] = s;
            }
        }
    } else {
        uint32_t step_fp = (out_frames > 1u) ?
            (uint32_t)(((uint64_t)(in_frames - 1u) << 16) / (uint64_t)(out_frames - 1u)) : 0u;
        uint32_t pos_fp = 0u;

        for (i = 0; i < out_frames; i++) {
            uint32_t idx = pos_fp >> 16;
            int32_t frac = (int32_t)(pos_fp & 0xFFFFu);
            int32_t s0_l, s0_r, s1_l, s1_r;
            size_t k0 = (size_t)idx;
            size_t k1 = (k0 + 1u < in_frames) ? (k0 + 1u) : k0;

            if (snd_src_channels >= 2) {
                s0_l = pbuf[k0 * 2u];
                s0_r = pbuf[k0 * 2u + 1u];
                s1_l = pbuf[k1 * 2u];
                s1_r = pbuf[k1 * 2u + 1u];
            } else {
                s0_l = pbuf[k0];
                s0_r = s0_l;
                s1_l = pbuf[k1];
                s1_r = s1_l;
            }

            snd_stage[i * 2u] = (int16_t)(s0_l + (((s1_l - s0_l) * frac) >> 16));
            snd_stage[i * 2u + 1u] = (int16_t)(s0_r + (((s1_r - s0_r) * frac) >> 16));
            pos_fp += step_fp;
        }
    }

    sysLwMutexLock(&snd_lock, 0);

    free_frames = PS3_SND_RING_FRAMES - snd_used_frames;
    if (out_frames > free_frames) {
        unsigned int drop = out_frames - free_frames;
        snd_read_pos = (snd_read_pos + drop) % PS3_SND_RING_FRAMES;
        snd_used_frames -= drop;
        snd_stat_drops += drop;
    }

    wpos = snd_write_pos;
    for (i = 0; i < out_frames; i++) {
        unsigned int dst_idx = wpos * PS3_SND_HW_CHANNELS;
        snd_ring[dst_idx] = snd_stage[i * 2u];
        snd_ring[dst_idx + 1u] = snd_stage[i * 2u + 1u];
        wpos++;
        if (wpos >= PS3_SND_RING_FRAMES) {
            wpos = 0;
        }
    }

    snd_write_pos = wpos;
    snd_used_frames += out_frames;

    sysLwMutexUnlock(&snd_lock);
    return 0;
}

static void ps3_snd_close(void)
{
    snd_shutdown_port();
}

static int ps3_snd_suspend(void)
{
    if (!snd_opened) {
        return 0;
    }

    snd_paused = 1;
    sysLwMutexLock(&snd_lock, 0);
    snd_reset_fifo_locked(0);
    sysLwMutexUnlock(&snd_lock);
    snd_clear_hw_ring();
    return 0;
}

static int ps3_snd_resume(void)
{
    if (!snd_opened) {
        return 0;
    }

    sysLwMutexLock(&snd_lock, 0);
    snd_reset_fifo_locked(1);
    sysLwMutexUnlock(&snd_lock);
    snd_paused = 0;
    return 0;
}

static sound_device_t sdl_device =
{
    "sdl",
    ps3_snd_init,
    ps3_snd_write,
    NULL,
    NULL,
    NULL,
    ps3_snd_close,
    ps3_snd_suspend,
    ps3_snd_resume,
    1,
    2
};

int sound_init_sdl_device(void)
{
    return sound_register_device(&sdl_device);
}
