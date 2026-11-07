





























#include "vice.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <assert.h>

#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif

#include "archdep.h"
#include "clkguard.h"
#include "cmdline.h"
#include "debug.h"
#include "fixpoint.h"
#include "lib.h"
#include "log.h"
#include "machine.h"
#include "maincpu.h"
#include "monitor.h"
#include "resources.h"
#include "sound.h"
#include "types.h"
#include "uiapi.h"
#include "util.h"
#include "vsync.h"
#include "math.h"
#include "ui.h"


static log_t sound_log = LOG_ERR;



#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif



typedef struct sound_register_devices_s {
    char *name;
    int (*init)(void);
    int is_playback_device;
} sound_register_devices_t;


static sound_register_devices_t sound_register_devices[] = {

    


#ifdef USE_PULSE
    { "pulse", sound_init_pulse_device, SOUND_PLAYBACK_DEVICE },
#endif
#ifdef USE_ARTS
    { "arts", sound_init_arts_device, SOUND_PLAYBACK_DEVICE },
#endif
#ifdef USE_ALSA
    { "alsa", sound_init_alsa_device, SOUND_PLAYBACK_DEVICE },
#endif
#ifdef USE_COREAUDIO
    { "coreaudio", sound_init_coreaudio_device, SOUND_PLAYBACK_DEVICE },
#endif
#ifdef USE_OSS



#if !defined(__FreeBSD__) && !defined(__bsdi__)
    { "uss", sound_init_uss_device, SOUND_PLAYBACK_DEVICE },
#endif
#endif
#ifdef USE_DMEDIA
    { "sgi", sound_init_sgi_device, SOUND_PLAYBACK_DEVICE },
#endif


#if defined(HAVE_SYS_AUDIOIO_H) && !defined(__OpenBSD__)
#if defined(__NetBSD__)
    { "netbsd", sound_init_sun_device, SOUND_PLAYBACK_DEVICE },
#else
    { "sun", sound_init_sun_device, SOUND_PLAYBACK_DEVICE },
#endif
#endif
#if defined(HAVE_SYS_AUDIO_H)
    { "hpux", sound_init_hpux_device, SOUND_PLAYBACK_DEVICE },
#endif
#ifdef USE_AIX_AUDIO
    { "aix", sound_init_aix_device, SOUND_PLAYBACK_DEVICE },
#endif

#ifdef WIN32_COMPILE
#ifdef USE_DXSOUND
    { "dx", sound_init_dx_device, SOUND_PLAYBACK_DEVICE },
#endif
#if !defined(__XBOX__)
    { "wmm", sound_init_wmm_device, SOUND_PLAYBACK_DEVICE },
#endif
#endif

#if defined(__OS2__) && !defined(USE_SDLUI)
    { "dart", sound_init_dart_device, SOUND_PLAYBACK_DEVICE },
#endif

#ifdef BEOS_COMPILE
    { "beos", sound_init_beos_device, SOUND_PLAYBACK_DEVICE },
    { "bsp", sound_init_bsp_device, SOUND_PLAYBACK_DEVICE },
#endif

#if defined(AMIGA_SUPPORT) && defined(HAVE_DEVICES_AHI_H)
    { "ahi", sound_init_ahi_device, SOUND_PLAYBACK_DEVICE },
#endif

    
#ifdef USE_SDL_AUDIO
    { "sdl", sound_init_sdl_device, SOUND_PLAYBACK_DEVICE },
#endif

    


    { "dummy", sound_init_dummy_device, SOUND_PLAYBACK_DEVICE },

    { "fs", sound_init_fs_device, SOUND_RECORD_DEVICE },
    { "dump", sound_init_dump_device, SOUND_RECORD_DEVICE },
    { "wav", sound_init_wav_device, SOUND_RECORD_DEVICE },
    { "voc", sound_init_voc_device, SOUND_RECORD_DEVICE },
    { "iff", sound_init_iff_device, SOUND_RECORD_DEVICE },
    { "aiff", sound_init_aiff_device, SOUND_RECORD_DEVICE },

#ifdef USE_LAMEMP3
    { "mp3", sound_init_mp3_device, SOUND_RECORD_DEVICE },
#endif

#ifdef USE_FLAC
    { "flac", sound_init_flac_device, SOUND_RECORD_DEVICE },
#endif

#ifdef USE_VORBIS
    { "ogg", sound_init_vorbis_device, SOUND_RECORD_DEVICE },
#endif

    { "soundmovie", sound_init_movie_device, SOUND_RECORD_DEVICE },
    { NULL, NULL, 0 }
};



static uint16_t offset = 0;

static sound_chip_t *sound_calls[20];

uint16_t sound_chip_register(sound_chip_t *chip)
{
    assert(chip != NULL);

    sound_calls[offset >> 5] = chip;
    offset += 0x20;

    assert((offset >> 5) < 20);

    return offset - 0x20;
}



static sound_t *sound_machine_open(int chipno)
{
    sound_t *retval = NULL;
    int i;

    for (i = 0; i < (offset >> 5); i++) {
        if (sound_calls[i]->open) {
            retval = sound_calls[i]->open(chipno);
        }
    }
    return retval;
}

static int sound_machine_init(sound_t *psid, int speed, int cycles_per_sec)
{
    int retval = 1;
    int i;

    for (i = 0; i < (offset >> 5); i++) {
        if (sound_calls[i]->init) {
            retval &= sound_calls[i]->init(psid, speed, cycles_per_sec);
        }
    }
    return retval;
}

static void sound_machine_close(sound_t *psid)
{
    int i;

    for (i = 0; i < (offset >> 5); i++) {
        if (sound_calls[i]->close) {
            sound_calls[i]->close(psid);
        }
    }
}









