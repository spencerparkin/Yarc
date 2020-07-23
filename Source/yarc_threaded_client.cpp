#include "yarc_threaded_client.h"

namespace Yarc
{
	ThreadedClient::ThreadedClient(ClientInterface* givenClient)
	{
		this->client = givenClient;
	}

	/*virtual*/ ThreadedClient::~ThreadedClient()
	{
	}

	/*virtual*/ bool ThreadedClient::Connect(const char* address, uint16_t port /*= 6379*/, double timeoutSeconds /*= -1.0*/)
	{
		return false;
	}

	/*virtual*/ bool ThreadedClient::Disconnect()
	{
		return false;
	}

	/*virtual*/ bool ThreadedClient::IsConnected()
	{
		return false;
	}

	/*virtual*/ bool ThreadedClient::Update(bool canBlock /*= false*/)
	{
		return false;
	}

	/*virtual*/ bool ThreadedClient::Flush(void)
	{
		return false;
	}

	/*virtual*/ bool ThreadedClient::MakeRequestAsync(const DataType* requestData, Callback callback /*= [](const DataType*) -> bool { return true; }*/, bool deleteData /*= true*/)
	{
		return false;
	}

	/*virtual*/ bool ThreadedClient::MakeRequestSync(const DataType* requestData, DataType*& responseData, bool deleteData /*= true*/)
	{
		return false;
	}

	/*virtual*/ bool ThreadedClient::MakeTransactionRequestAsync(DynamicArray<const DataType*>& requestDataArray, Callback callback /*= [](const DataType*) -> bool { return true; }*/, bool deleteData /*= true*/)
	{
		return false;
	}

	/*virtual*/ bool ThreadedClient::MakeTransactionRequestSync(DynamicArray<const DataType*>& requestDataArray, DataType*& responseData, bool deleteData /*= true*/)
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

	/*virtual*/ bool ThreadedClient::MessageHandler(const DataType* messageData)
	{
		return false;
	}
}