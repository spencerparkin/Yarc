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

	bool ByteStream::ReadBufferNow(uint8_t* buffer, uint32_t bufferSize)
	{
		uint32_t bufferOffset = 0;
		uint32_t remainingBytes = bufferSize;
		while (remainingBytes > 0)
		{
			uint32_t bytesRead = this->ReadBuffer(&buffer[bufferOffset], remainingBytes);
			if (bytesRead == -1)
				return false;

			remainingBytes -= bytesRead;
			bufferOffset += bytesRead;
		}

		return true;
	}

	bool ByteStream::WriteBufferNow(const uint8_t* buffer, uint32_t bufferSize)
	{
		uint32_t bufferOffset = 0;
		uint32_t remainingBytes = bufferSize;
		while (remainingBytes > 0)
		{
			uint32_t bytesWritten = this->WriteBuffer((const uint8_t*)&buffer[bufferOffset], remainingBytes);
			if (bytesWritten == -1)
				return false;

			remainingBytes -= bytesWritten;
			bufferOffset += bytesWritten;
		}

		return true;
	}

	bool ByteStream::WriteFormat(const char* format, ...)
	{
		va_list args;
		va_start(args, format);
		char buffer[1024];
		sprintf_s(buffer, sizeof(buffer), format, args);
		va_end(args);

		return this->WriteBufferNow((const uint8_t*)buffer, ::strlen(buffer));
	}

	bool ByteStream::ReadByte(uint8_t& byte)
	{
		return this->ReadBufferNow(&byte, 1);
	}

	bool ByteStream::WriteByte(uint8_t byte)
	{
		return this->WriteBufferNow(&byte, 1);
	}

	//----------------------------------- StringStream -----------------------------------

	StringStream::StringStream(std::string* givenStringBuffer)
	{
		this->stringBuffer = givenStringBuffer;
		this->readOffset = 0;
	}

	/*virtual*/ StringStream::~StringStream()
	{
	}

	/*virtual*/ uint32_t StringStream::ReadBuffer(uint8_t* buffer, uint32_t bufferSize)
	{
		uint32_t i = 0;
		
		while (i < bufferSize)
		{
			if (this->readOffset >= this->stringBuffer->length())
				break;

			buffer[i] = this->stringBuffer->c_str()[i];

			i++;
		}

		return i;
	}

	/*virtual*/ uint32_t StringStream::WriteBuffer(const uint8_t* buffer, uint32_t bufferSize)
	{
		for (uint32_t i = 0; i < bufferSize; i++)
		{
			*this->stringBuffer += buffer[i];
		}

		return bufferSize;
	}
}