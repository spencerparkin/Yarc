#pragma once

#include "yarc_client_iface.h"
#include "yarc_linked_list.h"
#include <WS2tcpip.h>
#include <Windows.h>

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
		virtual void SignalThreadExit(void) override;

		ClientInterface* GetClient() { return this->client; }

	private:

		static DWORD __stdcall ThreadMain(LPVOID param);

		DWORD ThreadFunc(void);

		// We want to own a client here rather than inherit from one,
		// because we want to be able to run any blocking client in a thread.
		ClientInterface* client;

		bool threadExitSignaled;
		HANDLE threadHandle;

		// Urf...
	};
}