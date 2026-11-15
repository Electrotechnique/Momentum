/* Audio Library for Teensy 3.X
   Portions Copyright (c) 2019, Alexander Davis info@matrixwide.com
   Portions Copyright (c) 2021, Vince R. Pearson

   Permission is hereby granted, free of charge, to any person obtaining a copy
   of this software and associated documentation files (the "Software"), to deal
   in the Software without restriction, including without limitation the rights
   to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
   copies of the Software, and to permit persons to whom the Software is
   furnished to do so, subject to the following conditions:

   The above copyright notice, development funding notice, and this permission
   notice shall be included in all copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
   AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
   LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
   OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
   THE SOFTWARE.
*/

// String machine style ensemble chorus. See effect_ensemble.h.
//
// Delay of each tap (in samples) = BASE_DELAY - (55 * sin(slow) + 36.67 * sin(fast)),
// with fast = 10 x slow, matching the LFO shape of the original table based version.

#include <Arduino.h>
#include "effect_ensemble.h"

extern "C"
{
  extern const int16_t AudioWaveformSine[257];
}

#define BASE_DELAY 512 // samples (~11.6ms)
#define LFO_INTERVAL 8  // samples between LFO updates (must divide 128)

// The six taps are partly decorrelated, so their average is 4-9dB quieter than the
// input (more for bright sounds). Output = sum of taps / WET_DIVISOR: 6 = plain average,
// 3 = +6dB make-up gain so the level holds up at 100% effect mix.
#define WET_DIVISOR 3

static inline int16_t saturate16(int32_t v)
{
  return v > 32767 ? 32767 : (v < -32768 ? -32768 : (int16_t)v);
}

// LFO depths in samples, scaled by 256 (Q8). Original table: 110/2 and 110/3 samples.
#define SLOW_DEPTH_Q8 14080 // 55.0 * 256
#define FAST_DEPTH_Q8 9387  // 36.67 * 256

// Tap phase offsets: 60 degrees apart on the slow LFO, and 10x that on the fast LFO
static const uint32_t TAP_OFFSET_SLOW[ENSEMBLE_TAPS] = {
    0u, 715827883u, 1431655765u, 2147483648u, 2863311531u, 3579139413u};
static const uint32_t TAP_OFFSET_FAST[ENSEMBLE_TAPS] = {
    0u, 2863311531u, 1431655765u, 0u, 2863311531u, 1431655765u}; // 10 * slow offset, mod 2^32

// Right channel: 90 degrees on the slow LFO, which is 900 = 180 degrees on the fast LFO
#define RIGHT_OFFSET_SLOW 1073741824u

// Interpolated sine, Q15
static inline int32_t sineQ15(uint32_t phase)
{
  uint32_t index = phase >> 24;
  int32_t frac = (phase >> 9) & 0x7FFF;
  int32_t s0 = AudioWaveformSine[index];
  int32_t s1 = AudioWaveformSine[index + 1];
  return s0 + (((s1 - s0) * frac) >> 15);
}

// LFO modulation in samples, Q16. Depths are Q8 samples, up to 2x the original: the delay
// then swings 512 +/- 183 samples, inside the 1024 sample buffer
static inline int32_t lfoQ16(int32_t slowQ15, int32_t fastQ15, int32_t slowDepthQ8, int32_t fastDepthQ8)
{
  return (slowQ15 * slowDepthQ8 + fastQ15 * fastDepthQ8) >> 7;
}

AudioEffectEnsemble::AudioEffectEnsemble() : AudioStream(1, inputQueueArray)
{
  memset(delayBuffer, 0, sizeof(delayBuffer));
  writeIndex = 0;
  lfoPhaseSlow = 0;
  lfoPhaseFast = 0;
  slowDepthQ8 = SLOW_DEPTH_Q8;
  fastDepthQ8 = FAST_DEPTH_Q8;
  levelTrimQ15 = 32768;
  baseDelayQ16 = BASE_DELAY << 16;
  requestedDelay = BASE_DELAY;
  depthScale = 1.0f;
  lfoRate(6.0f);
}

void AudioEffectEnsemble::depth(float scale)
{
  if (scale < 0.25f)
    scale = 0.25f; // below this it's a fixed comb-filtered delay, not a chorus
  else if (scale > 2.0f)
    scale = 2.0f;
  // Host measurement of wet level vs depth (saw 110/440/1760Hz): 100% +2.2/-0.5/-2.5dB,
  // 25% +5.2/+2.2/-0.5dB, 0% +6dB at every pitch. The trim keeps 25-100% near 100%'s range.
  float trimDb = scale < 1.0f ? -2.0f * (1.0f - scale) : 0.0f;
  int32_t trim = (int32_t)(32768.0f * powf(10.0f, trimDb / 20.0f) + 0.5f);
  __disable_irq();
  slowDepthQ8 = (int32_t)(SLOW_DEPTH_Q8 * scale);
  fastDepthQ8 = (int32_t)(FAST_DEPTH_Q8 * scale);
  levelTrimQ15 = trim;
  __enable_irq();
  depthScale = scale;
  updateBaseDelay(); // the shortest delay depends on the depth
}

void AudioEffectEnsemble::baseDelay(float samples)
{
  requestedDelay = samples;
  updateBaseDelay();
}

void AudioEffectEnsemble::updateBaseDelay()
{
  // The taps swing +/-(55 + 36.67) samples x depth around the base delay, and the cubic
  // interpolation reads one sample before and two after the tap
  const float reach = (55.0f + 36.67f) * depthScale + 4.0f;
  float s = requestedDelay;
  if (s < reach)
    s = reach;
  else if (s > ENSEMBLE_BUFFER_SIZE - reach)
    s = ENSEMBLE_BUFFER_SIZE - reach;
  baseDelayQ16 = (int32_t)(s * 65536.0f);
}

