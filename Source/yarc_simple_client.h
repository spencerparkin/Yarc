#pragma once

#include "yarc_api.h"
#include "yarc_thread.h"
#include "yarc_client_iface.h"
#include "yarc_linked_list.h"
#include "yarc_socket_stream.h"
#include "yarc_thread_safe_list.h"
#include <stdint.h>
#include <string>

namespace Yarc
{
	class YARC_API SimpleClient : public ClientInterface
	{
	public:

		SimpleClient();
		virtual ~SimpleClient();

		// When used as a DLL, these ensure that the client is allocated and freed in the proper heap.
		static SimpleClient* Create();
		static void Destroy(SimpleClient* client);

		virtual bool Update(void) override;
		virtual bool Flush(void) override;
		virtual bool MakeRequestAsync(const ProtocolData* requestData, Callback callback = [](const ProtocolData*) -> bool { return true; }, bool deleteData = true) override;
		virtual bool MakeTransactionRequestAsync(DynamicArray<const ProtocolData*>& requestDataArray, Callback callback = [](const ProtocolData*) -> bool { return true; }, bool deleteData = true) override;
		virtual bool MakeTransactionRequestSync(DynamicArray<const ProtocolData*>& requestDataArray, ProtocolData*& responseData, bool deleteData = true) override;

		SocketStream* GetSocketStream() { return this->socketStream; }

		typedef std::function<bool(SimpleClient*)> EventCallback;

		void SetPostConnectCallback(EventCallback givenCallback);
		void SetPreDisconnectCallback(EventCallback givenCallback);

		EventCallback GetPostConnectCallback(void);
		EventCallback GetPreDisconnectCallback(void);

	protected:

		EventCallback* postConnectCallback;
		EventCallback* preDisconnectCallback;

		virtual bool SetupSocketConnectionAndThread(void);
		virtual bool ShutDownSocketConnectionAndThread(void);

		typedef LinkedList<Callback> CallbackList;
		CallbackList* callbackList;

		void EnqueueCallback(Callback callback);
		Callback DequeueCallback();

		void ThreadFunc(void);

		Thread* thread;
		SocketStream* socketStream;

		typedef ThreadSafeList<ProtocolData*> ProtocolDataList;
		ProtocolDataList* serverDataList;

		volatile bool threadExitSignal;
	};
}
