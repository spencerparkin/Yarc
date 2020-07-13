#include "yarc_simple_client.h"
#include "yarc_data_types.h"
#include <assert.h>

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
		this->pendingTransactionList = new ReductionObjectList();
	}

	/*virtual*/ SimpleClient::~SimpleClient()
	{
		(void)this->Disconnect();
		delete[] this->buffer;
		delete this->callbackList;
		delete this->address;
		DeleteList<ReductionObject>(*this->pendingTransactionList);
		delete this->pendingTransactionList;
	}

	/*virtual*/ bool SimpleClient::Connect(const char* address, uint16_t port, uint32_t timeout /*= 30*/)
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

		*this->address = address;
		this->port = port;

		return true;
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
		ReductionObject::ReduceList(this->pendingTransactionList);

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

	/*virtual*/ bool SimpleClient::MakeRequestAsync(const DataType* requestData, Callback callback)
	{
		bool success = true;

		try
		{
			if (this->socket == INVALID_SOCKET)
				throw;

			uint8_t protocolData[10 * 1024];
			uint32_t protocolDataSize = sizeof(protocolData);

			if (!requestData->Print(protocolData, protocolDataSize))
				throw;

			this->callbackList->AddTail(callback);

			uint32_t i = 0;
			while (i < protocolDataSize)
			{
				uint32_t j = ::send(this->socket, (char*)& protocolData[i], protocolDataSize - i, 0);
				if (j == SOCKET_ERROR)
				{
					int error = ::WSAGetLastError();
					this->Disconnect();
					throw;
				}

				i += j;
			}
		}
		catch (...)
		{
			success = false;
		}

		delete requestData;

		return success;
	}

	/*virtual*/ bool SimpleClient::MakeTransactionRequestAsync(const DynamicArray<DataType*>& requestDataArray, Callback callback)
	{
		PendingTransaction* pendingTransaction = new PendingTransaction();
		pendingTransaction->callback = callback;
		pendingTransaction->queueCount = 0;

		if (this->MakeRequestAsync(DataType::ParseCommand("MULTI"), [=](const DataType* responseData) {
				if (!Cast<Error>(responseData))
					pendingTransaction->multiCommandOkay = true;
				return true;
			}))
		{
			// It's important to point out that while in typical asynchronous systems, requests are
			// not guarenteed to be fulfilled in the same order that they were made, that is not
			// the case here.  In other words, these commands will be executed on the database
			// server in the same order that they're given here.
			for (unsigned int i = 0; i < requestDataArray.GetCount(); i++)
			{
				if (this->MakeRequestAsync(requestDataArray[i], [=](const DataType* responseData) {
						if (!Cast<Error>(responseData))
							pendingTransaction->queueCount--;
						return true;
					}))
				{
					pendingTransaction->queueCount++;
				}
			}

			if (pendingTransaction->queueCount == requestDataArray.GetCount())
			{
				if (this->MakeRequestAsync(DataType::ParseCommand("EXEC"), [=](const DataType* responseData) {
						pendingTransaction->responseData = const_cast<DataType*>(responseData);
						return false;
					}))
				{
					this->pendingTransactionList->AddTail(pendingTransaction);
					return true;
				}
			}
		}

		delete pendingTransaction;
		return false;
	}

	//------------------------------ PendingTransaction ------------------------------

	SimpleClient::PendingTransaction::PendingTransaction()
	{
		this->multiCommandOkay = false;
		this->queueCount = -1;
		this->responseData = nullptr;
	}

	/*virtual*/ SimpleClient::PendingTransaction::~PendingTransaction()
	{
		delete this->responseData;
	}

	/*virtual*/ ReductionObject::ReductionResult SimpleClient::PendingTransaction::Reduce()
	{
		if (this->multiCommandOkay && this->queueCount == 0 && this->responseData)
		{
			if (!this->callback(this->responseData))
				this->responseData = nullptr;

			return RESULT_DELETE;
		}

		return RESULT_NONE;
	}
}