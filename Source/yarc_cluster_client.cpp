#include "yarc_cluster_client.h"
#include "yarc_data_types.h"

namespace Yarc
{
	//----------------------------------------- ClusterClient -----------------------------------------

	ClusterClient::ClusterClient()
	{
		this->clusterNodeList = new ReductionObjectList();
		this->requestList = new ReductionObjectList();
		this->state = STATE_NONE;
	}

	/*virtual*/ ClusterClient::~ClusterClient()
	{
		this->Disconnect();
		delete this->clusterNodeList;
		delete this->requestList;
	}

	/*virtual*/ bool ClusterClient::Connect(const char* address, uint16_t port /*= 6379*/, uint32_t timeout /*= 30*/)
	{
		if (this->IsConnected())
			return false;

		ClusterNode* clusterNode = new ClusterNode(this);
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
		DeleteList<ReductionObject>(*this->clusterNodeList);
		DeleteList<ReductionObject>(*this->requestList);

		this->state = STATE_NONE;

		return true;
	}

	/*virtual*/ bool ClusterClient::IsConnected()
	{
		for (ReductionObjectList::Node* node = this->clusterNodeList->GetHead(); node; node = node->GetNext())
		{
			ClusterNode* clusterNode = (ClusterNode*)node->value;
			if (clusterNode->client->IsConnected())
				return true;
		}

		return false;
	}

	void ClusterClient::SignalClusterConfigDirty(void)
	{
		if (this->state == STATE_CLUSTER_CONFIG_STABLE)
			this->state = STATE_CLUSTER_CONFIG_DIRTY;
	}

