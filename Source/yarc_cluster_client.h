#pragma once

#include "yarc_client_iface.h"
#include "yarc_simple_client.h"
#include "yarc_linked_list.h"
#include "yarc_dynamic_array.h"
#include "yarc_reducer.h"

namespace Yarc
{
#if false		// TODO: Revisit this code once simple client is working with RESP3

	// Note that in contrast to the simple client, requests made here asynchronously are
	// not guarenteed to be responded to in the same order they were made.  They should,
	// however, be fulfilled in the same order they were made.
	class YARC_API ClusterClient : public ClientInterface
	{
	public:
		
		ClusterClient();
		virtual ~ClusterClient();

		// When used as a DLL, these ensure that the client is allocated and freed in the proper heap.
		static ClusterClient* Create();
		static void Destroy(ClusterClient* client);

		virtual bool Connect(const char* address, uint16_t port = 6379, double timeoutSeconds = -1.0) override;
		virtual bool Disconnect() override;
		virtual bool IsConnected() override;
		virtual bool Update(bool canBlock = false) override;
		virtual bool Flush(void) override;
		virtual bool MakeRequestAsync(const ProtocolData* requestData, Callback callback = [](const ProtocolData*) -> bool { return true; }, bool deleteData = true) override;
		virtual bool MakeTransactionRequestAsync(DynamicArray<const ProtocolData*>& requestDataArray, Callback callback = [](const ProtocolData*) -> bool { return true; }, bool deleteData = true) override;

	private:

		enum State
		{
			STATE_NONE,
			STATE_CLUSTER_CONFIG_DIRTY,
			STATE_CLUSTER_CONFIG_QUERY_PENDING,
			STATE_CLUSTER_CONFIG_STABLE,
			STATE_CLUSTER_CONFIG_INCOMPLETE,
		};

		State state;

		class NodeClient : public SimpleClient
		{
		public:
			NodeClient(ClusterClient* givenClusterClient);
			virtual ~NodeClient();

			virtual bool MessageHandler(const ProtocolData* messageData) override;

			ClusterClient* clusterClient;
		};

		class ClusterNode;

		class Request : public ReductionObject
		{
		public:

			Request(Callback givenCallback, ClusterClient* givenClusterClient);
			virtual ~Request();

			virtual ReductionResult Reduce() override;

			virtual uint16_t CalcHashSlot() = 0;
			virtual bool MakeRequestAsync(ClusterNode* clusterNode, Callback callback) = 0;

			bool ParseRedirectAddressAndPort(const char* errorMessage);

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
			char redirectAddress[64];
			uint16_t redirectPort;
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

			ClusterNode(ClusterClient* givenClusterClient);
			virtual ~ClusterNode();

			virtual ReductionResult Reduce() override;

			bool HandlesSlot(uint16_t slot) const;

			NodeClient* client;
			
			struct SlotRange
			{
				uint16_t minSlot, maxSlot;

				bool Combine(const SlotRange& slotRangeA, const SlotRange& slotRangeB);
			};

			DynamicArray<SlotRange> slotRangeArray;
		};

		ClusterNode* FindClusterNodeForSlot(uint16_t slot);
		ClusterNode* FindClusterNodeForIPPort(const char* ipAddress, uint16_t port);
		ClusterNode* GetRandomClusterNode();
		void ProcessClusterConfig(const ProtocolData* responseData);
		void SignalClusterConfigDirty(void);
		bool ClusterConfigHasFullSlotCoverage(void);

		ReductionObjectList* requestList;
		ReductionObjectList* clusterNodeList;
		uint32_t retryClusterConfigCountdown;
	};
#endif
}