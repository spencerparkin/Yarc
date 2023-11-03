#include "yarc_connection_pool.h"
#include <assert.h>

namespace Yarc
{
	static ConnectionPool theConnectionPool;

	/*static*/ ConnectionPool* ConnectionPool::Get()
	{
		return &theConnectionPool;
	}

	ConnectionPool::ConnectionPool()
	{
		this->socketStreamMap = new SocketStreamMap;

#if defined __WINDOWS__
		WSADATA data;
		::WSAStartup(MAKEWORD(2, 2), &data);
#endif
	}

	/*virtual*/ ConnectionPool::~ConnectionPool()
	{
		for (SocketStreamMap::iterator mapIter = this->socketStreamMap->begin(); mapIter != this->socketStreamMap->end(); mapIter++)
		{
			SocketStreamList* socketStreamList = mapIter->second;
			for (SocketStreamList::iterator listIter = socketStreamList->begin(); listIter != socketStreamList->end(); listIter++)
			{
				SocketStream* socketStream = *listIter;
				delete socketStream;
			}

			delete socketStreamList;
		}

		delete this->socketStreamMap;

#if defined __WINDOWS__
		::WSACleanup();
#endif
	}

	SocketStream* ConnectionPool::CheckoutSocketStream(const Address& address, double connectionTimeoutSeconds /*= 0.5*/)
	{
		SocketStream* socketStream = nullptr;

		std::string ipPort = address.GetIPAddressAndPort();

		SocketStreamMap::iterator mapIter = this->socketStreamMap->find(ipPort);
		if (mapIter != this->socketStreamMap->end())
		{
			SocketStreamList* socketStreamList = mapIter->second;
			if (socketStreamList->size() > 0)
			{
				SocketStreamList::iterator listIter = socketStreamList->begin();
				socketStream = *listIter;
				socketStreamList->erase(listIter);
			}
		}
		
		if (!socketStream)
		{
			socketStream = new SocketStream();
			if (!socketStream->Connect(address, connectionTimeoutSeconds))
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

		SocketStreamList* socketStreamList = nullptr;
		SocketStreamMap::iterator mapIter = this->socketStreamMap->find(ipPort);
		if (mapIter != this->socketStreamMap->end())
			socketStreamList = mapIter->second;
		else
		{
			socketStreamList = new SocketStreamList;
			this->socketStreamMap->insert(std::pair<std::string, SocketStreamList*>(ipPort, socketStreamList));
		}

		socketStreamList->push_back(socketStream);
	}
}