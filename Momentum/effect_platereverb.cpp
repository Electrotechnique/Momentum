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

// Trimmed for Momentum from hexefx_audiolib_i16 effect_platereverb_i16. See effect_platereverb.h.

#include <Arduino.h>
#include "effect_platereverb.h"

#define INP_ALLP_COEFF (0.65f)
#define LOOP_ALLOP_COEFF (0.65f)
#define TREBLE_LOSS_FREQ (0.3f)
#define BASS_LOSS_FREQ (0.06f)

bool AudioEffectPlateReverb::begin()
{
    inputGain = 0.5f;
    wet_gain = 1.0f;
    dry_gain = 0.0f;
    in_allp_k = INP_ALLP_COEFF;
    loop_allp_k = LOOP_ALLOP_COEFF;
    rv_time_scaler = 1.0f;
    rv_time_k = 0.2f;

    if (!in_allp_1L.init(&in_allp_k)) return false;
    if (!in_allp_2L.init(&in_allp_k)) return false;
    if (!in_allp_3L.init(&in_allp_k)) return false;
    if (!in_allp_4L.init(&in_allp_k)) return false;
    if (!in_allp_1R.init(&in_allp_k)) return false;
    if (!in_allp_2R.init(&in_allp_k)) return false;
    if (!in_allp_3R.init(&in_allp_k)) return false;
    if (!in_allp_4R.init(&in_allp_k)) return false;
    in_allp_out_L = 0.0f;
    in_allp_out_R = 0.0f;
    if (!lp_allp_2.init(&loop_allp_k)) return false;
    if (!lp_allp_3.init(&loop_allp_k)) return false;
    if (!lp_allp_4.init(&loop_allp_k)) return false;
    lp_allp_out = 0.0f;
    if (!lp_dly1.init(LP_DLY1_BUF_LEN)) return false;
    if (!lp_dly2.init(LP_DLY2_BUF_LEN)) return false;
    if (!lp_dly3.init(LP_DLY3_BUF_LEN)) return false;
    if (!lp_dly4.init(LP_DLY4_BUF_LEN)) return false;
    lp_hidamp_k = 1.0f;
    lp_lodamp_k = 0.0f;
    flt1.init(BASS_LOSS_FREQ, &lp_lodamp_k, TREBLE_LOSS_FREQ, &lp_hidamp_k);
    flt2.init(BASS_LOSS_FREQ, &lp_lodamp_k, TREBLE_LOSS_FREQ, &lp_hidamp_k);
    flt3.init(BASS_LOSS_FREQ, &lp_lodamp_k, TREBLE_LOSS_FREQ, &lp_hidamp_k);
    flt4.init(BASS_LOSS_FREQ, &lp_lodamp_k, TREBLE_LOSS_FREQ, &lp_hidamp_k);
    master_lp_k = 1.0f;
    master_hp_k = 0.0f;
    flt_masterL.init(0.08f, &master_hp_k, 0.1f, &master_lp_k);
    flt_masterR.init(0.08f, &master_hp_k, 0.1f, &master_lp_k);
    flags.bypass = 1;
    flags.cleanup_done = 0;
    return true;
}

