#include "yarc_simple_client.h"
#include "yarc_data_types.h"
#include "yarc_misc.h"
#include <assert.h>
#include <time.h>

#pragma comment(lib, "Ws2_32.lib")

namespace Yarc
{
	//------------------------------ SimpleClient ------------------------------

	SimpleClient::SimpleClient()
	{
		this->socket = INVALID_SOCKET;
		this->bufferSize = 10 * 1024;
		this->buffer = new uint8_t[this->bufferSize];
		this->bufferReadOffset = 0;
		this->bufferParseOffset = 0;
		this->callbackList = new CallbackList();
		this->address = new std::string();
		this->port = 0;
		this->pendingRequestFlushPoint = 10000;
		this->updateCallCount = 0;
	}

	/*virtual*/ SimpleClient::~SimpleClient()
	{
		(void)this->Disconnect();
		delete[] this->buffer;
		delete this->callbackList;
		delete this->address;
	}

	/*static*/ SimpleClient* SimpleClient::Create()
	{
		return new SimpleClient();
	}

	/*static*/ void SimpleClient::Destroy(SimpleClient* client)
	{
		delete client;
	}

	/*virtual*/ bool SimpleClient::Connect(const char* address, uint16_t port /*= 6379*/, double timeoutSeconds /*= -1.0*/)
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

				result = ::connect(this->socket, (SOCKADDR*)& sockaddr, sizeof(sockaddr));
				if (result != SOCKET_ERROR)
					throw new InternalException();

				int error = ::WSAGetLastError();
				if (error != WSAEWOULDBLOCK)
					throw new InternalException();

				double startTime = double(::clock()) / double(CLOCKS_PER_SEC);
				double elapsedTime = 0.0f;
				while (elapsedTime < timeoutSeconds)
				{
					fd_set write_set, exc_set;
					FD_ZERO(&write_set);
					FD_ZERO(&exc_set);
					FD_SET(this->socket, &write_set);
					FD_SET(this->socket, &exc_set);

					timeval tval;
					tval.tv_sec = 0;
					tval.tv_usec = 1000;

					int32_t count = ::select(0, NULL, &write_set, &exc_set, &tval);
					if (count == SOCKET_ERROR)
						throw new InternalException();

					if (FD_ISSET(this->socket, &exc_set))
						throw new InternalException();

					// Is the socket writable?
					if (FD_ISSET(this->socket, &write_set))
						break;

					double currentTime = double(::clock()) / double(CLOCKS_PER_SEC);
					elapsedTime = currentTime - startTime;
				}

				if (elapsedTime >= timeoutSeconds)
					throw new InternalException();

