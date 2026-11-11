/*  Stereo plate reverb for Teensy 4
 *
 *  Author: Piotr Zapart
 *          www.hexefx.com
 *
 * Copyright (c) 2021 by Piotr Zapart
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

/***
 * Algorithm based on plate reverbs developed for SpinSemi FV-1 DSP chip
 * Allpass + modulated delay line based lush plate reverb
 *
 * Trimmed for Momentum from hexefx_audiolib_i16 (github.com/hexeguitar/hexefx_audiolib_i16,
 * effect_platereverb_i16): the pitch shift, shimmer and freeze features are removed, saving
 * 73KB of RAM and their CPU time. With those disabled the sound is unchanged.
 *
 * Two inputs (L, R), two outputs (L, R). In series: the dry signal passes through, mixed
 * internally with the reverb. Delay buffers are malloc'd (RAM2), about 120KB.
 */

#ifndef _EFFECT_PLATEREVERB_H_
#define _EFFECT_PLATEREVERB_H_

#include <Arduino.h>
#include "AudioStream.h"
#include "arm_math.h"
#include "basic_allpass.h"
#include "basic_delay.h"
#include "basic_lfo.h"
#include "basic_shelvFilter.h"

class AudioEffectPlateReverb : public AudioStream
{
public:
    AudioEffectPlateReverb() : AudioStream(2, inputQueueArray) { initialized = begin(); }
    virtual void update();
    bool begin(void);

    // Reverb time, 0.0 - 1.0
    void size(float n)
    {
        n = constrain(n, 0.0f, 1.0f);
        n = 2 * n - n * n;
        n = map(n, 0.0f, 1.0f, 0.2f, rv_time_k_max);
        __disable_irq();
        rv_time_k = n;
        __enable_irq();
    }

    // Treble loss in the reverb tail, 0.0 - 1.0
    void hidamp(float n)
    {
        n = 1.0f - constrain(n, 0.0f, 1.0f);
        __disable_irq();
        lp_hidamp_k = n;
        __enable_irq();
    }

    // Bass loss in the reverb tail, 0.0 - 1.0
    void lodamp(float n)
    {
        n = -constrain(n, 0.0f, 1.0f);
        float tscal = 1.0f + n * 0.12f; // n is negative here
        __disable_irq();
        lp_lodamp_k = n;
        rv_time_scaler = tscal; // limit the max reverb time, otherwise it will clip
        __enable_irq();
    }

    // Reverb output lowpass, 0.0 - 1.0
    void lowpass(float n)
    {
        n = 1.0f - constrain(n, 0.0f, 1.0f);
        __disable_irq();
        master_lp_k = n;
        __enable_irq();
    }

    // Reverb output highpass, 0.0 - 1.0
    void hipass(float n)
    {
        n = -constrain(n, 0.0f, 1.0f);
        __disable_irq();
        master_hp_k = n;
        __enable_irq();
    }

    // Tail diffusion, 0.0 - 1.0. Lower values give more distinct echoes
    void diffusion(float n)
    {
        n = constrain(n, 0.0f, 1.0f);
        n = map(n, 0.0f, 1.0f, 0.005f, 0.65f);
        __disable_irq();
        in_allp_k = n;
        loop_allp_k = n;
        __enable_irq();
    }

    // Dry/wet mix, 0.0 (dry) - 1.0 (wet), roughly constant energy crossfade
    void mix(float m)
    {
        m = constrain(m, 0.0f, 1.0f);
        // Cheap energy constant crossfade, signalsmith-audio.co.uk/writing/2021/cheap-energy-crossfade/
        float x2 = 1.0f - m;
        float A = m * x2;
        float B = A * (1.0f + 1.4186f * A);
        float C = B + m;
        float D = B + x2;
        __disable_irq();
        wet_gain = C * C;
        dry_gain = D * D;
        __enable_irq();
    }

    // Bypass passes the input straight through and saves the CPU time. The tail is cleared.
    void bypass_set(bool state)
    {
        __disable_irq();
        flags.bypass = state;
        __enable_irq();
    }
    bool bypass_get(void) { return flags.bypass; }

    // Delay line modulation depth, 0.0 - 1.0. Higher values add chorusing
    void chorus(float c)
    {
        c = map(c, 0.0f, 1.0f, 1.0f, 100.0f);
        __disable_irq();
        LFO_AMPLset = (uint32_t)c;
        __enable_irq();
    }

