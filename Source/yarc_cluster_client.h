#pragma once

#include "yarc_client_iface.h"
#include "yarc_simple_client.h"
#include "yarc_linked_list.h"
#include "yarc_dynamic_array.h"

namespace Yarc
{
	class YARC_API ClusterClient : public ClientInterface
	{
	public:
		
		ClusterClient();
		virtual ~ClusterClient();

		virtual bool Connect(const char* address, uint16_t port, uint32_t timeout = 30) override;
		virtual bool Disconnect() override;
		virtual bool IsConnected() override;
		virtual bool Update(bool canBlock = false) override;
		virtual bool MakeRequestAsync(const DataType* requestData, Callback callback) override;

	private:

		enum State
		{
			STATE_NONE,
			STATE_CLUSTER_CONFIG_DIRTY,
			STATE_CLUSTER_CONFIG_QUERY_PENDING,
			STATE_CLUSTER_CONFIG_STABLE,
		};

		State state;

		class Processable
		{
		public:
			Processable();
			virtual ~Processable();

			enum ProcessResult
			{
				PROC_RESULT_NONE,
				PROC_RESULT_DELETE,
				PROC_RESULT_BAIL
			};

			virtual ProcessResult Process(ClusterClient* clusterClient) = 0;

			static void ProcessList(LinkedList<Processable*>* processableList, ClusterClient* clusterClient);
		};

		typedef LinkedList<Processable*> ProcessableList;

		class Request : public Processable
		{
		public:

			Request(const DataType* givenRequestData, Callback givenCallback);
			virtual ~Request();

			virtual ProcessResult Process(ClusterClient* clusterClient) override;

			enum State
			{
				STATE_NONE,
				STATE_UNSENT,
				STATE_PENDING,
				STATE_ASKING,
				STATE_READY
			};

			State state;
			const DataType* requestData;
			const DataType* responseData;
			Callback callback;
			char redirectAddress[64];
			uint16_t redirectPort;
		};

		class ClusterNode : public Processable
		{
		public:

			ClusterNode();
			virtual ~ClusterNode();

			virtual ProcessResult Process(ClusterClient* clusterClient) override;

			bool HandlesSlot(uint16_t slot) const;

			SimpleClient* client;
			
			struct SlotRange
			{
				uint16_t minSlot, maxSlot;
			};

			DynamicArray<SlotRange> slotRangeArray;
		};

		uint16_t CalculateSlot(const DataType* requestData);
		ClusterNode* FindClusterNodeForSlot(uint16_t slot);
		ClusterNode* FindClusterNodeForIPPort(const char* ipAddress, uint16_t port);
		ClusterNode* GetRandomClusterNode();
		void ProcessClusterConfig(const DataType* responseData);
		void SignalClusterConfigDirty(void);

		ProcessableList* requestList;
		ProcessableList* clusterNodeList;
	};
}