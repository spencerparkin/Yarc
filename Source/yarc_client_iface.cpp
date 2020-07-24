#include "yarc_client_iface.h"
#include "yarc_protocol_data.h"

namespace Yarc
{
	ClientInterface::ClientInterface()
	{
		this->callbackMap = new CallbackMap();
	}

	/*virtual*/ ClientInterface::~ClientInterface()
	{
		delete this->callbackMap;
	}

	/*virtual*/ bool ClientInterface::MakeRequestSync(const ProtocolData* requestData, ProtocolData*& responseData, bool deleteData /*= true*/)
	{
		bool requestServiced = false;

		Callback callback = [&](const ProtocolData* givenResponseData) {
			responseData = const_cast<ProtocolData*>(givenResponseData);
			requestServiced = true;
			return false;
		};

		if (!this->MakeRequestAsync(requestData, callback, deleteData))
			return false;

		// Note that by blocking here, we ensure that we don't starve socket
		// threads that need to run for us to get the data from the server.
		while (!requestServiced && this->IsConnected())
			this->Update();

		return requestServiced;
	}

	/*virtual*/ bool ClientInterface::MakeTransactionRequestSync(DynamicArray<const ProtocolData*>& requestDataArray, ProtocolData*& responseData, bool deleteData /*= true*/)
	{
		bool requestServiced = false;

		Callback callback = [&](const ProtocolData* givenResponseData) {
			responseData = const_cast<ProtocolData*>(givenResponseData);
			requestServiced = true;
			return false;
		};

		if (!this->MakeTransactionRequestAsync(requestDataArray, callback, deleteData))
			return false;

		while (!requestServiced && this->IsConnected())
			this->Update();

		return requestServiced;
	}

	/*virtual*/ bool ClientInterface::RegisterSubscriptionCallback(const char* channel, Callback callback)
	{
		this->UnregisterSubscriptionCallback(channel);
		std::string key = channel;
		this->callbackMap->insert(std::pair<std::string, Callback>(key, callback));
		return true;
	}

	/*virtual*/ bool ClientInterface::UnregisterSubscriptionCallback(const char* channel)
	{
		std::string key = channel;
		CallbackMap::iterator iter = this->callbackMap->find(key);
		if (iter != this->callbackMap->end())
			this->callbackMap->erase(iter);

		return true;
	}

	/*virtual*/ bool ClientInterface::MessageHandler(const ProtocolData* messageData)
	{
		// TODO: Try to cast given message data to a PushData, then dispatch callback
		//       we find in our callbackMap member.

		return false;
	}
}