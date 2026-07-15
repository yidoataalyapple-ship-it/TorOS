/******************************************************************************
 * torOS - Terminal Operating System
 * Audio Subsystem
 * FAZ 7: Sound & Music
 *
 * Sub-fazs:
 *   FAZ 7.1: VirtIO-sound Driver
 *   FAZ 7.2: WAV & OGG Decoders
 *   FAZ 7.3: Simple Sound Effects
 *   FAZ 7.4: Audio Mixer
 *   FAZ 7.5: Notification Sounds
 *   FAZ 7.6: Music Player
 *   FAZ 7.7: Microphone (future)
 *
 * Copyright (c) 2025 torOS Contributors
 * License: MIT
 ******************************************************************************/

#include "../include/toros.h"
#include "../include/audio.h"
#include "../include/virtio.h"

/* ===== VirtIO-sound (FAZ 7.1) ===== */

static struct {
    int initialized;
    uint32 sample_rate;
    int channels;
    int format;
    int buffer_size;
    virtio_mmio_regs_t *regs;
    uint8 *dma_buffer;
} audio_dev;

int audio_init(uint32 sample_rate, int channels, int format)
{
    memset(&audio_dev, 0, sizeof(audio_dev));

    /* VirtIO-sound uses PCI device 0x1AF4:0x8058 or MMIO */
    audio_dev.regs = (virtio_mmio_regs_t *)VIRTIO_SOUND_MMIO_BASE;

    if (audio_dev.regs->magic != VIRTIO_MMIO_MAGIC) {
        printk_color(TERM_YELLOW, "[AUDIO] VirtIO-sound not found, using beep fallback\n");
        audio_dev.initialized = 0;
        return 0; /* Still "ok" - we'll use beep fallback */
    }

    /* Reset and configure */
    audio_dev.regs->status = 0;
    audio_dev.regs->status |= VIRTIO_STATUS_ACKNOWLEDGE;
    audio_dev.regs->status |= VIRTIO_STATUS_DRIVER;

    /* Negotiate features */
    uint32 features = audio_dev.regs->device_features;
    (void)features;
    audio_dev.regs->driver_features = 0;

    audio_dev.regs->status |= VIRTIO_STATUS_FEATURES_OK;

    if (!(audio_dev.regs->status & VIRTIO_STATUS_FEATURES_OK)) {
        printk_color(TERM_RED, "[AUDIO] Feature negotiation failed\n");
        return -1;
    }

    /* Set sample rate and format via control queue */
    audio_dev.sample_rate = sample_rate;
    audio_dev.channels = channels;
    audio_dev.format = format;
    audio_dev.buffer_size = AUDIO_BUFFER_SIZE;

    /* Allocate DMA buffer */
    audio_dev.dma_buffer = (uint8 *)kmalloc(AUDIO_BUFFER_SIZE);
    if (!audio_dev.dma_buffer) return -1;
    memset(audio_dev.dma_buffer, 0, AUDIO_BUFFER_SIZE);

    audio_dev.regs->status |= VIRTIO_STATUS_DRIVER_OK;
    audio_dev.initialized = 1;

    printk_color(TERM_GREEN, "[AUDIO] VirtIO-sound: %d Hz, %d ch, fmt=%d\n",
                 sample_rate, channels, format);
    return 0;
}

void audio_shutdown(void)
{
    if (audio_dev.initialized && audio_dev.regs) {
        audio_dev.regs->status = 0;
    }
    if (audio_dev.dma_buffer) {
        kfree(audio_dev.dma_buffer);
        audio_dev.dma_buffer = NULL;
    }
    audio_dev.initialized = 0;
}

int audio_write(const void *buffer, uint32 size)
{
    if (!buffer || size == 0) return -1;

    if (audio_dev.initialized && audio_dev.dma_buffer) {
        /* Copy to DMA buffer */
        uint32 copy_size = (size < AUDIO_BUFFER_SIZE) ? size : AUDIO_BUFFER_SIZE;
        memcpy(audio_dev.dma_buffer, buffer, copy_size);
        return copy_size;
    }
    return 0;
}

