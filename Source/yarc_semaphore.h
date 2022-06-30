#pragma once

#if defined __WINDOWS__
#   define WIN32_LEAN_AND_MEAN
#   include <Windows.h>
#elif defined __LINUX__
#   include <pthread.h>
#endif

namespace Yarc
{
	class YARC_API Semaphore
	{
	public:
		Semaphore(int count)
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

		void Decrement(double timeoutMilliseconds)
		{
#if defined __WINDOWS__
			::WaitForSingleObject(this->semaphoreHandle, (DWORD)timeoutMilliseconds);
#endif
		}

		static void DecrementMulti(int semaphoreCount, Semaphore** semaphoreArray, double timeoutMilliseconds = 0.0, bool waitAll = false)
		{
			if (timeoutMilliseconds == 0.0)
				return;
#if defined __WINDOWS__
			HANDLE* semaphoreHandleArray = new HANDLE[semaphoreCount];
			for (int i = 0; i < semaphoreCount; i++)
				semaphoreHandleArray[i] = semaphoreArray[i]->semaphoreHandle;
			::WaitForMultipleObjects(semaphoreCount, semaphoreHandleArray, waitAll, (DWORD)timeoutMilliseconds);
			delete[] semaphoreHandleArray;
#endif
		}

#if defined __WINDOWS__
		HANDLE semaphoreHandle;
#endif
	};
}