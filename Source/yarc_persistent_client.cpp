#include "yarc_persistent_client.h"

namespace Yarc
{
	//---------------------------------- PersistentClient ----------------------------------

	PersistentClient::PersistentClient()
	{
		this->persistConnection = false;
		this->pendingRequestList = new ReductionObjectList;
	}

	/*virtual*/ PersistentClient::~PersistentClient()
	{
		DeleteList<ReductionObject>(*this->pendingRequestList);
		delete this->pendingRequestList;
	}

	/*static*/ PersistentClient* PersistentClient::Create()
	{
		return new PersistentClient();
	}

	/*static*/ void PersistentClient::Destroy(PersistentClient* client)
	{
		delete client;
	}

	/*virtual*/ bool PersistentClient::Connect(const char* address, uint16_t port /*= 6379*/, double timeoutSeconds /*= -1.0*/)
	{
		if (this->IsConnected())
			return false;

		if (SimpleClient::Connect(address, port, timeoutSeconds))
		{
			this->persistConnection = true;
			return true;
		}

		return false;
	}

	/*virtual*/ bool PersistentClient::Disconnect()
	{
		if (!this->IsConnected())
			return false;

		if (SimpleClient::Disconnect())
		{
			this->persistConnection = false;
			return true;
		}

		return false;
	}

	/*virtual*/ bool PersistentClient::Update(bool canBlock /*= false*/)
	{
		if (this->IsConnected())
		{
			ReductionObject::ReduceList(this->pendingRequestList);

			return this->Update(canBlock);
		}
		
		if (this->persistConnection)
			return SimpleClient::Connect(this->address->c_str(), this->port, canBlock ? -1.0 : 0.0);

		return true;
	}

	/*virtual*/ bool PersistentClient::MakeRequestAsync(const DataType* requestData, Callback callback /*= [](const DataType*) -> bool { return true; }*/, bool deleteData /*= true*/)
	{
		if (this->IsConnected())
			return SimpleClient::MakeRequestAsync(requestData, callback, deleteData);

		this->pendingRequestList->AddTail(new PendingRequest(requestData, callback, deleteData, this));
		return true;
	}

	void PersistentClient::ProcessPendingRequest(PendingRequest* request)
	{
		SimpleClient::MakeRequestAsync(request->requestData, request->callback, request->deleteData);
	}

	//---------------------------------- PersistentClient::PendingRequest ----------------------------------

	PersistentClient::PendingRequest::PendingRequest(const DataType* givenRequestData, Callback givenCallback, bool givenDeleteData, PersistentClient* givenClient)
	{
		this->requestData = givenRequestData;
		this->callback = givenCallback;
		this->deleteData = givenDeleteData;
		this->client = givenClient;
	}

	/*virtual*/ PersistentClient::PendingRequest::~PendingRequest()
	{
	}

	/*virtual*/ ReductionObject::ReductionResult PersistentClient::PendingRequest::Reduce()
	{
		this->client->ProcessPendingRequest(this);
		return RESULT_DELETE;
	}
}