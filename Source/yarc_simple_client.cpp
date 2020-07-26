#include "yarc_simple_client.h"
#include "yarc_protocol_data.h"
#include "yarc_misc.h"

namespace Yarc
{
	//------------------------------ SimpleClient ------------------------------

	SimpleClient::SimpleClient(ConnectionConfig* givenConnectionConfig /*= nullptr*/) : ClientInterface(givenConnectionConfig)
	{
		this->socketStream = new SocketStream();
		this->callbackList = new CallbackList();
		this->serverDataList = new ProtocolDataList();
		this->threadHandle = nullptr;
	}

	/*virtual*/ SimpleClient::~SimpleClient()
	{
		this->ShutDownSocketConnectionAndThread();
		delete this->socketStream;
		delete this->callbackList;
		this->serverDataList->Delete();
		delete this->serverDataList;
	}

	/*static*/ SimpleClient* SimpleClient::Create()
	{
		return new SimpleClient();
	}

	/*static*/ void SimpleClient::Destroy(SimpleClient* client)
	{
		delete client;
	}

	/*virtual*/ bool SimpleClient::SetupSocketConnectionAndThread(void)
	{
		bool success = true;

		try
		{
			if (!this->socketStream->IsConnected())
			{
				std::string address = this->connectionConfig->GetResolvedIPAddress();
				uint16_t port = this->connectionConfig->port;
				double timeoutSeconds = this->connectionConfig->connectionTimeoutSeconds;

				if (!this->socketStream->Connect(address.c_str(), port, timeoutSeconds))
					throw new InternalException();
			}

			if (!this->threadHandle)
			{
				this->threadHandle = ::CreateThread(nullptr, 0, &SimpleClient::ThreadMain, this, 0, nullptr);
				if (!this->threadHandle)
					throw new InternalException();
			}
		}
		catch (InternalException* exc)
		{
			delete exc;
			success = false;
			this->ShutDownSocketConnectionAndThread();
		}

		return success;
	}

	/*virtual*/ bool SimpleClient::ShutDownSocketConnectionAndThread(void)
	{
		if (this->socketStream->IsConnected())
			this->socketStream->Disconnect();

		if (this->threadHandle)
		{
			::WaitForSingleObject(this->threadHandle, INFINITE);
			this->threadHandle = nullptr;
		}

		return !this->socketStream->IsConnected() && this->threadHandle == nullptr;
	}

	/*virtual*/ bool SimpleClient::Update(void)
	{
		switch(this->connectionConfig->disposition)
		{
			case ConnectionConfig::Disposition::NORMAL:
			{
				break;
			}
			case ConnectionConfig::Disposition::LAZY:
			{
				clock_t lastSocketReadWriteTime = this->socketStream->GetLastSocketReadWriteTime();
				clock_t currentTime = ::clock();
				clock_t socketIdleTime = currentTime - lastSocketReadWriteTime;
				double socketIdleTimeSeconds = double(socketIdleTime) / double(CLOCKS_PER_SEC);
				if (socketIdleTimeSeconds >= this->connectionConfig->maxConnectionIdleTimeSeconds)
				{
					this->ShutDownSocketConnectionAndThread();
				}
				
				break;
			}
			case ConnectionConfig::Disposition::PERSISTENT:
			{
				if (!this->socketStream->IsConnected())
				{
					this->SetupSocketConnectionAndThread();
				}

				break;
			}
		}

		while (this->serverDataList->GetCount() > 0)
		{
			ProtocolData* serverData = this->serverDataList->RemoveHead();
			
			bool deleteServerData = true;

			ProtocolData* messageData = Cast<PushData>(serverData);
			if (!messageData)
			{
				// For backwards compatibility with RESP1, here we check for the old message structure.
				const ArrayData* arrayData = Cast<ArrayData>(serverData);
				if (arrayData && arrayData->GetCount() > 0)
				{
					const BlobStringData* blobStringData = Cast<BlobStringData>(arrayData->GetElement(0));
					if (blobStringData && blobStringData->GetValue() == "messsage")
						messageData = serverData;
				}
			}

			if (messageData)
			{
				if (*this->pushDataCallback)
					deleteServerData = (*this->pushDataCallback)(messageData);
			}
			else
			{
				Callback callback = this->DequeueCallback();
				if (callback)
					deleteServerData = callback(serverData);
			}

			if (deleteServerData)
				delete serverData;
		}

		return true;
	}

	/*static*/ DWORD __stdcall SimpleClient::ThreadMain(LPVOID param)
	{
		SimpleClient* client = (SimpleClient*)param;
		return client->ThreadFunc();
	}

	DWORD SimpleClient::ThreadFunc(void)
	{
		while (this->socketStream->IsConnected())
		{
			ProtocolData* serverData = nullptr;
			if (!ProtocolData::ParseTree(this->socketStream, serverData))
				break;

			if (serverData)
				this->serverDataList->AddTail(serverData);
		}

		return 0;
	}

	/*virtual*/ bool SimpleClient::Flush(void)
	{
		while (this->callbackList->GetCount() > 0)
			if (!this->Update())
				return false;

		return true;
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
		if (!this->socketStream->IsConnected())
			if (!this->SetupSocketConnectionAndThread())
				return false;

		if (!ProtocolData::PrintTree(this->socketStream, requestData))
			return false;

		this->EnqueueCallback(callback);
		return true;
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
}