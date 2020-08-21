#pragma once

#if defined __WINDOWS__
#   define WIN32_LEAN_AND_MEAN
#   include <Windows.h>
#elif defined __LINUX__
#   include <pthread.h>
#endif

namespace Yarc
{
    class Mutex
    {
    public:
        Mutex()
        {
#if defined __WINDOWS__
            ::InitializeCriticalSection(&this->criticalSection);
#elif defined __LINUX__
            pthread_mutex_init(&this->mutex, NULL);
#endif
        }

        virtual ~Mutex()
        {
#if defined __WINDOWS__
            ::DeleteCriticalSection(&this->criticalSection);
#elif defined __LINUX__
            pthread_mutex_destroy(&this->mutex);
#endif
        }

#if defined __WINDOWS__
        CRITICAL_SECTION criticalSection;
#elif defined __LINUX__
        pthread_mutex_t mutex;
#endif
    };

    class MutexLocker
    {
    public:
        MutexLocker(Mutex& mutex)
        {
            this->cachedMutex = &mutex;
#if defined __WINDOWS__
            ::EnterCriticalSection(&this->cachedMutex->criticalSection);
#elif defined __LINUX__
            pthread_mutex_lock(&this->cachedMutex->mutex);
#endif
        }

        virtual ~MutexLocker()
        {
#if defined __WINDOWS__
            ::LeaveCriticalSection(&this->cachedMutex->criticalSection);
#elif defined __LINUX__
            pthread_mutex_unlock(&this->cachedMutex->mutex);
#endif
        }

    private:
        Mutex* cachedMutex;
    };
}