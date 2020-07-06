#include "yarc_client_iface.h"

namespace Yarc
{
	ClientInterface::ClientInterface()
	{
		this->fallbackCallback = new Callback;
	}

	/*virtual*/ ClientInterface::~ClientInterface()
	{
		delete this->fallbackCallback;
	}

	/*virtual*/ bool ClientInterface::MakeRequestSync(const DataType* requestData, DataType*& responseData)
	{
		bool requestServiced = false;

		Callback callback = [&](const DataType* dataType) {
			responseData = const_cast<DataType*>(dataType);
			requestServiced = true;
			return false;
		};

		if (!this->MakeRequestAsync(requestData, callback))
			return false;

		// Note that by blocking here, we ensure that we don't starve socket
		// threads that need to run for us to get the data from the server.
		while (!requestServiced && this->IsConnected())
			this->Update(true);

		return requestServiced;
	}
}