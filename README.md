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

using namespace Yarc;

int main()
{
	// This connects to 127.0.0.1 on port 6379 by default.
	auto client = Yarc::SimpleClient::Create();
	client->MakeRequestAsync(ProtocolData::ParseCommand("SET greeting \"Hello, world!\""));

	ProtocolData* responseData = nullptr;
	if (client->MakeRequestSync(ProtocolData::ParseCommand("GET greeting"), responeData))
	{
		BlobStringData* blobStringData = Cast<BlobStringData>(responseData);
		if (blobStringData)
			std::cout << "greeting = " << blobStringData->GetValue() << std::endl;
		
		Yarc::ProtocolData::Destroy(responseData);  // Be sure to free it in the proper heap!
	}
		
	Yarc::SimpleClient::Destroy(client);    // Again, free in proper heap.
	return 0;
}
```

Redis Cluster support is obtained by simply replacing `SimpleClient` with `ClusterClient`, and then replacing `#include <yarc_simple_client.h>` with `#include <yarc_cluster_client.h>`.  It has not been well tested!  I wouldn't trust it.

The simple client is fairly stable, and I have a few production use-cases for it.  As for the cluster client, the tester application thrashes a mini local cluster of 6 nodes (3 masters, 3 slaves, 1 slave per master) all while performing live resharding of the hash slots.  This is as far as the cluster client has been tested, but that was a long time ago, and I'm not sure if it still works.  In any case, I would revisit and revamp the code before I ever tried to use it for some application.

One obvious downside to the API shown above is that it's using an old callback style for asynchronous processing.  A more modern API would use promises or the asyc/await keywords.