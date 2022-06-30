#include "yarc_simple_client.h"
#include "yarc_protocol_data.h"
#include "yarc_connection_pool.h"
#include "yarc_misc.h"

namespace Yarc
{
	//------------------------------ SimpleClient ------------------------------

	SimpleClient::SimpleClient(double connectionTimeoutSeconds /*= 0.5*/, double connectionRetrySeconds /*= 5.0*/, bool tryToRecycleConnection /*= true*/) :
		unsentSemaphore(MAXINT32),
		servedSemaphore(MAXINT32),
		messageSemaphore(MAXINT32)
	{
		this->connectionTimeoutSeconds = connectionTimeoutSeconds;
		this->connectionRetrySeconds = connectionRetrySeconds;
		this->lastFailedConnectionAttemptTime = 0;
		this->tryToRecycleConnection = tryToRecycleConnection;
		this->socketStream = nullptr;
		this->requestList = new ReductionObjectList();
		this->messageList = new ReductionObjectList();
		this->thread = nullptr;
		this->postConnectCallback = new EventCallback;
		this->preDisconnectCallback = new EventCallback;
		this->threadExitSignal = false;
	}

	/*virtual*/ SimpleClient::~SimpleClient()
	{
		if (this->tryToRecycleConnection)
		{
			if (this->thread && this->thread->IsStillRunning() && this->socketStream && this->socketStream->IsConnected())
			{
				if (this->Flush(5.0))
				{
					if (this->thread && this->thread->IsStillRunning() && this->socketStream && this->socketStream->IsConnected())
					{
						this->threadExitSignal = true;

						// Here the reception thread should exit without us having to close the socket.
						ProtocolData* responseData = nullptr;
						if (this->MakeRequestSync(ProtocolData::ParseCommand("PING"), responseData))
						{
							delete responseData;
							GetConnectionPool()->CheckinSocketStream(this->socketStream);
							this->socketStream = nullptr;
						}
					}
				}
			}
		}
		
		if (this->socketStream)
		{
			// This should cause our reception thread to exit.
			this->socketStream->Disconnect();
			delete this->socketStream;
		}

		if (this->thread)
		{
			this->thread->WaitForThreadExit();
			delete this->thread;
		}
		
		DeleteList<ReductionObject*>(*this->requestList);
		DeleteList<ReductionObject*>(*this->messageList);
		delete this->requestList;
		delete this->messageList;
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

	/*virtual*/ bool SimpleClient::Update(double semaphoreTimeoutSeconds /*= 0.0*/)
	{
		// Are we still waiting between connection retry attempts?
		if (this->lastFailedConnectionAttemptTime != 0)
		{
			::clock_t currentTime = ::clock();
			double elapsedTimeSeconds = (double(currentTime) - double(this->lastFailedConnectionAttemptTime)) / double(CLOCKS_PER_SEC);
			if (elapsedTimeSeconds >= this->connectionRetrySeconds)
				this->lastFailedConnectionAttemptTime = 0;
			else
				return false;
		}

		// Make sure we have a connection to the Redis database.
		if (!this->socketStream)
		{
			this->socketStream = GetConnectionPool()->CheckoutSocketStream(this->address, this->connectionTimeoutSeconds);
			if (!this->socketStream)
			{
				this->lastFailedConnectionAttemptTime = ::clock();
				return false;
			}

			if (this->socketStream->IsConnected() && *this->postConnectCallback)
				(*this->postConnectCallback)(this);
		}
		
		// If we lose our connection, we can't continue or recycle our connection in the pool.
		if (!this->socketStream->IsConnected())
		{
			if (this->thread && this->thread->IsStillRunning())
			{
				this->thread->WaitForThreadExit();
				delete this->thread;
				this->thread = nullptr;
			}

			delete this->socketStream;
			this->socketStream = nullptr;

			// We must also purge our current request list since it has become invalid.
			DeleteList<ReductionObject*>(*this->requestList);

			return false;
		}

		// Make sure our reception thread is running.
		if (!this->thread)
		{
			this->thread = new Thread();
			if (!this->thread->SpawnThread([=]() { this->ThreadFunc(); }))
			{
				delete this->thread;
				this->thread = nullptr;
				return false;
			}
		}

		// The thread will exit if the server closes its connection.
		if (!this->thread->IsStillRunning())
		{
			delete this->thread;
			this->thread = nullptr;
			return false;
		}
		
		// Don't eat up CPU time if there is nothing for us to do.  This is especially important
		// during a flush operation so that we're not busy-waiting.  Doing so can starve the very
		// threads for which we are busy-waiting.
		Semaphore* semaphoreArray[] = { &this->unsentSemaphore, &this->servedSemaphore, &this->messageSemaphore };
		Semaphore::DecrementMulti(3, semaphoreArray, semaphoreTimeoutSeconds * 1000.0, false);

		// We're in business!  Make a pass on the request list.
		if(this->requestList->GetCount() > 0)
		{
			MutexLocker locker(this->requestListMutex);
			ReductionObject::ReduceList(this->requestList, this);
		}

		// And make a pass on the message list as well.
		if(this->messageList->GetCount() > 0)
		{
			MutexLocker locker(this->messageListMutex);
			ReductionObject::ReduceList(this->messageList, this);
		}

		return true;
	}

	void SimpleClient::ThreadFunc(void)
	{
		while (this->socketStream->IsConnected() && !this->threadExitSignal)
		{
			// Here we block on the socket until woken up.
			ProtocolData* serverData = nullptr;
			if (!ProtocolData::ParseTree(this->socketStream, serverData))
				break;

			if (serverData)
			{
				// Whenever we get something from the server, we have to determine if
				// it's a response to a request or data that has been pushed to us without
				// a request having first been sent for the it.
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
					MutexLocker locker(this->messageListMutex);
					Message* message = new Message();
					message->messageData = messageData;
					this->messageList->AddTail(message);
					this->messageSemaphore.Increment();
				}
				else
				{
					// In the usual case, the server data is a response to the next pending request.
					MutexLocker locker(this->requestListMutex);
					bool foundRequest = false;
					for (ReductionObjectList::Node* node = this->requestList->GetHead(); node; node = node->GetNext())
					{
						Request* request = static_cast<Request*>(node->value);
						if (request->state == Request::State::SENT)
						{
							request->responseData = serverData;
							request->state = Request::State::SERVED;
							this->servedSemaphore.Increment();
							foundRequest = true;
							break;
						}
					}

					// This shouldn't happen, but if it does, we need to handle it!
					if (!foundRequest)
						delete serverData;
				}
			}
		}
	}