static int sound_machine_calculate_samples(sound_t **psid, int16_t *pbuf, int nr, int soc, int scc, int *delta_t)
{
    int i;
    int temp;

    if (sound_calls[0]->cycle_based() || (!sound_calls[0]->cycle_based() && sound_calls[0]->chip_enabled)) {
        temp = sound_calls[0]->calculate_samples(psid, pbuf, nr, soc, scc, delta_t);
    } else {
        memset(pbuf, 0, nr * sizeof(int16_t) * soc); 
        temp = nr;
    }

    for (i = 1; i < (offset >> 5); i++) {
        if (sound_calls[i]->chip_enabled) {
            sound_calls[i]->calculate_samples(psid, pbuf, temp, soc, scc, delta_t);
        }
    }
    return temp;
}

static void sound_machine_store(sound_t *psid, uint16_t addr, uint8_t val)
{
    sound_calls[addr >> 5]->store(psid, (uint16_t)(addr & 0x1f), val);
}

static uint8_t sound_machine_read(sound_t *psid, uint16_t addr)
{
    return sound_calls[addr >> 5]->read(psid, (uint16_t)(addr & 0x1f));
}

static void sound_machine_reset(sound_t *psid, CLOCK cpu_clk)
{
    int i;

    for (i = 0; i < (offset >> 5); i++) {
        if (sound_calls[i]->reset) {
            sound_calls[i]->reset(psid, cpu_clk);
        }
    }
}

static int sound_machine_cycle_based(void)
{
    int i;
    int retval = 0;

    for (i = 0; i < (offset >> 5); i++) {
        retval |= sound_calls[i]->cycle_based();
    }
    return retval;
}

static int sound_machine_channels(void)
{
    int i;
    int retval = 0;
    int temp;

    for (i = 0; i < (offset >> 5); i++) {
        temp = sound_calls[i]->channels();
        if (temp > retval) {
            retval = temp;
        }
    }
    return retval;
}








static int playback_enabled;           
static int sample_rate;                
static char *device_name = NULL;       
static char *device_arg = NULL;        
static char *recorddevice_name = NULL; 
static char *recorddevice_arg = NULL;  
static int buffer_size;                
static int suspend_time;               
static int speed_adjustment_setting;   
static int volume;
static int amp;
static int fragment_size;
static int output_option;


static int fragment_divisor[] = {
    32, 
    16, 
     8, 
     4, 
     2, 
     1  
};

static char *playback_devices_cmdline = NULL;
static char *record_devices_cmdline = NULL;



static int sdev_open = FALSE;



int sound_state_changed;
int sid_state_changed;


static int cycle_based = 0;

static int set_output_option(int val, void *param)
{
    switch (val) {
        case SOUND_OUTPUT_SYSTEM:
        case SOUND_OUTPUT_MONO:
        case SOUND_OUTPUT_STEREO:
            break;
        default:
            return -1;
    }

    if (output_option != val) {
        output_option = val;
        sound_state_changed = TRUE;
    }
    return 0;
}

static int set_playback_enabled(int value, void *param)
{
    int val = value ? 1 : 0;

    if (val) {
        vsync_disable_timer();
    }

    playback_enabled = val;
    sound_machine_enable(playback_enabled);
    return 0;
}

static int set_sample_rate(int val, void *param)
{
    if (val <= 0) {
        return -1;
    }

    sample_rate = val;
    sound_state_changed = TRUE;
    return 0;
}

static int set_device_name(const char *val, void *param)
{
    if (!val || val[0] == '\0') {
        
#ifdef BEOS_COMPILE
        if (CheckForHaiku()) {
            util_string_set(&device_name, "bsp");
        } else
#endif
        {
            util_string_set(&device_name, sound_register_devices[0].name);
        }
    } else {
        util_string_set(&device_name, val);
    }
    sound_state_changed = TRUE;
    return 0;
}

static int set_device_arg(const char *val, void *param)
{
    util_string_set(&device_arg, val);
    sound_state_changed = TRUE;
    return 0;
}

static int set_recorddevice_name(const char *val, void *param)
{
    util_string_set(&recorddevice_name, val);
    sound_state_changed = TRUE;
    return 0;
}

static int set_recorddevice_arg(const char *val, void *param)
{
    util_string_set(&recorddevice_arg, val);
    sound_state_changed = TRUE;
    return 0;
}

static int set_buffer_size(int val, void *param)
{
    if (val > 0) {
        buffer_size = val;
    } else {
        if (machine_class == VICE_MACHINE_VSID) {
            buffer_size = SOUND_SAMPLE_MAX_BUFFER_SIZE;
        } else {
            buffer_size = SOUND_SAMPLE_BUFFER_SIZE;
        }
    }

    sound_state_changed = TRUE;
    return 0;
}

static int set_fragment_size(int val, void *param)
{
    if (val < SOUND_FRAGMENT_VERY_SMALL) {
        val = SOUND_FRAGMENT_VERY_SMALL;
    } else if (val > SOUND_FRAGMENT_VERY_LARGE) {
        val = SOUND_FRAGMENT_VERY_LARGE;
    }
    fragment_size = val;
    sound_state_changed = TRUE;
    return 0;
}

static int set_suspend_time(int val, void *param)
{
    suspend_time = val;

    if (suspend_time < 0) {
        suspend_time = 0;
    }

    sound_state_changed = TRUE;
    return 0;
}

