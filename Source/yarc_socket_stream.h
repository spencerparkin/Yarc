#pragma once

#include "yarc_byte_stream.h"
#include <WS2tcpip.h>
#include <string>
#include <functional>
#include <time.h>

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

		SOCKET socket;
		Address address;
		clock_t lastSocketReadWriteTime;
	};
}