#pragma once

#include <stdint.h>
#include <WS2tcpip.h>
#include <string>

namespace Yarc
{
	class ByteStream
	{
	public:

		ByteStream();
		virtual ~ByteStream();

		// Read from the stream into the given buffer of the given size.
		// The size is modified to reflect the number of bytes read.
		virtual bool ReadBuffer(uint8_t* buffer, uint32_t& bufferSize) { return false; }

		// Write to the stream from the given buffer of the given size.
		// The size is modified to relfect the number of bytes written.
		virtual bool WriteBuffer(const uint8_t* buffer, uint32_t& bufferSize) { return false; }

		// Read a byte from the stream into the given byte.
		virtual bool ReadByte(uint8_t& byte);

		// Write a byte to the stream from the given byte.
		virtual bool WriteByte(uint8_t byte);
	};

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