#pragma once

#include "yarc_api.h"
#include "yarc_client_iface.h"
#include "yarc_linked_list.h"
#include "yarc_reducer.h"
#include <stdint.h>
#include <WS2tcpip.h>
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

		virtual bool Connect(const char* address, uint16_t port = 6379, double timeoutSeconds = -1.0) override;
		virtual bool Disconnect() override;
		virtual bool IsConnected() override { return this->socket != INVALID_SOCKET; }
		virtual bool Update(bool canBlock = false) override;
		virtual bool Flush(void) override;		// When pipelining, flush should be called periodically to prevent server overload.
		virtual bool MakeRequestAsync(const ProtocolData* requestData, Callback callback = [](const ProtocolData*) -> bool { return true; }, bool deleteData = true) override;
		virtual bool MakeTransactionRequestAsync(DynamicArray<const ProtocolData*>& requestDataArray, Callback callback = [](const ProtocolData*) -> bool { return true; }, bool deleteData = true) override;
		virtual bool MakeTransactionRequestSync(DynamicArray<const ProtocolData*>& requestDataArray, ProtocolData*& responseData, bool deleteData = true) override;

		const char* GetAddress() const { return this->address->c_str(); }
		uint16_t GetPort() const { return this->port; }

	protected:

		typedef LinkedList<Callback> CallbackList;
		CallbackList* callbackList;

		void EnqueueCallback(Callback callback);
		Callback DequeueCallback();

		SOCKET socket;

		std::string* address;
		uint16_t port;

		class ServerResult : public ReductionObject
		{
		public:
			ServerResult(SimpleClient* givenClient, const ProtocolData* givenServerData);
			virtual ~ServerResult();
			
			SimpleClient* client;
			const ProtocolData* serverData;
		};

		class ServerResponseResult : public ServerResult
		{
		public:
			ServerResponseResult(SimpleClient* givenClient, const ProtocolData* givenServerData, Callback givenCallback);
			virtual ~ServerResponseResult();

			ReductionResult Reduce() override;

			Callback callback;
		};

		class ServerMessageResult : public ServerResult
		{
		public:
			ServerMessageResult(SimpleClient* givenClient, const ProtocolData* givenServerData);
			virtual ~ServerMessageResult();

			ReductionResult Reduce() override;
		};

		ReductionObjectList* serverResultList;

		ServerResult* ClassifyServerData(const ProtocolData* serverData);
	};
}