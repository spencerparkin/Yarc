#pragma once

#include "yarc_api.h"
#include <stdint.h>

namespace Yarc
{
	class Exception
	{
	public:
		Exception()
		{
		}

		virtual ~Exception()
		{
		}
	};
	
	// These are thrown internally.
	class InternalException : public Exception
	{
	public:
		InternalException()
		{
		}

		virtual ~InternalException()
		{
		}
	};

	// These can get thrown to the user of the API.
	class ExternalException : public Exception
	{
	public:
		ExternalException()
		{
		}

		virtual ~ExternalException()
		{
		}
	};

	class RecursionGuard
	{
	public:
		RecursionGuard(uint32_t* callCount)
		{
			this->callCount = callCount;
			(*this->callCount)++;
		}

		virtual ~RecursionGuard()
		{
			(*this->callCount)--;
		}

		bool IsRecursing()
		{
			return *this->callCount >= 2;
		}

	private:
		uint32_t* callCount;
	};

	extern YARC_API uint32_t RandomNumber(uint32_t min, uint32_t max);
}