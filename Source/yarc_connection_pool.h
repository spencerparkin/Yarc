#pragma once

#include "yarc_socket_stream.h"

namespace Yarc
{
	class ConnectionPool
	{
	public:
		ConnectionPool();
		virtual ~ConnectionPool();

		SocketStream* CheckoutSocketStream(const Address& address);
		void CheckinSocketStream(SocketStream* socketStream);
	};

	extern ConnectionPool theConnectionPool;
}