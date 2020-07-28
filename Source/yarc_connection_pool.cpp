#include "yarc_connection_pool.h"

namespace Yarc
{
	ConnectionPool theConnectionPool;

	ConnectionPool::ConnectionPool()
	{
	}

	/*virtual*/ ConnectionPool::~ConnectionPool()
	{
	}

	SocketStream* ConnectionPool::CheckoutSocketStream(const Address& address)
	{
		return nullptr;
	}

	void ConnectionPool::CheckinSocketStream(SocketStream* socketStream)
	{
	}
}