	/*virtual*/ bool SimpleClient::Flush(double timeoutSeconds /*= 0.0*/)
	{
		clock_t startTime = ::clock();

		while (this->requestList->GetCount() > 0)
		{
			this->Update(3.0);

			if (timeoutSeconds > 0.0)
			{
				clock_t currentTime = ::clock();
				double elapsedTimeSeconds = double(currentTime - startTime) / double(CLOCKS_PER_SEC);
				if (elapsedTimeSeconds >= timeoutSeconds)
					return false;
			}
		}

		return true;
	}

	/*virtual*/ int SimpleClient::MakeRequestAsync(const ProtocolData* requestData, Callback callback /*= [](const ProtocolData*) -> bool { return true; }*/, bool deleteData /*= true*/)
	{
		Request* request = new Request();
		request->requestData = requestData;
		request->ownsRequestDataMem = deleteData;
		request->callback = callback;
		request->state = Request::State::UNSENT;

		MutexLocker locker(this->requestListMutex);
		this->requestList->AddTail(request);
		this->unsentSemaphore.Increment();

		return request->requestID;
	}

	/*virtual*/ bool SimpleClient::CancelAsyncRequest(int requestID)
	{
		MutexLocker locker(this->requestListMutex);

		for (ReductionObjectList::Node* node = this->requestList->GetHead(); node; node = node->GetNext())
		{
			Request* request = static_cast<Request*>(node->value);
			if (request->requestID == requestID)
			{
				if (request->state == Request::State::UNSENT)
				{
					delete request;
					this->requestList->Remove(node);
				}
				else
				{
					request->callback = [](const ProtocolData*) -> bool { return true; };
				}

				return true;
			}
		}

		return false;
	}

