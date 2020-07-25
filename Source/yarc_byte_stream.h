#pragma once

#include <stdint.h>

namespace Yarc
{
	class ByteStream
	{
	public:

		ByteStream();
		virtual ~ByteStream();

		// Read from the stream into the given buffer of the given size.
		// The size is modified to reflect the number of bytes read.
		virtual bool ReadBuffer(uint8_t* buffer, uint32_t& bufferSize) { return false; }

		// Write to the stream from the given buffer of the given size.
		// The size is modified to relfect the number of bytes written.
		virtual bool WriteBuffer(const uint8_t* buffer, uint32_t& bufferSize) { return false; }

		// Read a byte from the stream into the given byte.
		virtual bool ReadByte(uint8_t& byte);

		// Write a byte to the stream from the given byte.
		virtual bool WriteByte(uint8_t byte);
	};
}