/*
 * torOS Audio Subsystem Header
 * AC'97 / Intel HDA / virtio-sound driver, mixer, PCM playback
 */

#ifndef _AUDIO_H
#define _AUDIO_H

#include "toros.h"

/* ===== Audio Formats ===== */
#define AUDIO_FMT_U8        0
#define AUDIO_FMT_S16_LE    1
#define AUDIO_FMT_S24_LE    2
#define AUDIO_FMT_S32_LE    3
#define AUDIO_FMT_FLOAT     4

/* ===== Standard Rates ===== */
#define AUDIO_RATE_8000     8000
#define AUDIO_RATE_11025    11025
#define AUDIO_RATE_16000    16000
#define AUDIO_RATE_22050    22050
#define AUDIO_RATE_32000    32000
#define AUDIO_RATE_44100    44100
#define AUDIO_RATE_48000    48000
#define AUDIO_RATE_96000    96000

/* ===== AC'97 Registers ===== */
#define AC97_RESET          0x00
#define AC97_MASTER_VOL     0x02
#define AC97_HEADPHONE_VOL  0x04
#define AC97_MONO_VOL       0x06
#define AC97_MASTER_TONE    0x08
#define AC97_PC_BEEP        0x0A
#define AC97_PHONE_VOL      0x0C
#define AC97_MIC_VOL        0x0E
#define AC97_LINE_IN_VOL    0x10
#define AC97_CD_VOL         0x12
#define AC97_VIDEO_VOL      0x14
#define AC97_AUX_VOL        0x16
#define AC97_PCM_OUT_VOL    0x18
#define AC97_RECORD_SELECT  0x1A
#define AC97_RECORD_GAIN    0x1C
#define AC97_RECORD_GAIN_MIC 0x1E
#define AC97_GENERAL_PURPOSE 0x20
#define AC97_3D_CONTROL     0x22
#define AC97_PAGING         0x24
#define AC97_POWERDOWN      0x26
#define AC97_EXTENDED_ID    0x28
#define AC97_VENDOR_ID1     0x7C
#define AC97_VENDOR_ID2     0x7E

/* ===== Intel HDA ===== */
#define HDA_GCTL            0x08
#define HDA_WAKEEN          0x0C
#define HDA_STATESTS        0x0E
#define HDA_GSTS            0x10
#define HDA_OUTPAY          0x14
#define HDA_INPAY           0x16
#define HDA_INTCTL          0x20
#define HDA_INTSTS          0x24
#define HDA_WALCLK          0x30
#define HDA_SYNC            0x34
#define HDA_CORBLBASE       0x40
#define HDA_CORBUBASE       0x44
#define HDA_CORBWP          0x48
#define HDA_CORBRP          0x4A
#define HDA_CORBCTL         0x4C
#define HDA_CORBSTS         0x4D
#define HDA_CORBSIZE        0x4E
#define HDA_RIRBLBASE       0x50
#define HDA_RIRBUBASE       0x54
#define HDA_RIRBWP          0x58
#define HDA_RINTCNT         0x5A
#define HDA_RIRBCTL         0x5C
#define HDA_RIRBSTS         0x5D
#define HDA_RIRBSIZE        0x5E

/* ===== VirtIO Sound ===== */
#define VIRTIO_SOUND_PCM_FMT_U8     0
#define VIRTIO_SOUND_PCM_FMT_S16    1
#define VIRTIO_SOUND_PCM_FMT_S24    2
#define VIRTIO_SOUND_PCM_FMT_S32    3

#define VIRTIO_SOUND_PCM_RATE_8000  0
#define VIRTIO_SOUND_PCM_RATE_11025 1
#define VIRTIO_SOUND_PCM_RATE_16000 2
#define VIRTIO_SOUND_PCM_RATE_22050 3
#define VIRTIO_SOUND_PCM_RATE_32000 4
#define VIRTIO_SOUND_PCM_RATE_44100 5
#define VIRTIO_SOUND_PCM_RATE_48000 6
#define VIRTIO_SOUND_PCM_RATE_96000 7

/* ===== PCM Buffer ===== */
#define PCM_BUFFER_SIZE     (8192 * 4)
#define PCM_BUFFER_COUNT    4

typedef struct {
    uint8 *data;
    uint32 size;
    uint32 used;
    volatile uint32 ready;
} pcm_buffer_t;

/* ===== Audio Device ===== */
typedef struct {
    uint32 type;        /* 0=AC97, 1=HDA, 2=virtio */
    uint32 mmio_base;
    uint32 irq;
    uint32 initialized;
    uint32 sample_rate;
    uint32 channels;
    uint32 format;
    uint32 playing;
    pcm_buffer_t buffers[PCM_BUFFER_COUNT];
    uint32 current_buffer;
    uint32 buffer_pos;
    spinlock_t lock;
} audio_device_t;

/* ===== Mixer ===== */
#define MIXER_CHANNELS      8

typedef struct {
    int32 master_volume;    /* 0-100 */
    int32 channel_volumes[MIXER_CHANNELS];
    int32 mute;
    int32 channel_mutes[MIXER_CHANNELS];
} mixer_state_t;

/* ===== Audio API ===== */
void audio_init(void);
void audio_shutdown(void);
int audio_open(uint32 rate, uint32 channels, uint32 format);
void audio_close(void);
int audio_write(const void *data, uint32 size);
int audio_play(void);
int audio_stop(void);
int audio_pause(void);
int audio_resume(void);
int audio_is_playing(void);

/* ===== Mixer API ===== */
void mixer_init(void);
void mixer_set_master_volume(int volume);
int mixer_get_master_volume(void);
void mixer_set_channel_volume(int channel, int volume);
int mixer_get_channel_volume(int channel);
void mixer_set_mute(int mute);
int mixer_get_mute(void);
void mixer_set_channel_mute(int channel, int mute);

/* ===== Format Conversion ===== */
int audio_convert_format(void *dst, int dst_fmt, const void *src, int src_fmt, int samples);
int audio_resample(void *dst, uint32 dst_rate, const void *src, uint32 src_rate, int samples, int channels, int format);

/* ===== PCM Ring Buffer ===== */
void pcm_buffer_init(void);
int pcm_buffer_write(const void *data, uint32 size);
int pcm_buffer_read(void *data, uint32 size);
int pcm_buffer_available(void);
void pcm_buffer_reset(void);

/* ===== Tone Generator ===== */
void audio_generate_sine(void *buffer, uint32 freq, uint32 rate, uint32 samples, int format);
void audio_generate_square(void *buffer, uint32 freq, uint32 rate, uint32 samples, int format);
void audio_generate_triangle(void *buffer, uint32 freq, uint32 rate, uint32 samples, int format);
void audio_beep(uint32 freq, uint32 duration_ms);

#endif