				arg = 0;
				result = ::ioctlsocket(this->socket, FIONBIO, &arg);
				if (result != NO_ERROR)
					throw new InternalException();
			}

			*this->address = address;
			this->port = port;
		}
		catch (InternalException * exc)
		{
			success = false;
			this->Disconnect();
			delete exc;
		}
		
		return success;
	}

	/*virtual*/ bool SimpleClient::Disconnect()
	{
		if (this->socket == INVALID_SOCKET)
			return false;

		::closesocket(this->socket);
		this->socket = INVALID_SOCKET;

		return true;
	}

	/*virtual*/ bool SimpleClient::Update(bool canBlock /*= false*/)
	{
		RecursionGuard recursionGuard(&this->updateCallCount);
		if (recursionGuard.IsRecursing())
			return false;

		bool processedServerData = false;
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

			timeval tval;
			tval.tv_sec = 0;
			tval.tv_usec = 0;

			int32_t count = ::select(0, &read_set, NULL, &exc_set, &tval);
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

			// Parse and dispatch as much as we can based on what we have.
			while (this->bufferParseOffset < this->bufferReadOffset)
			{
				const uint8_t* protocolData = &this->buffer[this->bufferParseOffset];
				uint32_t protocolDataSize = this->bufferSize - this->bufferParseOffset;

				bool validStart = true;
				DataType* serverData = DataType::ParseTree(protocolData, protocolDataSize, &validStart);
				assert(validStart);
				if (!serverData)
					break;	// A parse error is not a bug here.  It just means we haven't yet read enough data from the socket.
				else
				{
					processedServerData = true;
					bool freeServerData = true;

					this->bufferParseOffset += protocolDataSize;
					assert(this->bufferParseOffset <= this->bufferReadOffset);

					ServerDataKind serverDataKind = this->ClassifyServerData(serverData);
					switch (serverDataKind)
					{
						case ServerDataKind::MESSAGE:
						{
							freeServerData = this->MessageHandler(serverData);
							break;
						}
						case ServerDataKind::RESPONSE:
						{
							if (this->callbackList->GetCount() > 0)
							{
								Callback callback = this->callbackList->GetHead()->value;
								this->callbackList->Remove(this->callbackList->GetHead());
								freeServerData = callback(serverData);
							}

							break;
						}
					}

					if (freeServerData)
						delete serverData;
				}
			}
		}

		return processedServerData;
	}

	SimpleClient::ServerDataKind SimpleClient::ClassifyServerData(const DataType* serverData)
	{
		const Array* serverDataArray = Cast<Array>(serverData);
		if (serverDataArray && serverDataArray->GetSize() > 0)
		{
			const BulkString* stringData = Cast<BulkString>(serverDataArray->GetElement(0));
			if (stringData)
			{
				char buffer[512];
				stringData->GetString((uint8_t*)buffer, sizeof(buffer));
				if (strcmp(buffer, "message") == 0)
					return ServerDataKind::MESSAGE;
			}
		}

		return ServerDataKind::RESPONSE;
	}

	/*virtual*/ bool SimpleClient::Flush(void)
	{
		while (this->callbackList->GetCount() > 0 && this->IsConnected())
			if (!this->Update(true))
				break;

		return this->IsConnected();
	}

	/*virtual*/ bool SimpleClient::MakeRequestAsync(const DataType* requestData, Callback callback, bool deleteData /*= true*/)
	{
		bool success = true;
		uint32_t protocolDataSize = 1024 * 1024;
		uint8_t* protocolData = nullptr;

		try
		{
			if (!this->IsConnected())
				throw new InternalException();

			// Internally, we check the size of our callback-list, and if it gets too big, we force a flush
			// operation before sending any more requests to the server.  This prevents the server from needing
			// to queue up too much memory before it's able to send responses.
			if (this->callbackList->GetCount() > this->pendingRequestFlushPoint)
				if (!this->Flush())
					throw new InternalException();

			protocolData = new uint8_t[protocolDataSize];
			if (!protocolData)
				throw new InternalException();

			if (!requestData->Print(protocolData, protocolDataSize))
				throw new InternalException();

			uint32_t i = 0;
			while (i < protocolDataSize)
			{
				uint32_t j = ::send(this->socket, (char*)& protocolData[i], protocolDataSize - i, 0);
				if (j == SOCKET_ERROR)
				{
					int error = ::WSAGetLastError();
					this->Disconnect();
					throw new InternalException();
				}

				i += j;
			}

			this->callbackList->AddTail(callback);
		}
		catch (InternalException* exc)
		{
			success = false;
			delete exc;
		}

		if(deleteData)
			delete requestData;

		delete[] protocolData;

		return success;
	}

	/*virtual*/ bool SimpleClient::MakeTransactionRequestAsync(DynamicArray<const DataType*>& requestDataArray, Callback callback, bool deleteData /*= true*/)
	{
		bool success = true;
		uint32_t i = 0;

		try
		{
			if (!this->MakeRequestAsync(DataType::ParseCommand("MULTI"), [=](const DataType* responseData) { return true; }))
				throw new InternalException();

			// It's important to point out that while in typical asynchronous systems, requests are
			// not guarenteed to be fulfilled in the same order that they were made, that is not
			// the case here.  In other words, these commands will be executed on the database
			// server in the same order that they're given here.
			while(i < requestDataArray.GetCount())
			{
				if (!this->MakeRequestAsync(requestDataArray[i++], [=](const DataType* responseData) {
					// Note that we don't need to worry if there was an error queueing the command.
					// The server will remember the error, and discard the transaction when EXEC is called.
					return true;
				}))
				{
					throw new InternalException();
				}
			}

			if (!this->MakeRequestAsync(DataType::ParseCommand("EXEC"), callback))
				throw new InternalException();
		}
		catch (InternalException* exc)
		{
			delete exc;
			success = false;
		}

		if (deleteData)
			while (i < requestDataArray.GetCount())
				delete requestDataArray[i++];

		return success;
	}

	/*virtual*/ bool SimpleClient::MakeTransactionRequestSync(DynamicArray<const DataType*>& requestDataArray, DataType*& responseData, bool deleteData /*= true*/)
	{
		bool success = true;
		uint32_t i = 0;

		try
		{
			if (!this->MakeRequestSync(DataType::ParseCommand("EXEC"), responseData))
				throw new InternalException();

			if (!Cast<Error>(responseData))
			{
				delete responseData;

				while (i < requestDataArray.GetCount())
				{
					if (!this->MakeRequestSync(requestDataArray[i], responseData, deleteData))
						throw new InternalException();

					if (!Cast<Error>(responseData))
						delete responseData;
					else
					{
						this->MakeRequestAsync(DataType::ParseCommand("DISCARD"), [](const DataType*) { return true; });
						break;
					}
				}

				if (i == requestDataArray.GetCount())
				{
					if (!this->MakeRequestSync(DataType::ParseCommand("EXEC"), responseData))
						throw new InternalException();
				}
			}
		}
		catch (InternalException* exc)
		{
			delete exc;
			success = false;
		}

		if (deleteData)
			while (i < requestDataArray.GetCount())
				delete requestDataArray[i++];

		return success;
	}
}