void AudioEffectPlateReverb::update()
{
    audio_block_t *blockL, *blockR;
    float sampleL, sampleR;
    float acc;
    float rv_time;
    uint32_t offset;
    float lfo_fr;

    blockL = receiveWritable(0);
    blockR = receiveWritable(1);

    if (!initialized || flags.bypass)
    {
        // Pass through. Clear the tail once so it doesn't resume when re-enabled
        if (initialized && !flags.cleanup_done)
        {
            in_allp_1L.reset();
            in_allp_2L.reset();
            in_allp_3L.reset();
            in_allp_4L.reset();
            in_allp_1R.reset();
            in_allp_2R.reset();
            in_allp_3R.reset();
            in_allp_4R.reset();
            lp_allp_2.reset();
            lp_allp_3.reset();
            lp_allp_4.reset();
            lp_dly1.reset();
            lp_dly2.reset();
            lp_dly3.reset();
            lp_dly4.reset();
            lp_allp_out = 0.0f;
            flags.cleanup_done = 1;
        }
        if (blockL)
        {
            transmit(blockL, 0);
            release(blockL);
        }
        if (blockR)
        {
            transmit(blockR, 1);
            release(blockR);
        }
        return;
    }

    // Missing input (silence): use a zeroed block so the tail keeps decaying
    if (!blockL)
    {
        blockL = allocate();
        if (!blockL)
        {
            if (blockR)
                release(blockR);
            return;
        }
        memset(blockL->data, 0, sizeof(blockL->data));
    }
    if (!blockR)
    {
        blockR = allocate();
        if (!blockR)
        {
            release(blockL);
            return;
        }
        memset(blockR->data, 0, sizeof(blockR->data));
    }

    flags.cleanup_done = 0;
    rv_time = rv_time_k * rv_time_scaler;
    const float wet = wet_gain;
    const float dry = dry_gain;

    for (int i = 0; i < AUDIO_BLOCK_SAMPLES; i++)
    {
        // do the LFOs
        lfo1.update();
        lfo2.update();
        sampleL = (float)blockL->data[i] * (1.0f / 32768.0f);
        acc = sampleL * inputGain;
        // chained input allpasses, channel L
        acc = in_allp_1L.process(acc);
        acc = in_allp_2L.process(acc);
        acc = in_allp_3L.process(acc);
        in_allp_out_L = in_allp_4L.process(acc);
        // chained input allpasses, channel R
        sampleR = (float)blockR->data[i] * (1.0f / 32768.0f);
        acc = sampleR * inputGain;
        acc = in_allp_1R.process(acc);
        acc = in_allp_2R.process(acc);
        acc = in_allp_3R.process(acc);
        in_allp_out_R = in_allp_4R.process(acc);
        // tank
        acc = lp_dly1.process(lp_allp_out + in_allp_out_R);
        acc = flt1.process(acc) * rv_time;
        acc = lp_allp_2.process(acc + in_allp_out_L);
        acc = lp_dly2.process(acc);
        acc = flt2.process(acc) * rv_time;
        acc = lp_allp_3.process(acc + in_allp_out_R);
        acc = lp_dly3.process(acc);
        acc = flt3.process(acc) * rv_time;
        acc = lp_allp_4.process(acc + in_allp_out_L);
        acc = lp_dly4.process(acc);
        lp_allp_out = flt4.process(acc) * rv_time;

        // Channel L
        acc = lp_dly1.getTap(lp_dly1_offset_L) * 0.8f;
        acc += lp_dly2.getTap(lp_dly2_offset_L) * 0.7f;
        acc += lp_dly3.getTap(lp_dly3_offset_L) * 0.6f;
        acc += lp_dly4.getTap(lp_dly4_offset_L) * 0.5f;
        acc = flt_masterL.process(acc);
        sampleL = acc * wet + sampleL * dry;
        if (sampleL > 1.0f)
            sampleL = 1.0f;
        else if (sampleL < -1.0f)
            sampleL = -1.0f;
        blockL->data[i] = (int16_t)(sampleL * 32767.0f);

        // Channel R
        acc = lp_dly1.getTap(lp_dly1_offset_R) * 0.8f;
        acc += lp_dly2.getTap(lp_dly2_offset_R) * 0.7f;
        acc += lp_dly3.getTap(lp_dly3_offset_R) * 0.6f;
        acc += lp_dly4.getTap(lp_dly4_offset_R) * 0.5f;
        acc = flt_masterR.process(acc);
        sampleR = acc * wet + sampleR * dry;
        if (sampleR > 1.0f)
            sampleR = 1.0f;
        else if (sampleR < -1.0f)
            sampleR = -1.0f;
        blockR->data[i] = (int16_t)(sampleR * 32767.0f);

        // modulate the delay lines
        lfo1.get(BASIC_LFO_PHASE_0, &offset, &lfo_fr); // lfo1 sin output
        acc = lp_dly1.getTap(offset, lfo_fr);
        lp_dly1.write_toOffset(acc, LFO_AMPL * 2);
        lp_dly1.updateIndex();
        lfo1.get(BASIC_LFO_PHASE_90, &offset, &lfo_fr); // lfo1 cos output
        acc = lp_dly2.getTap(offset, lfo_fr);
        lp_dly2.write_toOffset(acc, LFO_AMPL * 2);
        lp_dly2.updateIndex();
        lfo2.get(BASIC_LFO_PHASE_0, &offset, &lfo_fr); // lfo2 sin output
        acc = lp_dly3.getTap(offset, lfo_fr);
        lp_dly3.write_toOffset(acc, LFO_AMPL * 2);
        lp_dly3.updateIndex();
        lfo2.get(BASIC_LFO_PHASE_90, &offset, &lfo_fr); // lfo2 cos output
        acc = lp_dly4.getTap(offset, lfo_fr);
        lp_dly4.write_toOffset(acc, LFO_AMPL * 2);
        lp_dly4.updateIndex();
    }
    if (LFO_AMPL != LFO_AMPLset)
    {
        lfo1.setDepth(LFO_AMPLset);
        lfo2.setDepth(LFO_AMPLset);
        LFO_AMPL = LFO_AMPLset;
    }
    transmit(blockL, 0);
    transmit(blockR, 1);
    release(blockL);
    release(blockR);
}
