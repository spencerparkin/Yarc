#pragma once

#include "yarc_api.h"
#include "yarc_client_iface.h"
#include "yarc_linked_list.h"
#include "yarc_reducer.h"
#include "yarc_byte_stream.h"
#include <stdint.h>
#include <WS2tcpip.h>
#include <string>

namespace Yarc
{
	// TODO: Add connection dispositions: normal, persistent or lazy.
	//       The lazy disposition would make scaling Redis cluster to many nodes more practical.
	//       Note this may mean removing the connect, disconnect and is-connected functions.
	class YARC_API SimpleClient : public ClientInterface
	{
	public:

		SimpleClient();
		virtual ~SimpleClient();

		// When used as a DLL, these ensure that the client is allocated and freed in the proper heap.
		static SimpleClient* Create();
		static void Destroy(SimpleClient* client);

		virtual bool Connect(const char* address, uint16_t port = 6379, double timeoutSeconds = -1.0) override;
		virtual bool Disconnect() override;
		virtual bool IsConnected() override;
		virtual bool Update(void) override;
		virtual bool Flush(void) override;
		virtual bool MakeRequestAsync(const ProtocolData* requestData, Callback callback = [](const ProtocolData*) -> bool { return true; }, bool deleteData = true) override;
		virtual bool MakeTransactionRequestAsync(DynamicArray<const ProtocolData*>& requestDataArray, Callback callback = [](const ProtocolData*) -> bool { return true; }, bool deleteData = true) override;
		virtual bool MakeTransactionRequestSync(DynamicArray<const ProtocolData*>& requestDataArray, ProtocolData*& responseData, bool deleteData = true) override;

		SocketStream* GetSocketStream() { return this->socketStream; }

	protected:

		typedef LinkedList<Callback> CallbackList;
		CallbackList* callbackList;

		void EnqueueCallback(Callback callback);
		Callback DequeueCallback();

		SocketStream* socketStream;
	};
}