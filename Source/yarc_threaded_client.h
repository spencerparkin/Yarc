#pragma once

#include "yarc_client_iface.h"

namespace Yarc
{
	// Given a single-threaded client, this client will run it in
	// a dedicated thread while providing a thread-safe interface to it.
	// Given callbacks will always be executed in the main thread.
	// The cluster client is a good candidate for this class as its
	// processing is relatively heavier than other clients.
	class ThreadedClient : public ClientInterface
	{
	public:
		ThreadedClient(ClientInterface* givenClient);
		virtual ~ThreadedClient();

		virtual bool Connect(const char* address, uint16_t port = 6379, double timeoutSeconds = -1.0) override;
		virtual bool Disconnect() override;
		virtual bool IsConnected() override;
		virtual bool Update(bool canBlock = false) override;
		virtual bool Flush(void) override;
		virtual bool MakeRequestAsync(const ProtocolData* requestData, Callback callback = [](const ProtocolData*) -> bool { return true; }, bool deleteData = true) override;
		virtual bool MakeRequestSync(const ProtocolData* requestData, ProtocolData*& responseData, bool deleteData = true) override;
		virtual bool MakeTransactionRequestAsync(DynamicArray<const ProtocolData*>& requestDataArray, Callback callback = [](const ProtocolData*) -> bool { return true; }, bool deleteData = true) override;
		virtual bool MakeTransactionRequestSync(DynamicArray<const ProtocolData*>& requestDataArray, ProtocolData*& responseData, bool deleteData = true) override;
		virtual bool RegisterSubscriptionCallback(const char* channel, Callback callback) override;
		virtual bool UnregisterSubscriptionCallback(const char* channel) override;
		virtual bool MessageHandler(const ProtocolData* messageData) override;

	private:

		ClientInterface* client;

		// TODO: Own mutex for client.
		// TODO: Own mutices for send and receive lists.
		// TODO: Own thread handle here.  Can we be platform agnostic?
	};
}