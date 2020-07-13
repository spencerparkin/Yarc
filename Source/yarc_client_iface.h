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

		// This is a convenience routine for issuing a sequence of commands that are executed
		// by the database server as a single atomic operation that fails or succeeds as a whole.
		// It is also necessary to use this function rather than issue the MULTI and EXEC commands
		// manually in the case that the client interface does not guarentee requests be fulfilled
		// in the same order that they are issued asynchronously.  Alternatively, all commands
		// comprising the transaction could be issued synchronously, but that's not as efficient.
		virtual bool MakeTransactionRequestAsync(const DynamicArray<DataType*>& requestDataArray, Callback callback) = 0;

		void SetFallbackCallback(Callback callback) { *this->fallbackCallback = callback; }

	protected:

		Callback* fallbackCallback;
	};
}