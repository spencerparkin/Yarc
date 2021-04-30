#include "yarc_client_iface.h"
#include "yarc_protocol_data.h"
#if defined __WINDOWS__
#	include <WS2tcpip.h>
#elif defined __LINUX__
#	include <sys/socket.h>
#endif
#include <time.h>

namespace Yarc
{
	//------------------------- ClientInterface -------------------------

	ClientInterface::ClientInterface()
	{
		this->pushDataCallback = new Callback;
	}

	/*virtual*/ ClientInterface::~ClientInterface()
	{
		delete this->pushDataCallback;
	}

	/*virtual*/ bool ClientInterface::MakeRequestSync(const ProtocolData* requestData, ProtocolData*& responseData, bool deleteData /*= true*/, double timeoutSeconds /*= 5.0*/)
	{
		responseData = nullptr;
		bool requestServiced = false;

		Callback callback = [&](const ProtocolData* givenResponseData) {
			responseData = const_cast<ProtocolData*>(givenResponseData);
			requestServiced = true;
			return false;
		};

		int requestID = this->MakeRequestAsync(requestData, callback, deleteData);
		clock_t startTime = ::clock();
		while (!requestServiced)
		{
			this->Update();
			
			clock_t currentTime = ::clock();
			double elapsedTimeSeconds = double(currentTime - startTime) / double(CLOCKS_PER_SEC);
			if (elapsedTimeSeconds >= timeoutSeconds)
				break;
		}

		if (!requestServiced)
			this->CancelAsyncRequest(requestID);

		return requestServiced;
	}

	/*virtual*/ bool ClientInterface::MakeTransactionRequestSync(DynamicArray<const ProtocolData*>& requestDataArray, ProtocolData*& responseData, bool deleteData /*= true*/, double timeoutSeconds /*= 5.0*/)
	{
		return false;
	}

	/*virtual*/ bool ClientInterface::RegisterPushDataCallback(Callback givenPushDataCallback)
	{
		*this->pushDataCallback = givenPushDataCallback;
		return true;
	}
}