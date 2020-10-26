#include "../chocdoom/deh_str.h"
#include "../chocdoom/i_sound.h"
#include "../chocdoom/i_system.h"
#include "../chocdoom/m_misc.h"
#include "../chocdoom/w_wad.h"
#include "../chocdoom/z_zone.h"

#include "audio/audio.hpp"

// chunks of this file are copied from i_sdlsound
// only works as the wad is "mmapped"

struct active_sound
{
    short rate;
    byte *data;
    uint32_t length;
    uint32_t offset;
};

static boolean use_sfx_prefix;

#define SOUND_CHANNELS CHANNEL_COUNT - 1

static active_sound channel_sounds[SOUND_CHANNELS];

static boolean SetupSound(sfxinfo_t *sfxinfo, int channel)
{
    int lumpnum;
    unsigned int lumplen;
    int samplerate;
    unsigned int length;
    byte *data;

    // need to load the sound

    lumpnum = sfxinfo->lumpnum;
    data = (byte *)W_CacheLumpNum(lumpnum, PU_STATIC);
    lumplen = W_LumpLength(lumpnum);

    // Check the header, and ensure this is a valid sound

    if (lumplen < 8
     || data[0] != 0x03 || data[1] != 0x00)
    {
        // Invalid sound

        return false;
    }

    // 16 bit sample rate field, 32 bit length field

    samplerate = (data[3] << 8) | data[2];
    length = (data[7] << 24) | (data[6] << 16) | (data[5] << 8) | data[4];

    // If the header specifies that the length of the sound is greater than
    // the length of the lump itself, this is an invalid sound lump

    // We also discard sound lumps that are less than 49 samples long,
    // as this is how DMX behaves - although the actual cut-off length
    // seems to vary slightly depending on the sample rate.  This needs
    // further investigation to better understand the correct
    // behavior.

    if (length > lumplen - 8 || length <= 48)
    {
        return false;
    }

    // The DMX sound library seems to skip the first 16 and last 16
    // bytes of the lump - reason unknown.

    data += 16;
    length -= 32;

    if(samplerate != 11025)
    {
        printf("unhandled rate %i\n", samplerate);
        return false;
    }

    channel_sounds[channel].rate = samplerate;
    channel_sounds[channel].data = data;
    channel_sounds[channel].length = length;
    channel_sounds[channel].offset = 0;

    // don't need the original lump any more
  
    W_ReleaseLumpNum(lumpnum);

    return true;
}

static void GetSfxLumpName(sfxinfo_t *sfx, char *buf, size_t buf_len)
{
    // Linked sfx lumps? Get the lump number for the sound linked to.

    if (sfx->link != NULL)
    {
        sfx = sfx->link;
    }

    // Doom adds a DS* prefix to sound lumps; Heretic and Hexen don't
    // do this.

    if (use_sfx_prefix)
    {
        M_snprintf(buf, buf_len, "ds%s", DEH_String(sfx->name));
    }
    else
    {
        M_StringCopy(buf, DEH_String(sfx->name), buf_len);
    }
}

static void RefillBuffer(void *data)
{
    int channel = (intptr_t)data;

    auto &sound = channel_sounds[channel];

    if(sound.offset == sound.length)
    {
        blit::channels[channel].off();
        return;
    }

    int i = 0;
    for(i = 0; i < 64 && sound.offset < sound.length; i += 2, sound.offset++)
        blit::channels[channel].wave_buffer[i] = sound.data[sound.offset] - 127;

    // pad end
    for(;i < 64; i++)
        blit::channels[channel].wave_buffer[i] = 0;

}

static boolean I_Blit_InitSound(boolean _use_sfx_prefix)
{
    use_sfx_prefix = _use_sfx_prefix;

    for(int i = 0; i < SOUND_CHANNELS; i++)
    {
        blit::channels[i].waveforms = blit::Waveform::WAVE;
        blit::channels[i].wave_callback_arg = (void *)(uintptr_t)i;
        blit::channels[i].callback_waveBufferRefresh = &RefillBuffer;
    }
    return true;
}

static void I_Blit_ShutdownSound(void)
{
}

static int I_Blit_GetSfxLumpNum(sfxinfo_t *sfx)
{
    char namebuf[9];

    GetSfxLumpName(sfx, namebuf, sizeof(namebuf));

    return W_GetNumForName(namebuf);
}

static void I_Blit_UpdateSound()
{

}

static void I_Blit_UpdateSoundParams(int handle, int vol, int sep)
{
    if(handle >= SOUND_CHANNELS)
        return;

    blit::channels[handle].volume = vol * 222;
}

static int I_Blit_StartSound(sfxinfo_t *sfxinfo, int channel, int vol, int sep)
{
    if(channel >= SOUND_CHANNELS)
        return -1;

    blit::channels[channel].off();

    if(!SetupSound(sfxinfo, channel))
        return -1;

    blit::channels[channel].adsr = 0xFFFF00;
    blit::channels[channel].trigger_sustain();

    I_Blit_UpdateSoundParams(channel, vol, sep);

    return channel;
}

static void I_Blit_StopSound(int handle)
{
    if (handle >= SOUND_CHANNELS)
        return;

    blit::channels[handle].off();
}

static boolean I_Blit_SoundIsPlaying(int handle)
{
    if(handle > SOUND_CHANNELS)
        return false;

    return blit::channels[handle].adsr_phase != blit::ADSRPhase::OFF;
}

static snddevice_t sound_blit_devices[] =
{
    SNDDEVICE_SB
};

sound_module_t sound_blit_module =
{
    sound_blit_devices,
    1,
    I_Blit_InitSound,
    I_Blit_ShutdownSound,
    I_Blit_GetSfxLumpNum,
    I_Blit_UpdateSound,
    I_Blit_UpdateSoundParams,
    I_Blit_StartSound,
    I_Blit_StopSound,
    I_Blit_SoundIsPlaying
};