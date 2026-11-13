#ifndef MOMENTUM_THREAD_SAFETY_H
#define MOMENTUM_THREAD_SAFETY_H

// The display runs in its own TeensyThreads thread, preemptively alongside the main loop.
// The Strings, encoder map and patch/bank/performance lists the display renders from are
// written by the main loop. displayLock is held by the display thread while it renders, and
// by the main loop while it changes that state (UI callbacks, MIDI CC / program change,
// patch recall, SD card changes). Note on/off handling doesn't take it.
//
// The heap itself is made thread safe in malloc_lock.cpp.
//
// Included once, from main.cpp.

#include <TeensyThreads.h>

// TeensyThreads' Mutex isn't recursive. Display functions call each other and are called
// from code that already holds the lock, so allow the owning thread to lock again.
//
// Waiting uses try_lock() and yield() only. Mutex::lock() suspends the waiting thread, but
// if the holder unlocks between the waiter's failed try and its suspend, nothing ever
// restarts it (lost wake-up): the display froze for good under heavy MIDI CC traffic on TSynth.
class RecursiveMutex
{
public:
    void lock()
    {
        int id = threads.id();
        if (owner == id)
        {
            depth++;
            return;
        }
        while (!mutex.try_lock())
            threads.yield(); // let the holder run and release it
        owner = id;
        depth = 1;
    }
    void unlock()
    {
        if (--depth == 0)
        {
            owner = -1;
            mutex.unlock();
        }
    }

private:
    Threads::Mutex mutex;
    volatile int owner = -1;
    int depth = 0;
};

class LockGuard
{
public:
    explicit LockGuard(RecursiveMutex &m) : m(m) { m.lock(); }
    ~LockGuard() { m.unlock(); }

private:
    RecursiveMutex &m;
};

RecursiveMutex displayLock;

#endif