	/*virtual*/ bool SimpleClient::MakeTransactionRequestAsync(DynamicArray<const ProtocolData*>& requestDataArray, Callback callback /*= [](const ProtocolData*) -> bool { return true; }*/, bool deleteData /*= true*/)
	{
		uint32_t i = 0;

		auto lambda = [&]() -> bool
		{
			this->MakeRequestAsync(ProtocolData::ParseCommand("MULTI"), [=](const ProtocolData* responseData) { return true; });

			// It's important to point out that while in typical asynchronous systems, requests are
			// not guarenteed to be fulfilled in the same order that they were made, that is not
			// the case here.  In other words, these commands will be executed on the database
			// server in the same order that they're given here.
			while(i < requestDataArray.GetCount())
			{
				this->MakeRequestAsync(requestDataArray[i++], [=](const ProtocolData* responseData) {
					// Note that we don't need to worry if there was an error queueing the command.
					// The server will remember the error, and discard the transaction when EXEC is called.
					return true;
				});
			}

			this->MakeRequestAsync(ProtocolData::ParseCommand("EXEC"), callback);
			return true;
		};

		bool success = lambda();

		if (deleteData)
			while (i < requestDataArray.GetCount())
				delete requestDataArray[i++];

		return success;
	}

	/*virtual*/ bool SimpleClient::MakeTransactionRequestSync(DynamicArray<const ProtocolData*>& requestDataArray, ProtocolData*& responseData, bool deleteData /*= true*/, double timeoutSeconds /*= 5.0*/)
	{
		uint32_t i = 0;

		auto lambda = [&]() -> bool
		{
			if (!this->MakeRequestSync(ProtocolData::ParseCommand("EXEC"), responseData, timeoutSeconds))
				return false;

			if (!Cast<SimpleErrorData>(responseData))
			{
				delete responseData;
				
				while (i < requestDataArray.GetCount())
				{
					if (!this->MakeRequestSync(requestDataArray[i], responseData, deleteData, timeoutSeconds))
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
					if (!this->MakeRequestSync(ProtocolData::ParseCommand("EXEC"), responseData, true, timeoutSeconds))
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

	//------------------------------ SimpleClient::Request ------------------------------

	int SimpleClient::Request::nextRequestID = 0;

	SimpleClient::Request::Request()
	{
		this->requestID = this->nextRequestID++;
		this->requestData = nullptr;
		this->responseData = nullptr;
		this->ownsRequestDataMem = false;
		this->ownsResponseDataMem = false;
		this->state = State::UNSENT;
	}

	/*virtual*/ SimpleClient::Request::~Request()
	{
		if (this->ownsRequestDataMem)
			delete this->requestData;

		if (this->ownsResponseDataMem)
			delete this->responseData;
	}

	/*virtual*/ ReductionObject::ReductionResult SimpleClient::Request::Reduce(void* userData)
	{
		SimpleClient* client = static_cast<SimpleClient*>(userData);

		switch (this->state)
		{
			case State::UNSENT:
			{
				// Send the request over the socket.
				if (!ProtocolData::PrintTree(client->socketStream, this->requestData))
				{
					// We must bail here, because requests can't be sent out of order.
					return ReductionResult::RESULT_BAIL;
				}

				this->state = State::SENT;
				return ReductionResult::RESULT_NONE;
			}
			case State::SENT:
			{
				// Nothing we can do here but wait.  The reception thread advances this state of the request.
				return ReductionResult::RESULT_NONE;
			}
			case State::SERVED:
			{
				// Simply dispatch the response to the user's callback.
				this->ownsResponseDataMem = this->callback(this->responseData);
				return ReductionResult::RESULT_DELETE;
			}
		}

		return ReductionResult::RESULT_NONE;
	}

	//------------------------------ SimpleClient::Request ------------------------------

	SimpleClient::Message::Message()
	{
		this->messageData = nullptr;
		this->ownsMessageData = false;
	}

	/*virtual*/ SimpleClient::Message::~Message()
	{
		if (this->ownsMessageData)
			delete this->messageData;
	}

	/*virtual*/ ReductionObject::ReductionResult SimpleClient::Message::Reduce(void* userData)
	{
		SimpleClient* client = static_cast<SimpleClient*>(userData);
		
		if (*client->pushDataCallback)
			this->ownsMessageData = (*client->pushDataCallback)(this->messageData);
		else
			this->ownsMessageData = true;

		return ReductionResult::RESULT_DELETE;
	}
}