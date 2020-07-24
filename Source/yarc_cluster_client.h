#pragma once

#include "yarc_client_iface.h"
#include "yarc_simple_client.h"
#include "yarc_linked_list.h"
#include "yarc_dynamic_array.h"
#include "yarc_reducer.h"

namespace Yarc
{
	// TODO: For large clusters, it might not be practical to maintain a
	//       connection to all master nodes all the time.  Could connection
	//       pooling be a solution to this problem?

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
		virtual bool Update(void) override;
		virtual bool Flush(void) override;
		virtual bool MakeRequestAsync(const ProtocolData* requestData, Callback callback = [](const ProtocolData*) -> bool { return true; }, bool deleteData = true) override;
		virtual bool MakeTransactionRequestAsync(DynamicArray<const ProtocolData*>& requestDataArray, Callback callback = [](const ProtocolData*) -> bool { return true; }, bool deleteData = true) override;
		virtual void SignalThreadExit(void) override;

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

			// TODO: Heaven help us, must we make this a threaded-client that owns a node client?
			//       I need to take a hard look at the cluster client and re-evaluate how to design it.
			//       Right now, the simple client will always block, and this prevents the other
			//       internals of the cluster client from functioning.  Note that we probably don't
			//       need this pointer allocated all the time.  We could just bring it up when needed
			//       and then expire it when it's been idle for too long.  This way, if there are 1000
			//       nodes in the cluster, we're not liketly to be trying to run 1000 threads on the
			//       local machine all the time.  In other words, do a lazy-load or on-demand-load
			//       of clients when we're using the cluster.
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
}