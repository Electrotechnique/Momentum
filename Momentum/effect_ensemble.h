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

// String machine style ensemble chorus (after the Roland RS-202 / Lowrey Symphonic Strings).
// Six modulated delay taps, each driven by a slow + fast sine LFO pair, 60 degrees apart.
// The right channel uses the same taps with the LFO shifted by 90 degrees for stereo spread.
//
// The LFO and delay interpolation are computed per sample in fixed point, so the delay
// time glides smoothly instead of stepping (stepping caused audible zipper/grit noise).

#ifndef effect_ensemble_h_
#define effect_ensemble_h_

#include <Arduino.h>
#include "AudioStream.h"

#define ENSEMBLE_BUFFER_SIZE 2048 // must be a power of two; 46ms, room for a 40ms delay at any depth
#define ENSEMBLE_BUFFER_MASK (ENSEMBLE_BUFFER_SIZE - 1)
#define ENSEMBLE_TAPS 6

class AudioEffectEnsemble : public AudioStream
{
public:
  AudioEffectEnsemble(void);
  virtual void update(void);
  // Rate is the effect amount (2.0 - 20.0); same mapping as the original table based version
  void lfoRate(float rate);
  // Modulation depth, 0.25 - 2.0 of the original depth (1.0). Below 1.0 the taps are more
  // alike and add up louder, so the level is trimmed a little (-2dB x (1 - depth)).
  void depth(float scale);
  // Base delay of the taps in samples; 512 (11.6ms) is the original. Used as set, except that
  // it can't be shorter than the LFO swing at the current depth (about 2.2ms at 100%, 4.2ms at
  // 200%), so the taps never reach the write position.
  void baseDelay(float samples);
  // The base delay in use, in samples (after the depth limit)
  float effectiveDelaySamples() { return baseDelayQ16 * (1.0f / 65536.0f); }

private:
  audio_block_t *inputQueueArray[1];
  int16_t delayBuffer[ENSEMBLE_BUFFER_SIZE];
  uint16_t writeIndex;
  uint32_t lfoPhaseSlow;
  uint32_t lfoPhaseFast;
  uint32_t lfoIncSlow;
  uint32_t lfoIncFast;
  volatile int32_t slowDepthQ8;
  volatile int32_t fastDepthQ8;
  volatile int32_t levelTrimQ15; // 32768 = no trim
  volatile int32_t baseDelayQ16;
  float requestedDelay; // samples, as set with baseDelay()
  float depthScale;     // as set with depth()

  void updateBaseDelay();

  inline int32_t readTap(int32_t delayQ16);
};

#endif
