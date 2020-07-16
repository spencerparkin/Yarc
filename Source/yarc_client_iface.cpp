#include "yarc_client_iface.h"
#include "yarc_data_types.h"

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

	/*virtual*/ bool ClientInterface::MakeRequestSync(const DataType* requestData, DataType*& responseData)
	{
		bool requestServiced = false;

		Callback callback = [&](const DataType* dataType) {
			responseData = const_cast<DataType*>(dataType);
			requestServiced = true;
			return false;
		};

		if (!this->MakeRequestAsync(requestData, callback))
			return false;

		// Note that by blocking here, we ensure that we don't starve socket
		// threads that need to run for us to get the data from the server.
		while (!requestServiced && this->IsConnected())
			this->Update(true);

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

	/*virtual*/ bool ClientInterface::MessageHandler(const DataType* messageData)
	{
		const Array* messageDataArray = Cast<Array>(messageData);
		if (messageDataArray && messageDataArray->GetSize() >= 2)
		{
			const BulkString* stringData = Cast<BulkString>(messageDataArray->GetElement(1));
			if (stringData)
			{
				char buffer[512];
				stringData->GetString((uint8_t*)buffer, sizeof(buffer));
				std::string key = buffer;
				CallbackMap::iterator iter = this->callbackMap->find(key);
				if (iter != this->callbackMap->end())
				{
					Callback callback = iter->second;
					return callback(messageData);
				}
			}
		}

		return true;
	}
}