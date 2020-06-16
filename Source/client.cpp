#include "client.h"
#include "data_types.h"
#include <assert.h>

#pragma comment(lib, "Ws2_32.lib")

namespace Yarc
{
	Client::Client()
	{
		this->socket = INVALID_SOCKET;
		this->bufferSize = 10 * 1024;
		this->buffer = new uint8_t[this->bufferSize];
		this->bufferReadOffset = 0;
		this->bufferParseOffset = 0;
	}

	/*virtual*/ Client::~Client()
	{
		delete[] this->buffer;
	}

	bool Client::Connect(const char* address, uint16_t port, uint32_t timeout /*= 30*/)
	{
		if (this->socket != INVALID_SOCKET)
			return false;

		this->socket = ::socket(AF_INET, SOCK_STREAM, 0);

		sockaddr_in sockaddr;
		sockaddr.sin_family = AF_INET;
		::InetPtonA(sockaddr.sin_family, address, &sockaddr.sin_addr);
		sockaddr.sin_port = ::htons(port);

		int result = ::connect(this->socket, (SOCKADDR*)&sockaddr, sizeof(sockaddr));
		if (result == SOCKET_ERROR)
			return false;

		return true;
	}

	bool Client::Disconnect()
	{
		if (this->socket == INVALID_SOCKET)
			return false;

		::closesocket(this->socket);
		this->socket = INVALID_SOCKET;

		return true;
	}

	bool Client::Update(bool canBlock /*= false*/)
	{
		bool tryRead = false;

		if (canBlock)
			tryRead = true;
		else
		{
			fd_set read_set, exc_set;
			FD_ZERO(&read_set);
			FD_ZERO(&exc_set);
			FD_SET(this->socket, &read_set);
			FD_SET(this->socket, &exc_set);

			timeval timeout;
			timeout.tv_sec = 0;
			timeout.tv_usec = 0;

			int32_t count = ::select(0, &read_set, NULL, &exc_set, &timeout);
			assert(count != SOCKET_ERROR);
			assert(!FD_ISSET(this->socket, &exc_set));

			// Does the socket have data for us to read?
			if (FD_ISSET(this->socket, &read_set))
				tryRead = true;
		}

		if (tryRead)
		{
			// Do we have room to receive the data?
			if (this->bufferReadOffset == this->bufferSize)
			{
				if (this->bufferParseOffset > 0)
				{
					for (uint32_t i = this->bufferParseOffset; i < this->bufferSize; i++)
						this->buffer[i - this->bufferParseOffset] = this->buffer[i];

					this->bufferReadOffset -= this->bufferParseOffset;
					this->bufferParseOffset = 0;
				}
				else
				{
					uint32_t newBufferSize = this->bufferSize * 2;
					uint8_t* newBuffer = new uint8_t[newBufferSize];
					
					for (uint32_t i = 0; i < this->bufferSize; i++)
						newBuffer[i] = this->buffer[i];

					this->buffer = newBuffer;
					this->bufferSize = newBufferSize;
				}
			}

			uint32_t count = ::recv(this->socket, (char*)&this->buffer[this->bufferReadOffset], this->bufferSize - this->bufferReadOffset, 0);
			this->bufferReadOffset += count;

			const uint8_t* protocolData = &this->buffer[this->bufferParseOffset];
			uint32_t protocolDataSize = this->bufferSize - this->bufferParseOffset;

			DataType* dataType = DataType::ParseTree(protocolData, protocolDataSize);
			if (dataType)
			{
				Callback callback = this->fallbackCallback;

				if (this->callbackList.GetCount() > 0)
				{
					callback = this->callbackList.GetHead()->value;
					this->callbackList.Remove(this->callbackList.GetHead());
				}

				bool freeDataType = true;
				if (callback)
					freeDataType = callback(dataType);

				if (freeDataType)
					delete dataType;
			}
		}

		return true;
	}

	bool Client::MakeRequestAsync(const DataType* requestData, Callback callback)
	{
		if (this->socket == INVALID_SOCKET)
			return false;

		uint8_t protocolData[10 * 1024];
		uint32_t protocolDataSize = sizeof(protocolData);
		
		if (!requestData->Print(protocolData, protocolDataSize))
			return false;

		this->callbackList.AddTail(callback);

		uint32_t i = 0;
		while (i < protocolDataSize)
		{
			uint32_t j = ::send(this->socket, (char*)&protocolData[i], protocolDataSize - i, 0);
			if (j == SOCKET_ERROR)
				return false;

			i += j;
		}

		return true;
	}

	bool Client::MakeReqeustSync(const DataType* requestData, DataType*& responseData)
	{
		bool requestServiced = false;

		Callback callback = [&](const DataType* dataType) {
			responseData = const_cast<DataType*>(dataType);
			requestServiced = true;
			return false;
		};

		if (!this->MakeRequestAsync(requestData, callback))
			return false;

		// Note that by blocking here, we ensure that we don't starve socket
		// threads that need to run for us to get the data from the server.
		while (!requestServiced)
			this->Update(true);

		return true;
	}
}