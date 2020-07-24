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

	//----------------------------------- SocketStream -----------------------------------

	SocketStream::SocketStream(SOCKET* givenSocket)
	{
		this->socket = givenSocket;
		this->canBlock = true;
	}

	/*virtual*/ SocketStream::~SocketStream()
	{
	}

	/*virtual*/ bool SocketStream::ReadBuffer(uint8_t* buffer, uint32_t& bufferSize)
	{
		if (!this->canBlock)
		{
			fd_set readSet, excSet;
			FD_ZERO(&readSet);
			FD_ZERO(&excSet);
			FD_SET(*this->socket, &readSet);
			FD_SET(*this->socket, &excSet);

			timeval timeVal;
			timeVal.tv_sec = 0;
			timeVal.tv_usec = 0;

			int32_t count = ::select(0, &readSet, NULL, &excSet, &timeVal);
			if(count == SOCKET_ERROR)
				return false;

			if (!FD_ISSET(this->socket, &excSet))
				return false;

			// Does the socket have data for us to read?
			if (!FD_ISSET(this->socket, &readSet))
				return false;
		}

		uint32_t readCount = ::recv(*this->socket, (char*)buffer, bufferSize, 0);
		if (readCount == SOCKET_ERROR)
		{
			*this->socket = INVALID_SOCKET;
			return false;
		}

		bufferSize = readCount;
		return true;
	}

	/*virtual*/ bool SocketStream::WriteBuffer(const uint8_t* buffer, uint32_t& bufferSize)
	{
		uint32_t writeCount = ::send(*this->socket, (const char*)buffer, bufferSize, 0);
		if (writeCount == SOCKET_ERROR)
		{
			*this->socket = INVALID_SOCKET;
			return false;
		}

		bufferSize = writeCount;
		return true;
	}
}