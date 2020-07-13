#include "yarc_client_iface.h"
#include "yarc_data_types.h"

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

	/*virtual*/ bool ClientInterface::MakeTransactionRequestAsync(const DynamicArray<DataType*>& requestDataArray, Callback callback)
	{
		DataType* responseData = nullptr;
		if (!this->MakeRequestSync(DataType::ParseCommand("MULTI"), responseData))
			return false;
		
		bool commandSucceded = !Cast<Error>(responseData);
		delete responseData;
		if (!commandSucceded)
			return false;

		unsigned int queuedCount = 0;

		if (this->RequestOrderPreserved())
		{
			unsigned int j = 0;

			for (unsigned int i = 0; i < requestDataArray.GetCount(); i++)
			{
				if (!this->MakeRequestAsync(requestDataArray[i], [&](const DataType* responseData) {
						j++;
						if (!Cast<Error>(responseData))
							queuedCount++;
						return true;
					}))
				{
					return false;
				}
			}

			while (j < requestDataArray.GetCount())
			{
				if (!this->IsConnected())
					return false;

				this->Update(true);
			}
		}
		else
		{
			for (unsigned int i = 0; i < requestDataArray.GetCount(); i++)
			{
				if (!this->MakeRequestSync(requestDataArray[i], responseData))
					return false;
				
				if (!Cast<Error>(responseData))
					queuedCount++;

				delete responseData;
			}
		}

		if (queuedCount < requestDataArray.GetCount())
			return false;

		return this->MakeRequestAsync(DataType::ParseCommand("EXEC"), callback);
	}
}