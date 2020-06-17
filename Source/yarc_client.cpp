#include "yarc_client.h"
#include "yarc_data_types.h"
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
		this->callbackList = new CallbackList();
		this->fallbackCallback = new Callback;
	}

	/*virtual*/ Client::~Client()
	{
		(void)this->Disconnect();
		delete[] this->buffer;
		delete this->callbackList;
		delete this->fallbackCallback;
	}

	bool Client::Connect(const char* address, uint16_t port, uint32_t timeout /*= 30*/)
	{
		if (this->socket != INVALID_SOCKET)
			return false;

		WSADATA data;
		int result = ::WSAStartup(MAKEWORD(2, 2), &data);
		if (result != 0)
			return false;

		this->socket = ::socket(AF_INET, SOCK_STREAM, 0);

		sockaddr_in sockaddr;
		sockaddr.sin_family = AF_INET;
		::InetPtonA(sockaddr.sin_family, address, &sockaddr.sin_addr);
		sockaddr.sin_port = ::htons(port);

		result = ::connect(this->socket, (SOCKADDR*)&sockaddr, sizeof(sockaddr));
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
		bool processedResponse = false;
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
			if (count == SOCKET_ERROR)
			{
				int error = ::WSAGetLastError();
				this->Disconnect();
				return false;
			}

			this->bufferReadOffset += count;

			const uint8_t* protocolData = &this->buffer[this->bufferParseOffset];
			uint32_t protocolDataSize = this->bufferSize - this->bufferParseOffset;

			// Note that it's not an error to fail to parse here.  If that happens, it means
			// we have not yet read enough data from the socket stream to be parseable.
			bool validStart = true;
			DataType* dataType = DataType::ParseTree(protocolData, protocolDataSize, &validStart);
			assert(validStart);
			if (dataType)
			{
				processedResponse = true;

				this->bufferParseOffset += protocolDataSize;

				Callback callback = *this->fallbackCallback;

				if (this->callbackList->GetCount() > 0)
				{
					callback = this->callbackList->GetHead()->value;
					this->callbackList->Remove(this->callbackList->GetHead());
				}

				bool freeDataType = true;
				if (callback)
					freeDataType = callback(dataType);

				if (freeDataType)
					delete dataType;
			}

			assert(this->bufferParseOffset <= this->bufferReadOffset);
		}

		return processedResponse;
	}

	bool Client::MakeRequestAsync(const DataType* requestData, Callback callback)
	{
		if (this->socket == INVALID_SOCKET)
			return false;

		uint8_t protocolData[10 * 1024];
		uint32_t protocolDataSize = sizeof(protocolData);
		
		if (!requestData->Print(protocolData, protocolDataSize))
			return false;

		this->callbackList->AddTail(callback);

		uint32_t i = 0;
		while (i < protocolDataSize)
		{
			uint32_t j = ::send(this->socket, (char*)&protocolData[i], protocolDataSize - i, 0);
			if (j == SOCKET_ERROR)
			{
				int error = ::WSAGetLastError();
				this->Disconnect();
				return false;
			}

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
		while (!requestServiced && this->IsConnected())
			this->Update(true);

		return requestServiced;
	}
}