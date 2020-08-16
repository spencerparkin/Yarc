# Yarc -- Yet Another Redis Client

Yarc is a C++-based Linux & Windows client for Redis.  It directly supports...

 * RESP, RESP3 (attributes, streaming, maps, etc.)
 * Pipelining
 * Pub-Sub
 * Cluster
 * Transactions
 * Connection Pooling
 
Other Redis features would be exposed indirectly through the raw-use of the Redis server communication protocol.

Here is the hello-world of the Yarc library...

```C++
#include <yarc_simple_client.h>
#include <yarc_protocol_data.h>
#include <yarc_byte_stream.h>
#include <yarc_connection_pool.h>

using namespace Yarc;

int main()
{
	ConnectionPool connectionPool;
	SetConnectionPool(&connectionPool);

	// This connects to 127.0.0.1 on port 6379 by default.
	ClientInterface* client = new SimpleClient();
	client->MakeRequestAsync(ProtocolData::ParseCommand("SET greeting \"Hello, world!\""));

	ProtocolData* responseData = nullptr;
	if (client->MakeRequestSync(ProtocolData::ParseCommand("GET greeting"), responeData))
	{
		BlobStringData* blobStringData = Cast<BlobStringData>(responseData);
		if (blobStringData)
			std::cout << "greeting = " << blobStringData->GetValue() << std::endl;
		
		delete responseData;
	}
		
	delete client;
	return 0;
}
```

Redis Cluster support is obtained by simply replacing `SimpleClient` with `ClusterClient`, and then replacing `#include <yarc_simple_client.h>` with `#include <yarc_cluster_client.h>`.

The client is fairly stable, and I have one production use-case for it.  The tester application thrashes a mini local cluster of 6 nodes (3 masters, 3 slaves, 1 slave per master) all while performing live resharding of the hash slots.  This is as far as the cluster client has been tested.
