/*
 * torOS Audio Subsystem
 * AC'97 / Intel HDA / virtio-sound driver, mixer, PCM playback, format conversion
 */

#include "../include/toros.h"
#include "../include/audio.h"

static audio_device_t audio_dev;
static mixer_state_t mixer;
static pcm_buffer_t pcm_ring[PCM_BUFFER_COUNT];

/* ===== AC'97 ===== */

#define AC97_NAMBAR     0x00
#define AC97_NABMBAR    0x40

static inline uint16 ac97_read_nam(uint32 reg)
{
    return *(volatile uint16 *)(audio_dev.mmio_base + AC97_NAMBAR + reg);
}

static inline void ac97_write_nam(uint32 reg, uint16 val)
{
    *(volatile uint16 *)(audio_dev.mmio_base + AC97_NAMBAR + reg) = val;
}

static inline uint32 ac97_read_nabm(uint32 reg)
{
    return *(volatile uint32 *)(audio_dev.mmio_base + AC97_NABMBAR + reg);
}

static inline void ac97_write_nabm(uint32 reg, uint32 val)
{
    *(volatile uint32 *)(audio_dev.mmio_base + AC97_NABMBAR + reg) = val;
}

static int ac97_reset(void)
{
    /* Cold reset */
    ac97_write_nam(AC97_RESET, 0xFFFF);
    rtc_mdelay(100);
    ac97_write_nam(AC97_RESET, 0x0000);
    rtc_mdelay(100);

    /* Check if codec ready */
    uint16 ext_id = ac97_read_nam(AC97_EXTENDED_ID);
    if (ext_id == 0xFFFF) {
        printk_color(TERM_RED, "[AUDIO] AC'97 codec not found\n");
        return -1;
    }

    /* Set volumes */
    ac97_write_nam(AC97_MASTER_VOL, 0x0000);    /* 0dB = max volume (inverted) */
    ac97_write_nam(AC97_PCM_OUT_VOL, 0x0000);
    ac97_write_nam(AC97_MIC_VOL, 0x8000);       /* 20dB boost on */

    printk_color(TERM_GREEN, "[AUDIO] AC'97 codec ready (ExtID:%04X)\n", ext_id);
    return 0;
}

/* ===== VirtIO Sound ===== */

#define VIRTIO_SOUND_BASE   0x09003000

static volatile uint32 *virtio_sound_regs = NULL;

static void virtio_sound_write(uint32 offset, uint32 val)
{
    if (virtio_sound_regs) virtio_sound_regs[offset >> 2] = val;
}

static uint32 virtio_sound_read(uint32 offset)
{
    return virtio_sound_regs ? virtio_sound_regs[offset >> 2] : 0;
}

static int virtio_sound_init(void)
{
    virtio_sound_regs = (volatile uint32 *)VIRTIO_SOUND_BASE;

    virtio_sound_write(VIRTIO_PCI_STATUS, 0);
    virtio_sound_write(VIRTIO_PCI_STATUS, VIRTIO_STATUS_ACKNOWLEDGE);
    virtio_sound_write(VIRTIO_PCI_STATUS, VIRTIO_STATUS_ACKNOWLEDGE | VIRTIO_STATUS_DRIVER);

    uint32 features = virtio_sound_read(VIRTIO_PCI_HOST_FEATURES);
    virtio_sound_write(VIRTIO_PCI_GUEST_FEATURES, features & ~(1 << 32));

    virtio_sound_write(VIRTIO_PCI_STATUS, VIRTIO_STATUS_ACKNOWLEDGE |
                        VIRTIO_STATUS_DRIVER | VIRTIO_STATUS_FEATURES_OK);
    virtio_sound_write(VIRTIO_PCI_STATUS, VIRTIO_STATUS_ACKNOWLEDGE |
                        VIRTIO_STATUS_DRIVER | VIRTIO_STATUS_FEATURES_OK |
                        VIRTIO_STATUS_DRIVER_OK);

    printk_color(TERM_GREEN, "[AUDIO] VirtIO Sound ready\n");
    return 0;
}

/* ===== Audio Core ===== */

