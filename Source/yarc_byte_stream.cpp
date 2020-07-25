#include "yarc_byte_stream.h"
#include <cstdarg>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

namespace Yarc
{
	//----------------------------------- ByteStream -----------------------------------

	ByteStream::ByteStream()
	{
	}

	/*virtual*/ ByteStream::~ByteStream()
	{
	}

	/*virtual*/ bool ByteStream::ReadByte(uint8_t& byte)
	{
		return this->ReadBuffer(&byte, 1) == 1;
	}

	/*virtual*/ bool ByteStream::WriteByte(uint8_t byte)
	{
		return this->WriteBuffer(&byte, 1) == 1;
	}

	bool ByteStream::WriteFormat(const char* format, ...)
	{
		va_list args;
		va_start(args, format);
		char buffer[1024];
		sprintf_s(buffer, sizeof(buffer), format, args);
		va_end(args);

		uint32_t bufferOffset = 0;
		uint32_t remainingBytes = ::strlen(buffer);
		while (remainingBytes > 0)
		{
			uint32_t bytesWritten = this->WriteBuffer((const uint8_t*)&buffer[bufferOffset], remainingBytes);
			if (bytesWritten == 0)
				return false;

			remainingBytes -= bytesWritten;
			bufferOffset += bytesWritten;
		}

		return true;
	}
}