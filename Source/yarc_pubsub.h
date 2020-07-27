#pragma once

#include "yarc_simple_client.h"
#include <map>

namespace Yarc
{
	// Redis doesn't let you handle pub-sub on a single connection; otherwise,
	// this interface might have been part of the standard client interface.
	// Anyhow, it is provided here for convenience in working with the pub-sub mechanism.
	class YARC_API PubSub
	{
	public:

		PubSub();
		virtual ~PubSub();

		ConnectionConfig connectionConfig;

		static PubSub* Create(void);
		static void Destroy(PubSub* pubSub);

		bool Subscribe(const std::string& channel, ClientInterface::Callback callback);
		bool Unsubscribe(const std::string& channel);

		// Here we always take ownership of the given data.  It will get freed,
		// so the caller should consider their pointer stale after the call.
		bool Publish(const std::string& channel, ProtocolData* publishData);

		// These overloads are provided for convenience.
		bool Publish(const std::string& channel, const uint8_t* buffer, uint32_t bufferSize);
		bool Publish(const std::string& channel, const std::string& message);

		bool Update(void);

	private:

		bool Resubscribe(void);

		bool DispatchPubSubMessage(const ProtocolData* messageData);

		SimpleClient* inputClient;
		SimpleClient* outputClient;

		typedef std::map<std::string, ClientInterface::Callback> CallbackMap;
		CallbackMap* callbackMap;
	};
}