void AudioEffectEnsemble::lfoRate(float rate)
{
  // The original advanced a 5880 entry table once every (counts + 1) samples,
  // with counts = 300 / rate. Same rate here, without the integer rounding.
  float counts = 300.0f / rate;
  if (counts < 1.0f)
    counts = 1.0f;
  float slowPeriodSamples = (counts + 1.0f) * 5880.0f;
  uint32_t inc = (uint32_t)(4294967296.0f / slowPeriodSamples);
  __disable_irq();
  lfoIncSlow = inc;
  lfoIncFast = inc * 10;
  __enable_irq();
}

// 4-point cubic Hermite interpolated read from the delay line, delay in samples Q16.
// Linear interpolation dulls and warbles the highs as the fractional delay sweeps.
inline int32_t AudioEffectEnsemble::readTap(int32_t delayQ16)
{
  uint32_t pos = ((uint32_t)writeIndex << 16) - (uint32_t)delayQ16;
  uint32_t i0 = (pos >> 16) & ENSEMBLE_BUFFER_MASK;
  float x = (float)(pos & 0xFFFF) * (1.0f / 65536.0f);
  float ym1 = delayBuffer[(i0 - 1) & ENSEMBLE_BUFFER_MASK];
  float y0 = delayBuffer[i0];
  float y1 = delayBuffer[(i0 + 1) & ENSEMBLE_BUFFER_MASK];
  float y2 = delayBuffer[(i0 + 2) & ENSEMBLE_BUFFER_MASK];
  float c1 = 0.5f * (y1 - ym1);
  float c2 = ym1 - 2.5f * y0 + 2.0f * y1 - 0.5f * y2;
  float c3 = 0.5f * (y2 - ym1) + 1.5f * (y0 - y1);
  return (int32_t)(((c3 * x + c2) * x + c1) * x + y0);
}

void AudioEffectEnsemble::update(void)
{
  audio_block_t *outblock = allocate();
  audio_block_t *outblockB = allocate();
  if (!outblock || !outblockB)
  {
    if (outblock)
      release(outblock);
    if (outblockB)
      release(outblockB);
    audio_block_t *tmp = receiveReadOnly(0);
    if (tmp)
      release(tmp);
    return;
  }

  // No input still runs the delay line so the chorus tail decays naturally
  audio_block_t *block = receiveReadOnly(0);

  const uint32_t incSlow = lfoIncSlow;
  const uint32_t incFast = lfoIncFast;
  const int32_t baseQ16 = baseDelayQ16;
  const int32_t sd = slowDepthQ8;
  const int32_t fd = fastDepthQ8;
  const int32_t trim = levelTrimQ15;

  // Tap delays from the LFOs (Q16 samples). The LFOs are below 5Hz, so they are worked out
  // every LFO_INTERVAL samples and the delays interpolated in between, rather than doing
  // 18 sine lookups every sample.
  auto tapDelays = [&](uint32_t phSlow, uint32_t phFast, int32_t *dl, int32_t *dr) {
    for (int t = 0; t < ENSEMBLE_TAPS; t++)
    {
      uint32_t phS = phSlow + TAP_OFFSET_SLOW[t];
      int32_t fast = sineQ15(phFast + TAP_OFFSET_FAST[t]);
      dl[t] = baseQ16 - lfoQ16(sineQ15(phS), fast, sd, fd);
      // Right: fast LFO is shifted 180 degrees, i.e. negated
      dr[t] = baseQ16 - lfoQ16(sineQ15(phS + RIGHT_OFFSET_SLOW), -fast, sd, fd);
    }
  };
  int32_t dl[ENSEMBLE_TAPS], dr[ENSEMBLE_TAPS], stepL[ENSEMBLE_TAPS], stepR[ENSEMBLE_TAPS];
  tapDelays(lfoPhaseSlow, lfoPhaseFast, dl, dr);

  for (int i = 0; i < AUDIO_BLOCK_SAMPLES; i++)
  {
    if ((i % LFO_INTERVAL) == 0)
    {
      int32_t el[ENSEMBLE_TAPS], er[ENSEMBLE_TAPS];
      tapDelays(lfoPhaseSlow + LFO_INTERVAL * incSlow, lfoPhaseFast + LFO_INTERVAL * incFast, el, er);
      for (int t = 0; t < ENSEMBLE_TAPS; t++)
      {
        stepL[t] = (el[t] - dl[t]) / LFO_INTERVAL;
        stepR[t] = (er[t] - dr[t]) / LFO_INTERVAL;
      }
    }

    delayBuffer[writeIndex] = block ? block->data[i] : 0;

    int32_t sumL = 0;
    int32_t sumR = 0;
    for (int t = 0; t < ENSEMBLE_TAPS; t++)
    {
      sumL += readTap(dl[t]);
      sumR += readTap(dr[t]);
      dl[t] += stepL[t];
      dr[t] += stepR[t];
    }
    int32_t outL = sumL / WET_DIVISOR;
    int32_t outR = sumR / WET_DIVISOR;
    if (trim != 32768)
    {
      outL = (int32_t)(((int64_t)outL * trim) >> 15);
      outR = (int32_t)(((int64_t)outR * trim) >> 15);
    }
    outblock->data[i] = saturate16(outL);
    outblockB->data[i] = saturate16(outR);

    lfoPhaseSlow += incSlow;
    lfoPhaseFast += incFast;
    writeIndex = (writeIndex + 1) & ENSEMBLE_BUFFER_MASK;
  }

  transmit(outblock, 0);
  transmit(outblockB, 1);
  release(outblock);
  release(outblockB);
  if (block)
    release(block);
}
