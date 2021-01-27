//
// Copyright(C) 2005-2014 Simon Howard
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// DESCRIPTION:
//     OPL SDL interface.
//

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <assert.h>

#include "dbopl.h"

#include "audio/audio.hpp"

#include "opl.h"
#include "opl_internal.h"

#include "opl_queue.h"

#define MAX_SOUND_SLICE_TIME 100 /* ms */

typedef struct
{
    unsigned int rate;        // Number of times the timer is advanced per sec.
    unsigned int enabled;     // Non-zero if timer is enabled.
    unsigned int value;       // Last value that was set.
    uint64_t expire_time;     // Calculated time that timer will expire.
} opl_timer_t;


// Queue of callbacks waiting to be invoked.

static opl_callback_queue_t *callback_queue;


// Current time, in us since startup:

static uint64_t current_time;

// If non-zero, playback is currently paused.

static int opl_blit_paused;

// Time offset (in us) due to the fact that callbacks
// were previously paused.

static uint64_t pause_offset;

// OPL software emulator structure.

static Chip opl_chip;

// Register number that was written.

static int register_num = 0;

// Timers; DBOPL does not do timer stuff itself.

static opl_timer_t timer1 = { 12500, 0, 0, 0 };
static opl_timer_t timer2 = { 3125, 0, 0, 0 };


// Advance time by the specified number of samples, invoking any
// callback functions as appropriate.

static void AdvanceTime(unsigned int nsamples)
{
    opl_callback_t callback;
    void *callback_data;
    uint64_t us;

    // Advance time.

    us = ((uint64_t) nsamples * OPL_SECOND) / blit::sample_rate;
    current_time += us;

    if (opl_blit_paused)
    {
        pause_offset += us;
    }

    // Are there callbacks to invoke now?  Keep invoking them
    // until there are no more left.

    while (!OPL_Queue_IsEmpty(callback_queue)
        && current_time >= OPL_Queue_Peek(callback_queue) + pause_offset)
    {
        // Pop the callback from the queue to invoke it.

        if (!OPL_Queue_Pop(callback_queue, &callback, &callback_data))
        {
            break;
        }

        callback(callback_data);

    }
}

// Call the OPL emulator code to fill the specified buffer.

static void FillBuffer(int16_t *buffer, unsigned int nsamples)
{
    unsigned int i;

    int32_t mix_buffer[64];
    Chip__GenerateBlock2(&opl_chip, nsamples, mix_buffer);

    for (i=0; i<nsamples; ++i)
        buffer[i] = (int16_t) mix_buffer[i];
}

// Callback function to fill a new sound buffer:

static void OPL_Blit_Callback(blit::AudioChannel &channel)
{
    int16_t *buffer;
    unsigned int buffer_len;
    unsigned int filled = 0;

    // Buffer length in samples (quadrupled, because of 16-bit and stereo)

    buffer = channel.wave_buffer;
    buffer_len = 64;

    // Repeatedly call the OPL emulator update function until the buffer is
    // full.

    while (filled < buffer_len)
    {
        uint64_t next_callback_time;
        uint64_t nsamples;

        // Work out the time until the next callback waiting in
        // the callback queue must be invoked.  We can then fill the
        // buffer with this many samples.

        if (opl_blit_paused || OPL_Queue_IsEmpty(callback_queue))
        {
            nsamples = buffer_len - filled;
        }
        else
        {
            next_callback_time = OPL_Queue_Peek(callback_queue) + pause_offset;

            nsamples = (next_callback_time - current_time) * blit::sample_rate;
            nsamples = (nsamples + OPL_SECOND - 1) / OPL_SECOND;

            if (nsamples > buffer_len - filled)
            {
                nsamples = buffer_len - filled;
            }
        }

        // Add emulator output to buffer.

        FillBuffer(buffer + filled, nsamples);
        filled += nsamples;

        // Invoke callbacks for this point in time.

        AdvanceTime(nsamples);
    }
}

static void opl_blit_Shutdown(void)
{
    OPL_Queue_Destroy(callback_queue);

/*
    if (opl_chip != NULL)
    {
        OPLDestroy(opl_chip);
        opl_chip = NULL;
    }
    */

}

