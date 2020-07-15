#pragma once

#include "yarc_client_iface.h"
#include "yarc_simple_client.h"
#include "yarc_linked_list.h"
#include "yarc_dynamic_array.h"
#include "yarc_reducer.h"

namespace Yarc
{
	class YARC_API ClusterClient : public ClientInterface
	{
	public:
		
		ClusterClient();
		virtual ~ClusterClient();

		virtual bool Connect(const char* address, uint16_t port = 6379, uint32_t timeout = 30) override;
		virtual bool Disconnect() override;
		virtual bool IsConnected() override;
		virtual bool Update(bool canBlock = false) override;
		virtual bool Flush(void) override;
		virtual bool MakeRequestAsync(const DataType* requestData, Callback callback) override;
		virtual bool MakeTransactionRequestAsync(const DynamicArray<DataType*>& requestDataArray, Callback callback) override;

	private:

		enum State
		{
			STATE_NONE,
			STATE_CLUSTER_CONFIG_DIRTY,
			STATE_CLUSTER_CONFIG_QUERY_PENDING,
			STATE_CLUSTER_CONFIG_STABLE,
		};

		State state;

		class ClusterNode;

		class Request : public ReductionObject
		{
		public:

			Request(Callback givenCallback, ClusterClient* givenClusterClient);
			virtual ~Request();

			virtual ReductionResult Reduce() override;

			virtual uint16_t CalcHashSlot() = 0;
			virtual bool MakeRequestAsync(ClusterNode* clusterNode, Callback callback) = 0;

			enum State
			{
				STATE_NONE,
				STATE_UNSENT,
				STATE_PENDING,
				STATE_ASKING,
				STATE_READY
			};

			State state;
			ClusterClient* clusterClient;
			const DataType* responseData;
			Callback callback;
			char redirectAddress[64];
			uint16_t redirectPort;
		};

		class SingleRequest : public Request
		{
		public:
			SingleRequest(Callback givenCallback, ClusterClient* givenClusterClient);
			virtual ~SingleRequest();

			uint16_t CalcHashSlot() override;
			virtual bool MakeRequestAsync(ClusterNode* clusterNode, Callback callback) override;

			const DataType* requestData;
		};

		class MultiRequest : public Request
		{
		public:
			MultiRequest(Callback givenCallback, ClusterClient* givenClusterClient);
			virtual ~MultiRequest();

			uint16_t CalcHashSlot() override;
			virtual bool MakeRequestAsync(ClusterNode* clusterNode, Callback callback) override;

			DynamicArray<DataType*> requestDataArray;
		};

		class ClusterNode : public ReductionObject
		{
		public:

			ClusterNode();
			virtual ~ClusterNode();

			virtual ReductionResult Reduce() override;

			bool HandlesSlot(uint16_t slot) const;

			SimpleClient* client;
			
			struct SlotRange
			{
				uint16_t minSlot, maxSlot;
			};

			DynamicArray<SlotRange> slotRangeArray;
		};

		ClusterNode* FindClusterNodeForSlot(uint16_t slot);
		ClusterNode* FindClusterNodeForIPPort(const char* ipAddress, uint16_t port);
		ClusterNode* GetRandomClusterNode();
		void ProcessClusterConfig(const DataType* responseData);
		void SignalClusterConfigDirty(void);

		ReductionObjectList* requestList;
		ReductionObjectList* clusterNodeList;
	};
}