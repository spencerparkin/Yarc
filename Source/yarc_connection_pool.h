#pragma once

#include "yarc_socket_stream.h"
#include <map>
#include <list>

namespace Yarc
{
	class YARC_API ConnectionPool
	{
	public:
		ConnectionPool();
		virtual ~ConnectionPool();

		static ConnectionPool* Create();
		static void Destroy(ConnectionPool* connectionPool);

		SocketStream* CheckoutSocketStream(const Address& address);
		void CheckinSocketStream(SocketStream* socketStream);

	private:

		typedef std::list<SocketStream*> SocketStreamList;
		typedef std::map<std::string, SocketStreamList*> SocketStreamMap;
		SocketStreamMap* socketStreamMap;
	};

	YARC_API ConnectionPool* GetConnectionPool();
	YARC_API void SetConnectionPool(ConnectionPool* givenConnectionPool);
}