#pragma once

#include "yarc_api.h"
#include <functional>

namespace Yarc
{
	class DataType;

	class YARC_API ClientInterface
	{
	public:
		ClientInterface();
		virtual ~ClientInterface();

		// The return value indicates whether the callback takes ownership of the memory.
		typedef std::function<bool(const DataType*)> Callback;

		virtual bool Connect(const char* address, uint16_t port, uint32_t timeout = 30) = 0;
		virtual bool Disconnect() = 0;
		virtual bool IsConnected() = 0;
		virtual bool Update(bool canBlock = false) = 0;
		virtual bool MakeRequestAsync(const DataType* requestData, Callback callback) = 0;
		virtual bool MakeRequestSync(const DataType* requestData, DataType*& responseData);

		void SetFallbackCallback(Callback callback) { *this->fallbackCallback = callback; }

	protected:

		Callback* fallbackCallback;
	};
}