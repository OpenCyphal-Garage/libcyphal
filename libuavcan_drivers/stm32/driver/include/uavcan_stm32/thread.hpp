/*
 * Copyright (C) 2014 Pavel Kirienko <pavel.kirienko@gmail.com>
 */

#pragma once

#include <uavcan_stm32/build_config.hpp>

#if UAVCAN_STM32_CHIBIOS
# include <ch.hpp>
#elif UAVCAN_STM32_NUTTX
# include <nuttx/config.h>
# include <nuttx/fs/fs.h>
# include <poll.h>
# include <errno.h>
# include <cstdio>
# include <ctime>
# include <cstring>
#else
# error "Unknown OS"
#endif

#include <uavcan/uavcan.hpp>

namespace uavcan_stm32
{
class CanDriver;

#if UAVCAN_STM32_CHIBIOS

class BusEvent
{
    chibios_rt::CounterSemaphore sem_;

public:
    BusEvent(CanDriver& can_driver)
        : sem_(0)
    {
        (void)can_driver;
    }

    bool wait(uavcan::MonotonicDuration duration);

    void signal();

    void signalFromInterrupt();
};

class Mutex
{
    chibios_rt::Mutex mtx_;

public:
    void lock();
    void unlock();
};

#elif UAVCAN_STM32_NUTTX

/**
 * All bus events are reported as POLLIN.
 */
class BusEvent : uavcan::Noncopyable
{
    static const unsigned MaxPollWaiters = 8;

    ::file_operations file_ops_;
    ::pollfd* pollset_[MaxPollWaiters];
    CanDriver& can_driver_;
    bool signal_;

    static int openTrampoline(::file* filp);
    static int closeTrampoline(::file* filp);
    static int pollTrampoline(::file* filp, ::pollfd* fds, bool setup);

    int open(::file* filp);
    int close(::file* filp);
    int poll(::file* filp, ::pollfd* fds, bool setup);

    int addPollWaiter(::pollfd* fds);
    int removePollWaiter(::pollfd* fds);

public:
    static const char* const DevName;

    BusEvent(CanDriver& can_driver);
    ~BusEvent();

    bool wait(uavcan::MonotonicDuration duration);

    void signalFromInterrupt();
};

class Mutex
{
    pthread_mutex_t mutex_;

public:
    Mutex()
    {
        init();
    }

    int init()
    {
        return pthread_mutex_init(&mutex_, NULL);
    }

    int deinit()
    {
        return pthread_mutex_destroy(&mutex_);
    }

    void lock()
    {
        (void)pthread_mutex_lock(&mutex_);
    }

    void unlock()
    {
        (void)pthread_mutex_unlock(&mutex_);
    }
};

class MutexLocker
{
    Mutex& mutex_;

public:
    MutexLocker(Mutex& mutex)
        : mutex_(mutex)
    {
        mutex_.lock();
    }

    ~MutexLocker()
    {
        mutex_.unlock();
    }
};

#endif


#if UAVCAN_STM32_CHIBIOS

class MutexLocker
{
    Mutex& mutex_;

public:
    MutexLocker(Mutex& mutex)
        : mutex_(mutex)
    {
        mutex_.lock();
    }

    ~MutexLocker()
    {
        mutex_.unlock();
    }
};

#endif
}
