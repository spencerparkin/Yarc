#include "yarc_connection_pool.h"
#include <assert.h>

namespace Yarc
{
	static ConnectionPool* theConnectionPool = nullptr;

	ConnectionPool* GetConnectionPool()
	{
		assert(theConnectionPool != nullptr);
		return theConnectionPool;
	}

	void SetConnectionPool(ConnectionPool* givenConnectionPool)
	{
		theConnectionPool = givenConnectionPool;
	}

	ConnectionPool::ConnectionPool()
	{
		this->socketStreamMap = new SocketStreamMap;
	}

	/*virtual*/ ConnectionPool::~ConnectionPool()
	{
		for (SocketStreamMap::iterator iter = this->socketStreamMap->begin(); iter != this->socketStreamMap->end(); iter++)
		{
			SocketStream* socketStream = iter->second;
			delete socketStream;
		}

		delete this->socketStreamMap;
	}

	SocketStream* ConnectionPool::CheckoutSocketStream(const Address& address)
	{
		SocketStream* socketStream = nullptr;

		std::string ipPort = address.GetIPAddressAndPort();

		SocketStreamMap::iterator iter = this->socketStreamMap->find(ipPort);
		if (iter != this->socketStreamMap->end())
			socketStream = iter->second;
		else
		{
			socketStream = new SocketStream();
			if (!socketStream->Connect(address))
			{
				delete socketStream;
				socketStream = nullptr;
			}
		}

		return socketStream;
	}

	void ConnectionPool::CheckinSocketStream(SocketStream* socketStream)
	{
		std::string ipPort = socketStream->GetAddress().GetIPAddressAndPort();

		SocketStreamMap::iterator iter = this->socketStreamMap->find(ipPort);
		if (iter != this->socketStreamMap->end())
			delete socketStream;
		else
			this->socketStreamMap->insert(std::pair<std::string, SocketStream*>(ipPort, socketStream));
	}
}