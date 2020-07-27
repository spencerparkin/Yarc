#include "yarc_socket_stream.h"
#include "yarc_misc.h"
#include <time.h>

#pragma comment(lib, "Ws2_32.lib")

namespace Yarc
{
	//----------------------------------- SocketStream -----------------------------------

	SocketStream::SocketStream()
	{
		this->socket = INVALID_SOCKET;
		this->address = new std::string();
		this->port = 0;
		this->lastSocketReadWriteTime = 0;
	}

	/*virtual*/ SocketStream::~SocketStream()
	{
		(void)this->Disconnect();
		delete this->address;
	}

	bool SocketStream::Connect(const char* address, uint16_t port /*= 6379*/, double timeoutSeconds /*= -1.0*/)
	{
		bool success = true;

		try
		{
			if (this->socket != INVALID_SOCKET)
				throw new InternalException();

			WSADATA data;
			int result = ::WSAStartup(MAKEWORD(2, 2), &data);
			if (result != 0)
				throw new InternalException();

			this->socket = ::socket(AF_INET, SOCK_STREAM, 0);
			if (this->socket == INVALID_SOCKET)
				throw new InternalException();

			sockaddr_in sockaddr;
			sockaddr.sin_family = AF_INET;
			::InetPtonA(sockaddr.sin_family, address, &sockaddr.sin_addr);
			sockaddr.sin_port = ::htons(port);

			if (timeoutSeconds < 0.0)
			{
				result = ::connect(this->socket, (SOCKADDR*)&sockaddr, sizeof(sockaddr));
				if (result == SOCKET_ERROR)
					throw new InternalException();
			}
			else
			{
				u_long arg = 1;
				result = ::ioctlsocket(this->socket, FIONBIO, &arg);
				if (result != NO_ERROR)
					throw new InternalException();

				result = ::connect(this->socket, (SOCKADDR*)&sockaddr, sizeof(sockaddr));
				if (result != SOCKET_ERROR)
					throw new InternalException();

				int error = ::WSAGetLastError();
				if (error != WSAEWOULDBLOCK)
					throw new InternalException();

				bool timedOut = true;
				double startTime = double(::clock()) / double(CLOCKS_PER_SEC);
				double elapsedTime = 0.0f;
				while (elapsedTime <= timeoutSeconds)
				{
					fd_set writeSet, excSet;
					FD_ZERO(&writeSet);
					FD_ZERO(&excSet);
					FD_SET(this->socket, &writeSet);
					FD_SET(this->socket, &excSet);

					timeval timeVal;
					timeVal.tv_sec = 0;
					timeVal.tv_usec = 1000;

					int32_t count = ::select(0, NULL, &writeSet, &excSet, &timeVal);
					if (count == SOCKET_ERROR)
						throw new InternalException();

					if (FD_ISSET(this->socket, &excSet))
						throw new InternalException();

					// Is the socket writable?
					if (FD_ISSET(this->socket, &writeSet))
					{
						timedOut = false;
						break;
					}

					double currentTime = double(::clock()) / double(CLOCKS_PER_SEC);
					elapsedTime = currentTime - startTime;
				}

				if (timedOut)
					throw new InternalException();

				arg = 0;
				result = ::ioctlsocket(this->socket, FIONBIO, &arg);
				if (result != NO_ERROR)
					throw new InternalException();
			}

			*this->address = address;
			this->port = port;
		}
		catch (InternalException* exc)
		{
			success = false;
			this->Disconnect();
			delete exc;
		}

		return success;
	}

	bool SocketStream::IsConnected(void)
	{
		return this->socket != INVALID_SOCKET;
	}

	bool SocketStream::Disconnect(void)
	{
		if (this->socket != INVALID_SOCKET)
		{
			::closesocket(this->socket);
			this->socket = INVALID_SOCKET;
		}

		return true;
	}

	/*virtual*/ uint32_t SocketStream::ReadBuffer(uint8_t* buffer, uint32_t bufferSize)
	{
		if (!this->IsConnected())
			return -1;

		// Note that it's safe to block here even if the thread needs to exit,
		// because we signal the thread to exit by simply closing the socket,
		// at which point we should return from this function.
		uint32_t readCount = ::recv(this->socket, (char*)buffer, bufferSize, 0);
		if (readCount == SOCKET_ERROR)
		{
			// We should check the error code here, but typically, this means
			// that we have lost our connection.  We do this on purpose to
			// signal thread shut-down, but it could also happen by accident.
			this->socket = INVALID_SOCKET;
			return -1;
		}

		if(readCount > 0)
			this->lastSocketReadWriteTime = ::clock();

		return readCount;
	}

	/*virtual*/ uint32_t SocketStream::WriteBuffer(const uint8_t* buffer, uint32_t bufferSize)
	{
		if (!this->IsConnected())
			return -1;

		uint32_t writeCount = ::send(this->socket, (const char*)buffer, bufferSize, 0);
		if (writeCount == SOCKET_ERROR)
		{
			this->socket = INVALID_SOCKET;
			return -1;
		}

		if(writeCount > 0)
			this->lastSocketReadWriteTime = ::clock();

		return writeCount;
	}
}