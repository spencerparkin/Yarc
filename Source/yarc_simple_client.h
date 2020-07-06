#pragma once

#include "yarc_api.h"
#include "yarc_client_iface.h"
#include "yarc_linked_list.h"
#include <stdint.h>
#include <WS2tcpip.h>

namespace Yarc
{
	class YARC_API SimpleClient : public ClientInterface
	{
	public:

		SimpleClient();
		virtual ~SimpleClient();

		virtual bool Connect(const char* address, uint16_t port, uint32_t timeout = 30) override;
		virtual bool Disconnect() override;
		virtual bool IsConnected() override { return this->socket != INVALID_SOCKET; }
		virtual bool Update(bool canBlock = false) override;
		virtual bool MakeRequestAsync(const DataType* requestData, Callback callback) override;

	private:

		typedef LinkedList<Callback> CallbackList;
		CallbackList* callbackList;

		SOCKET socket;

		uint8_t* buffer;
		uint32_t bufferSize;
		uint32_t bufferReadOffset;
		uint32_t bufferParseOffset;
	};
}