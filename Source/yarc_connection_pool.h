#pragma once

#include "yarc_socket_stream.h"
#include <map>

namespace Yarc
{
	class ConnectionPool
	{
	public:
		ConnectionPool();
		virtual ~ConnectionPool();

		SocketStream* CheckoutSocketStream(const Address& address);
		void CheckinSocketStream(SocketStream* socketStream);

	private:

		typedef std::map<std::string, SocketStream*> SocketStreamMap;
		SocketStreamMap socketStreamMap;
	};

	extern ConnectionPool theConnectionPool;
}