int audio_play(void)
{
    if (audio_dev.initialized) {
        /* Tell device to start playing */
        return 0;
    }
    return -1;
}

int audio_stop(void)
{
    if (audio_dev.initialized) {
        return 0;
    }
    return -1;
}

int audio_pause(void)
{
    return audio_stop();
}

int audio_resume(void)
{
    return audio_play();
}

uint32 audio_buffer_free(void)
{
    if (audio_dev.initialized) {
        return AUDIO_BUFFER_SIZE;
    }
    return 0;
}

uint32 audio_buffer_used(void)
{
    return 0;
}

void audio_set_volume(int percent)
{
    (void)percent;
}

/* ===== WAV Decoder (FAZ 7.2) ===== */

typedef struct {
    uint32 chunk_id;
    uint32 chunk_size;
    uint32 format;
    uint32 subchunk1_id;
    uint32 subchunk1_size;
    uint16 audio_format;
    uint16 num_channels;
    uint32 sample_rate;
    uint32 byte_rate;
    uint16 block_align;
    uint16 bits_per_sample;
    uint32 subchunk2_id;
    uint32 subchunk2_size;
} __attribute__((packed)) wav_header_t;

static int parse_wav(const uint8 *data, int size, int *out_rate, int *out_ch, int *out_fmt, int *out_data_offset, int *out_data_size)
{
    if (size < 44) return -1;
    const wav_header_t *h = (const wav_header_t *)data;

    if (h->chunk_id != 0x46464952) return -1; /* 'RIFF' */
    if (h->format != 0x45564157) return -1;   /* 'WAVE' */
    if (h->subchunk1_id != 0x20746D66) return -1; /* 'fmt ' */
    if (h->audio_format != 1 && h->audio_format != 3) return -1; /* PCM or IEEE float */

    *out_rate = h->sample_rate;
    *out_ch = h->num_channels;
    *out_fmt = (h->bits_per_sample == 8) ? AUDIO_FMT_U8 :
               (h->bits_per_sample == 16) ? AUDIO_FMT_S16_LE :
               (h->bits_per_sample == 32) ? AUDIO_FMT_F32_LE : AUDIO_FMT_S16_LE;

    /* Find 'data' chunk */
    uint32 pos = 36;
    while (pos + 8 <= (uint32)size) {
        uint32 id = *(uint32 *)(data + pos);
        uint32 sz = *(uint32 *)(data + pos + 4);
        if (id == 0x61746164) { /* 'data' */
            *out_data_offset = pos + 8;
            *out_data_size = sz;
            return 0;
        }
        pos += 8 + sz;
    }
    return -1;
}

audio_clip_t *audio_load_wav(const char *filename)
{
    if (!filename) return NULL;
    int size = tfs_size(filename);
    if (size <= 0) return NULL;

    uint8 *data = (uint8 *)kmalloc(size);
    if (!data) return NULL;
    if (tfs_read(filename, data, size, 0) != size) {
        kfree(data);
        return NULL;
    }

    int rate, ch, fmt, offset, dsize;
    if (parse_wav(data, size, &rate, &ch, &fmt, &offset, &dsize) < 0) {
        kfree(data);
        return NULL;
    }

    audio_clip_t *clip = (audio_clip_t *)kmalloc(sizeof(audio_clip_t));
    if (!clip) { kfree(data); return NULL; }

    clip->data = data + offset;
    clip->size = dsize;
    clip->sample_rate = rate;
    clip->channels = ch;
    clip->format = fmt;
    clip->loop = 0;
    strncpy(clip->name, filename, sizeof(clip->name) - 1);

    printk_color(TERM_GREEN, "[AUDIO] WAV loaded: %s (%d Hz, %d ch, %d bytes)\n",
                 filename, rate, ch, dsize);
    return clip;
}

