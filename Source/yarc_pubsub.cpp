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
		this->subscriptionMap = new SubscriptionMap;
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
		delete this->subscriptionMap;
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
		SubscriptionMap::iterator iter = this->subscriptionMap->find(channel);
		if (iter != this->subscriptionMap->end())
			this->subscriptionMap->erase(iter);

		SubscriptionData subData;
		subData.callback = callback;
		subData.subscribed = false;
		this->subscriptionMap->insert(std::pair<std::string, SubscriptionData>(channel, subData));
		return true;
	}

	bool PubSub::Unsubscribe(const std::string& channel)
	{
		return this->Unsubscribe(channel.c_str());
	}

	bool PubSub::Unsubscribe(const char* channel)
	{
		SubscriptionMap::iterator iter = this->subscriptionMap->find(channel);
		if (iter != this->subscriptionMap->end())
			this->subscriptionMap->erase(iter);
		else
			return false;

		ProtocolData* responseData = nullptr;
		if (!this->inputClient->MakeRequestSync(ProtocolData::ParseCommand("UNSUBSCRIBE %s", channel), responseData))
			return false;

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
					SubscriptionMap::iterator iter = this->subscriptionMap->find(channel);
					if (iter != this->subscriptionMap->end())
					{
						ClientInterface::Callback callback = iter->second.callback;
						deleteData = callback(messageData);
					}
				}
			}
		}

		return deleteData;
	}

	bool PubSub::Update(void)
	{
		for (SubscriptionMap::iterator iter = this->subscriptionMap->begin(); iter != this->subscriptionMap->end(); iter++)
		{
			SubscriptionData& subData = iter->second;
			if (!subData.subscribed)
			{
				std::string channel = iter->first;
				this->inputClient->MakeRequestAsync(ProtocolData::ParseCommand("SUBSCRIBE %s", channel.c_str()));
				subData.subscribed = true;
			}
		}

		this->inputClient->Update();
		this->outputClient->Update();
		return true;
	}

	bool PubSub::Resubscribe(void)
	{
		for (SubscriptionMap::iterator iter = this->subscriptionMap->begin(); iter != this->subscriptionMap->end(); iter++)
		{
			SubscriptionData& subData = iter->second;
			subData.subscribed = false;
		}

		return true;
	}
}