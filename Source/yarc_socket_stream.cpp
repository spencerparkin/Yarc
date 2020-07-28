#include "yarc_socket_stream.h"
#include "yarc_misc.h"
#include <time.h>
#include <string.h>

#pragma comment(lib, "Ws2_32.lib")

namespace Yarc
{
	//----------------------------------- Address -----------------------------------

	Address::Address()
	{
		::strcpy_s(this->ipAddress, sizeof(this->ipAddress), "127.0.0.1");
		this->hostname[0] = '\0';
		this->port = 6379;
	}

	/*virtual*/ Address::~Address()
	{
	}

	bool Address::operator==(const Address& address) const
	{
		if (this->port != address.port)
			return false;

		return 0 == ::strcmp(this->GetResolvedIPAddress(), address.GetResolvedIPAddress());
	}

	void Address::SetIPAddress(const char* givenIPAddress)
	{
		::strcpy_s(this->ipAddress, sizeof(this->ipAddress), givenIPAddress);
	}

	void Address::SetHostname(const char* givenHostname)
	{
		::strcpy_s(this->hostname, sizeof(this->hostname), givenHostname);
	}

	const char* Address::GetResolvedIPAddress() const
	{
		if (::strlen(this->hostname) > 0)
		{
			WSADATA data;
			::WSAStartup(MAKEWORD(2, 2), &data);

			char portStr[16];
			sprintf_s(portStr, sizeof(portStr), "%d", this->port);

			::addrinfo* addrInfo = nullptr;
			if (0 == ::getaddrinfo(this->hostname, portStr, nullptr, &addrInfo))
			{
				while (addrInfo)
				{
					if (addrInfo->ai_family == AF_INET)
						break;

					addrInfo = addrInfo->ai_next;
				}

				if (addrInfo)
				{
					::sockaddr_in* sockAddr = (::sockaddr_in*)addrInfo->ai_addr;
					::sprintf_s(this->ipAddress, sizeof(this->ipAddress), "%d.%d.%d.%d",
						sockAddr->sin_addr.S_un.S_un_b.s_b1,
						sockAddr->sin_addr.S_un.S_un_b.s_b2,
						sockAddr->sin_addr.S_un.S_un_b.s_b3,
						sockAddr->sin_addr.S_un.S_un_b.s_b4);


				}
			}
		}

		return this->ipAddress;
	}

	//----------------------------------- SocketStream -----------------------------------

	SocketStream::SocketStream()
	{
		this->socket = INVALID_SOCKET;
		this->lastSocketReadWriteTime = 0;
	}

	/*virtual*/ SocketStream::~SocketStream()
	{
		(void)this->Disconnect();
	}

	bool SocketStream::Connect(const Address& givenAddress, double timeoutSeconds /*= -1.0*/)
	{
		bool success = true;

		try
		{
			this->address = givenAddress;

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
			::InetPtonA(sockaddr.sin_family, this->address.ipAddress, &sockaddr.sin_addr);
			sockaddr.sin_port = ::htons(this->address.port);

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
		// There's really no way to know until you try to read or write on the socket.
		// But as far as we know, if we have a valid socket handle, then we should assume we're connected.
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