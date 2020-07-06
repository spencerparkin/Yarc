#pragma once

#include "yarc_client_iface.h"
#include "yarc_simple_client.h"
#include "yarc_linked_list.h"

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

		class Node
		{
		public:

			Node();
			virtual ~Node();

			bool HandlesSlot(uint16_t slot) const { return this->minSlot <= slot && slot <= maxSlot; }

			SimpleClient* client;
			uint16_t minSlot, maxSlot;
		};

		typedef LinkedList<Node*> NodeList;
		NodeList* nodeList;

		uint16_t CalculateSlot(const DataType* requestData);
		Node* FindNodeForSlot(uint16_t slot);
	};
}