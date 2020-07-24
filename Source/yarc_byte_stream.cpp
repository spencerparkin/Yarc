#include "yarc_byte_stream.h"

namespace Yarc
{
	//----------------------------------- ByteStream -----------------------------------

	ByteStream::ByteStream()
	{
	}

	/*virtual*/ ByteStream::~ByteStream()
	{
	}

	//----------------------------------- FiniteBufferStream -----------------------------------
	
	FiniteBufferStream::FiniteBufferStream(uint8_t* givenBuffer, uint32_t givenBufferSize)
	{
		this->buffer = givenBuffer;
		this->bufferSize = givenBufferSize;
	}

	/*virtual*/ FiniteBufferStream::~FiniteBufferStream()
	{
	}

	//----------------------------------- FiniteBufferInputStream -----------------------------------

	FiniteBufferInputStream::FiniteBufferInputStream(uint8_t* givenBuffer, uint32_t givenBufferSize) : FiniteBufferStream(givenBuffer, givenBufferSize)
	{
		this->readOffset = 0;
	}

	/*virtual*/ FiniteBufferInputStream::~FiniteBufferInputStream()
	{
	}

	/*virtual*/ bool FiniteBufferInputStream::ReadByte(uint8_t& byte)
	{
		if (this->readOffset >= this->bufferSize)
			return false;

		byte = this->buffer[this->readOffset++];
		return true;
	}

	/*virtual*/ bool FiniteBufferInputStream::WriteByte(uint8_t byte)
	{
		return false;
	}

	//----------------------------------- FiniteBufferOutputStream -----------------------------------

	FiniteBufferOutputStream::FiniteBufferOutputStream(uint8_t* givenBuffer, uint32_t givenBufferSize) : FiniteBufferStream(givenBuffer, givenBufferSize)
	{
		this->writeOffset = 0;
	}

	/*virtual*/ FiniteBufferOutputStream::~FiniteBufferOutputStream()
	{
	}

	/*virtual*/ bool FiniteBufferOutputStream::ReadByte(uint8_t& byte)
	{
		return false;
	}

	/*virtual*/ bool FiniteBufferOutputStream::WriteByte(uint8_t byte)
	{
		if (this->writeOffset >= this->bufferSize)
			return false;

		this->buffer[this->writeOffset++] = byte;
		return true;
	}
}