static int set_speed_adjustment_setting(int val, void *param)
{
    if (val == SOUND_ADJUST_DEFAULT) {
        speed_adjustment_setting = SOUND_ADJUST_EXACT;
    } else {
        switch (val) {
            case SOUND_ADJUST_FLEXIBLE:
            case SOUND_ADJUST_ADJUSTING:
            case SOUND_ADJUST_EXACT:
                break;
            default:
                return -1;
        }
        speed_adjustment_setting = val;
    }

    return 0;
}

static int set_volume(int val, void *param)
{
    volume = val;

    if (volume < 0) {
        volume = 0;
    }

    if (volume > 100) {
        volume = 100;
    }

    amp = (int)((exp((double)volume / 100.0 * log(2.0)) - 1.0) * 4096.0);

    ui_display_volume(volume);

    return 0;
}

static const resource_string_t resources_string[] = {
    { "SoundDeviceName", "", RES_EVENT_NO, NULL,
      &device_name, set_device_name, NULL },
    { "SoundDeviceArg", "", RES_EVENT_NO, NULL,
      &device_arg, set_device_arg, NULL },
    { "SoundRecordDeviceName", "", RES_EVENT_STRICT, (resource_value_t)"",
      &recorddevice_name, set_recorddevice_name, NULL },
    { "SoundRecordDeviceArg", "", RES_EVENT_NO, NULL,
      &recorddevice_arg, set_recorddevice_arg, NULL },
    RESOURCE_STRING_LIST_END
};

static const resource_int_t resources_int[] = {
    { "Sound", 1, RES_EVENT_SAME, NULL,
      (void *)&playback_enabled, set_playback_enabled, NULL },
    { "SoundSampleRate", SOUND_SAMPLE_RATE, RES_EVENT_NO, NULL,
      (void *)&sample_rate, set_sample_rate, NULL },
    { "SoundBufferSize", 0, RES_EVENT_NO, NULL,
      (void *)&buffer_size, set_buffer_size, NULL },
    { "SoundFragmentSize", ARCHDEP_SOUND_FRAGMENT_SIZE, RES_EVENT_NO, NULL,
      (void *)&fragment_size, set_fragment_size, NULL },
    { "SoundSuspendTime", 0, RES_EVENT_NO, NULL,
      (void *)&suspend_time, set_suspend_time, NULL },
    { "SoundSpeedAdjustment", SOUND_ADJUST_EXACT, RES_EVENT_NO, NULL,
      (void *)&speed_adjustment_setting, set_speed_adjustment_setting, NULL },
    { "SoundVolume", 100, RES_EVENT_NO, NULL,
      (void *)&volume, set_volume, NULL },
    { "SoundOutput", ARCHDEP_SOUND_OUTPUT_MODE, RES_EVENT_NO, NULL,
      (void *)&output_option, set_output_option, NULL },
    RESOURCE_INT_LIST_END
};

int sound_resources_init(void)
{
    if (resources_register_string(resources_string) < 0) {
        return -1;
    }

    return resources_register_int(resources_int);
}

void sound_resources_shutdown(void)
{
    lib_free(device_name);
    lib_free(device_arg);
    lib_free(recorddevice_name);
    lib_free(recorddevice_arg);
    lib_free(playback_devices_cmdline);
    lib_free(record_devices_cmdline);
}



static const cmdline_option_t cmdline_options[] =
{
    { "-sound", SET_RESOURCE, CMDLINE_ATTRIB_NONE,
      NULL, NULL, "Sound", (resource_value_t)1,
      NULL, "Enable sound playback" },
    { "+sound", SET_RESOURCE, CMDLINE_ATTRIB_NONE,
      NULL, NULL, "Sound", (resource_value_t)0,
      NULL, "Disable sound playback" },
    { "-soundrate", SET_RESOURCE, CMDLINE_ATTRIB_NEED_ARGS,
      NULL, NULL, "SoundSampleRate", NULL,
      "<value>", "Set sound sample rate to <value> Hz" },
    { "-soundbufsize", SET_RESOURCE, CMDLINE_ATTRIB_NEED_ARGS,
      NULL, NULL, "SoundBufferSize", NULL,
      "<value>", "Set sound buffer size to <value> msec" },
    { "-soundfragsize", SET_RESOURCE, CMDLINE_ATTRIB_NEED_ARGS,
      NULL, NULL, "SoundFragmentSize", NULL,
      "<value>", "Set sound fragment size (0: very small, 1: small, 2: medium, 3: large, 4: very large)" },
    { "-soundsync", SET_RESOURCE, CMDLINE_ATTRIB_NEED_ARGS,
      NULL, NULL, "SoundSpeedAdjustment", NULL,
      "<sync>", "Set sound speed adjustment (0: flexible, 1: adjusting, 2: exact)" },
    { "-soundoutput", SET_RESOURCE, CMDLINE_ATTRIB_NEED_ARGS,
      NULL, NULL, "SoundOutput", NULL,
      "<output mode>", "Sound output mode: (0: system decides mono/stereo, 1: always mono, 2: always stereo)" },
    { "-soundsuspend", SET_RESOURCE, CMDLINE_ATTRIB_NEED_ARGS,
      NULL, NULL, "SoundSuspendTime", NULL,
      "<Seconds>", "Specify the pause interval when audio underflows (clicks) happen. 0 means no pause is done." },
    { "-soundvolume", SET_RESOURCE, CMDLINE_ATTRIB_NEED_ARGS,
      NULL, NULL, "SoundVolume", NULL,
      "<Volume>", "Specify the sound volume (0..100)" },
    CMDLINE_LIST_END
};

