#pragma once

#include "yarc_api.h"
#include "yarc_thread.h"
#include "yarc_client_iface.h"
#include "yarc_linked_list.h"
#include "yarc_socket_stream.h"
#include "yarc_thread_safe_list.h"
#include "yarc_reducer.h"
#include <stdint.h>
#include <string>

namespace Yarc
{
	class YARC_API SimpleClient : public ClientInterface
	{
		friend class Request;
		friend class Message;

	public:

		SimpleClient(bool tryToRecycleConnection = true);
		virtual ~SimpleClient();

		// When used as a DLL, these ensure that the client is allocated and freed in the proper heap.
		static SimpleClient* Create();
		static void Destroy(SimpleClient* client);

		virtual bool Update(void) override;
		virtual bool Flush(void) override;
		virtual int MakeRequestAsync(const ProtocolData* requestData, Callback callback = [](const ProtocolData*) -> bool { return true; }, bool deleteData = true) override;
		virtual bool CancelAsyncRequest(int requestID) override;
		virtual bool MakeTransactionRequestAsync(DynamicArray<const ProtocolData*>& requestDataArray, Callback callback = [](const ProtocolData*) -> bool { return true; }, bool deleteData = true) override;
		virtual bool MakeTransactionRequestSync(DynamicArray<const ProtocolData*>& requestDataArray, ProtocolData*& responseData, bool deleteData = true, double timeoutSeconds = 5.0) override;

		SocketStream* GetSocketStream() { return this->socketStream; }

		typedef std::function<bool(SimpleClient*)> EventCallback;

		void SetPostConnectCallback(EventCallback givenCallback);
		void SetPreDisconnectCallback(EventCallback givenCallback);

		EventCallback GetPostConnectCallback(void);
		EventCallback GetPreDisconnectCallback(void);

	protected:

		class Request : public ReductionObject
		{
		public:
			enum class State
			{
				UNSENT,
				SENT,
				SERVED
			};

			Request();
			virtual ~Request();

			virtual ReductionResult Reduce(void* userData) override;

			const ProtocolData* requestData;
			ProtocolData* responseData;
			Callback callback;
			bool ownsRequestDataMem;
			bool ownsResponseDataMem;
			State state;
			int requestID;
			static int nextRequestID;
		};

		class Message : public ReductionObject
		{
		public:
			Message();
			virtual ~Message();

			virtual ReductionResult Reduce(void* userData) override;

			ProtocolData* messageData;
			bool ownsMessageData;
		};

		ReductionObjectList* requestList;
		Mutex requestListMutex;

		ReductionObjectList* messageList;
		Mutex messageListMutex;

		EventCallback* postConnectCallback;
		EventCallback* preDisconnectCallback;

		void ThreadFunc(void);

		Thread* thread;
		SocketStream* socketStream;
		bool tryToRecycleConnection;
		volatile bool threadExitSignal;
	};
}
