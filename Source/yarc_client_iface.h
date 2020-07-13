#pragma once

#include "yarc_api.h"
#include "yarc_dynamic_array.h"
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

		// This should be called in the same thread where requests are made.
		virtual bool Update(bool canBlock = false) = 0;

		// These always take ownership of the request data, so the caller should not delete or maintain their pointer.
		// In the synchronous case, the caller takes ownership of the response data.
		// In the asynchronous case, the caller takes owership of the data if the callback returns false.
		virtual bool MakeRequestAsync(const DataType* requestData, Callback callback) = 0;
		virtual bool MakeRequestSync(const DataType* requestData, DataType*& responseData);
		
		// If a sequence of requests are made asynchronously, are they nevertheless guarenteed
		// to be fulfilled in the same order that they were sent?
		virtual bool RequestOrderPreserved(void) = 0;

		// Requests made individually may be interleaved with other client requests, but if
		// batched here as a single transaction, the list of requests is fullfilled as a single
		// atomic operation within the database.  This operation either succeeds or fails as a whole.
		virtual bool MakeTransactionRequestAsync(const DynamicArray<DataType*>& requestDataArray, Callback callback);

		void SetFallbackCallback(Callback callback) { *this->fallbackCallback = callback; }

	protected:

		Callback* fallbackCallback;
	};
}