void audio_init(void)
{
    printk_color(TERM_YELLOW, "[BOOT] Audio Subsystem...\n");

    memset(&audio_dev, 0, sizeof(audio_device_t));
    spin_init(&audio_dev.lock);
    audio_dev.sample_rate = AUDIO_RATE_44100;
    audio_dev.channels = 2;
    audio_dev.format = AUDIO_FMT_S16_LE;

    /* Allocate PCM buffers */
    for (int i = 0; i < PCM_BUFFER_COUNT; i++) {
        audio_dev.buffers[i].data = (uint8 *)kmalloc(PCM_BUFFER_SIZE);
        if (audio_dev.buffers[i].data) {
            memset(audio_dev.buffers[i].data, 0, PCM_BUFFER_SIZE);
            audio_dev.buffers[i].size = PCM_BUFFER_SIZE;
        }
    }

    /* Try AC'97 */
    audio_dev.mmio_base = 0x09004000;
    if (ac97_reset() == 0) {
        audio_dev.type = 0;
        audio_dev.initialized = 1;
    }
    /* Try VirtIO Sound */
    else if (virtio_sound_init() == 0) {
        audio_dev.type = 2;
        audio_dev.initialized = 1;
    }

    /* Init mixer */
    mixer_init();

    /* Init PCM ring */
    pcm_buffer_init();

    if (audio_dev.initialized) {
        printk_color(TERM_GREEN, "[BOOT] Audio: %s, %dHz, %dch, %s\n",
                     audio_dev.type == 0 ? "AC'97" : audio_dev.type == 1 ? "HDA" : "VirtIO",
                     audio_dev.sample_rate, audio_dev.channels,
                     audio_dev.format == AUDIO_FMT_U8 ? "U8" :
                     audio_dev.format == AUDIO_FMT_S16_LE ? "S16" :
                     audio_dev.format == AUDIO_FMT_S24_LE ? "S24" : "S32");
    } else {
        printk_color(TERM_YELLOW, "[BOOT] Audio: No hardware found\n");
    }
}

void audio_shutdown(void)
{
    audio_stop();
    for (int i = 0; i < PCM_BUFFER_COUNT; i++) {
        if (audio_dev.buffers[i].data) kfree(audio_dev.buffers[i].data);
    }
    audio_dev.initialized = 0;
}

int audio_open(uint32 rate, uint32 channels, uint32 format)
{
    if (!audio_dev.initialized) return -1;
    audio_dev.sample_rate = rate;
    audio_dev.channels = channels;
    audio_dev.format = format;
    printk_color(TERM_CYAN, "[AUDIO] Open: %dHz, %dch, fmt=%d\n", rate, channels, format);
    return 0;
}

void audio_close(void)
{
    audio_stop();
}

int audio_write(const void *data, uint32 size)
{
    if (!audio_dev.initialized || !data || size == 0) return -1;
    spin_lock(&audio_dev.lock);

    pcm_buffer_t *buf = &audio_dev.buffers[audio_dev.current_buffer];
    if (buf->used + size > buf->size) {
        /* Move to next buffer */
        buf->ready = 1;
        audio_dev.current_buffer = (audio_dev.current_buffer + 1) % PCM_BUFFER_COUNT;
        buf = &audio_dev.buffers[audio_dev.current_buffer];
        buf->used = 0;
        buf->ready = 0;
    }

    uint32 to_copy = size;
    if (buf->used + to_copy > buf->size) to_copy = buf->size - buf->used;
    memcpy(buf->data + buf->used, data, to_copy);
    buf->used += to_copy;

    spin_unlock(&audio_dev.lock);
    return (int)to_copy;
}

int audio_play(void)
{
    if (!audio_dev.initialized) return -1;
    audio_dev.playing = 1;
    printk_color(TERM_GREEN, "[AUDIO] Playback started\n");
    return 0;
}

int audio_stop(void)
{
    audio_dev.playing = 0;
    pcm_buffer_reset();
    printk_color(TERM_YELLOW, "[AUDIO] Playback stopped\n");
    return 0;
}

int audio_pause(void)
{
    audio_dev.playing = 0;
    return 0;
}

int audio_resume(void)
{
    audio_dev.playing = 1;
    return 0;
}

int audio_is_playing(void) { return audio_dev.playing; }

/* ===== Mixer ===== */

void mixer_init(void)
{
    mixer.master_volume = 80;
    for (int i = 0; i < MIXER_CHANNELS; i++) {
        mixer.channel_volumes[i] = 80;
        mixer.channel_mutes[i] = 0;
    }
    mixer.mute = 0;
    printk_color(TERM_GREEN, "[AUDIO] Mixer initialized\n");
}

void mixer_set_master_volume(int volume)
{
    if (volume < 0) volume = 0;
    if (volume > 100) volume = 100;
    mixer.master_volume = volume;
    if (audio_dev.type == 0) {
        /* AC'97: 0x1F = mute, 0x00 = max */
        uint16 vol = (uint16)((100 - volume) * 0x1F / 100);
        vol = (vol << 8) | vol;
        ac97_write_nam(AC97_MASTER_VOL, vol);
    }
}

int mixer_get_master_volume(void) { return mixer.master_volume; }

void mixer_set_channel_volume(int channel, int volume)
{
    if (channel < 0 || channel >= MIXER_CHANNELS) return;
    if (volume < 0) volume = 0;
    if (volume > 100) volume = 100;
    mixer.channel_volumes[channel] = volume;
}

int mixer_get_channel_volume(int channel)
{ return (channel >= 0 && channel < MIXER_CHANNELS) ? mixer.channel_volumes[channel] : 0; }

void mixer_set_mute(int mute) { mixer.mute = mute; }
int mixer_get_mute(void) { return mixer.mute; }
void mixer_set_channel_mute(int channel, int mute)
{ if (channel >= 0 && channel < MIXER_CHANNELS) mixer.channel_mutes[channel] = mute; }

/* ===== PCM Ring Buffer ===== */

