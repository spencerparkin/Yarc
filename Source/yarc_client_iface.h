#pragma once

#include "yarc_api.h"
#include "yarc_dynamic_array.h"
#include <functional>
#include <string>
#include <map>

namespace Yarc
{
	class ProtocolData;

	class YARC_API ClientInterface
	{
	public:
		ClientInterface();
		virtual ~ClientInterface();

		// The return value indicates whether the callback takes ownership of the memory.
		typedef std::function<bool(const ProtocolData* responseData)> Callback;

		// These should be pretty self-explanatory.
		virtual bool Connect(const char* address, uint16_t port = 6379, double timeoutSeconds = -1.0) = 0;
		virtual bool Disconnect() = 0;
		virtual bool IsConnected() = 0;

		// This should be called in the same thread where requests are made; it is where
		// callbacks will be called.
		virtual bool Update(void) = 0;

		// Wait for all pending requests to get responses.
		// When pipelining, flush should be called periodically to prevent server overload.
		virtual bool Flush(void) = 0;

		// In the synchronous case, the caller takes ownership of the response data.
		// In the asynchronous case, the caller takes owership of the data if the callback returns false.
		// The order responses are received is left undefined, but the fulfillment of requests
		// is defined to always be in the same order that requests were made.
		virtual bool MakeRequestAsync(const ProtocolData* requestData, Callback callback = [](const ProtocolData*) -> bool { return true; }, bool deleteData = true) = 0;
		virtual bool MakeRequestSync(const ProtocolData* requestData, ProtocolData*& responseData, bool deleteData = true);

		// This is a convenience routine for issuing a sequence of commands that are executed
		// by the database server as a single atomic operation that fails or succeeds as a whole.
		// It is also necessary to use this function rather than issue the MULTI and EXEC commands
		// manually in the case that the client interface does not guarentee requests be fulfilled
		// in the same order that they are issued asynchronously.  Alternatively, all commands
		// comprising the transaction could be issued synchronously, but that's not as efficient.
		// Transactions also guarentee that no command from any other client is serviced in the middle
		// of the transaction.  This is an important property needed for matters of concurrancy.
		virtual bool MakeTransactionRequestAsync(DynamicArray<const ProtocolData*>& requestDataArray, Callback callback = [](const ProtocolData*) -> bool { return true; }, bool deleteData = true) = 0;
		virtual bool MakeTransactionRequestSync(DynamicArray<const ProtocolData*>& requestDataArray, ProtocolData*& responseData, bool deleteData = true);

		// These routines are used in conjunction with the pub-sub mechanism.  The client can be
		// thought of as always in pipelining mode.  However, when it receives a message from the
		// server (which is not in response to any request), then it is dispatched to the appropriate
		// subscription callback, if one has been registered.  Note that while you can still send
		// requests and get responses when susbcribed to one or more channels, you are limited to
		// only being able to issue the (P)SUBSCRIBE, (P)UNSUBSCRIBE, and QUIT commands.  This seems
		// to me to be somewhat of a limitation of Redis, but perhaps there is a good reason for it.
		// In any case, you can use multiple clients to overcome the limitation.
		virtual bool RegisterSubscriptionCallback(const char* channel, Callback callback);
		virtual bool UnregisterSubscriptionCallback(const char* channel);

		// This is used internally to dispatch messages for the pub-sub mechanism, but could also
		// be overridden by users of the client for their own custom handling.
		virtual bool MessageHandler(const ProtocolData* messageData);

	protected:

		typedef std::map<std::string, Callback> CallbackMap;
		CallbackMap* callbackMap;
	};
}