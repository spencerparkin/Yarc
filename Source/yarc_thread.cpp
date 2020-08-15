#include "yarc_thread.h"
#include <string.h>

namespace Yarc
{
    Thread::Thread()
    {
#if defined __WINDOWS__
        this->threadHandle = nullptr;
#elif defined __LINUX__
        memset(&this->thread, 0, sizeof(this->thread));
        this->threadRunning = false;
#endif
    }

    /*virtual*/ Thread::~Thread()
    {
        this->KillThread();
    }

    bool Thread::SpawnThread(Func func)
    {
        this->cachedFunc = func;

#if defined __WINDOWS__
        if(this->threadHandle)
            return false;
        
        this->threadHandle = ::CreateThread(nullptr, 0, &Thread::ThreadMain, this, 0, nullptr);
        if(!threadHandle)
            return false;
#elif defined __LINUX__
        if(!this->threadRunning)
            return false;
        
        int result = pthread_create(&this->thread, nullptr, &Thread::ThreadMain, this);
        if(result != 0)
            return false;
        
        this->threadRunning = true;
#endif
        return true;
    }

	bool Thread::WaitForThreadExit(void)
    {
#if defined __WINDOWS__
        if(!this->threadHandle)
            return false;

        ::WaitForSingleObject(this->threadHandle, INFINITE);
        this->threadHandle = nullptr;
#elif defined __LINUX__
        if(!this->threadRunning)
            return false;
        
        int result = pthread_join(this->thread, nullptr);
        if(result != 0)
            return false;

        this->threadRunning = false;
#endif
        return true;
    }

	void Thread::KillThread(void)
    {
#if defined __WINDOWS__
        if(this->threadHandle)
        {
            ::TerminateThread(this->threadHandle, 0);
            this->threadHandle = nullptr;
        }
#elif defined __LINUX__
        // TODO: Write this.
#endif
    }

    bool Thread::IsStillRunning(void)
    {
#if defined __WINDOWS__
        if(!this->threadHandle)
            return false;

        DWORD exitCode = 0;
        if (::GetExitCodeThread(this->threadHandle, &exitCode))
            if (exitCode != STILL_ACTIVE)
                return false;
        
        return true;
#elif defined __LINUX__
        return this->threadRunning;
#endif
    }

#if defined __WINDOWS__
    /*static*/ DWORD __stdcall Thread::ThreadMain(LPVOID param)
    {
        Thread* thread = (Thread*)param;
        thread->cachedFunc();
    }
#elif defined __LINUX__
    /*static*/ void* Thread::ThreadMain(void* arg)
    {
        Thread* thread = (Thread*)arg;
        thread->cachedFunc();
        thread->threadRunning = false;
        return nullptr;
    }
#endif
}