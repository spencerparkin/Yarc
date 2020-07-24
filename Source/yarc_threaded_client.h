#pragma once

#include "yarc_client_iface.h"
#include "yarc_reducer.h"

namespace Yarc
{
	// All other client derivatives are designed to potentially block when being updated.
	// This one will never do so, because it runs the given client on a thread where blocking I/O may be performed.
	class ThreadedClient : public ClientInterface
	{
	public:
		ThreadedClient(ClientInterface* givenClient);
		virtual ~ThreadedClient();

		virtual bool Connect(const char* address, uint16_t port = 6379, double timeoutSeconds = -1.0) override;
		virtual bool Disconnect() override;
		virtual bool IsConnected() override;
		virtual bool Update(void) override;	// This call will never block.
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