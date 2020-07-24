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

	/*virtual*/ bool PersistentClient::Update()
	{
		if (this->IsConnected())
		{
			ReductionObject::ReduceList(this->pendingRequestList);

			return SimpleClient::Update();
		}
		
		if (this->persistConnection)
			return SimpleClient::Connect(this->address->c_str(), this->port);

		return true;
	}

	/*virtual*/ bool PersistentClient::MakeRequestAsync(const ProtocolData* requestData, Callback callback /*= [](const ProtocolData*) -> bool { return true; }*/, bool deleteData /*= true*/)
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

	PersistentClient::PendingRequest::PendingRequest(const ProtocolData* givenRequestData, Callback givenCallback, bool givenDeleteData, PersistentClient* givenClient)
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