#include "yarc_cluster_client.h"

namespace Yarc
{
	ClusterClient::ClusterClient()
	{
		this->nodeList = new NodeList();
	}

	/*virtual*/ ClusterClient::~ClusterClient()
	{
		this->Disconnect();
		delete this->nodeList;
	}

	/*virtual*/ bool ClusterClient::Connect(const char* address, uint16_t port, uint32_t timeout /*= 30*/)
	{
		if (this->IsConnected())
			return false;

		// TODO: Write this.  We need only connect to a single node of the cluster, then mark our current cluster configuration dirty.  It will get queried for in the update.
		return false;
	}

	/*virtual*/ bool ClusterClient::Disconnect()
	{
		while (this->nodeList->GetCount() > 0)
		{
			NodeList::Node* node = this->nodeList->GetHead();
			Node* clusterNode = node->value;
			clusterNode->client->Disconnect();
			delete clusterNode;
			this->nodeList->Remove(node);
		}

		return true;
	}

	/*virtual*/ bool ClusterClient::IsConnected()
	{
		for (NodeList::Node* node = this->nodeList->GetHead(); node; node = node->GetNext())
		{
			Node* clusterNode = node->value;
			if (clusterNode->client && clusterNode->client->IsConnected())
				return true;
		}

		return false;
	}

	/*virtual*/ bool ClusterClient::Update(bool canBlock /*= false*/)
	{
		// TODO: Write this.  Discard nodes whose connections have failed.  Manage async requests and redirects here.
		//       Also, if we know the current configuration is out-of-date, query for it here.
		return false;
	}

	/*virtual*/ bool ClusterClient::MakeRequestAsync(const DataType* requestData, Callback callback)
	{
		// TODO: Write this.
		return false;
	}

	ClusterClient::Node::Node()
	{
		this->client = nullptr;
		this->minSlot = 0;
		this->maxSlot = 0;
	}

	/*virtual*/ ClusterClient::Node::~Node()
	{
		delete this->client;
	}

	uint16_t ClusterClient::CalculateSlot(const DataType* requestData)
	{
		// TODO: Write this.  Use the CRC16 hash on the documentation page.  Multiple keys is the only tricky part, I think.
		return 0;
	}

	ClusterClient::Node* ClusterClient::FindNodeForSlot(uint16_t slot)
	{
		// A linear search here is fine since the number of nodes in
		// practice should never be too far beyond 1000.
		for (NodeList::Node* node = this->nodeList->GetHead(); node; node = node->GetNext())
		{
			Node* clusterNode = node->value;
			if (clusterNode->HandlesSlot(slot))
				return clusterNode;
		}

		return nullptr;
	}
}