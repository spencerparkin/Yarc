#pragma once

#include "yarc_api.h"
#include "yarc_thread.h"
#include "yarc_client_iface.h"
#include "yarc_linked_list.h"
#include "yarc_socket_stream.h"
#include "yarc_thread_safe_list.h"
#include "yarc_semaphore.h"
#include <stdint.h>
#include <string>
#include <time.h>

namespace Yarc
{
	class YARC_API SimpleClient : public ClientInterface
	{
		friend class Request;
		friend class Message;

	public:

		SimpleClient(double connectionTimeoutSeconds = 0.5, double connectionRetrySeconds = 5.0, bool tryToRecycleConnection = true);
		virtual ~SimpleClient();

		// When used as a DLL, these ensure that the client is allocated and freed in the proper heap.
		static SimpleClient* Create();
		static void Destroy(SimpleClient* client);

		virtual bool Update(double timeoutMilliseconds = 1.0) override;
		virtual bool Flush(double timeoutSeconds = 5.0) override;
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

		class Request
		{
		public:
			Request();
			virtual ~Request();

			const ProtocolData* requestData;
			ProtocolData* responseData;
			Callback callback;
			bool ownsRequestDataMem;
			bool ownsResponseDataMem;
			int requestID;
			static int nextRequestID;
		};

		class Message
		{
		public:
			Message();
			virtual ~Message();

			ProtocolData* messageData;
			bool ownsMessageData;
		};

		typedef ThreadSafeList<Request*> RequestList;
		typedef ThreadSafeList<Message*> MessageList;

		RequestList* unsentRequestList;
		RequestList* sentRequestList;
		RequestList* servedRequestList;

		MessageList* messageList;
		
		//Semaphore responseSemaphore;

		EventCallback* postConnectCallback;
		EventCallback* preDisconnectCallback;

		void ThreadFunc(void);

		Request* AllocRequest();
		void DeallocRequest(Request* request);

		Thread* thread;
		SocketStream* socketStream;
		bool tryToRecycleConnection;
		volatile bool threadExitSignal;
		double connectionTimeoutSeconds;
		double connectionRetrySeconds;
		::clock_t lastFailedConnectionAttemptTime;
		int numRequestsInFlight;
	};
}
