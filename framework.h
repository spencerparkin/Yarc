#pragma once

#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers
// Windows Header Files
#include <windows.h>

#if defined YARC_EXPORT
#	define YARC_API		__declspec(dllexport)
#elif defined YARC_IMPORT
#	define YARC_API		__declspec(dllimport)
#else
#	define YARC_API
#endif