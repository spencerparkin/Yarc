#include "yarc_pubsub.h"
#include "yarc_protocol_data.h"

namespace Yarc
{
	/*static*/ PubSub* PubSub::Create(void)
	{
		return new PubSub();
	}

	/*static*/ void PubSub::Destroy(PubSub* pubSub)
	{
		delete pubSub;
	}

	PubSub::PubSub(void)
	{
		this->callbackMap = new CallbackMap;
		this->inputClient = new SimpleClient();
		this->outputClient = new SimpleClient();

		this->inputClient->RegisterPushDataCallback([this](const ProtocolData* messageData) -> bool {
			return this->DispatchPubSubMessage(messageData);
		});

		this->inputClient->SetPostConnectCallback([this](SimpleClient*) -> bool {
			return this->Resubscribe();
		});
	}

	/*virtual*/ PubSub::~PubSub()
	{
		delete this->inputClient;
		delete this->outputClient;
		delete this->callbackMap;
	}

	void PubSub::SetAddress(const Address& address)
	{
		this->inputClient->address = address;
		this->outputClient->address = address;
	}

	void PubSub::SetAddress(const char* address)
	{
		this->inputClient->address.SetIPAddress(address);
		this->outputClient->address.SetIPAddress(address);
	}

	bool PubSub::Subscribe(const std::string& channel, ClientInterface::Callback callback)
	{
		return this->Subscribe(channel.c_str(), callback);
	}

	bool PubSub::Subscribe(const char* channel, ClientInterface::Callback callback)
	{
		ProtocolData* responseData = nullptr;
		if (!this->inputClient->MakeRequestSync(ProtocolData::ParseCommand("SUBSCRIBE %s", channel), responseData))
			return false;

		// TODO: Make sure we got +OK from the server.

		CallbackMap::iterator iter = this->callbackMap->find(channel);
		if (iter != this->callbackMap->end())
			this->callbackMap->erase(iter);

		this->callbackMap->insert(std::pair<std::string, ClientInterface::Callback>(channel, callback));
		return true;
	}

	bool PubSub::Unsubscribe(const std::string& channel)
	{
		return this->Unsubscribe(channel.c_str());
	}

	bool PubSub::Unsubscribe(const char* channel)
	{
		CallbackMap::iterator iter = this->callbackMap->find(channel);
		if (iter != this->callbackMap->end())
			this->callbackMap->erase(iter);
		else
			return false;

		ProtocolData* responseData = nullptr;
		if (!this->inputClient->MakeRequestSync(ProtocolData::ParseCommand("UNSUBSCRIBE %s", channel), responseData))
			return false;

		// TODO: Make sure we got +OK from the server.

		return true;
	}

	bool PubSub::Publish(const std::string& channel, ProtocolData* publishData)
	{
		return this->Publish(channel.c_str(), publishData);
	}

	bool PubSub::Publish(const char* channel, ProtocolData* publishData)
	{
		ArrayData* commandData = new ArrayData();
		commandData->SetCount(3);
		commandData->SetElement(0, new BlobStringData("PUBLISH"));
		commandData->SetElement(1, new BlobStringData(channel));
		commandData->SetElement(2, publishData);
		return this->outputClient->MakeRequestAsync(commandData);
	}

	bool PubSub::Publish(const std::string& channel, const uint8_t* buffer, uint32_t bufferSize)
	{
		return this->Publish(channel, new BlobStringData(buffer, bufferSize));
	}

	bool PubSub::Publish(const char* channel, const uint8_t* buffer, uint32_t bufferSize)
	{
		return this->Publish(channel, new BlobStringData(buffer, bufferSize));
	}

	bool PubSub::Publish(const std::string& channel, const std::string& message)
	{
		return this->Publish(channel, new BlobStringData(message));
	}

	bool PubSub::Publish(const char* channel, const char* message)
	{
		return this->Publish(channel, new BlobStringData(message));
	}

	bool PubSub::DispatchPubSubMessage(const ProtocolData* messageData)
	{
		bool deleteData = true;

		const PushData* pushData = Cast<PushData>(messageData);
		if (pushData)
		{
			// TODO: Handle RESP3 message format here.
		}
		else
		{
			const ArrayData* arrayData = Cast<ArrayData>(messageData);
			if (arrayData && arrayData->GetCount() >= 2)
			{
				const BlobStringData* channelData = Cast<BlobStringData>(arrayData->GetElement(1));
				if (channelData)
				{
					std::string channel = channelData->GetValue();
					CallbackMap::iterator iter = this->callbackMap->find(channel);
					if (iter != this->callbackMap->end())
					{
						ClientInterface::Callback callback = iter->second;
						deleteData = callback(messageData);
					}
				}
			}
		}

		return deleteData;
	}

	bool PubSub::Update(void)
	{
		this->inputClient->Update();
		this->outputClient->Update();
		return true;
	}

	bool PubSub::Resubscribe(void)
	{
		for (CallbackMap::iterator iter = this->callbackMap->begin(); iter != this->callbackMap->end(); iter++)
		{
			const std::string& channel = iter->first;
			if (!this->inputClient->MakeRequestAsync(ProtocolData::ParseCommand("SUBSCRIBE %s", channel.c_str())))
				return false;
		}

		this->inputClient->Flush();
		return true;
	}

	// For one of my use-cases, this function cannot be made inline,
	// because however the compiler impliments it, it gives the wrong
	// answer, which is quite odd.
	uint32_t PubSub::GetSubscriptionCount()
	{
		return (uint32_t)this->callbackMap->size();
	}
}