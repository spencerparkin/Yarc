#pragma once

#include <functional>
#if defined __WINDOWS__
#	include <WS2tcpip.h>
#	include <Windows.h>
#elif defined __LINUX__
#	include <pthread.h>
#endif

namespace Yarc
{
	class Thread
	{
	public:

		Thread();
		virtual ~Thread();

		typedef std::function<void(void)> Func;

		// Just spawn a thread to run the given lambda function.
		bool SpawnThread(Func func);

		// The caller should signal the thread to shutdown before calling this.
		bool WaitForThreadExit(void);

		// It is bad practice to ever call this function if the thread is running.
		// If the thread is not running, it is just a no-op.
		void KillThread(void);

		bool IsStillRunning(void);

		static void Sleep(double timeoutMilliseconds);

private:

#if defined __WINDOWS__
		static DWORD __stdcall ThreadMain(LPVOID param);
		HANDLE threadHandle;
#elif defined __LINUX__
		static void* ThreadMain(void* arg);
		pthread_t thread;
		volatile bool threadRunning;
#endif
		Func cachedFunc;
	};
}