static cmdline_option_t devs_cmdline_options[] =
{
    { "-sounddev", SET_RESOURCE, CMDLINE_ATTRIB_NEED_ARGS,
      NULL, NULL, "SoundDeviceName", NULL,
      "<Name>", NULL },
    { "-soundarg", SET_RESOURCE, CMDLINE_ATTRIB_NEED_ARGS,
      NULL, NULL, "SoundDeviceArg", NULL,
      "<args>", "Specify initialization parameters for sound driver" },
    { "-soundrecdev", SET_RESOURCE, CMDLINE_ATTRIB_NEED_ARGS,
      NULL, NULL, "SoundRecordDeviceName", NULL,
      "<Name>", NULL },
    { "-soundrecarg", SET_RESOURCE, CMDLINE_ATTRIB_NEED_ARGS,
      NULL, NULL, "SoundRecordDeviceArg", NULL,
      "<args>", "Specify initialization parameters for recording sound driver" },
    CMDLINE_LIST_END
};

int sound_cmdline_options_init(void)
{
    int i;
    int started_playback = 0;
    int started_record = 0;
    char *temp = NULL;

    if (cmdline_register_options(cmdline_options) < 0) {
        return -1;
    }

    playback_devices_cmdline = lib_stralloc("Specify sound driver. (");
    record_devices_cmdline = lib_stralloc("Specify recording sound driver. (");

    for (i = 0; sound_register_devices[i].name; i++) {
        if (sound_register_devices[i].is_playback_device) {
            if (started_playback) {
                temp = util_concat(playback_devices_cmdline, "/", sound_register_devices[i].name, NULL);
            } else {
                temp = util_concat(playback_devices_cmdline, sound_register_devices[i].name, NULL);
                started_playback = 1;
            }
            lib_free(playback_devices_cmdline);
            playback_devices_cmdline = temp;
        } else {
            if (started_record) {
                temp = util_concat(record_devices_cmdline, "/", sound_register_devices[i].name, NULL);
            } else {
                temp = util_concat(record_devices_cmdline, sound_register_devices[i].name, NULL);
                started_record = 1;
            }
            lib_free(record_devices_cmdline);
            record_devices_cmdline = temp;
        }
    }
    temp = util_concat(playback_devices_cmdline, ")", NULL);
    lib_free(playback_devices_cmdline);
    playback_devices_cmdline = temp;

    temp = util_concat(record_devices_cmdline, ")", NULL);
    lib_free(record_devices_cmdline);
    record_devices_cmdline = temp;

    devs_cmdline_options[0].description = playback_devices_cmdline;
    devs_cmdline_options[2].description = record_devices_cmdline;

    return cmdline_register_options(devs_cmdline_options);
}




static unsigned int cycles_per_sec;
static unsigned int cycles_per_rfsh;
static double rfsh_per_sec;


static int speed_percent;


static int warp_mode_enabled;

typedef struct {
    
    int sound_output_channels;

    
    int sound_chip_channels;

    
    sound_t *psid[SOUND_SIDS_MAX];

    
    soundclk_t clkstep;

    
    soundclk_t origclkstep;

    
    soundclk_t clkfactor;

    
    soundclk_t fclk;

    
    CLOCK wclk;

    
    CLOCK lastclk;

    
    int16_t buffer[SOUND_CHANNELS_MAX * SOUND_BUFSIZE];

    
    int bufptr;

    
    sound_device_t *playdev;

    
    sound_device_t *recdev;

    
    int fragsize;

    
    int fragnr;

    
    int bufsize;

    
    int prevused;
    int prevfill;

    
    int issuspended;
    int16_t lastsample[SOUND_CHANNELS_MAX];
} snddata_t;

static snddata_t snddata;


#define MAX_SOUND_DEVICES 24

static sound_device_t *sound_devices[MAX_SOUND_DEVICES];

static int sound_device_count = 0;

int sound_register_device(sound_device_t *pdevice)
{
    if (sound_device_count < MAX_SOUND_DEVICES) {
        sound_devices[sound_device_count] = pdevice;
        sound_device_count++;
    } else {
        log_error(sound_log, "available sound devices exceed VICEs storage");
    }

    return 0;
}

unsigned int sound_device_num(void)
{
    return sound_device_count;
}

const char *sound_device_name(unsigned int num)
{
    return sound_devices[num]->name;
}



static time_t disabletime;

static void suspendsound(const char *reason)
{
    disabletime = time(0);
    log_warning(sound_log, "suspend, disabling sound for %d secs (%s)",
                suspend_time, reason);
    sound_state_changed = TRUE;
}

static void enablesound(void)
{
    time_t diff;
    if (!disabletime) {
        return;
    }
    diff = time(0) - disabletime;
    if (diff < 0 || diff >= (time_t)suspend_time) {
        disabletime = 0;
    }
}


static int sound_error(const char *msg)
{
    sound_close();

    if (console_mode || video_disabled_mode) {
        log_message(sound_log, "%s", msg);
    } else {
        char *txt = lib_msprintf("Sound: %s", msg);
        ui_error(txt);
        lib_free(txt);
    }

    playback_enabled = 0;

    if (!console_mode) {
        ui_update_menus();
    }

    return 1;
}

static int16_t *temp_buffer = NULL;
static int temp_buffer_size = 0;

static int16_t *realloc_buffer(int size)
{
    if (temp_buffer_size < size) {
        temp_buffer = lib_realloc(temp_buffer, size);
        if (temp_buffer) {
            temp_buffer_size = size;
            memset(temp_buffer, 0, size);
        } else {
            temp_buffer_size = 0;
        }
    }
    return temp_buffer;
}






