#include "yarc_cluster_client.h"
#include "yarc_protocol_data.h"
#include "yarc_byte_stream.h"

namespace Yarc
{
	//----------------------------------------- ClusterClient -----------------------------------------

	ClusterClient::ClusterClient()
	{
		this->clusterNodeList = new ReductionObjectList();
		this->requestList = new ReductionObjectList();
		this->state = STATE_CLUSTER_CONFIG_DIRTY;
		this->retryClusterConfigCountdown = 0;
	}

	/*virtual*/ ClusterClient::~ClusterClient()
	{
		DeleteList<ReductionObject*>(*this->clusterNodeList);
		delete this->clusterNodeList;
		DeleteList<ReductionObject*>(*this->requestList);
		delete this->requestList;
	}

	/*static*/ ClusterClient* ClusterClient::Create()
	{
		return new ClusterClient();
	}

	/*static*/ void ClusterClient::Destroy(ClusterClient* client)
	{
		delete client;
	}

	void ClusterClient::SignalClusterConfigDirty(void)
	{
		if (this->state == STATE_CLUSTER_CONFIG_STABLE)
			this->state = STATE_CLUSTER_CONFIG_DIRTY;
	}

	/*virtual*/ bool ClusterClient::Update(double timeoutSeconds /*= 0.0*/)
	{
		ReductionObject::ReduceList(this->clusterNodeList);

		switch (this->state)
		{
			case STATE_CLUSTER_CONFIG_DIRTY:
			{
				if (this->clusterNodeList->GetCount() == 0)
				{
					ClusterNode* clusterNode = new ClusterNode();
					clusterNode->client->address = this->address;
					this->clusterNodeList->AddTail(clusterNode);
				}

				// TODO: Instead of picking a random node, we might choose the node that gave the MOVED error.
				ClusterNode* clusterNode = this->GetRandomClusterNode();
				if (clusterNode)
				{
					ProtocolData* commandData = ProtocolData::ParseCommand("CLUSTER SLOTS");
					clusterNode->client->MakeRequestAsync(commandData, [&](const ProtocolData* responseData) {
						this->ProcessClusterConfig(responseData);
						if (this->ClusterConfigHasFullSlotCoverage())
							this->state = STATE_CLUSTER_CONFIG_STABLE;
						else
						{
							// Redis cluster returns an error for commands when the full
							// slot range is not covered by the cluster, so we might as
							// well just queue up requests until we get a complete picture
							// of the cluster configurations.  Also, after initial stand-up
							// of the cluster or resharding of the cluster, it can take the
							// gossip protocol a little bit of time to communicate all slots.
							this->state = STATE_CLUSTER_CONFIG_INCOMPLETE;
							this->retryClusterConfigCountdown = 32;
						}
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
			case STATE_CLUSTER_CONFIG_INCOMPLETE:
			{
				if (this->retryClusterConfigCountdown == 0)
					this->state = STATE_CLUSTER_CONFIG_DIRTY;
				else
					this->retryClusterConfigCountdown--;
				break;
			}
			default:
			{
				break;
			}
		}

		return true;
	}

	bool ClusterClient::ClusterConfigHasFullSlotCoverage(void)
	{
		LinkedList<ClusterNode::SlotRange> slotRangeList;
		for (ReductionObjectList::Node* node = this->clusterNodeList->GetHead(); node; node = node->GetNext())
		{
			ClusterNode* clusterNode = (ClusterNode*)node->value;
			for (int i = 0; i < (signed)clusterNode->slotRangeArray.GetCount(); i++)
				slotRangeList.AddTail(clusterNode->slotRangeArray[i]);
		}

		while (slotRangeList.GetCount() > 1)
		{
			bool combined = false;

			for (LinkedList<ClusterNode::SlotRange>::Node* nodeA = slotRangeList.GetHead(); nodeA && !combined; nodeA = nodeA->GetNext())
			{
				ClusterNode::SlotRange& slotRangeA = nodeA->value;

				for (LinkedList<ClusterNode::SlotRange>::Node* nodeB = nodeA->GetNext(); nodeB && !combined; nodeB = nodeB->GetNext())
				{
					ClusterNode::SlotRange& slotRangeB = nodeB->value;

					ClusterNode::SlotRange combinedSlotRange;
					if (combinedSlotRange.Combine(slotRangeA, slotRangeB))
					{
						slotRangeList.Remove(nodeA);
						slotRangeList.Remove(nodeB);
						slotRangeList.AddTail(combinedSlotRange);
						combined = true;
					}
				}
			}

			if (!combined)
				break;
		}

		if (slotRangeList.GetCount() > 1)
			return false;

		ClusterNode::SlotRange& slotRange = slotRangeList.GetHead()->value;
		if (slotRange.minSlot != 0 || slotRange.maxSlot != 16383)
			return false;

		return true;
	}

	/*virtual*/ bool ClusterClient::Flush(double timeoutSeconds /*= 5.0*/)
	{
		while (this->requestList->GetCount() > 0)
			this->Update(timeoutSeconds);
		
		return true;
	}

	/*virtual*/ int ClusterClient::MakeRequestAsync(const ProtocolData* requestData, Callback callback /*= [](const ProtocolData*) -> bool { return true; }*/, bool deleteData /*= true*/)
	{
		SingleRequest* request = new SingleRequest(callback, this);
		request->requestData = requestData;
		request->deleteData = deleteData;
		this->requestList->AddTail(request);
		return 0;
	}

	/*virtual*/ bool ClusterClient::CancelAsyncRequest(int requestID)
	{
		// TODO: Write this.
		return false;
	}

	/*virtual*/ bool ClusterClient::MakeTransactionRequestAsync(DynamicArray<const ProtocolData*>& requestDataArray, Callback callback /*= [](const ProtocolData*) -> bool { return true; }*/, bool deleteData /*= true*/)
	{
		if (requestDataArray.GetCount() == 0)
			return false;

		if (requestDataArray.GetCount() == 1)
		{
			this->MakeRequestAsync(requestDataArray[0], callback);
			return true;
		}

		MultiRequest* request = new MultiRequest(callback, this);
		request->requestDataArray = requestDataArray;
		request->deleteData = deleteData;

		uint16_t hashSlot = ProtocolData::CalcCommandHashSlot(requestDataArray[0]);

		// All commands in the sequence must hash to the same slot.
		// Hash-tagging is used to accommodate the use of multiple keys.
		unsigned int i;
		for (i = 1; i < requestDataArray.GetCount(); i++)
			if (ProtocolData::CalcCommandHashSlot(requestDataArray[i]) != hashSlot)
				break;

		if (i != requestDataArray.GetCount())
		{
			delete request;
			return false;
		}

		this->requestList->AddTail(request);
		return true;
	}

	void ClusterClient::ProcessClusterConfig(const ProtocolData* responseData)
	{
		const ArrayData* clusterNodeArrayData = Cast<ArrayData>(responseData);
		if (clusterNodeArrayData)
		{
			ReductionObjectList::Node* node = nullptr;
			for (node = this->clusterNodeList->GetHead(); node; node = node->GetNext())
			{
				ClusterNode* clusterNode = (ClusterNode*)node->value;
				clusterNode->slotRangeArray.SetCount(0);
			}

			for (uint32_t i = 0; i < clusterNodeArrayData->GetCount(); i++)
			{
				const ArrayData* clusterNodeInfoArrayData = Cast<ArrayData>(clusterNodeArrayData->GetElement(i));
				if (clusterNodeInfoArrayData && clusterNodeInfoArrayData->GetCount() >= 3)
				{
					const NumberData* clusterNodeMinSlotIntegerData = Cast<NumberData>(clusterNodeInfoArrayData->GetElement(0));
					const NumberData* clusterNodeMaxSlotIntegerData = Cast<NumberData>(clusterNodeInfoArrayData->GetElement(1));

					ClusterNode::SlotRange slotRange;
					slotRange.minSlot = clusterNodeMinSlotIntegerData ? (uint16_t)clusterNodeMinSlotIntegerData->GetValue() : 0;
					slotRange.maxSlot = clusterNodeMaxSlotIntegerData ? (uint16_t)clusterNodeMaxSlotIntegerData->GetValue() : 0;

					const ArrayData* clusterNodeIPPortArrayData = Cast<ArrayData>(clusterNodeInfoArrayData->GetElement(2));
					if (clusterNodeIPPortArrayData && clusterNodeIPPortArrayData->GetCount() >= 2)
					{
						const BlobStringData* clusterNodeIPStringData = Cast<BlobStringData>(clusterNodeIPPortArrayData->GetElement(0));
						const NumberData* clusterNodePortIntegerData = Cast<NumberData>(clusterNodeIPPortArrayData->GetElement(1));

						Address address;
						address.SetHostname("");

						if (clusterNodeIPStringData)
							address.SetIPAddress(clusterNodeIPStringData->GetValue().c_str());

						address.port = clusterNodePortIntegerData ? (uint16_t)clusterNodePortIntegerData->GetValue() : 0;

						ClusterNode* clusterNode = this->FindClusterNodeForAddress(address);
						if (!clusterNode)
						{
							clusterNode = new ClusterNode();
							clusterNode->client->address = address;
							this->clusterNodeList->AddTail(clusterNode);
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
					delete clusterNode;
					this->clusterNodeList->Remove(node);
				}

				node = nextNode;
			}
		}
	}

	/*virtual*/ bool ClusterClient::RegisterPushDataCallback(Callback givenPushDataCallback)
	{
		for (ReductionObjectList::Node* node = this->clusterNodeList->GetHead(); node; node = node->GetNext())
		{
			ClusterNode* clusterNode = (ClusterNode*)node->value;
			clusterNode->client->RegisterPushDataCallback(givenPushDataCallback);
		}

		return true;
	}

	//----------------------------------------- Request -----------------------------------------

	ClusterClient::Request::Request(Callback givenCallback, ClusterClient* givenClusterClient)
	{
		this->responseData = nullptr;
		this->callback = givenCallback;
		this->clusterClient = givenClusterClient;
		this->state = STATE_UNSENT;
		this->deleteData = false;
		this->redirectCount = 0;
	}

	/*virtual*/ ClusterClient::Request::~Request()
	{
		delete this->responseData;
	}

	/*virtual*/ ReductionObject::ReductionResult ClusterClient::Request::Reduce(void* userData)
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
					this->MakeRequestAsync(clusterNode, [this](const ProtocolData* responseData) {
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
				const SimpleErrorData* errorData = Cast<SimpleErrorData>(this->responseData);
				if (errorData)
					if (this->HandleError(errorData, result))
						break;
				
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

	bool ClusterClient::Request::HandleError(const SimpleErrorData* errorData, ReductionResult& result)
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
		std::string errorCode = errorData->GetErrorCode();
		if (errorCode == "ASK")
		{
			if (++this->redirectCount == 2)
			{
				this->state = STATE_UNSENT;
				this->redirectCount = 0;
				return true;
			}

			this->ParseRedirectAddressAndPort(errorData->GetValue().c_str());

			delete this->responseData;
			this->responseData = nullptr;

			ClusterNode* clusterNode = this->clusterClient->FindClusterNodeForAddress(this->redirectAddress);
			if (!clusterNode)
			{
				this->state = STATE_UNSENT;
				return true;
			}

			ProtocolData* askingCommandData = ProtocolData::ParseCommand("ASKING");
			clusterNode->client->MakeRequestAsync(askingCommandData, [this](const ProtocolData* askingResponseData) {

				// Note that we re-find the cluster node here just to be sure it hasn't gone stale on us.
				ClusterNode* clusterNode = this->clusterClient->FindClusterNodeForAddress(this->redirectAddress);
				if (!clusterNode)
					this->state = STATE_UNSENT;
				else
				{
					this->MakeRequestAsync(clusterNode, [this](const ProtocolData* responseData) {
						this->responseData = responseData;
						this->state = STATE_READY;
						return false;
					});

					this->state = STATE_PENDING;
				}

				return true;
			});

			this->state = STATE_PENDING;
			return true;
		}
		else if (errorCode == "MOVED")
		{
			this->clusterClient->SignalClusterConfigDirty();
			result = RESULT_BAIL;	// Don't process any more requests until we've reconfigured.

			if (++this->redirectCount == 2)
			{
				this->state = STATE_UNSENT;
				this->redirectCount = 0;
				return true;
			}

			this->ParseRedirectAddressAndPort(errorData->GetValue().c_str());

			delete this->responseData;
			this->responseData = nullptr;

			ClusterNode* clusterNode = this->clusterClient->FindClusterNodeForAddress(this->redirectAddress);
			if (!clusterNode)
				this->state = STATE_UNSENT;
			else
			{
				this->MakeRequestAsync(clusterNode, [this](const ProtocolData* responseData) {
					this->responseData = responseData;
					this->state = STATE_READY;
					return false;
				});

				this->state = STATE_PENDING;
			}

			return true;
		}

		return false;
	}

	bool ClusterClient::Request::ParseRedirectAddressAndPort(const char* errorMessage)
	{
		char buffer[512];
		strcpy(buffer, errorMessage);

		uint32_t i = 0;
		char* token = ::strtok(buffer, " :");
		while (token)
		{
			if (i == 2)
				this->redirectAddress.SetIPAddress(token);
			else if (i == 3)
				this->redirectAddress.port = ::atoi(token);
			token = ::strtok(nullptr, " :");
			i++;
		}

		return true;
	}

	//----------------------------------------- SingleRequest -----------------------------------------

	ClusterClient::SingleRequest::SingleRequest(Callback givenCallback, ClusterClient* givenClusterClient) : Request(givenCallback, givenClusterClient)
	{
		this->requestData = nullptr;
	}

	/*virtual*/ ClusterClient::SingleRequest::~SingleRequest()
	{
		if (this->deleteData)
			delete this->requestData;
	}

	uint16_t ClusterClient::SingleRequest::CalcHashSlot()
	{
		return ProtocolData::CalcCommandHashSlot(this->requestData);
	}

	/*virtual*/ bool ClusterClient::SingleRequest::MakeRequestAsync(ClusterNode* clusterNode, Callback callback)
	{
		clusterNode->client->MakeRequestAsync(this->requestData, callback, false);
		return true;
	}

	//----------------------------------------- MultiRequest -----------------------------------------

	ClusterClient::MultiRequest::MultiRequest(Callback givenCallback, ClusterClient* givenClusterClient) : Request(givenCallback, givenClusterClient)
	{
	}

	/*virtual*/ ClusterClient::MultiRequest::~MultiRequest()
	{
		if (this->deleteData)
			for (unsigned int i = 0; i < this->requestDataArray.GetCount(); i++)
				delete this->requestDataArray[i];
	}

	uint16_t ClusterClient::MultiRequest::CalcHashSlot()
	{
		// It's already been verified that all commands in the array hash to the same slot.
		return ProtocolData::CalcCommandHashSlot(this->requestDataArray[0]);
	}

	/*virtual*/ bool ClusterClient::MultiRequest::MakeRequestAsync(ClusterNode* clusterNode, Callback callback)
	{
		clusterNode->client->MakeTransactionRequestAsync(this->requestDataArray, callback, false);
		return true;
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

	bool ClusterClient::ClusterNode::SlotRange::Combine(const SlotRange& slotRangeA, const SlotRange& slotRangeB)
	{
		if (slotRangeA.maxSlot + 1 == slotRangeB.minSlot)
		{
			this->minSlot = slotRangeA.minSlot;
			this->maxSlot = slotRangeB.maxSlot;
			return true;
		}

		if (slotRangeB.maxSlot + 1 == slotRangeA.minSlot)
		{
			this->minSlot = slotRangeB.minSlot;
			this->maxSlot = slotRangeA.maxSlot;
			return true;
		}

		return false;
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

	/*virtual*/ ReductionObject::ReductionResult ClusterClient::ClusterNode::Reduce(void* userData)
	{
		if (this->client)
			this->client->Update();

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

	ClusterClient::ClusterNode* ClusterClient::FindClusterNodeForAddress(const Address& address)
	{
		// A linear search is fine here for now.  If we ever discover it's a point of slowness, we could easily optimize it.
		for (ReductionObjectList::Node* node = this->clusterNodeList->GetHead(); node; node = node->GetNext())
		{
			ClusterNode* clusterNode = (ClusterNode*)node->value;
			if (clusterNode->client->address == address)
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