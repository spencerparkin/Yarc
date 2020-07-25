#pragma once

#include "yarc_byte_stream.h"
#include <WS2tcpip.h>
#include <string>

namespace Yarc
{
	class SocketStream : public ByteStream
	{
	public:

		SocketStream();
		virtual ~SocketStream();

		bool Connect(const char* address, uint16_t port = 6379, double timeoutSeconds = -1.0);
		bool IsConnected(void);
		bool Disconnect(void);

		virtual bool ReadBuffer(uint8_t* buffer, uint32_t& bufferSize) override;
		virtual bool WriteBuffer(const uint8_t* buffer, uint32_t& bufferSize) override;

		volatile bool exitSignaled;

		std::string GetAddress() { return *this->address; }
		uint16_t GetPort() { return this->port; }

	protected:

		SOCKET socket;
		std::string* address;
		uint16_t port;
	};
}