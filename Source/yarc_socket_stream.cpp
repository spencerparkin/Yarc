#include "yarc_socket_stream.h"
#include "yarc_misc.h"
#include <time.h>
#include <string.h>

#if defined __WINDOWS__
#	pragma comment(lib, "Ws2_32.lib")
#endif

namespace Yarc
{
	//----------------------------------- Address -----------------------------------

	Address::Address()
	{
		::strcpy(this->ipAddress, "127.0.0.1");
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
		::strcpy(this->ipAddress, givenIPAddress);
	}

	void Address::SetHostname(const char* givenHostname)
	{
		::strcpy(this->hostname, givenHostname);
	}

	const char* Address::GetResolvedIPAddress() const
	{
		if (::strlen(this->hostname) > 0)
		{
#if defined __WINDOWS__
			WSADATA data;
			::WSAStartup(MAKEWORD(2, 2), &data);
#endif

			char portStr[16];
			sprintf(portStr, "%d", this->port);

			struct addrinfo* addrInfo = nullptr;
			if (0 == getaddrinfo(this->hostname, portStr, nullptr, &addrInfo))
			{
				while (addrInfo)
				{
					if (addrInfo->ai_family == AF_INET)
						break;

					addrInfo = addrInfo->ai_next;
				}

				if (addrInfo)
				{
#if defined __WINDOWS__
					::sockaddr_in* sockAddr = (::sockaddr_in*)addrInfo->ai_addr;
					::sprintf(this->ipAddress, "%d.%d.%d.%d",
						sockAddr->sin_addr.S_un.S_un_b.s_b1,
						sockAddr->sin_addr.S_un.S_un_b.s_b2,
						sockAddr->sin_addr.S_un.S_un_b.s_b3,
						sockAddr->sin_addr.S_un.S_un_b.s_b4);
#elif defined __LINUX__
					getnameinfo(addrInfo->ai_addr, addrInfo->ai_addrlen, this->ipAddress, sizeof(this->ipAddress), nullptr, 0, NI_NAMEREQD|NI_NUMERICHOST);
#endif
				}
			}
		}

		return this->ipAddress;
	}

	std::string Address::GetIPAddressAndPort() const
	{
		char ipPort[64];
		sprintf(ipPort, "%s:%d", this->GetResolvedIPAddress(), this->port);
		return ipPort;
	}

	//----------------------------------- SocketStream -----------------------------------

	SocketStream::SocketStream()
	{
		this->sock = INVALID_SOCKET;
		this->lastSocketReadWriteTime = 0;
	}

	/*virtual*/ SocketStream::~SocketStream()
	{
		(void)this->Disconnect();
	}

	bool SocketStream::Connect(const Address& givenAddress, double timeoutSeconds /*= -1.0*/)
	{
		int result = 0;

		auto lambda = [&]() -> bool
		{
			this->address = givenAddress;

			if (this->sock != INVALID_SOCKET)
				return false;

#if defined __WINDOWS__
			WSADATA data;
			int result = ::WSAStartup(MAKEWORD(2, 2), &data);
			if (result != 0)
				return false;
#endif

			this->sock = ::socket(AF_INET, SOCK_STREAM, 0);
			if (this->sock == INVALID_SOCKET)
				return false;

			sockaddr_in sockaddr;
			sockaddr.sin_family = AF_INET;
#if defined __WINDOWS__
			::InetPtonA(sockaddr.sin_family, this->address.ipAddress, &sockaddr.sin_addr);
#elif defined __LINUX__
			inet_pton(sockaddr.sin_family, this->address.ipAddress, &sockaddr.sin_addr);
#endif
			sockaddr.sin_port = ::htons(this->address.port);

			if (timeoutSeconds < 0.0)
			{
				result = ::connect(this->sock, (SOCKADDR*)&sockaddr, sizeof(sockaddr));
				if (result == SOCKET_ERROR)
					return false;
			}
			else
			{
				u_long arg = 1;
#if defined __WINDOWS__
				result = ::ioctlsocket(this->sock, FIONBIO, &arg);
#elif defined __LINUX__
				result = ioctl(this->sock, FIONBIO, &arg);
#endif
				if (result != NO_ERROR)
					return false;

				result = ::connect(this->sock, (SOCKADDR*)&sockaddr, sizeof(sockaddr));
				if (result != SOCKET_ERROR)
					return false;

#if defined __WINDOWS__
				int error = ::WSAGetLastError();
				if (error != WSAEWOULDBLOCK)
					return false;
#elif defined __LINUX__
				if(errno == EWOULDBLOCK)
					return false;
#endif

				bool timedOut = true;
				double startTime = double(::clock()) / double(CLOCKS_PER_SEC);
				double elapsedTime = 0.0f;
				while (elapsedTime <= timeoutSeconds)
				{
					fd_set writeSet, excSet;
					FD_ZERO(&writeSet);
					FD_ZERO(&excSet);
					FD_SET(this->sock, &writeSet);
					FD_SET(this->sock, &excSet);

					timeval timeVal;
					timeVal.tv_sec = 0;
					timeVal.tv_usec = 1000;

					int32_t count = ::select(0, NULL, &writeSet, &excSet, &timeVal);
					if (count == SOCKET_ERROR)
						return false;

					if (FD_ISSET(this->sock, &excSet))
						return false;

					// Is the socket writable?
					if (FD_ISSET(this->sock, &writeSet))
					{
						timedOut = false;
						break;
					}

					double currentTime = double(::clock()) / double(CLOCKS_PER_SEC);
					elapsedTime = currentTime - startTime;
				}

				if (timedOut)
					return false;

				arg = 0;
#if defined __WINDOWS__
				result = ::ioctlsocket(this->sock, FIONBIO, &arg);
#elif defined __LINUX__
				result = ioctl(this->sock, FIONBIO, &arg);
#endif
				if (result != NO_ERROR)
					return false;
			}

			return true;
		};
		
		bool success = lambda();

		if(!success)
			this->Disconnect();

		return success;
	}

	bool SocketStream::IsConnected(void)
	{
		// There's really no way to know until you try to read or write on the socket.
		// But as far as we know, if we have a valid socket handle, then we should assume we're connected.
		return this->sock != INVALID_SOCKET;
	}

	bool SocketStream::Disconnect(void)
	{
		if (this->sock != INVALID_SOCKET)
		{
#if defined __WINDOWS__
			::closesocket(this->sock);
#elif defined __LINUX__
			close(this->sock);
#endif
			this->sock = INVALID_SOCKET;
		}

		return true;
	}

	/*virtual*/ uint32_t SocketStream::ReadBuffer(uint8_t* buffer, uint32_t bufferSize)
	{
		if (!this->IsConnected())
			return -1;

		uint32_t readCount = ::recv(this->sock, (char*)buffer, bufferSize, 0);
		if (readCount == uint32_t(SOCKET_ERROR))
		{
			this->sock = INVALID_SOCKET;
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

		uint32_t writeCount = ::send(this->sock, (const char*)buffer, bufferSize, 0);
		if (writeCount == uint32_t(SOCKET_ERROR))
		{
			this->sock = INVALID_SOCKET;
			return -1;
		}

		if(writeCount > 0)
			this->lastSocketReadWriteTime = ::clock();

		return writeCount;
	}
}