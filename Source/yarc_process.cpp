#include "yarc_process.h"
#include <string.h>
#if defined __LINUX__
#   include <unistd.h>
#   include <sys/wait.h>
#elif defined __WINDOWS__
#   include <Windows.h>
#endif

namespace Yarc
{
    Process::Process(void)
    {
#if defined __WINDOWS__
        ::ZeroMemory(&procInfo, sizeof(procInfo));
#elif defined __LINUX__
        this->childPid = 0;
#endif
    }
    
    /*virtual*/ Process::~Process(void)
    {
    }

    bool Process::Spawn(const std::string& command)
    {
#if defined __WINDOWS__
        if(procInfo.processHandle != NULL)
            return false;
        
        STARTUPINFO startupInfo;
        ::ZeroMemory(&startupInfo, sizeof(startupInfo));
        startupInfo.cb = sizeof(startupInfo);
        
        std::wstring wCommand(command.c_str());
        if (!::CreateProcessW(nullptr, (LPWSTR)command.c_str(), nullptr, nullptr, FALSE, 0, nullptr, nodeFolderPathCStr, &startupInfo, &procInfo))
            return false;
#elif defined __LINUX__
        if(this->childPid > 0)
            return false;
        
        char* argArray[20];
        int argArraySize = sizeof(argArray) / sizeof(char*);
        memset(argArray, 0, argArraySize);
        char buffer[1024];
        strcpy(buffer, command.c_str());
        char* token = strtok(buffer, " ");
        int i = 0;
        while(token != NULL)
        {
            if(i >= argArraySize - 1)
                return false;
            argArray[i++] = token;
            token = strtok(NULL, " ");
        }

        int pid = fork();
        if(pid < 0)
            return false;
        else if(pid == 0)
        {
            execv(argArray[0], argArray);
            exit(0);    // Did we just leak a bunch of stuff?
        }

        this->childPid = pid;
#endif
        return true;
    }

    void Process::WaitForExit(void)
    {
#if defined __WINDOWS__
        if(node.processHandle)
            ::WaitForSingleObject((HANDLE)node.processHandle, INFINITE);
#elif defined __LINUX__
        if(this->childPid > 0)
        {
            int status = 0;
            waitpid(this->childPid, &status, 0);
        }
#endif
    }
}