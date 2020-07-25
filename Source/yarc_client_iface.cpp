#include "yarc_client_iface.h"
#include "yarc_protocol_data.h"

namespace Yarc
{
	ClientInterface::ClientInterface()
	{
		this->pushDataCallback = new Callback;
	}

	/*virtual*/ ClientInterface::~ClientInterface()
	{
		delete this->pushDataCallback;
	}

	/*virtual*/ bool ClientInterface::MakeRequestSync(const ProtocolData* requestData, ProtocolData*& responseData, bool deleteData /*= true*/)
	{
		bool requestServiced = false;

		Callback callback = [&](const ProtocolData* givenResponseData) {
			responseData = const_cast<ProtocolData*>(givenResponseData);
			requestServiced = true;
			return false;
		};

		if (!this->MakeRequestAsync(requestData, callback, deleteData))
			return false;

		// Note that by blocking here, we ensure that we don't starve socket
		// threads that need to run for us to get the data from the server.
		while (!requestServiced && this->IsConnected())
			this->Update();

		return requestServiced;
	}

	/*virtual*/ bool ClientInterface::MakeTransactionRequestSync(DynamicArray<const ProtocolData*>& requestDataArray, ProtocolData*& responseData, bool deleteData /*= true*/)
	{
		bool requestServiced = false;

		Callback callback = [&](const ProtocolData* givenResponseData) {
			responseData = const_cast<ProtocolData*>(givenResponseData);
			requestServiced = true;
			return false;
		};

		if (!this->MakeTransactionRequestAsync(requestDataArray, callback, deleteData))
			return false;

		while (!requestServiced && this->IsConnected())
			this->Update();

		return requestServiced;
	}

	/*virtual*/ bool ClientInterface::RegisterPushDataCallback(Callback givenPushDataCallback)
	{
		*this->pushDataCallback = givenPushDataCallback;
		return true;
	}
}