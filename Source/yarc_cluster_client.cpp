#include "yarc_cluster_client.h"
#include "yarc_data_types.h"

namespace Yarc
{
	//----------------------------------------- ClusterClient -----------------------------------------

	ClusterClient::ClusterClient()
	{
		this->clusterNodeList = new ProcessableList();
		this->requestList = new ProcessableList();
		this->state = STATE_NONE;
	}

	/*virtual*/ ClusterClient::~ClusterClient()
	{
		this->Disconnect();
		delete this->clusterNodeList;
		delete this->requestList;
	}

	/*virtual*/ bool ClusterClient::Connect(const char* address, uint16_t port, uint32_t timeout /*= 30*/)
	{
		if (this->IsConnected())
			return false;

		ClusterNode* clusterNode = new ClusterNode();
		if (!clusterNode->client->Connect(address, port, timeout))
		{
			delete clusterNode;
			return false;
		}

		this->clusterNodeList->AddTail(clusterNode);
		this->state = STATE_CLUSTER_CONFIG_DIRTY;

		return true;
	}

	/*virtual*/ bool ClusterClient::Disconnect()
	{
		DeleteList<Processable>(*this->clusterNodeList);
		DeleteList<Processable>(*this->requestList);
		
		this->state = STATE_NONE;

		return true;
	}

	/*virtual*/ bool ClusterClient::IsConnected()
	{
		for (ProcessableList::Node* node = this->clusterNodeList->GetHead(); node; node = node->GetNext())
		{
			ClusterNode* clusterNode = (ClusterNode*)node->value;
			if (clusterNode->client->IsConnected())
				return true;
		}

		return false;
	}

	/*virtual*/ bool ClusterClient::Update(bool canBlock /*= false*/)
	{
		Processable::ProcessList(this->clusterNodeList, this);

		switch (this->state)
		{
			case STATE_CLUSTER_CONFIG_DIRTY:
			{
				ClusterNode* clusterNode = this->GetRandomClusterNode();
				if (clusterNode)
				{
					DataType* commandData = DataType::ParseCommand("CLUSTER SLOTS");
					clusterNode->client->MakeRequestAsync(commandData, [&](const DataType* responseData) {
						this->ProcessClusterConfig(responseData);
						this->state = STATE_CLUSTER_CONFIG_STABLE;
						return true;
					});

					this->state = STATE_CLUSTER_CONFIG_QUERY_PENDING;
				}

				break;
			}
			case STATE_CLUSTER_CONFIG_QUERY_PENDING:
			{
				// Nothing to do here but wait.
				break;
			}
			case STATE_CLUSTER_CONFIG_STABLE:
			{
				Processable::ProcessList(this->requestList, this);
				break;
			}
			default:
			{
				break;
			}
		}

		return true;
	}

	/*virtual*/ bool ClusterClient::MakeRequestAsync(const DataType* requestData, Callback callback)
	{
		Request* request = new Request(requestData, callback);
		this->requestList->AddTail(request);
		return true;
	}