static unsigned int GetSliceSize(void)
{
    int limit;
    int n;

    limit = (opl_sample_rate * MAX_SOUND_SLICE_TIME) / 1000;

    // Try all powers of two, not exceeding the limit.

    for (n=0;; ++n)
    {
        // 2^n <= limit < 2^n+1 ?

        if ((1 << (n + 1)) > limit)
        {
            return (1 << n);
        }
    }

    // Should never happen?

    return 1024;
}

static int opl_blit_Init(unsigned int port_base)
{
    opl_blit_paused = 0;
    pause_offset = 0;

    // Queue structure of callbacks to invoke.

    callback_queue = OPL_Queue_Create();
    current_time = 0;

    // Create the emulator structure:

    DBOPL_InitTables();
    Chip__Chip(&opl_chip);
    Chip__Setup(&opl_chip, blit::sample_rate);

    blit::channels[7].waveforms = blit::Waveform::WAVE;
    blit::channels[7].wave_buffer_callback = &OPL_Blit_Callback;

    blit::channels[7].adsr = 0xFFFF00;
    blit::channels[7].trigger_sustain();

    return 1;
}

static unsigned int opl_blit_PortRead(opl_port_t port)
{
    unsigned int result = 0;

    if (timer1.enabled && current_time > timer1.expire_time)
    {
        result |= 0x80;   // Either have expired
        result |= 0x40;   // Timer 1 has expired
    }

    if (timer2.enabled && current_time > timer2.expire_time)
    {
        result |= 0x80;   // Either have expired
        result |= 0x20;   // Timer 2 has expired
    }

    return result;
}

static void OPLTimer_CalculateEndTime(opl_timer_t *timer)
{
    int tics;

    // If the timer is enabled, calculate the time when the timer
    // will expire.

    if (timer->enabled)
    {
        tics = 0x100 - timer->value;
        timer->expire_time = current_time
                           + ((uint64_t) tics * OPL_SECOND) / timer->rate;
    }
}

static void WriteRegister(unsigned int reg_num, unsigned int value)
{
    switch (reg_num)
    {
        case OPL_REG_TIMER1:
            timer1.value = value;
            OPLTimer_CalculateEndTime(&timer1);
            break;

        case OPL_REG_TIMER2:
            timer2.value = value;
            OPLTimer_CalculateEndTime(&timer2);
            break;

        case OPL_REG_TIMER_CTRL:
            if (value & 0x80)
            {
                timer1.enabled = 0;
                timer2.enabled = 0;
            }
            else
            {
                if ((value & 0x40) == 0)
                {
                    timer1.enabled = (value & 0x01) != 0;
                    OPLTimer_CalculateEndTime(&timer1);
                }

                if ((value & 0x20) == 0)
                {
                    timer1.enabled = (value & 0x02) != 0;
                    OPLTimer_CalculateEndTime(&timer2);
                }
            }

            break;

        default:
            Chip__WriteReg(&opl_chip, reg_num, value);
            break;
    }
}

static void opl_blit_PortWrite(opl_port_t port, unsigned int value)
{
    if (port == OPL_REGISTER_PORT)
    {
        register_num = value;
    }
    else if (port == OPL_DATA_PORT)
    {
        WriteRegister(register_num, value);
    }
}

static void opl_blit_SetCallback(uint64_t us, opl_callback_t callback,
                                void *data)
{
    OPL_Queue_Push(callback_queue, callback, data,
                   current_time - pause_offset + us);
}

static void opl_blit_ClearCallbacks(void)
{
    OPL_Queue_Clear(callback_queue);
}

static void opl_blit_Lock(void)
{
}

static void opl_blit_Unlock(void)
{
}

static void opl_blit_SetPaused(int paused)
{
    opl_blit_paused = paused;
}

static void opl_blit_AdjustCallbacks(float factor)
{
    OPL_Queue_AdjustCallbacks(callback_queue, current_time, factor);
}

opl_driver_t opl_blit_driver =
{
    "blit",
    opl_blit_Init,
    opl_blit_Shutdown,
    opl_blit_PortRead,
    opl_blit_PortWrite,
    opl_blit_SetCallback,
    opl_blit_ClearCallbacks,
    opl_blit_Lock,
    opl_blit_Unlock,
    opl_blit_SetPaused,
    opl_blit_AdjustCallbacks,
};

