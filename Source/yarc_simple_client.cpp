#include "yarc_simple_client.h"
#include "yarc_protocol_data.h"
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
		this->callbackList = new CallbackList();
		this->address = new std::string();
		this->port = 0;
		this->serverResultList = new ReductionObjectList;
	}

	/*virtual*/ SimpleClient::~SimpleClient()
	{
		(void)this->Disconnect();
		delete this->callbackList;
		delete this->address;
		DeleteList<ReductionObject>(*this->serverResultList);
		delete this->serverResultList;
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
		catch (InternalException* exc)
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
		// TODO: Write this.

		// If we are going to incidentally be recursive, we can at least control
		// where that happens by processing server results here.  Specifically, our
		// API may get called within a callback.  We want that to be okay in
		// ideally all cases, but it does make me nervous.
		ReductionObject::ReduceList(this->serverResultList);

		return true;
	}

	SimpleClient::ServerResult* SimpleClient::ClassifyServerData(const ProtocolData* serverData)
	{
		// TODO: If given data casts as PushData, return a new ServerMessageResult() class.

		return new ServerResponseResult(this, serverData, this->DequeueCallback());
	}

	/*virtual*/ bool SimpleClient::Flush(void)
	{
		while (this->callbackList->GetCount() > 0 && this->IsConnected())
			if (!this->Update(true))
				break;

		return this->IsConnected();
	}

	void SimpleClient::EnqueueCallback(Callback callback)
	{
		this->callbackList->AddTail(callback);
	}

	SimpleClient::Callback SimpleClient::DequeueCallback()
	{
		Callback callback = this->callbackList->GetHead()->value;
		this->callbackList->Remove(this->callbackList->GetHead());
		return callback;
	}

	/*virtual*/ bool SimpleClient::MakeRequestAsync(const ProtocolData* requestData, Callback callback /*= [](const ProtocolData*) -> bool { return true; }*/, bool deleteData /*= true*/)
	{
		// TODO: Write this.
		return false;
	}

	/*virtual*/ bool SimpleClient::MakeTransactionRequestAsync(DynamicArray<const ProtocolData*>& requestDataArray, Callback callback /*= [](const ProtocolData*) -> bool { return true; }*/, bool deleteData /*= true*/)
	{
		bool success = true;
		uint32_t i = 0;

		try
		{
			if (!this->MakeRequestAsync(ProtocolData::ParseCommand("MULTI"), [=](const ProtocolData* responseData) { return true; }))
				throw new InternalException();

			// It's important to point out that while in typical asynchronous systems, requests are
			// not guarenteed to be fulfilled in the same order that they were made, that is not
			// the case here.  In other words, these commands will be executed on the database
			// server in the same order that they're given here.
			while(i < requestDataArray.GetCount())
			{
				if (!this->MakeRequestAsync(requestDataArray[i++], [=](const ProtocolData* responseData) {
					// Note that we don't need to worry if there was an error queueing the command.
					// The server will remember the error, and discard the transaction when EXEC is called.
					return true;
				}))
				{
					throw new InternalException();
				}
			}

			if (!this->MakeRequestAsync(ProtocolData::ParseCommand("EXEC"), callback))
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

	/*virtual*/ bool SimpleClient::MakeTransactionRequestSync(DynamicArray<const ProtocolData*>& requestDataArray, ProtocolData*& responseData, bool deleteData /*= true*/)
	{
		bool success = true;
		uint32_t i = 0;

		try
		{
			if (!this->MakeRequestSync(ProtocolData::ParseCommand("EXEC"), responseData))
				throw new InternalException();

			if (!Cast<SimpleErrorData>(responseData))
			{
				delete responseData;

				while (i < requestDataArray.GetCount())
				{
					if (!this->MakeRequestSync(requestDataArray[i], responseData, deleteData))
						throw new InternalException();

					if (!Cast<SimpleErrorData>(responseData))
						delete responseData;
					else
					{
						this->MakeRequestAsync(ProtocolData::ParseCommand("DISCARD"), [](const ProtocolData*) { return true; });
						break;
					}
				}

				if (i == requestDataArray.GetCount())
				{
					if (!this->MakeRequestSync(ProtocolData::ParseCommand("EXEC"), responseData))
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

	//------------------------------ SimpleClient::ServerResult ------------------------------

	SimpleClient::ServerResult::ServerResult(SimpleClient* givenClient, const ProtocolData* givenServerData)
	{
		this->client = givenClient;
		this->serverData = givenServerData;
	}

	/*virtual*/ SimpleClient::ServerResult::~ServerResult()
	{
		delete this->serverData;
	}

	//------------------------------ SimpleClient::ServerResponseResult ------------------------------

	SimpleClient::ServerResponseResult::ServerResponseResult(SimpleClient* givenClient, const ProtocolData* givenServerData, Callback givenCallback) : ServerResult(givenClient, givenServerData)
	{
		this->callback = givenCallback;
	}

	/*virtual*/ SimpleClient::ServerResponseResult::~ServerResponseResult()
	{
	}

	ReductionObject::ReductionResult SimpleClient::ServerResponseResult::Reduce()
	{
		bool freeServerData = this->callback(serverData);
		if (!freeServerData)
			this->serverData = nullptr;		// The callback took ownership of the memory.

		return RESULT_DELETE;
	}

	//------------------------------ SimpleClient::ServerMessageResponse ------------------------------

	SimpleClient::ServerMessageResult::ServerMessageResult(SimpleClient* givenClient, const ProtocolData* givenServerData) : ServerResult(givenClient, givenServerData)
	{
	}

	/*virtual*/ SimpleClient::ServerMessageResult::~ServerMessageResult()
	{
	}

	ReductionObject::ReductionResult SimpleClient::ServerMessageResult::Reduce()
	{
		this->client->MessageHandler(this->serverData);
		return RESULT_DELETE;
	}
}