	void ClusterClient::ProcessClusterConfig(const DataType* responseData)
	{
		const Array* clusterNodeArray = Cast<Array>(responseData);
		if (clusterNodeArray)
		{
			ProcessableList::Node* node = nullptr;
			for (node = this->clusterNodeList->GetHead(); node; node = node->GetNext())
			{
				ClusterNode* clusterNode = (ClusterNode*)node->value;
				clusterNode->slotRangeArray.SetCount(0);
			}

			for (uint32_t i = 0; i < clusterNodeArray->GetSize(); i++)
			{
				const Array* clusterNodeInfoArray = Cast<Array>(clusterNodeArray->GetElement(i));
				if (clusterNodeInfoArray && clusterNodeInfoArray->GetSize() >= 3)
				{
					const Integer* clusterNodeMinSlotInteger = Cast<Integer>(clusterNodeInfoArray->GetElement(0));
					const Integer* clusterNodeMaxSlotInteger = Cast<Integer>(clusterNodeInfoArray->GetElement(1));

					ClusterNode::SlotRange slotRange;
					slotRange.minSlot = clusterNodeMinSlotInteger ? clusterNodeMinSlotInteger->GetNumber() : 0;
					slotRange.maxSlot = clusterNodeMaxSlotInteger ? clusterNodeMaxSlotInteger->GetNumber() : 0;

					const Array* clusterNodeIPPortArray = Cast<Array>(clusterNodeInfoArray->GetElement(2));
					if (clusterNodeIPPortArray && clusterNodeIPPortArray->GetSize() >= 2)
					{
						const SimpleString* clustNodeIPString = Cast<SimpleString>(clusterNodeIPPortArray->GetElement(0));
						const Integer* clusterNodePortInteger = Cast<Integer>(clusterNodeIPPortArray->GetElement(1));

						const char* ipAddress = clustNodeIPString ? (const char*)clustNodeIPString->GetString() : nullptr;
						uint16_t port = clusterNodePortInteger ? clusterNodePortInteger->GetNumber() : 0;

						ClusterNode* clusterNode = this->FindClusterNodeForIPPort(ipAddress, port);
						if (!clusterNode)
						{
							clusterNode = new ClusterNode();
							if (clusterNode->client->Connect(ipAddress, port))
								this->clusterNodeList->AddTail(clusterNode);
							else
							{
								delete clusterNode;
								clusterNode = nullptr;
							}
						}

						if (clusterNode)
						{
							clusterNode->slotRangeArray.SetCount(clusterNode->slotRangeArray.GetCount() + 1);
							clusterNode->slotRangeArray[clusterNode->slotRangeArray.GetCount() - 1] = slotRange;
						}
					}
				}
			}

			// If the slot ranges of a cluster node were not updated, then the cluster node is stale.
			node = this->clusterNodeList->GetHead();
			while (node)
			{
				ProcessableList::Node* nextNode = node->GetNext();

				ClusterNode* clusterNode = (ClusterNode*)node->value;
				if (clusterNode->slotRangeArray.GetCount() == 0)
				{
					clusterNode->client->Disconnect();
					delete clusterNode;
					this->clusterNodeList->Remove(node);
				}

				node = nextNode;
			}
		}
	}

	//----------------------------------------- Processable -----------------------------------------

	ClusterClient::Processable::Processable()
	{
	}

	/*virtual*/ ClusterClient::Processable::~Processable()
	{
	}

	/*static*/ void ClusterClient::Processable::ProcessList(ProcessableList* processableList, ClusterClient* clusterClient)
	{
		ProcessableList::Node* node = processableList->GetHead();
		while (node)
		{
			ProcessableList::Node* nextNode = node->GetNext();
			Processable* processable = node->value;

			ProcessResult processResult = processable->Process(clusterClient);
			if (processResult == PROC_RESULT_BAIL)
				break;

			if(processResult == PROC_RESULT_DELETE)
			{
				delete processable;
				processableList->Remove(node);
			}

			node = nextNode;
		}
	}

	//----------------------------------------- Request -----------------------------------------

	ClusterClient::Request::Request(const DataType* givenRequestData, Callback givenCallback)
	{
		this->requestData = DataType::Clone(givenRequestData);
		this->responseData = nullptr;
		this->callback = givenCallback;
		this->state = STATE_UNSENT;
	}

	/*virtual*/ ClusterClient::Request::~Request()
	{
		delete this->requestData;
		delete this->responseData;
	}

