#pragma once

#include "yarc_api.h"
#include "yarc_client_iface.h"

namespace Yarc
{
#if false		// TODO: Revisit this code once simple client is working with RESP3

	class PersistentClient;

	// This is a use-case of the simple-client that is common-enough that
	// it deserves its own native support in the client library.  This class
	// maintains two connections to the same Redis database; one that can
	// receive messages; another that can send messages.  The Redis protocol
	// itself doesn't allow this with a single client context.  The class
	// also continually tries to re-connect to the database in the case that
	// it loses its connections.  The main use-case here is for pub-sub.
	class YARC_API PersistentDualClient : public ClientInterface
	{
	public:

		PersistentDualClient();
		virtual ~PersistentDualClient();

		// When used as a DLL, these ensure that the client is allocated and freed in the proper heap.
		static PersistentDualClient* Create();
		static void Destroy(PersistentDualClient* client);

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

		PersistentClient* inputClient;
		PersistentClient* outputClient;
	};
#endif
}