#pragma once

#include "yarc_api.h"
#include <stdint.h>
#include <string>

namespace Yarc
{
	class YARC_API ByteStream
	{
	public:

		ByteStream();
		virtual ~ByteStream();

		// Read from the stream into the given buffer of the given size.
		// Return the number of bytes written to the given buffer.
		virtual uint32_t ReadBuffer(uint8_t* buffer, uint32_t bufferSize) { return false; }

		// Write to the stream from the given buffer of the given size.
		// Return the number of bytes read from the given buffer.
		virtual uint32_t WriteBuffer(const uint8_t* buffer, uint32_t bufferSize) { return false; }

		// Read a byte from the stream into the given byte.
		virtual bool ReadByte(uint8_t& byte);

		// Write a byte to the stream from the given byte.
		virtual bool WriteByte(uint8_t byte);

		// Provided for convenience, this works just like printf or sprintf.
		bool WriteFormat(const char* format, ...);
	};

	class YARC_API StringStream : public ByteStream
	{
	public:

		StringStream(std::string* givenStringBuffer);
		virtual ~StringStream();

		virtual uint32_t ReadBuffer(uint8_t* buffer, uint32_t bufferSize) override;
		virtual uint32_t WriteBuffer(const uint8_t* buffer, uint32_t bufferSize) override;

		std::string* stringBuffer;
		uint32_t readOffset;
	};
}