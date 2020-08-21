#pragma once

#include "yarc_socket_stream.h"
#include <map>

namespace Yarc
{
	class YARC_API ConnectionPool
	{
	public:
		ConnectionPool();
		virtual ~ConnectionPool();

		SocketStream* CheckoutSocketStream(const Address& address);
		void CheckinSocketStream(SocketStream* socketStream);

	private:

		typedef std::map<std::string, SocketStream*> SocketStreamMap;
		SocketStreamMap* socketStreamMap;
	};

	YARC_API ConnectionPool* GetConnectionPool();
	YARC_API void SetConnectionPool(ConnectionPool* givenConnectionPool);
}