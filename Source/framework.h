#pragma once

#if defined YARC_EXPORT
#	define YARC_API		__declspec(dllexport)
#elif defined YARC_IMPORT
#	define YARC_API		__declspec(dllimport)
#else
#	define YARC_API
#endif