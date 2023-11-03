#pragma once

#include "yarc_socket_stream.h"
#include <map>
#include <list>

namespace Yarc
{
	class ConnectionPool
	{
	public:
		ConnectionPool();
		virtual ~ConnectionPool();

		static ConnectionPool* Get();

		SocketStream* CheckoutSocketStream(const Address& address, double connectionTimeoutSeconds = 0.5);
		void CheckinSocketStream(SocketStream* socketStream);

	private:

		typedef std::list<SocketStream*> SocketStreamList;
		typedef std::map<std::string, SocketStreamList*> SocketStreamMap;
		SocketStreamMap* socketStreamMap;
	};
}