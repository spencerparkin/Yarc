#include "yarc_threaded_client.h"

namespace Yarc
{
	ThreadedClient::ThreadedClient(ClientInterface* givenClient)
	{
		this->client = givenClient;
		this->threadHandle = nullptr;
		this->threadExitSignaled = false;
	}

	/*virtual*/ ThreadedClient::~ThreadedClient()
	{
	}

	/*virtual*/ bool ThreadedClient::Connect(const char* address, uint16_t port /*= 6379*/, double timeoutSeconds /*= -1.0*/)
	{
		if (this->client->IsConnected())
			return false;

		if (!this->client->Connect(address, port, timeoutSeconds))
			return false;

		if (this->threadHandle == nullptr)
		{
			this->threadExitSignaled = false;
			this->threadHandle = ::CreateThread(nullptr, 0, &ThreadedClient::ThreadMain, this, 0, nullptr);
			if (this->threadHandle == nullptr)
			{
				this->client->Disconnect();
				return false;
			}
		}

		return true;
	}

	/*virtual*/ bool ThreadedClient::Disconnect()
	{
		if (this->threadHandle != nullptr)
		{
			this->SignalThreadExit();
			::WaitForSingleObject(this->threadHandle, INFINITE);
			this->threadHandle = nullptr;
		}

		this->client->Disconnect();

		return false;
	}

	/*virtual*/ void ThreadedClient::SignalThreadExit(void)
	{
		this->client->SignalThreadExit();
		this->threadExitSignaled = true;
	}

	/*virtual*/ bool ThreadedClient::IsConnected()
	{
		return this->client->IsConnected();
	}

	/*virtual*/ bool ThreadedClient::Update(void)
	{
		return true;
	}

	/*static*/ DWORD __stdcall ThreadedClient::ThreadMain(LPVOID param)
	{
		ThreadedClient* client = (ThreadedClient*)param;
		return client->ThreadFunc();
	}

	DWORD ThreadedClient::ThreadFunc(void)
	{
		while (!this->threadExitSignaled)
		{
			if (!this->client->Update())
				break;

			//...
		}

		return 0;
	}

	/*virtual*/ bool ThreadedClient::Flush(void)
	{
		return false;
	}

	/*virtual*/ bool ThreadedClient::MakeRequestAsync(const ProtocolData* requestData, Callback callback /*= [](const ProtocolData*) -> bool { return true; }*/, bool deleteData /*= true*/)
	{
		return false;
	}

	/*virtual*/ bool ThreadedClient::MakeRequestSync(const ProtocolData* requestData, ProtocolData*& responseData, bool deleteData /*= true*/)
	{
		return false;
	}

	/*virtual*/ bool ThreadedClient::MakeTransactionRequestAsync(DynamicArray<const ProtocolData*>& requestDataArray, Callback callback /*= [](const ProtocolData*) -> bool { return true; }*/, bool deleteData /*= true*/)
	{
		return false;
	}

	/*virtual*/ bool ThreadedClient::MakeTransactionRequestSync(DynamicArray<const ProtocolData*>& requestDataArray, ProtocolData*& responseData, bool deleteData /*= true*/)
	{
		return false;
	}

	/*virtual*/ bool ThreadedClient::RegisterSubscriptionCallback(const char* channel, Callback callback)
	{
		return false;
	}

	/*virtual*/ bool ThreadedClient::UnregisterSubscriptionCallback(const char* channel)
	{
		return false;
	}

	/*virtual*/ bool ThreadedClient::MessageHandler(const ProtocolData* messageData)
	{
		return false;
	}
}