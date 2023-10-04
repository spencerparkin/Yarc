#pragma once

#if defined __WINDOWS__
#	if !defined WIN32_LEAN_AND_MEAN
#	   define WIN32_LEAN_AND_MEAN
	#endif
#   include <Windows.h>
#elif defined __LINUX__
#   include <pthread.h>
#endif

namespace Yarc
{
	class YARC_API Semaphore
	{
	public:
		Semaphore(int32_t count)
		{
#if defined __WINDOWS__
			this->semaphoreHandle = ::CreateSemaphore(NULL, 0, count, NULL);
#elif defined __LINUX__
			// TODO: Write this.
#endif
		}

		virtual ~Semaphore()
		{
#if defined __WINDOWS__
			::CloseHandle(this->semaphoreHandle);
#endif
		}

		void Increment()
		{
#if defined __WINDOWS__
			::ReleaseSemaphore(this->semaphoreHandle, 1, NULL);
#endif
		}

		bool Decrement(double timeoutMilliseconds)
		{
#if defined __WINDOWS__
			return WAIT_OBJECT_0 == ::WaitForSingleObject(this->semaphoreHandle, (timeoutMilliseconds >= 0.0f) ? (DWORD)timeoutMilliseconds : INFINITE);
#endif
		}

#if defined __WINDOWS__
		HANDLE semaphoreHandle;
#endif
	};
}