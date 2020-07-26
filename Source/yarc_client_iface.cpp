#include "yarc_client_iface.h"
#include "yarc_protocol_data.h"
#include <WS2tcpip.h>

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

	//------------------------- ConnectionConfig -------------------------

	ConnectionConfig::ConnectionConfig()
	{
		this->ipAddress[0] = '\0';
		this->hostname[0] = '\0';
		this->port = 6379;
		this->connectionTimeoutSeconds = -1.0;
		this->maxConnectionIdleTimeSeconds = 5.0 * 60.0;
		this->disposition = Disposition::NORMAL;
	}

	/*virtual*/ ConnectionConfig::~ConnectionConfig()
	{
	}

	void ConnectionConfig::SetIPAddress(const char* givenIPAddress)
	{
		::strcpy_s(this->ipAddress, sizeof(this->ipAddress), givenIPAddress);
	}

	void ConnectionConfig::SetHostname(const char* givenHostname)
	{
		::strcpy_s(this->hostname, sizeof(this->hostname), givenHostname);
	}

	const char* ConnectionConfig::GetResolvedIPAddress() const
	{
		if(::strlen(this->hostname) > 0)
		{
			char portStr[16];
			sprintf_s(portStr, sizeof(portStr), "%d", this->port);

			::addrinfo* addrInfo = nullptr;
			if (0 == ::getaddrinfo(this->hostname, portStr, nullptr, &addrInfo))
			{
				while (addrInfo)
				{
					if (addrInfo->ai_family == AF_INET)
						break;

					addrInfo = addrInfo->ai_next;
				}

				if (addrInfo)
				{
					::sockaddr_in* sockAddr = (::sockaddr_in*)addrInfo->ai_addr;
					sprintf_s(this->ipAddress, sizeof(this->ipAddress), "%d.%d.%d.%d",
								sockAddr->sin_addr.S_un.S_un_b.s_b1,
								sockAddr->sin_addr.S_un.S_un_b.s_b2,
								sockAddr->sin_addr.S_un.S_un_b.s_b3,
								sockAddr->sin_addr.S_un.S_un_b.s_b4);

					
				}
			}
		}
		
		return this->ipAddress;
	}
}