static void fill_buffer(int size, int rise)
{
    int c, i;
    int16_t *p;
    double factor;
    static int fb_enter_logged = 0;
    static int fb_done_logged = 0;

    if (fb_enter_logged < 4) {
        fb_enter_logged++;
        log_message(LOG_DEFAULT, "SND: fill_buffer enter size=%d rise=%d", size, rise);
    }

    p = realloc_buffer(size * sizeof(int16_t) * snddata.sound_output_channels);
    if (!p) {
        return;
    }

    for (c = 0; c < snddata.sound_output_channels; c++) {
        for (i = 0; i < size; i++) {
            if (rise < 0) {
                factor = (double)(size - i) / size;
            } else {
                if (rise > 0) {
                    factor = (double)i / size;
                } else {
                    factor = 1.0;
                }
            }

            p[i * snddata.sound_output_channels + c] = (int16_t)(snddata.lastsample[c] * factor);
        }
    }

    i = snddata.playdev->write(p, size * snddata.sound_output_channels);
    if (i) {
        sound_error("write to sound device failed.");
    }

    if (fb_done_logged < 4) {
        fb_done_logged++;
        log_message(LOG_DEFAULT, "SND: fill_buffer done rc=%d", i);
    }
}



static int sid_open(void)
{
    int c;

    for (c = 0; c < snddata.sound_chip_channels; c++) {
        if (!(snddata.psid[c] = sound_machine_open(c))) {
            return sound_error("Cannot open SID engine");
        }
    }

    return 0;
}


static int sid_init(void)
{
    int c, speed, speed_factor;

    

    cycle_based = sound_machine_cycle_based();

    

    speed_factor = speed_percent ? speed_percent : 100;
    speed = sample_rate * 100 / speed_factor;

    for (c = 0; c < snddata.sound_chip_channels; c++) {
        if (!sound_machine_init(snddata.psid[c], speed, cycles_per_sec)) {
            return sound_error("Cannot initialize SID engine");
        }
    }

    snddata.clkstep = SOUNDCLK_CONSTANT(cycles_per_sec) / sample_rate;

    snddata.origclkstep = snddata.clkstep;
    snddata.clkfactor = SOUNDCLK_CONSTANT(1.0);
    snddata.fclk = SOUNDCLK_CONSTANT(maincpu_clk);
    snddata.wclk = maincpu_clk;
    snddata.lastclk = maincpu_clk;

    return 0;
}


static void sid_close(void)
{
    int c;
    for (c = 0; c < snddata.sound_chip_channels; c++) {
        if (snddata.psid[c]) {
            sound_machine_close(snddata.psid[c]);
            snddata.psid[c] = NULL;
        }
    }
}

sound_t *sound_get_psid(unsigned int channel)
{
    return snddata.psid[channel];
}


int sound_open(void)
{
    int c, i, j;
    int channels_cap;
    int channels;
    sound_device_t *pdev, *rdev;
    char *playname, *recname;
    char *playparam, *recparam;
    char *err;
    int speed;
    int fragsize;
    int fragnr;
    char frag_str[8];
    double bufsize;

    if (suspend_time > 0 && disabletime) {
        return 1;
    }

    

    vsync_suspend_speed_eval();

    
    snddata.sound_chip_channels = sound_machine_channels();

    playname = device_name;
    if (playname && playname[0] == '\0') {
        playname = NULL;
    }

    playparam = device_arg;
    if (playparam && playparam[0] == '\0') {
        playparam = NULL;
    }

    recname = recorddevice_name;
    if (recname && recname[0] == '\0') {
        recname = NULL;
    }

    recparam = recorddevice_arg;
    if (recparam && recparam[0] == '\0') {
        recparam = NULL;
    }

    
    bufsize = ((buffer_size < 1 || buffer_size > 1000)
               ? SOUND_SAMPLE_BUFFER_SIZE : buffer_size) / 1000.0;
    speed = (sample_rate < 8000 || sample_rate > 96000)
            ? SOUND_SAMPLE_RATE : sample_rate;

    switch (output_option) {
        case SOUND_OUTPUT_SYSTEM:
        default:
            channels = (snddata.sound_chip_channels >= 2) ? 2 : 1;
            break;
        case SOUND_OUTPUT_MONO:
            channels = 1;
            break;
        case SOUND_OUTPUT_STEREO:
            channels = 2;
            break;
    }

    
    for (i = 0; (pdev = sound_devices[i]); i++) {
        if (!playname || (pdev->name && !strcasecmp(playname, pdev->name))) {
            break;
        }
    }

    





    


    fragsize = speed / ((rfsh_per_sec < 1.0) ? 1 : ((int)rfsh_per_sec))
               / fragment_divisor[fragment_size];
    if (pdev) {
        if (channels <= pdev->max_channels) {
            fragsize *= channels;
        }
    }

    for (i = 1; 1 << i < fragsize; i++) {
    }
    fragsize = 1 << i;
    fragnr = (int)((speed * bufsize + fragsize - 1) / fragsize);
    if (fragnr < 3) {
        fragnr = 3;
    }

    if (pdev) {
        if (pdev->init) {
            channels_cap = channels;
            if (pdev->init(playparam, &speed, &fragsize, &fragnr, &channels_cap)) {
                err = lib_msprintf("initialization failed for device `%s'.", pdev->name);
                sound_error(err);
                lib_free(err);
                return 1;
            }
            if (channels_cap != channels) {
                if (output_option != SOUND_OUTPUT_MONO) {
                    log_warning(sound_log, "sound device lacks stereo capability, switching to mono output");
                }
                snddata.sound_output_channels = 1;
            } else {
                snddata.sound_output_channels = channels;
            }
        }
        snddata.issuspended = 0;

        for (c = 0; c < snddata.sound_output_channels; c++) {
            snddata.lastsample[c] = 0;
        }

        snddata.playdev = pdev;
        snddata.fragsize = fragsize;
        snddata.fragnr = fragnr;
        snddata.bufsize = fragsize * fragnr;
        snddata.bufptr = 0;
        
        sprintf(frag_str, "%.1f", (1000.0 * fragsize / speed));
        log_message(sound_log,
                    "Opened device `%s', speed %dHz, fragment size %sms, buffer size %dms%s",
                    pdev->name, speed, frag_str,
                    (int)(1000.0 * snddata.bufsize / speed),
                    snddata.sound_output_channels > 1 ? ", stereo" : "");
        sample_rate = speed;

        if (sid_open() != 0 || sid_init() != 0) {
            return 1;
        }

        sid_state_changed = FALSE;
        (void)j;
        log_message(LOG_DEFAULT, "Sound: device ready");
    } else {
        err = lib_msprintf("device '%s' not found or not supported.", playname);
        sound_error(err);
        lib_free(err);
        return 1;
    }

    
    sdev_open = TRUE;
    sound_state_changed = FALSE;

    for (i = 0; (rdev = sound_devices[i]); i++) {
        if (recname && rdev->name && !strcasecmp(recname, rdev->name)) {
            break;
        }
    }

    if (recname && rdev == NULL) {
        ui_error("Recording device %s doesn't exist!", recname);
    }

    if (rdev) {
        if (rdev == pdev) {
            ui_error("Recording device must be different from playback device");
            resources_set_string("SoundRecordDeviceName", "");
            return 0;
        }

        if (rdev->bufferspace != NULL) {
            ui_error("Warning! Recording device %s seems to be a realtime device!");
        }

        if (rdev->init) {
            channels_cap = snddata.sound_output_channels;
            if (rdev->init(recparam, &speed, &fragsize, &fragnr, &channels_cap)) {
                ui_error("initialization failed for device `%s'.", rdev->name);
                resources_set_string("SoundRecordDeviceName", "");
                return 0;
            }

            if (sample_rate != speed
                || snddata.fragsize != fragsize
                || snddata.fragnr != fragnr
                || snddata.sound_output_channels != channels_cap) {
                ui_error("The recording device doesn't support current sound parameters");
                rdev->close();
                resources_set_string("SoundRecordDeviceName", "");
            } else {
                snddata.recdev = rdev;
                log_message(sound_log, "Opened recording device device `%s'", rdev->name);
            }
        }
    }
    return 0;
}