    bool isInitialized() { return initialized; }

private:
    struct flags_t
    {
        unsigned bypass : 1;
        unsigned cleanup_done : 1;
    } flags;
    audio_block_t *inputQueueArray[2];
    static const uint16_t IN_ALLP1_BUFL_LEN = 224u;
    static const uint16_t IN_ALLP2_BUFL_LEN = 420u;
    static const uint16_t IN_ALLP3_BUFL_LEN = 856u;
    static const uint16_t IN_ALLP4_BUFL_LEN = 1089u;
    static const uint16_t IN_ALLP1_BUFR_LEN = 156u;
    static const uint16_t IN_ALLP2_BUFR_LEN = 520u;
    static const uint16_t IN_ALLP3_BUFR_LEN = 956u;
    static const uint16_t IN_ALLP4_BUFR_LEN = 1289u;
    static const uint16_t LP_ALLP2_BUF_LEN = 2905u;
    static const uint16_t LP_ALLP3_BUF_LEN = 3175u;
    static const uint16_t LP_ALLP4_BUF_LEN = 2398u;
    static const uint16_t LP_DLY1_BUF_LEN = 3423u;
    static const uint16_t LP_DLY2_BUF_LEN = 4589u;
    static const uint16_t LP_DLY3_BUF_LEN = 4365u;
    static const uint16_t LP_DLY4_BUF_LEN = 3698u;
    const uint16_t lp_dly1_offset_L = 201;
    const uint16_t lp_dly2_offset_L = 145;
    const uint16_t lp_dly3_offset_L = 1897;
    const uint16_t lp_dly4_offset_L = 280;
    const uint16_t lp_dly1_offset_R = 1897;
    const uint16_t lp_dly2_offset_R = 1245;
    const uint16_t lp_dly3_offset_R = 487;
    const uint16_t lp_dly4_offset_R = 780;
    AudioFilterAllpass<IN_ALLP1_BUFL_LEN> in_allp_1L;
    AudioFilterAllpass<IN_ALLP2_BUFL_LEN> in_allp_2L;
    AudioFilterAllpass<IN_ALLP3_BUFL_LEN> in_allp_3L;
    AudioFilterAllpass<IN_ALLP4_BUFL_LEN> in_allp_4L;
    AudioFilterAllpass<IN_ALLP1_BUFR_LEN> in_allp_1R;
    AudioFilterAllpass<IN_ALLP2_BUFR_LEN> in_allp_2R;
    AudioFilterAllpass<IN_ALLP3_BUFR_LEN> in_allp_3R;
    AudioFilterAllpass<IN_ALLP4_BUFR_LEN> in_allp_4R;
    // The original's first loop allpass was allocated but never processed, so it's omitted
    AudioFilterAllpass<LP_ALLP2_BUF_LEN> lp_allp_2;
    AudioFilterAllpass<LP_ALLP3_BUF_LEN> lp_allp_3;
    AudioFilterAllpass<LP_ALLP4_BUF_LEN> lp_allp_4;
    uint16_t LFO_AMPL = 20u;
    uint16_t LFO_AMPLset = 20u;
    AudioBasicLfo lfo1 = AudioBasicLfo(1.35f, LFO_AMPL);
    AudioBasicLfo lfo2 = AudioBasicLfo(1.57f, LFO_AMPL);
    float inputGain;
    float wet_gain;
    float dry_gain;
    float in_allp_k;   // input allpass coeff (default 0.6)
    float in_allp_out_L; // L allpass chain output
    float in_allp_out_R; // R allpass chain output
    float loop_allp_k;   // loop allpass coeff (default 0.6)
    float lp_allp_out;
    AudioBasicDelay lp_dly1;
    AudioBasicDelay lp_dly2;
    AudioBasicDelay lp_dly3;
    AudioBasicDelay lp_dly4;
    float lp_hidamp_k; // loop high band damping coeff
    float lp_lodamp_k; // loop low band damping coeff
    AudioFilterShelvingLPHP flt1;
    AudioFilterShelvingLPHP flt2;
    AudioFilterShelvingLPHP flt3;
    AudioFilterShelvingLPHP flt4;
    float master_lp_k, master_hp_k;
    AudioFilterShelvingLPHP flt_masterL;
    AudioFilterShelvingLPHP flt_masterR;
    const float rv_time_k_max = 0.97f;
    float rv_time_k;      // reverb time coeff
    float rv_time_scaler; // with high lodamp settings lower the max reverb time to avoid clipping
    bool initialized = false;
};

#endif // _EFFECT_PLATEREVERB_H_
