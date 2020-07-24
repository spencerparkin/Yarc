#pragma once

#include <stdint.h>

namespace Yarc
{
	class ByteStream
	{
	public:

		ByteStream();
		virtual ~ByteStream();

		virtual bool ReadByte(uint8_t& byte) = 0;
		virtual bool WriteByte(uint8_t byte) = 0;
	};

	class FiniteBufferStream : public ByteStream
	{
	public:

		FiniteBufferStream(uint8_t* givenBuffer, uint32_t givenBufferSize);
		virtual ~FiniteBufferStream();

	protected:

		uint8_t* buffer;
		uint32_t bufferSize;
	};

	class FiniteBufferInputStream : public FiniteBufferStream
	{
	public:

		FiniteBufferInputStream(uint8_t* givenBuffer, uint32_t givenBufferSize);
		virtual ~FiniteBufferInputStream();

		virtual bool ReadByte(uint8_t& byte) override;
		virtual bool WriteByte(uint8_t byte) override;

	protected:

		uint32_t readOffset;
	};

	class FiniteBufferOutputStream : public FiniteBufferStream
	{
	public:

		FiniteBufferOutputStream(uint8_t* givenBuffer, uint32_t givenBufferSize);
		virtual ~FiniteBufferOutputStream();

		virtual bool ReadByte(uint8_t& byte) override;
		virtual bool WriteByte(uint8_t byte) override;

	protected:

		uint32_t writeOffset;
	};
}