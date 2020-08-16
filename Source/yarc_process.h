#pragma once

#include <string>

namespace Yarc
{
    class Process
    {
    public:

        Process(void);
        virtual ~Process(void);

        bool Spawn(const std::string& command);

        void WaitForExit(void);

#if defined __WINDOWS__
        PROCESS_INFORMATION procInfo;
#elif defined __LINUX__
        int childPid;
#endif
    };
}