// Makes malloc/free (and so new/delete and String) safe to use from both the main loop
// and the display thread.
//
// newlib calls __malloc_lock/__malloc_unlock around every heap operation, but its default
// versions do nothing. TeensyThreads can switch threads at any time, so the main loop
// (MIDI/CC handling) and the display thread could both be inside malloc at once and
// corrupt the heap. Interrupts are disabled while the heap is changed (a few
// microseconds), which also stops thread switches. Nesting is allowed.

#include <Arduino.h>

static volatile uint32_t mallocLockDepth = 0;
static uint32_t mallocLockPrimask = 0;

extern "C" void __malloc_lock(struct _reent *)
{
    uint32_t primask;
    __asm__ volatile("mrs %0, primask\n\tcpsid i" : "=r"(primask)::"memory");
    if (mallocLockDepth++ == 0)
        mallocLockPrimask = primask;
}

extern "C" void __malloc_unlock(struct _reent *)
{
    if (--mallocLockDepth == 0 && !(mallocLockPrimask & 1))
        __asm__ volatile("cpsie i" ::: "memory");
}
