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

	/*virtual*/ bool ByteStream::ReadByte(uint8_t& byte)
	{
		uint32_t size = 1;
		return this->ReadBuffer(&byte, size);
	}

	/*virtual*/ bool ByteStream::WriteByte(uint8_t byte)
	{
		uint32_t size = 1;
		return this->WriteBuffer(&byte, size);
	}
}