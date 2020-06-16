#pragma once

#include "api.h"
#include <stdint.h>
#include <WS2tcpip.h>
#include <functional>
#include "linked_list.h"

namespace Yarc
{
	class DataType;

	class YARC_API Client
	{
	public:

		Client();
		virtual ~Client();

		// The return value indicates whether the callback takes ownership of the memory.
		typedef std::function<bool(const DataType*)> Callback;

		bool Connect(const char* address, uint16_t port, uint32_t timeout = 30);
		bool Disconnect();
		bool Update(bool canBlock = false);
		bool MakeRequestAsync(const DataType* requestData, Callback callback);
		bool MakeReqeustSync(const DataType* requestData, DataType*& responseData);
		void SetFallbackCallback(Callback callback) { *this->fallbackCallback = callback; }

	private:

		typedef LinkedList<Callback> CallbackList;
		CallbackList* callbackList;

		Callback* fallbackCallback;

		SOCKET socket;

		uint8_t* buffer;
		uint32_t bufferSize;
		uint32_t bufferReadOffset;
		uint32_t bufferParseOffset;
	};
}