void sound_close(void)
{
    if (snddata.playdev) {
        log_message(sound_log, "Closing device `%s'", snddata.playdev->name);
        if (snddata.playdev->close) {
            snddata.playdev->close();
        }
        snddata.playdev = NULL;
    }

    if (snddata.recdev) {
        log_message(sound_log, "Closing recording device `%s'", snddata.recdev->name);
        if (snddata.recdev->close) {
            snddata.recdev->close();
        }
        snddata.recdev = NULL;
    }

    sid_close();

    snddata.prevused = snddata.prevfill = 0;

    sdev_open = FALSE;
    sound_state_changed = FALSE;

    if (temp_buffer) {
        lib_free(temp_buffer);
        temp_buffer = NULL;
    }

    

    vsync_suspend_speed_eval();
}


static int sound_run_sound(void)
{
    int nr = 0, i;
    int delta_t = 0;
    int16_t *bufferptr;
    static int overflow_warning_count = 0;

    
    if (!playback_enabled || (suspend_time > 0 && disabletime)) {
        return 1;
    }

    if (!snddata.playdev) {
        i = sound_open();
        if (i) {
            return i;
        }
    }

    
    if (cycle_based) {
        delta_t = maincpu_clk - snddata.lastclk;
        bufferptr = snddata.buffer + snddata.bufptr * snddata.sound_output_channels;
        nr = sound_machine_calculate_samples(snddata.psid,
                                             bufferptr,
                                             SOUND_BUFSIZE - snddata.bufptr,
                                             snddata.sound_output_channels,
                                             snddata.sound_chip_channels,
                                             &delta_t);
        if (delta_t) {
            if (overflow_warning_count < 25) {
                log_warning(sound_log, "%s", "Sound buffer overflow (cycle based)");
                overflow_warning_count++;
            } else {
                if (overflow_warning_count == 25) {
                    log_warning(sound_log, "Buffer overflow warning repeated 25 times, will now be ignored");
                    overflow_warning_count++;
                }
            }
        }
    } else {
        
        nr = (int)((SOUNDCLK_CONSTANT(maincpu_clk) - snddata.fclk)
                   / snddata.clkstep);
        if (!nr) {
            return 0;
        }
        if (snddata.bufptr + nr > SOUND_BUFSIZE) {
#ifndef ANDROID_COMPILE
            return sound_error("Sound buffer overflow.");
#else
            return 0;
#endif
        }
        bufferptr = snddata.buffer + snddata.bufptr * snddata.sound_output_channels;
        sound_machine_calculate_samples(snddata.psid,
                                        bufferptr,
                                        nr,
                                        snddata.sound_output_channels,
                                        snddata.sound_chip_channels,
                                        &delta_t);
        snddata.fclk += nr * snddata.clkstep;
    }

    if (amp < 4096) {
        if (amp) {
            for (i = 0; i < (nr * snddata.sound_output_channels); i++) {
                bufferptr[i] = bufferptr[i] * amp / 4096;
            }
        } else {
            memset(bufferptr, 0, nr * snddata.sound_output_channels * sizeof(int16_t));
        }
    }

    snddata.bufptr += nr;
    snddata.lastclk = maincpu_clk;

    return 0;
}