	/*virtual*/ bool ClusterClient::Update(bool canBlock /*= false*/)
	{
		ReductionObject::ReduceList(this->clusterNodeList);

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
				ReductionObject::ReduceList(this->requestList);
				break;
			}
			default:
			{
				break;
			}
		}

		return true;
	}

	/*virtual*/ bool ClusterClient::Flush(void)
	{
		// TODO: Write this.
		return false;
	}

	/*virtual*/ bool ClusterClient::MakeRequestAsync(const DataType* requestData, Callback callback)
	{
		SingleRequest* request = new SingleRequest(callback, this);
		request->requestData = requestData;
		this->requestList->AddTail(request);
		return true;
	}

	/*virtual*/ bool ClusterClient::MakeTransactionRequestAsync(const DynamicArray<DataType*>& requestDataArray, Callback callback)
	{
		if (requestDataArray.GetCount() == 0)
			return false;

		if (requestDataArray.GetCount() == 1)
			return this->MakeRequestAsync(requestDataArray[0], callback);

		MultiRequest* request = new MultiRequest(callback, this);
		request->requestDataArray = requestDataArray;

		uint16_t hashSlot = DataType::CalcCommandHashSlot(requestDataArray[0]);

		// All commands in the sequence must hash to the same slot.
		// Hash-tagging is used to accommodate the use of multiple keys.
		unsigned int i;
		for (i = 1; i < requestDataArray.GetCount(); i++)
			if (DataType::CalcCommandHashSlot(requestDataArray[i]) != hashSlot)
				break;

		if (i != requestDataArray.GetCount())
		{
			delete request;
			return false;
		}

		this->requestList->AddTail(request);
		return true;
	}

	void ClusterClient::ProcessClusterConfig(const DataType* responseData)
	{
		const Array* clusterNodeArray = Cast<Array>(responseData);
		if (clusterNodeArray)
		{
			ReductionObjectList::Node* node = nullptr;
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
						const BulkString* clusterNodeIPString = Cast<BulkString>(clusterNodeIPPortArray->GetElement(0));
						const Integer* clusterNodePortInteger = Cast<Integer>(clusterNodeIPPortArray->GetElement(1));

						char ipAddress[128];
						if (clusterNodeIPString)
							clusterNodeIPString->GetString((uint8_t*)ipAddress, sizeof(ipAddress));
						else
							ipAddress[0] = '\0';

						uint16_t port = clusterNodePortInteger ? clusterNodePortInteger->GetNumber() : 0;

						ClusterNode* clusterNode = this->FindClusterNodeForIPPort(ipAddress, port);
						if (!clusterNode)
						{
							clusterNode = new ClusterNode(this);
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
				ReductionObjectList::Node* nextNode = node->GetNext();

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

	//----------------------------------------- NodeClient -----------------------------------------

	ClusterClient::NodeClient::NodeClient(ClusterClient* givenClusterClient)
	{
		this->clusterClient = givenClusterClient;
	}

	/*virtual*/ ClusterClient::NodeClient::~NodeClient()
	{
	}

	/*virtual*/ bool ClusterClient::NodeClient::MessageHandler(const DataType* messageData)
	{
		return this->clusterClient->MessageHandler(messageData);
	}

	//----------------------------------------- Request -----------------------------------------

	ClusterClient::Request::Request(Callback givenCallback, ClusterClient* givenClusterClient)
	{
		this->responseData = nullptr;
		this->callback = givenCallback;
		this->clusterClient = givenClusterClient;
		this->state = STATE_UNSENT;
		this->redirectAddress[0] = '\0';
		this->redirectPort = 0;
	}

	/*virtual*/ ClusterClient::Request::~Request()
	{
		delete this->responseData;
	}

	/*virtual*/ ReductionObject::ReductionResult ClusterClient::Request::Reduce()
	{
		ReductionResult result = RESULT_NONE;

		switch (this->state)
		{
			case STATE_UNSENT:
			{
				uint16_t hashSlot = this->CalcHashSlot();
				ClusterNode* clusterNode = this->clusterClient->FindClusterNodeForSlot(hashSlot);
				if (!clusterNode)
				{
					this->clusterClient->SignalClusterConfigDirty();
					result = RESULT_BAIL;
				}
				else
				{
					bool requestMade = this->MakeRequestAsync(clusterNode, [=](const DataType* responseData) {
						this->responseData = responseData;
						this->state = STATE_READY;
						return false;	// We've taken ownership of the memory.
					});

					if (requestMade)
						this->state = STATE_PENDING;
				}
				
				break;
			}
			case STATE_ASKING:
			case STATE_PENDING:
			{
				// Nothing to do here but wait.
				break;
			}
			case STATE_READY:
			{
				// There are two kinds of redirections we need to handle here: -ASK and -MOVED.
				// Those of the first kind are a form of redirection used during the intermediate
				// stages of key migration for a slot from one node to another.  In this case, we
				// must perform the redirection, but proceed as if the cluster configuration has
				// not yet changed.  Only once the migration is complete should there be considered
				// a change in configuration, but we need not watch for that.  Queries to the
				// migrated slot will eventually result in a redirect of the second kind, at which
				// point we will not only perform a redirection, but also invalidate our current
				// understanding of the entire cluster configuration.  This is recommended as it is
				// likely that if one slot has migrated, then so too have many slots migrated.
				const Error* error = Cast<Error>(this->responseData);
				if (error)
				{
					const char* errorMessage = (const char*)error->GetString();
					if (strstr(errorMessage, "ASK") == errorMessage)
					{
						// TODO: Read redirect address into this->redirectAddress/port.

						delete this->responseData;
						this->responseData = nullptr;

						ClusterNode* clusterNode = this->clusterClient->FindClusterNodeForIPPort(this->redirectAddress, this->redirectPort);
						if (!clusterNode)
						{
							this->state = STATE_UNSENT;
							break;
						}

						DataType* askingCommandData = DataType::ParseCommand("ASKING");
						bool askingRequestMade = clusterNode->client->MakeRequestAsync(askingCommandData, [=](const DataType* askingResponseData) {
							
							// Note that we re-find the cluster node here just to be sure it hasn't gone stale on us.
							ClusterNode* clusterNode = this->clusterClient->FindClusterNodeForIPPort(this->redirectAddress, this->redirectPort);
							if (!clusterNode)
								this->state = STATE_UNSENT;
							else
							{
								bool requestMade = this->MakeRequestAsync(clusterNode, [=](const DataType* responseData) {
									this->responseData = responseData;
									this->state = STATE_READY;
									return false;
								});

								if (requestMade)
									this->state = STATE_PENDING;
								else
									this->state = STATE_UNSENT;
							}

							return true;
						});

						delete askingCommandData;

						if (askingRequestMade)
							this->state = STATE_ASKING;
						else
							this->state = STATE_UNSENT;

						break;
					}
					else if (strstr(errorMessage, "MOVED") == errorMessage)
					{
						// TODO: Read redirect address into this->redirectAddress/port.

						delete this->responseData;
						this->responseData = nullptr;

						ClusterNode* clusterNode = this->clusterClient->FindClusterNodeForIPPort(this->redirectAddress, this->redirectPort);
						if (!clusterNode)
						{
							this->state = STATE_UNSENT;
							break;
						}

						bool requestMade = clusterNode->client->MakeRequestAsync(responseData, [=](const DataType* responseData) {
							this->responseData = responseData;
							this->state = STATE_READY;
							return false;
						});

						if (requestMade)
							this->state = STATE_PENDING;
						else
							this->state = STATE_UNSENT;

						this->clusterClient->SignalClusterConfigDirty();
						result = RESULT_BAIL;
						
						break;
					}
				}
				
				// If we get here, the client gets to handle the response.
				if (!this->callback(this->responseData))
					this->responseData = nullptr;	// The callback took ownership of the memory.

				result = RESULT_DELETE;
				break;
			}
			case STATE_NONE:
			default:
			{
				result = RESULT_DELETE;
				break;
			}
		}

		return result;
	}

	//----------------------------------------- SingleRequest -----------------------------------------

	ClusterClient::SingleRequest::SingleRequest(Callback givenCallback, ClusterClient* givenClusterClient) : Request(givenCallback, givenClusterClient)
	{
		this->requestData = nullptr;
	}

	/*virtual*/ ClusterClient::SingleRequest::~SingleRequest()
	{
		delete this->requestData;
	}

	uint16_t ClusterClient::SingleRequest::CalcHashSlot()
	{
		return DataType::CalcCommandHashSlot(this->requestData);
	}

	/*virtual*/ bool ClusterClient::SingleRequest::MakeRequestAsync(ClusterNode* clusterNode, Callback callback)
	{
		bool requestSent = clusterNode->client->MakeRequestAsync(this->requestData, callback);
		this->requestData = nullptr;
		return requestSent;
	}

	//----------------------------------------- MultiRequest -----------------------------------------

	ClusterClient::MultiRequest::MultiRequest(Callback givenCallback, ClusterClient* givenClusterClient) : Request(givenCallback, givenClusterClient)
	{
	}

	/*virtual*/ ClusterClient::MultiRequest::~MultiRequest()
	{
		for (unsigned int i = 0; i < this->requestDataArray.GetCount(); i++)
			delete this->requestDataArray[i];
	}

	uint16_t ClusterClient::MultiRequest::CalcHashSlot()
	{
		// It's already been verified that all commands in the array hash to the same slot.
		return DataType::CalcCommandHashSlot(this->requestDataArray[0]);
	}

	/*virtual*/ bool ClusterClient::MultiRequest::MakeRequestAsync(ClusterNode* clusterNode, Callback callback)
	{
		bool transactionSent = clusterNode->client->MakeTransactionRequestAsync(this->requestDataArray, callback);
		this->requestDataArray.SetCount(0);
		return transactionSent;
	}

	//----------------------------------------- ClusterNode -----------------------------------------

	ClusterClient::ClusterNode::ClusterNode(ClusterClient* givenClusterClient)
	{
		this->client = new NodeClient(givenClusterClient);
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

	/*virtual*/ ReductionObject::ReductionResult ClusterClient::ClusterNode::Reduce()
	{
		this->client->Update(false);

		if (!this->client->IsConnected())
			return RESULT_DELETE;

		return RESULT_NONE;
	}

	ClusterClient::ClusterNode* ClusterClient::FindClusterNodeForSlot(uint16_t slot)
	{
		// A linear search here is fine since the number of nodes in
		// practice should never be too far beyond 1000.
		for (ReductionObjectList::Node* node = this->clusterNodeList->GetHead(); node; node = node->GetNext())
		{
			ClusterNode* clusterNode = (ClusterNode*)node->value;
			if (clusterNode->HandlesSlot(slot))
				return clusterNode;
		}

		return nullptr;
	}

	ClusterClient::ClusterNode* ClusterClient::FindClusterNodeForIPPort(const char* ipAddress, uint16_t port)
	{
		for (ReductionObjectList::Node* node = this->clusterNodeList->GetHead(); node; node = node->GetNext())
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

		ReductionObjectList::Node* node = this->clusterNodeList->GetHead();
		while (node && i-- > 0)
			node = node->GetNext();

		return node ? (ClusterNode*)node->value : nullptr;
	}
}