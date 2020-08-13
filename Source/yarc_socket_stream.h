#pragma once

#include "yarc_byte_stream.h"
#if defined __WINDOWS__
#	include <WS2tcpip.h>
#elif defined __LINUX__
#	include <sys/socket.h>
#	include <sys/types.h>
#endif
#include <string>
#include <functional>
#include <time.h>

#if defined __LINUX__
typedef int SOCKET;
#endif

namespace Yarc
{
	struct YARC_API Address
	{
		Address();
		virtual ~Address();

		bool operator==(const Address& address) const;

		void SetIPAddress(const char* givenIPAddress);
		void SetHostname(const char* givenHostname);

		const char* GetResolvedIPAddress() const;
		std::string GetIPAddressAndPort() const;

		char hostname[64];
		mutable char ipAddress[32];
		uint16_t port;
	};

	class SocketStream : public ByteStream
	{
	public:

		SocketStream();
		virtual ~SocketStream();

		bool Connect(const Address& givenAddress, double timeoutSeconds = -1.0);
		bool IsConnected(void);
		bool Disconnect(void);

		virtual uint32_t ReadBuffer(uint8_t* buffer, uint32_t bufferSize) override;
		virtual uint32_t WriteBuffer(const uint8_t* buffer, uint32_t bufferSize) override;

		const Address& GetAddress() const { return this->address; }

		clock_t GetLastSocketReadWriteTime() { return this->lastSocketReadWriteTime; }

	protected:

		SOCKET sock;
		Address address;
		clock_t lastSocketReadWriteTime;
	};
}