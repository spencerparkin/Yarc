#pragma once

#include "yarc_api.h"
#include "yarc_simple_client.h"
#include "yarc_reducer.h"

namespace Yarc
{
	// This client actively tries to re-connect in the event that it loses its
	// connection while at the same time trying remain operational.  In other words,
	// it will queue up requests until it is able to reconnect.
	class YARC_API PersistentClient : public SimpleClient
	{
	public:

		PersistentClient();
		virtual ~PersistentClient();

		// When used as a DLL, these ensure that the client is allocated and freed in the proper heap.
		static PersistentClient* Create();
		static void Destroy(PersistentClient* client);

		virtual bool Connect(const char* address, uint16_t port = 6379, double timeoutSeconds = -1.0) override;
		virtual bool Disconnect() override;
		virtual bool Update() override;
		virtual bool MakeRequestAsync(const ProtocolData* requestData, Callback callback = [](const ProtocolData*) -> bool { return true; }, bool deleteData = true) override;

	private:

		class PendingRequest : public ReductionObject
		{
		public:
			PendingRequest(const ProtocolData* givenRequestData, Callback givenCallback, bool givenDeleteData, PersistentClient* givenClient);
			virtual ~PendingRequest();

			virtual ReductionResult Reduce() override;

			const ProtocolData* requestData;
			Callback callback;
			bool deleteData;
			PersistentClient* client;
		};

		ReductionObjectList* pendingRequestList;

		bool ProcessPendingRequest(PendingRequest* request);

		bool persistConnection;
	};
}