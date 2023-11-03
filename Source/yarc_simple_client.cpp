#include "yarc_simple_client.h"
#include "yarc_protocol_data.h"
#include "yarc_connection_pool.h"
#include "yarc_misc.h"

namespace Yarc
{
	//------------------------------ SimpleClient ------------------------------

	SimpleClient::SimpleClient(double connectionTimeoutSeconds /*= 0.5*/, double connectionRetrySeconds /*= 5.0*/) : servedRequestListSemaphore(MAXINT32)
	{
		this->numRequestsInFlight = 0;
		this->connectionTimeoutSeconds = connectionTimeoutSeconds;
		this->connectionRetrySeconds = connectionRetrySeconds;
		this->lastFailedConnectionAttemptTime = 0;
		this->socketStream = nullptr;
		this->thread = nullptr;
		this->unsentRequestList = new RequestList();
		this->sentRequestList = new RequestList();
		this->servedRequestList = new RequestList();
		this->messageList = new MessageList();
		this->postConnectCallback = new EventCallback;
		this->preDisconnectCallback = new EventCallback;
		this->threadExitSignal = false;
	}

	/*virtual*/ SimpleClient::~SimpleClient()
	{
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
		
		this->unsentRequestList->Delete();
		this->sentRequestList->Delete();
		this->servedRequestList->Delete();
		this->messageList->Delete();

		delete this->unsentRequestList;
		delete this->sentRequestList;
		delete this->servedRequestList;
		delete this->messageList;
		delete this->postConnectCallback;
		delete this->preDisconnectCallback;
	}

	/*static*/ SimpleClient* SimpleClient::Create()
	{
		return new SimpleClient();
	}

	/*static*/ void SimpleClient::Destroy(SimpleClient* client, bool tryToRecycleConnection /*= false*/)
	{
		if (tryToRecycleConnection)
			client->TryToRecycleConnection();

		delete client;
	}

	void SimpleClient::TryToRecycleConnection()
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
						ConnectionPool::Get()->CheckinSocketStream(this->socketStream);
						this->socketStream = nullptr;
					}
				}
			}
		}
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

	/*virtual*/ bool SimpleClient::Update(double timeoutMilliseconds /*= 0.0*/)
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
			this->socketStream = ConnectionPool::Get()->CheckoutSocketStream(this->address, this->connectionTimeoutSeconds);
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

			// We must also purge our current list of sent requests since it has become invalid.
			this->sentRequestList->Delete();

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

		// Flush all pending unsent requests.
		while (true)
		{
			Request* request = this->unsentRequestList->RemoveHead();
			if (!request)
				break;
			
			// Notice that we must add it to the sent list before printing it to the socket,
			// because it's possible for the server to respond before it gets there, and the
			// reception thread needs it to be there to match the request with the response.
			this->sentRequestList->AddTail(request);
			ProtocolData::PrintTree(this->socketStream, request->requestData);
		}

		// Serve pending requests for as long as they're coming off the queue.
		while (this->numRequestsInFlight > 0)
		{
			Request* request = nullptr;

			// The typical time-out here is zero milliseconds so that a call to Update() is as fast as possible.
			if (!this->servedRequestListSemaphore.Decrement(timeoutMilliseconds))
				break;		// There is nothing to serve right now, so bail out.
			else
			{
				request = this->servedRequestList->RemoveHead();
				//assert(request != nullptr);
			}
			
			if (request)
			{
				request->ownsResponseDataMem = request->callback(request->responseData);
				this->DeallocRequest(request);
			}
		}

		// Flush all pending messages.
		while (true)
		{
			Message* message = this->messageList->RemoveHead();
			if (!message)
				break;
			
			if (*this->pushDataCallback)
				message->ownsMessageData = (*this->pushDataCallback)(message->messageData);
			else
				message->ownsMessageData = true;

			delete message;
		}

		return true;
	}

	SimpleClient::Request* SimpleClient::AllocRequest()
	{
		this->numRequestsInFlight++;
		return new Request();
	}

	void SimpleClient::DeallocRequest(Request* request)
	{
		delete request;
		this->numRequestsInFlight--;
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
				// a request having first been sent for it.
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
					Message* message = new Message();
					message->messageData = messageData;
					this->messageList->AddTail(message);
				}
				else
				{
					// In the usual case, the server data is a response to the next pending request.
					Request* request = this->sentRequestList->RemoveHead();
					if (!request)
					{
						// This *should* never happen.
						//assert(false);
					}
					else
					{
						// Assign the payload and send it on its way!
						request->responseData = serverData;
						this->servedRequestList->AddTail(request);
						this->servedRequestListSemaphore.Increment();
					}
				}
			}
		}
	}

	/*virtual*/ bool SimpleClient::Flush(double timeoutSeconds /*= 5.0*/)
	{
		clock_t startTime = ::clock();

		while (this->numRequestsInFlight > 0)
		{
			// This will block on a semaphore so that we're not exactly busy-waiting.
			this->Update(timeoutSeconds * 1000.0);

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

	// Note that it should be safe to call this from any thread.
	/*virtual*/ int SimpleClient::MakeRequestAsync(const ProtocolData* requestData, Callback callback /*= [](const ProtocolData*) -> bool { return true; }*/, bool deleteData /*= true*/)
	{
		Request* request = this->AllocRequest();
		request->requestData = requestData;
		request->ownsRequestDataMem = deleteData;
		request->callback = callback;

		this->unsentRequestList->AddTail(request);

		return request->requestID;
	}

	/*virtual*/ bool SimpleClient::CancelAsyncRequest(int requestID)
	{
		auto predicate = [=](Request* request) {
			return request->requestID == requestID;
		};

		// There is a very small chance that a request in flight fails to be
		// canceled because at the time of our checks, it was between lists.
		// Therefore, make two passes; wait a bit before making the second pass.
		// This doesn't necessarily solve the problem, but it makes me feel better.
		for (int i = 0; i < 2; i++)
		{
			Request* request = this->unsentRequestList->Find(predicate, nullptr, true);
			if (request)
			{
				this->DeallocRequest(request);
				return true;
			}

			request = this->sentRequestList->Find(predicate, nullptr, false);
			if (request)
			{
				request->callback = [](const ProtocolData*) -> bool { return true; };
				return true;
			}

			request = this->servedRequestList->Find(predicate, nullptr, false);
			if (request)
			{
				request->callback = [](const ProtocolData*) -> bool { return true; };
				return true;
			}

			Thread::Sleep(100);
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
	}

	/*virtual*/ SimpleClient::Request::~Request()
	{
		if (this->ownsRequestDataMem)
			delete this->requestData;

		if (this->ownsResponseDataMem)
			delete this->responseData;
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
}