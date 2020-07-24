#include "yarc_persistent_dual_client.h"
#include "yarc_persistent_client.h"
#include "yarc_protocol_data.h"
#include "yarc_misc.h"

namespace Yarc
{
	PersistentDualClient::PersistentDualClient()
	{
		this->inputClient = nullptr;
		this->outputClient = nullptr;
	}

	/*virtual*/ PersistentDualClient::~PersistentDualClient()
	{
		delete this->inputClient;
		delete this->outputClient;
	}

	/*static*/ PersistentDualClient* PersistentDualClient::Create()
	{
		return new PersistentDualClient();
	}

	/*static*/ void PersistentDualClient::Destroy(PersistentDualClient* client)
	{
		delete client;
	}

	/*virtual*/ bool PersistentDualClient::Connect(const char* address, uint16_t port /*= 6379*/, double timeoutSeconds /*= -1.0*/)
	{
		if (this->IsConnected())
			return false;

		try
		{
			this->inputClient = new PersistentClient();
			this->outputClient = new PersistentClient();

			if (!this->inputClient->Connect(address, port, timeoutSeconds))
				throw new InternalException();

			if (!this->outputClient->Connect(address, port, timeoutSeconds))
				throw new InternalException();
		}
		catch (InternalException* exc)
		{
			delete exc;
			this->Disconnect();
			return false;
		}

		return true;
	}

	/*virtual*/ bool PersistentDualClient::Disconnect()
	{
		if (this->inputClient)
		{
			this->inputClient->Disconnect();
			delete this->inputClient;
			this->inputClient = nullptr;
		}

		if (this->outputClient)
		{
			this->outputClient->Disconnect();
			delete this->outputClient;
			this->outputClient = nullptr;
		}

		return true;
	}

	/*virtual*/ bool PersistentDualClient::IsConnected()
	{
		if (!this->inputClient || !this->outputClient)
			return false;

		return this->inputClient->IsConnected() && this->outputClient->IsConnected();
	}

	/*virtual*/ bool PersistentDualClient::Update()
	{
		if (this->inputClient)
			this->inputClient->Update();

		if (this->outputClient)
			this->outputClient->Update();

		return true;
	}

	/*virtual*/ bool PersistentDualClient::Flush(void)
	{
		if (this->inputClient)
			this->inputClient->Flush();

		if (this->outputClient)
			this->outputClient->Flush();

		return true;
	}

	/*virtual*/ bool PersistentDualClient::MakeRequestAsync(const ProtocolData* requestData, Callback callback /*= [](const ProtocolData*) -> bool { return true; }*/, bool deleteData /*= true*/)
	{
		if (!this->outputClient || !this->outputClient->IsConnected())
			return false;

		return this->outputClient->MakeRequestAsync(requestData, callback, deleteData);
	}

	/*virtual*/ bool PersistentDualClient::MakeRequestSync(const ProtocolData* requestData, ProtocolData*& responseData, bool deleteData /*= true*/)
	{
		if (!this->outputClient || !this->outputClient->IsConnected())
			return false;

		return this->outputClient->MakeRequestSync(requestData, responseData, deleteData);
	}

	/*virtual*/ bool PersistentDualClient::MakeTransactionRequestAsync(DynamicArray<const ProtocolData*>& requestDataArray, Callback callback /*= [](const ProtocolData*) -> bool { return true; }*/, bool deleteData /*= true*/)
	{
		if (!this->outputClient || !this->outputClient->IsConnected())
			return false;

		return this->outputClient->MakeTransactionRequestAsync(requestDataArray, callback, deleteData);
	}

	/*virtual*/ bool PersistentDualClient::MakeTransactionRequestSync(DynamicArray<const ProtocolData*>& requestDataArray, ProtocolData*& responseData, bool deleteData /*= true*/)
	{
		if (!this->outputClient || !this->outputClient->IsConnected())
			return false;

		return this->outputClient->MakeTransactionRequestSync(requestDataArray, responseData, deleteData);
	}

	/*virtual*/ bool PersistentDualClient::RegisterSubscriptionCallback(const char* channel, Callback callback)
	{
		if (!this->inputClient && this->inputClient->IsConnected())
			return false;

		if (!this->inputClient->RegisterSubscriptionCallback(channel, callback))
			return false;

		char command[256];
		sprintf_s(command, sizeof(command), "SUBSCRIBE %s", channel);
		return this->inputClient->MakeRequestAsync(ProtocolData::ParseCommand(command));
	}

	/*virtual*/ bool PersistentDualClient::UnregisterSubscriptionCallback(const char* channel)
	{
		if (!this->inputClient && this->inputClient->IsConnected())
			return false;

		if (!this->inputClient->UnregisterSubscriptionCallback(channel))
			return false;

		char command[256];
		sprintf_s(command, sizeof(command), "UNSUBSCRIBE %s", channel);
		return this->inputClient->MakeRequestAsync(ProtocolData::ParseCommand(command));
	}

	/*virtual*/ bool PersistentDualClient::MessageHandler(const ProtocolData* messageData)
	{
		return false;
	}
}