void sound_reset(void)
{
    int c;

    snddata.fclk = SOUNDCLK_CONSTANT(maincpu_clk);
    snddata.wclk = maincpu_clk;
    snddata.lastclk = maincpu_clk;
    snddata.bufptr = 0;         
    for (c = 0; c < snddata.sound_chip_channels; c++) {
        if (snddata.psid[c]) {
            sound_machine_reset(snddata.psid[c], maincpu_clk);
        }
    }
}

static void prevent_clk_overflow_callback(CLOCK sub, void *data)
{
    int c;

    snddata.lastclk -= sub;
    snddata.fclk -= SOUNDCLK_CONSTANT(sub);
    snddata.wclk -= sub;
    for (c = 0; c < snddata.sound_chip_channels; c++) {
        if (snddata.psid[c]) {
            sound_machine_prevent_clk_overflow(snddata.psid[c], sub);
        }
    }
}



double sound_flush()
{
    int c, i, nr, space = 0, used;
    int j;
    static int drained_warning_count = 0;
    char *state;
    static time_t prev;
    time_t now;

    if (!playback_enabled) {
        if (sdev_open) {
            sound_close();
        }
        return 0;
    }

    if (sound_state_changed) {
        if (sdev_open) {
            sound_close();
        }
        sound_state_changed = FALSE;
    }

    if (suspend_time > 0) {
        enablesound();
    }
    if (sound_run_sound()) {
        return 0;
    }

    if (sid_state_changed) {
        if (sid_init() != 0) {
            return 0;
        }
        sid_state_changed = FALSE;
    }

    if (warp_mode_enabled && snddata.recdev == NULL) {
        snddata.bufptr = 0;
        return 0;
    }
    sound_resume();

    if (snddata.playdev->flush) {
        state = sound_machine_dump_state(snddata.psid[0]);
        i = snddata.playdev->flush(state);
        lib_free(state);
        if (i) {
            sound_error("cannot flush.");
            return 0;
        }
    }

    
    nr = snddata.bufptr - snddata.bufptr % snddata.fragsize;
    if (!nr) {
        return 0;
    }

    
    if (snddata.playdev->bufferspace) {
        space = snddata.playdev->bufferspace();
        if (space < 0 || space > snddata.bufsize) {
            log_warning(sound_log, "fragment problems %d %d", space, snddata.bufsize);
            sound_error("fragment problems.");
            return 0;
        }
        

        space -= space % snddata.fragsize;

        used = snddata.bufsize - space;
        if (used < snddata.fragsize) {
            snddata.prevfill = 0;
            (void)drained_warning_count;
            (void)prev;
            (void)now;
            vsync_sync_reset();
        }
        if (cycle_based || speed_adjustment_setting != SOUND_ADJUST_ADJUSTING) {
            if (speed_percent > 0) {
                snddata.clkfactor = SOUNDCLK_CONSTANT(speed_percent) / 100;
            }
        } else {
            if (snddata.prevfill) {
                snddata.prevused = used;
            }
            snddata.clkfactor = SOUNDCLK_MULT(snddata.clkfactor,
                                              SOUNDCLK_CONSTANT(1.0)
                                              + (SOUNDCLK_CONSTANT(0.9)
                                                 * (used - snddata.prevused))
                                              / snddata.bufsize);
        }
        snddata.prevused = used;
        snddata.prevfill = 0;

        if (!cycle_based && speed_adjustment_setting != SOUND_ADJUST_EXACT
            && snddata.recdev == NULL) {
            snddata.clkfactor = SOUNDCLK_MULT(snddata.clkfactor,
                                              SOUNDCLK_CONSTANT(0.9)
                                              + ((used + nr)
                                                 * SOUNDCLK_CONSTANT(0.12))
                                              / snddata.bufsize);
        }
        snddata.clkstep = SOUNDCLK_MULT(snddata.origclkstep,
                                        snddata.clkfactor);
        if (SOUNDCLK_CONSTANT(cycles_per_rfsh) / snddata.clkstep
            >= snddata.bufsize) {
            if (suspend_time > 0) {
                suspendsound("running too slow");
            } else {
                sound_error("running too slow.");
            }
            return 0;
        }
        

        if (nr > space) {
            nr = space; 

        }
    }

    if (nr) {
        
        if (snddata.playdev->write(snddata.buffer, nr * snddata.sound_output_channels)) {
            sound_error("write to sound device failed.");
            return 0;
        }

        if (snddata.recdev) {
            if (snddata.recdev->write(snddata.buffer, nr * snddata.sound_output_channels)) {
                sound_error("write to sound device failed.");
                return 0;
            }
        }
    }

    
    if (speed_percent == 0) {
        nr = snddata.bufptr;
    }

    snddata.bufptr -= nr;

    for (c = 0; c < snddata.sound_output_channels; c++) {
        snddata.lastsample[c] = snddata.buffer[(nr - 1) * snddata.sound_output_channels + c];
        for (i = 0; i < snddata.bufptr; i++) {
            snddata.buffer[i * snddata.sound_output_channels + c] =
                snddata.buffer[(i + nr) * snddata.sound_output_channels + c];
        }
    }

#ifndef __PS3__
    if (snddata.playdev->bufferspace
        && (cycle_based || speed_adjustment_setting == SOUND_ADJUST_EXACT))
    {
        int remspace =
            snddata.playdev->bufferspace() - snddata.bufptr;

        if (remspace < 0) {
            remspace = 0;
        }
        
        return (double)remspace / sample_rate;
    }
#endif

    return 0;
}


