#pragma once

#include "yarc_api.h"
#include "yarc_client_iface.h"
#include "yarc_linked_list.h"
#include <stdint.h>
#include <WS2tcpip.h>
#include <string>

namespace Yarc
{
	class YARC_API SimpleClient : public ClientInterface
	{
	public:

		SimpleClient();
		virtual ~SimpleClient();

		// When used as a DLL, these ensure that the client is allocated and freed in the proper heap.
		static SimpleClient* Create();
		static void Destroy(SimpleClient* client);

		virtual bool Connect(const char* address, uint16_t port = 6379, double timeoutSeconds = -1.0) override;
		virtual bool Disconnect() override;
		virtual bool IsConnected() override { return this->socket != INVALID_SOCKET; }
		virtual bool Update(bool canBlock = false) override;
		virtual bool Flush(void) override;
		virtual bool MakeRequestAsync(const DataType* requestData, Callback callback, bool deleteData = true) override;
		virtual bool MakeTransactionRequestAsync(DynamicArray<const DataType*>& requestDataArray, Callback callback, bool deleteData = true) override;
		virtual bool MakeTransactionRequestSync(DynamicArray<const DataType*>& requestDataArray, DataType*& responseData, bool deleteData = true) override;

		const char* GetAddress() const { return this->address->c_str(); }
		uint16_t GetPort() const { return this->port; }

	private:

		typedef LinkedList<Callback> CallbackList;
		CallbackList* callbackList;

		enum class ServerDataKind
		{
			RESPONSE,
			MESSAGE
		};

		ServerDataKind ClassifyServerData(const DataType* serverData);

		SOCKET socket;

		uint8_t* buffer;
		uint32_t bufferSize;
		uint32_t bufferReadOffset;
		uint32_t bufferParseOffset;
		uint32_t pendingRequestFlushPoint;
		uint32_t updateCallCount;

		std::string* address;
		uint16_t port;
	};
}