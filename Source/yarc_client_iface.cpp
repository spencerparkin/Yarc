#include "yarc_client_iface.h"
#include "yarc_protocol_data.h"

namespace Yarc
{
	//------------------------- ClientInterface -------------------------

	ClientInterface::ClientInterface(ConnectionConfig* givenConnectionConfig /*= nullptr*/)
	{
		if (!givenConnectionConfig)
			givenConnectionConfig = ConnectionConfig::Create();

		this->connectionConfig = givenConnectionConfig;
		this->pushDataCallback = new Callback;
	}

	/*virtual*/ ClientInterface::~ClientInterface()
	{
		delete this->connectionConfig;
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
		while (!requestServiced)
			if (!this->Update())
				return false;

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

		while (!requestServiced)
			if (!this->Update())
				return false;

		return requestServiced;
	}

	/*virtual*/ bool ClientInterface::RegisterPushDataCallback(Callback givenPushDataCallback)
	{
		*this->pushDataCallback = givenPushDataCallback;
		return true;
	}

	//------------------------- ClientInterface::ConnectionConfig -------------------------

	ClientInterface::ConnectionConfig::ConnectionConfig()
	{
		this->address = new std::string;
		this->port = 6379;
		this->connectionTimeoutSeconds = -1.0;
		this->maxConnectionIdleTimeSeconds = 5.0 * 60.0;
		this->disposition = Disposition::NORMAL;
	}

	/*virtual*/ ClientInterface::ConnectionConfig::~ConnectionConfig()
	{
		delete this->address;
	}

	/*static*/ ClientInterface::ConnectionConfig* ClientInterface::ConnectionConfig::Create(void)
	{
		return new ConnectionConfig();
	}

	ClientInterface::ConnectionConfig* ClientInterface::ConnectionConfig::Clone(void)
	{
		ConnectionConfig* config = new ConnectionConfig();

		*config->address = *this->address;
		config->port = this->port;
		config->connectionTimeoutSeconds = this->connectionTimeoutSeconds;
		config->maxConnectionIdleTimeSeconds = this->maxConnectionIdleTimeSeconds;
		config->disposition = this->disposition;

		return config;
	}
}