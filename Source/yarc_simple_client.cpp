#include "yarc_simple_client.h"
#include "yarc_protocol_data.h"
#include "yarc_connection_pool.h"
#include "yarc_misc.h"

namespace Yarc
{
	//------------------------------ SimpleClient ------------------------------

	SimpleClient::SimpleClient()
	{
		this->socketStream = nullptr;
		this->callbackList = new CallbackList();
		this->serverDataList = new ProtocolDataList();
		this->thread = nullptr;
		this->postConnectCallback = new EventCallback;
		this->preDisconnectCallback = new EventCallback;
		this->threadExitSignal = false;
	}

	/*virtual*/ SimpleClient::~SimpleClient()
	{
		this->ShutDownSocketConnectionAndThread();
		delete this->thread;
		delete this->callbackList;
		this->serverDataList->Delete();
		delete this->serverDataList;
		delete this->postConnectCallback;
		delete this->preDisconnectCallback;
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
		auto lambda = [&]() -> bool
		{
			this->callbackList->RemoveAll();

			if (!this->socketStream)
			{
				this->socketStream = GetConnectionPool()->CheckoutSocketStream(this->address);
				if (!this->socketStream || !this->socketStream->IsConnected())
					return false;
			}

			if (!this->thread)
			{
				this->threadExitSignal = false;
				this->thread = new Thread();
				if(!this->thread->SpawnThread([=, this]() { this->ThreadFunc(); }))
					return false;
			}

			if (*this->postConnectCallback)
				(*this->postConnectCallback)(this);
			
			return true;
		};

		bool success = lambda();
		
		if(!success)
			this->ShutDownSocketConnectionAndThread();

		return success;
	}

	/*virtual*/ bool SimpleClient::ShutDownSocketConnectionAndThread(void)
	{
		if (*this->preDisconnectCallback)
		{
			if(this->socketStream && this->socketStream->IsConnected() && this->thread != nullptr)
				(*this->preDisconnectCallback)(this);
		}

		bool canRecycleConnection = false;

		if (this->thread)
		{
			// If the socket is disconnected/closed for any reason, the blocking I/O will fail, and the thread will exit.
			// So that's one way to signal the thread to exit, if we need to do it that way.  Ideally, however, what we'll do is
			// simply set a flag, send a PING, get a PONG, and then exit based on that flag.  This not only accomplishes
			// the thread exit cleanly, but it also lets us recycle the connection in the connection pool with the
			// connection resting at a proper protocol boundary, both for sending and receiving protocol data.
			if (!this->socketStream)
			{
				// If our socket stream pointer is null, then the thread would crash anyway.  We should never get here ever.
				this->thread->KillThread();
			}
			else
			{
				if (this->socketStream->IsConnected())
				{
					this->threadExitSignal = true;
					ProtocolData* responseData = nullptr;
					if (!this->MakeRequestSync(ProtocolData::ParseCommand("PING"), responseData))
						this->socketStream->Disconnect();
					else
					{
						delete responseData;
						canRecycleConnection = true;
					}
				}

				this->thread->WaitForThreadExit();
			}

			delete this->thread;
			this->thread = nullptr;
		}

		if (this->socketStream)
		{
			if (canRecycleConnection)
				GetConnectionPool()->CheckinSocketStream(this->socketStream);
			else
				delete this->socketStream;

			this->socketStream = nullptr;
		}

		this->callbackList->RemoveAll();

		return true;
	}

	void SimpleClient::SetPostConnectCallback(EventCallback givenCallback)
	{
		*this->postConnectCallback = givenCallback;
	}

	void SimpleClient::SetPreDisconnectCallback(EventCallback givenCallback)
	{
		*this->preDisconnectCallback = givenCallback;
	}

	SimpleClient::EventCallback SimpleClient::GetPostConnectCallback(void)
	{
		return *this->postConnectCallback;
	}

	SimpleClient::EventCallback SimpleClient::GetPreDisconnectCallback(void)
	{
		return *this->preDisconnectCallback;
	}

	/*virtual*/ bool SimpleClient::Update(void)
	{
		if (!this->socketStream || !this->socketStream->IsConnected())
			return false;

		if (!this->thread || !this->thread->IsStillRunning())
			return false;

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
					if (blobStringData && blobStringData->GetValue() == "message")
						messageData = serverData;
				}
			}

			if (messageData)
			{
				if (*this->pushDataCallback)
					deleteServerData = (*this->pushDataCallback)(messageData);
			}
			else if (this->callbackList->GetCount() > 0)
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

	void SimpleClient::ThreadFunc(void)
	{
		while (this->socketStream->IsConnected())
		{
			ProtocolData* serverData = nullptr;
			if (!ProtocolData::ParseTree(this->socketStream, serverData))
				break;

			if (serverData)
				this->serverDataList->AddTail(serverData);

			if (this->threadExitSignal)
				break;
		}
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
		auto lambda = [&]() -> bool
		{
			if (!this->socketStream)
				if (!this->SetupSocketConnectionAndThread())
					return false;

			if (!ProtocolData::PrintTree(this->socketStream, requestData))
				return false;

			this->EnqueueCallback(callback);
			return true;
		};
		
		bool success = lambda();

		if (deleteData)
			delete requestData;

		return success;
	}

	/*virtual*/ bool SimpleClient::MakeTransactionRequestAsync(DynamicArray<const ProtocolData*>& requestDataArray, Callback callback /*= [](const ProtocolData*) -> bool { return true; }*/, bool deleteData /*= true*/)
	{
		uint32_t i = 0;

		auto lambda = [&]() -> bool
		{
			if (!this->MakeRequestAsync(ProtocolData::ParseCommand("MULTI"), [=](const ProtocolData* responseData) { return true; }))
				return false;

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
					return false;
				}
			}

			if (!this->MakeRequestAsync(ProtocolData::ParseCommand("EXEC"), callback))
				return false;
			
			return true;
		};

		bool success = lambda();

		if (deleteData)
			while (i < requestDataArray.GetCount())
				delete requestDataArray[i++];

		return success;
	}

	/*virtual*/ bool SimpleClient::MakeTransactionRequestSync(DynamicArray<const ProtocolData*>& requestDataArray, ProtocolData*& responseData, bool deleteData /*= true*/)
	{
		uint32_t i = 0;

		auto lambda = [&]() -> bool
		{
			if (!this->MakeRequestSync(ProtocolData::ParseCommand("EXEC"), responseData))
				return false;

			if (!Cast<SimpleErrorData>(responseData))
			{
				delete responseData;
				
				while (i < requestDataArray.GetCount())
				{
					if (!this->MakeRequestSync(requestDataArray[i], responseData, deleteData))
						return false;

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
						return false;
				}
			}

			return true;
		};

		bool success = lambda();

		if (deleteData)
			while (i < requestDataArray.GetCount())
				delete requestDataArray[i++];

		return success;
	}
}