#pragma once

#include "yarc_client_iface.h"
#include "yarc_simple_client.h"
#include "yarc_linked_list.h"
#include "yarc_dynamic_array.h"
#include "yarc_socket_stream.h"
#include "yarc_protocol_data.h"
#include "yarc_reducer.h"

namespace Yarc
{
	// During live resharding, I can't yet think of a scenario where two asynchronous
	// requests targeting the same hash slot are fulfilled by the cluster in an order
	// opposite that with which they were made.  Nevertheless, it is something I
	// continue to think about here.  If the order of commands is critical in a particular
	// case, then either use a Lua script or a transaction.
	class YARC_API ClusterClient : public ClientInterface
	{
	public:
		
		ClusterClient();
		virtual ~ClusterClient();

		// When used as a DLL, these ensure that the client is allocated and freed in the proper heap.
		static ClusterClient* Create();
		static void Destroy(ClusterClient* client);

		virtual bool Update(void) override;
		virtual bool Flush(double timeoutSeconds = 0.0) override;
		virtual int MakeRequestAsync(const ProtocolData* requestData, Callback callback = [](const ProtocolData*) -> bool { return true; }, bool deleteData = true) override;
		virtual bool CancelAsyncRequest(int requestID) override;
		virtual bool MakeTransactionRequestAsync(DynamicArray<const ProtocolData*>& requestDataArray, Callback callback = [](const ProtocolData*) -> bool { return true; }, bool deleteData = true) override;
		virtual bool RegisterPushDataCallback(Callback givenPushDataCallback) override;

	private:

		enum State
		{
			STATE_CLUSTER_CONFIG_DIRTY,
			STATE_CLUSTER_CONFIG_QUERY_PENDING,
			STATE_CLUSTER_CONFIG_STABLE,
			STATE_CLUSTER_CONFIG_INCOMPLETE,
		};

		State state;

		class ClusterNode;

		class Request : public ReductionObject
		{
		public:

			Request(Callback givenCallback, ClusterClient* givenClusterClient);
			virtual ~Request();

			virtual ReductionResult Reduce(void* userData) override;

			virtual uint16_t CalcHashSlot() = 0;
			virtual bool MakeRequestAsync(ClusterNode* clusterNode, Callback callback) = 0;

			bool ParseRedirectAddressAndPort(const char* errorMessage);

			bool HandleError(const SimpleErrorData* errorData, ReductionResult& result);

			enum State
			{
				STATE_NONE,
				STATE_UNSENT,
				STATE_PENDING,
				STATE_READY
			};

			State state;
			ClusterClient* clusterClient;
			const ProtocolData* responseData;
			Callback callback;
			Address redirectAddress;
			uint32_t redirectCount;
			bool deleteData;
		};

		class SingleRequest : public Request
		{
		public:
			SingleRequest(Callback givenCallback, ClusterClient* givenClusterClient);
			virtual ~SingleRequest();

			uint16_t CalcHashSlot() override;
			virtual bool MakeRequestAsync(ClusterNode* clusterNode, Callback callback) override;

			const ProtocolData* requestData;
		};

		class MultiRequest : public Request
		{
		public:
			MultiRequest(Callback givenCallback, ClusterClient* givenClusterClient);
			virtual ~MultiRequest();

			uint16_t CalcHashSlot() override;
			virtual bool MakeRequestAsync(ClusterNode* clusterNode, Callback callback) override;

			DynamicArray<const ProtocolData*> requestDataArray;
		};

		class ClusterNode : public ReductionObject
		{
		public:

			ClusterNode();
			virtual ~ClusterNode();

			virtual ReductionResult Reduce(void* userData) override;

			bool HandlesSlot(uint16_t slot) const;

			SimpleClient* client;
			
			struct SlotRange
			{
				uint16_t minSlot, maxSlot;

				bool Combine(const SlotRange& slotRangeA, const SlotRange& slotRangeB);
			};

			DynamicArray<SlotRange> slotRangeArray;
		};

		ClusterNode* FindClusterNodeForSlot(uint16_t slot);
		ClusterNode* FindClusterNodeForAddress(const Address& address);
		ClusterNode* GetRandomClusterNode();
		void ProcessClusterConfig(const ProtocolData* responseData);
		void SignalClusterConfigDirty(void);
		bool ClusterConfigHasFullSlotCoverage(void);

		ReductionObjectList* requestList;
		ReductionObjectList* clusterNodeList;
		uint32_t retryClusterConfigCountdown;
	};
}