void pcm_buffer_init(void)
{
    for (int i = 0; i < PCM_BUFFER_COUNT; i++) {
        pcm_ring[i].used = 0;
        pcm_ring[i].ready = 0;
        if (!pcm_ring[i].data) pcm_ring[i].data = (uint8 *)kmalloc(PCM_BUFFER_SIZE);
    }
}

int pcm_buffer_write(const void *data, uint32 size)
{
    if (!data || size == 0) return 0;
    for (int i = 0; i < PCM_BUFFER_COUNT; i++) {
        if (!pcm_ring[i].ready) {
            uint32 to_write = size;
            if (to_write > PCM_BUFFER_SIZE - pcm_ring[i].used)
                to_write = PCM_BUFFER_SIZE - pcm_ring[i].used;
            memcpy(pcm_ring[i].data + pcm_ring[i].used, data, to_write);
            pcm_ring[i].used += to_write;
            if (pcm_ring[i].used >= PCM_BUFFER_SIZE) pcm_ring[i].ready = 1;
            return (int)to_write;
        }
    }
    return 0;
}

int pcm_buffer_read(void *data, uint32 size)
{
    if (!data || size == 0) return 0;
    for (int i = 0; i < PCM_BUFFER_COUNT; i++) {
        if (pcm_ring[i].ready && pcm_ring[i].used > 0) {
            uint32 to_read = size;
            if (to_read > pcm_ring[i].used) to_read = pcm_ring[i].used;
            memcpy(data, pcm_ring[i].data, to_read);
            pcm_ring[i].used -= to_read;
            if (pcm_ring[i].used == 0) pcm_ring[i].ready = 0;
            return (int)to_read;
        }
    }
    return 0;
}

int pcm_buffer_available(void)
{
    int total = 0;
    for (int i = 0; i < PCM_BUFFER_COUNT; i++)
        if (pcm_ring[i].ready) total += pcm_ring[i].used;
    return total;
}

void pcm_buffer_reset(void)
{
    for (int i = 0; i < PCM_BUFFER_COUNT; i++) {
        pcm_ring[i].used = 0;
        pcm_ring[i].ready = 0;
    }
}

/* ===== Format Conversion ===== */

int audio_convert_format(void *dst, int dst_fmt, const void *src, int src_fmt, int samples)
{
    if (!dst || !src || samples <= 0) return -1;

    if (dst_fmt == src_fmt) {
        int sample_size = (dst_fmt == AUDIO_FMT_U8) ? 1 : (dst_fmt == AUDIO_FMT_S16_LE) ? 2 :
                          (dst_fmt == AUDIO_FMT_S24_LE) ? 3 : 4;
        memcpy(dst, src, samples * sample_size);
        return 0;
    }

    /* S16 -> U8 */
    if (src_fmt == AUDIO_FMT_S16_LE && dst_fmt == AUDIO_FMT_U8) {
        const int16 *s = (const int16 *)src;
        uint8 *d = (uint8 *)dst;
        for (int i = 0; i < samples; i++) d[i] = (uint8)((s[i] >> 8) + 128);
        return 0;
    }

    /* U8 -> S16 */
    if (src_fmt == AUDIO_FMT_U8 && dst_fmt == AUDIO_FMT_S16_LE) {
        const uint8 *s = (const uint8 *)src;
        int16 *d = (int16 *)dst;
        for (int i = 0; i < samples; i++) d[i] = (int16)((s[i] - 128) << 8);
        return 0;
    }

    /* Default: copy */
    memcpy(dst, src, samples * 2);
    return 0;
}

/* ===== Tone Generator ===== */

void audio_generate_sine(void *buffer, uint32 freq, uint32 rate, uint32 samples, int format)
{
    if (!buffer || freq == 0 || rate == 0) return;

    if (format == AUDIO_FMT_S16_LE) {
        int16 *buf = (int16 *)buffer;
        for (uint32 i = 0; i < samples; i++) {
            float t = (float)i / rate;
            buf[i] = (int16)(sinf(2.0f * 3.14159f * freq * t) * 32767.0f);
        }
    } else if (format == AUDIO_FMT_U8) {
        uint8 *buf = (uint8 *)buffer;
        for (uint32 i = 0; i < samples; i++) {
            float t = (float)i / rate;
            buf[i] = (uint8)(sinf(2.0f * 3.14159f * freq * t) * 127.0f + 128.0f);
        }
    }
}

void audio_generate_square(void *buffer, uint32 freq, uint32 rate, uint32 samples, int format)
{
    if (!buffer || freq == 0 || rate == 0) return;

    uint32 period = rate / freq;
    if (format == AUDIO_FMT_S16_LE) {
        int16 *buf = (int16 *)buffer;
        for (uint32 i = 0; i < samples; i++)
            buf[i] = ((i % period) < (period / 2)) ? 32767 : -32768;
    } else if (format == AUDIO_FMT_U8) {
        uint8 *buf = (uint8 *)buffer;
        for (uint32 i = 0; i < samples; i++)
            buf[i] = ((i % period) < (period / 2)) ? 255 : 0;
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