void sound_suspend(void)
{
    if (!snddata.playdev) {
        return;
    }

    log_message(LOG_DEFAULT, "SND: suspend enter dev=%s suspended=%d",
                snddata.playdev->name ? snddata.playdev->name : "?",
                snddata.issuspended);

    if (snddata.playdev->write && !snddata.issuspended
        && snddata.playdev->need_attenuation) {
        
        if (!snddata.playdev->bufferspace
            || snddata.playdev->bufferspace() >= snddata.fragsize) {
            fill_buffer(snddata.fragsize, -1);
        } else {
            log_warning(sound_log, "Buffer full during suspend");
        }
        
        if (!snddata.playdev) {
            return;
        }
        log_message(LOG_DEFAULT, "SND: suspend fill done");
    }

    if (snddata.playdev->suspend && !snddata.issuspended) {
        if (snddata.playdev->suspend()) {
            log_message(LOG_DEFAULT, "SND: suspend dev refused");
            return;
        }
    }
    snddata.issuspended = 1;
    log_message(LOG_DEFAULT, "SND: suspend done");
}


void sound_resume(void)
{
    if (!snddata.playdev) {
        return;
    }

    if (snddata.issuspended) {
        if (snddata.playdev->resume) {
            snddata.issuspended = snddata.playdev->resume();
        } else {
            snddata.issuspended = 0;
        }

        if (snddata.playdev->write && !snddata.issuspended
            && snddata.playdev->need_attenuation) {
            fill_buffer(snddata.fragsize, 1);
        }
    }
}


void sound_set_machine_parameter(long clock_rate, long ticks_per_frame)
{
    sid_state_changed = TRUE;

    cycles_per_sec = clock_rate;
    cycles_per_rfsh = ticks_per_frame;
    rfsh_per_sec = (1.0 / ((double)cycles_per_rfsh / (double)cycles_per_sec));
}


void sound_init(unsigned int clock_rate, unsigned int ticks_per_frame)
{
    char *devlist, *tmplist;
    int i;

    sound_log = log_open("Sound");

    sound_state_changed = FALSE;
    sid_state_changed = FALSE;

    cycles_per_sec = clock_rate;
    cycles_per_rfsh = ticks_per_frame;
    rfsh_per_sec = (1.0 / ((double)cycles_per_rfsh / (double)cycles_per_sec));

    clk_guard_add_callback(maincpu_clk_guard, prevent_clk_overflow_callback,
                           NULL);

    devlist = lib_stralloc("");

    for (i = 0; sound_register_devices[i].name; i++) {
        sound_register_devices[i].init();
        tmplist = lib_msprintf("%s %s", devlist, sound_register_devices[i].name);
        lib_free(devlist);
        devlist = tmplist;
    }

    log_message(sound_log, "Available sound devices:%s", devlist);

    lib_free(devlist);
}

long sound_sample_position(void)
{
    return (snddata.clkstep == 0)
           ? 0 : (long)((SOUNDCLK_CONSTANT(maincpu_clk) - snddata.fclk)
                        / snddata.clkstep);
}

int sound_dump(int chipno)
{
    if (chipno >= snddata.sound_chip_channels) {
        return -1;
    }
    mon_out("%s\n", sound_machine_dump_state(snddata.psid[chipno]));
    return 0;
}

int sound_read(uint16_t addr, int chipno)
{
    if (sound_run_sound()) {
        return -1;
    }

    if (chipno >= snddata.sound_chip_channels) {
        return -1;
    }

    return sound_machine_read(snddata.psid[chipno], addr);
}

void sound_store(uint16_t addr, uint8_t val, int chipno)
{
    int i;

    if (sound_run_sound()) {
        return;
    }

    if (chipno >= snddata.sound_chip_channels) {
        return;
    }

    sound_machine_store(snddata.psid[chipno], addr, val);

    if (!snddata.playdev->dump) {
        return;
    }

    i = snddata.playdev->dump(addr, val, maincpu_clk - snddata.wclk);

    snddata.wclk = maincpu_clk;

    if (i) {
        sound_error("store to sounddevice failed.");
    }
}


void sound_set_relative_speed(int value)
{
    if (value != speed_percent) {
        sid_state_changed = TRUE;
    }

    speed_percent = value;
}

void sound_set_warp_mode(int value)
{
    warp_mode_enabled = value;

    if (value) {
        sound_suspend();
    } else {
        sound_resume();
    }
}

void sound_snapshot_prepare(void)
{
    
    sound_run_sound();
}

void sound_snapshot_finish(void)
{
    snddata.lastclk = maincpu_clk;
}

void sound_dac_init(sound_dac_t *dac, int speed)
{
    
    dac->alpha = (float)(0.0318309886 / (0.0318309886 + 1.0 / (float)speed));
    dac->value = 0;
    dac->output = 0.0;
}




int sound_dac_calculate_samples(sound_dac_t *dac, int16_t *pbuf, int value, int nr, int soc, int cs)
{
    int i, sample;
    int off = 0;
    




    if (nr) {
        dac->output = dac->alpha * (dac->output + (float)(value - dac->value));
        dac->value = value;
        sample = (int)dac->output;
        if (!sample) {
            return nr;
        }
        if (cs & 1) {
            pbuf[off] = sound_audio_mix(pbuf[off], sample);
        }
        if (cs & 2) {
            pbuf[off + 1] = sound_audio_mix(pbuf[off + 1], sample);
        }
        off += soc;
    }

    for (i = 1; i < nr; i++) {
        dac->output *= dac->alpha;
        sample = (int)dac->output;
        if (cs & 1) {
            pbuf[off] = sound_audio_mix(pbuf[off], sample);
        }
        if (cs & 2) {
            pbuf[off + 1] = sound_audio_mix(pbuf[off + 1], sample);
        }
        off += soc;
    }
    return nr;
}


void sound_stop_recording(void)
{
    resources_set_string("SoundRecordDeviceName", "");
}

int sound_is_recording(void)
{
    return (strlen(recorddevice_name) > 0);
}