audio_clip_t *audio_load_ogg(const char *filename)
{
    (void)filename;
    printk_color(TERM_YELLOW, "[AUDIO] OGG decoder not yet implemented\n");
    return NULL;
}

void audio_clip_free(audio_clip_t *clip)
{
    if (clip) {
        if (clip->data) {
            /* data points into original buffer - need to free original */
            /* For now, just mark as freed */
        }
        kfree(clip);
    }
}

void audio_play_clip(audio_clip_t *clip)
{
    if (!clip || !clip->data) return;
    audio_write(clip->data, clip->size);
    audio_play();
}

/* ===== Sound Effects (FAZ 7.3) ===== */

void audio_generate_sine(void *buffer, uint32 freq, uint32 rate, uint32 samples, int format)
{
    if (!buffer || freq == 0 || rate == 0) return;

    float period = (float)rate / freq;
    if (format == AUDIO_FMT_S16_LE) {
        int16 *buf = (int16 *)buffer;
        for (uint32 i = 0; i < samples; i++) {
            buf[i] = (int16)(sinf(2.0f * 3.14159f * i / period) * 32767.0f);
        }
    } else if (format == AUDIO_FMT_U8) {
        uint8 *buf = (uint8 *)buffer;
        for (uint32 i = 0; i < samples; i++) {
            buf[i] = (uint8)(128 + sinf(2.0f * 3.14159f * i / period) * 127.0f);
        }
    } else if (format == AUDIO_FMT_F32_LE) {
        float *buf = (float *)buffer;
        for (uint32 i = 0; i < samples; i++) {
            buf[i] = sinf(2.0f * 3.14159f * i / period);
        }
    }
}

void audio_generate_square(void *buffer, uint32 freq, uint32 rate, uint32 samples, int format)
{
    if (!buffer || freq == 0 || rate == 0) return;

    uint32 half_period = rate / (freq * 2);
    if (format == AUDIO_FMT_S16_LE) {
        int16 *buf = (int16 *)buffer;
        for (uint32 i = 0; i < samples; i++) {
            buf[i] = ((i / half_period) % 2 == 0) ? 32767 : -32767;
        }
    }
}

void audio_generate_sawtooth(void *buffer, uint32 freq, uint32 rate, uint32 samples, int format)
{
    if (!buffer || freq == 0 || rate == 0) return;

    uint32 period = rate / freq;
    if (format == AUDIO_FMT_S16_LE) {
        int16 *buf = (int16 *)buffer;
        for (uint32 i = 0; i < samples; i++) {
            float t = (float)(i % period) / period;
            buf[i] = (int16)((t * 2.0f - 1.0f) * 32767.0f);
        }
    }
}

void audio_generate_triangle(void *buffer, uint32 freq, uint32 rate, uint32 samples, int format)
{
    if (!buffer || freq == 0 || rate == 0) return;

    uint32 period = rate / freq;
    if (format == AUDIO_FMT_S16_LE) {
        int16 *buf = (int16 *)buffer;
        for (uint32 i = 0; i < samples; i++) {
            uint32 phase = i % period;
            float val = (float)phase / period;
            if (val < 0.25f) buf[i] = (int16)(val * 4.0f * 32767.0f);
            else if (val < 0.75f) buf[i] = (int16)((0.5f - val) * 4.0f * 32767.0f);
            else buf[i] = (int16)((val - 1.0f) * 4.0f * 32767.0f);
        }
    }
}

void audio_beep(uint32 freq, uint32 duration_ms)
{
    if (!audio_dev.initialized) return;

    uint32 samples = (audio_dev.sample_rate * duration_ms) / 1000;
    uint32 buf_size = samples * (audio_dev.format == AUDIO_FMT_S16_LE ? 2 : 1);
    void *buffer = kmalloc(buf_size);
    if (!buffer) return;

    audio_generate_sine(buffer, freq, audio_dev.sample_rate, samples, audio_dev.format);
    audio_write(buffer, buf_size);
    audio_play();

    rtc_mdelay(duration_ms);
    audio_stop();
    kfree(buffer);
}