	/*virtual*/ ClusterClient::Processable::ProcessResult ClusterClient::Request::Process(ClusterClient* clusterClient)
	{
		ProcessResult processResult = PROC_RESULT_NONE;

		switch (this->state)
		{
			case STATE_UNSENT:
			{
				uint16_t slot = clusterClient->CalculateSlot(this->requestData);
				ClusterNode* clusterNode = clusterClient->FindClusterNodeForSlot(slot);
				if (!clusterNode)
				{
					clusterClient->state = ClusterClient::STATE_CLUSTER_CONFIG_DIRTY;
					processResult = PROC_RESULT_BAIL;
				}
				else
				{
					clusterNode->client->MakeRequestAsync(this->requestData, [&](const DataType* responseData) {
						this->responseData = responseData;
						this->state = STATE_READY;
						return false;	// We've taken ownership of the memory.
					});

					this->state = STATE_PENDING;
				}
				
				break;
			}
			case STATE_PENDING:
			{
				// Nothing to do here but wait.
				break;
			}
			case STATE_READY:
			{
				// TODO: Examine the response data.  If it's a redirect, handle that.  If it's an ask, handle that.  If neither of those, give it to the callback and we're done.
				//       We should dirty the cluster config if we get a redirect.

				if (!this->callback(this->responseData))
					this->responseData = nullptr;	// The callback took ownership of the memory.

				processResult = PROC_RESULT_DELETE;
				break;
			}
			default:
			{
				break;
			}
		}

		return processResult;
	}

	//----------------------------------------- ClusterNode -----------------------------------------

	ClusterClient::ClusterNode::ClusterNode()
	{
		this->client = new SimpleClient();
	}

	/*virtual*/ ClusterClient::ClusterNode::~ClusterNode()
	{
		delete this->client;
	}

	bool ClusterClient::ClusterNode::HandlesSlot(uint16_t slot) const
	{
		for (unsigned int i = 0; i < this->slotRangeArray.GetCount(); i++)
		{
			const SlotRange& slotRange = this->slotRangeArray[i];
			if (slotRange.minSlot <= slot && slot <= slotRange.maxSlot)
				return true;
		}

		return false;
	}

	/*virtual*/ ClusterClient::Processable::ProcessResult ClusterClient::ClusterNode::Process(ClusterClient* clusterClient)
	{
		this->client->Update(false);

		if (!this->client->IsConnected())
			return PROC_RESULT_DELETE;

		return PROC_RESULT_NONE;
	}

	uint16_t ClusterClient::CalculateSlot(const DataType* requestData)
	{
		// TODO: Write this.  Use the CRC16 hash on the documentation page.  Multiple keys is the only tricky part, I think.
		return 0;
	}

	ClusterClient::ClusterNode* ClusterClient::FindClusterNodeForSlot(uint16_t slot)
	{
		// A linear search here is fine since the number of nodes in
		// practice should never be too far beyond 1000.
		for (ProcessableList::Node* node = this->clusterNodeList->GetHead(); node; node = node->GetNext())
		{
			ClusterNode* clusterNode = (ClusterNode*)node->value;
			if (clusterNode->HandlesSlot(slot))
				return clusterNode;
		}

		return nullptr;
	}

	ClusterClient::ClusterNode* ClusterClient::FindClusterNodeForIPPort(const char* ipAddress, uint16_t port)
	{
		for (ProcessableList::Node* node = this->clusterNodeList->GetHead(); node; node = node->GetNext())
		{
			ClusterNode* clusterNode = (ClusterNode*)node->value;
			if (::strcmp(clusterNode->client->GetAddress(), ipAddress) == 0)
				if (clusterNode->client->GetPort() == port)
					return clusterNode;
		}

		return nullptr;
	}

	ClusterClient::ClusterNode* ClusterClient::GetRandomClusterNode()
	{
		unsigned int i = int(float(this->clusterNodeList->GetCount() - 1) * float(::rand()) / float(RAND_MAX));
		if (i < 0)
			i = 0;
		if (i > this->clusterNodeList->GetCount() - 1)
			i = this->clusterNodeList->GetCount() - 1;

		ProcessableList::Node* node = this->clusterNodeList->GetHead();
		while (node && i-- > 0)
			node = node->GetNext();

		return node ? (ClusterNode*)node->value : nullptr;
	}
}