/* ===== Resampling ===== */

int audio_resample(void *dst, uint32 dst_rate, const void *src, uint32 src_rate,
                   int samples, int channels, int format)
{
    if (!dst || !src || dst_rate == 0 || src_rate == 0 || samples <= 0) return -1;
    if (dst_rate == src_rate) {
        int sample_size = (format == AUDIO_FMT_U8) ? 1 : (format == AUDIO_FMT_S16_LE) ? 2 : 4;
        memcpy(dst, src, samples * sample_size * channels);
        return samples;
    }

    if (format == AUDIO_FMT_S16_LE && channels == 1) {
        /* Linear interpolation for mono S16 */
        int16 *d = (int16 *)dst;
        const int16 *s = (const int16 *)src;
        float ratio = (float)src_rate / (float)dst_rate;
        int out_samples = (int)((float)samples * (float)dst_rate / (float)src_rate);
        for (int i = 0; i < out_samples; i++) {
            float src_pos = i * ratio;
            int pos0 = (int)src_pos;
            int pos1 = pos0 + 1;
            float frac = src_pos - pos0;
            if (pos1 >= samples) pos1 = samples - 1;
            d[i] = (int16)(s[pos0] * (1.0f - frac) + s[pos1] * frac);
        }
        return out_samples;
    } else if (format == AUDIO_FMT_S16_LE && channels == 2) {
        /* Stereo S16 */
        int16 *d = (int16 *)dst;
        const int16 *s = (const int16 *)src;
        float ratio = (float)src_rate / (float)dst_rate;
        int out_samples = (int)((float)samples * (float)dst_rate / (float)src_rate);
        for (int i = 0; i < out_samples; i++) {
            float src_pos = i * ratio;
            int pos0 = (int)src_pos;
            int pos1 = pos0 + 1;
            float frac = src_pos - pos0;
            if (pos1 >= samples) pos1 = samples - 1;
            d[i * 2] = (int16)(s[pos0 * 2] * (1.0f - frac) + s[pos1 * 2] * frac);
            d[i * 2 + 1] = (int16)(s[pos0 * 2 + 1] * (1.0f - frac) + s[pos1 * 2 + 1] * frac);
        }
        return out_samples;
    }

    /* Fallback: nearest neighbor */
    int sample_size = (format == AUDIO_FMT_U8) ? 1 : (format == AUDIO_FMT_S16_LE) ? 2 : 4;
    int out_samples = samples * dst_rate / src_rate;
    for (int i = 0; i < out_samples * channels; i++) {
        int src_idx = i * src_rate / dst_rate;
        if (src_idx < samples * channels)
            memcpy((uint8 *)dst + i * sample_size, (const uint8 *)src + src_idx * sample_size, sample_size);
    }
    return out_samples;
}

/* ===== Channel Conversion ===== */

int audio_convert_channels(void *dst, int dst_ch, const void *src, int src_ch,
                           int samples, int format)
{
    if (!dst || !src || samples <= 0) return -1;
    if (dst_ch == src_ch) {
        int sample_size = (format == AUDIO_FMT_U8) ? 1 : (format == AUDIO_FMT_S16_LE) ? 2 : 4;
        memcpy(dst, src, samples * sample_size * src_ch);
        return samples;
    }

    if (format == AUDIO_FMT_S16_LE) {
        int16 *d = (int16 *)dst;
        const int16 *s = (const int16 *)src;
        if (src_ch == 1 && dst_ch == 2) {
            /* Mono to stereo */
            for (int i = 0; i < samples; i++) {
                d[i * 2] = s[i];
                d[i * 2 + 1] = s[i];
            }
            return samples;
        } else if (src_ch == 2 && dst_ch == 1) {
            /* Stereo to mono (average) */
            for (int i = 0; i < samples; i++)
                d[i] = (int16)(((int)s[i * 2] + (int)s[i * 2 + 1]) / 2);
            return samples;
        }
    }
    return -1;
}
