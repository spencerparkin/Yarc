# Yarc -- Yet Another Redis Client

I looked around for a C++ Redis client library for a while, but none of them satisfied me.  Either the API was stupid, or it just didn't make sense to me.  Yarc is fantastic because it's super simple and minimalistic.  I find that the smarter a library tries to be, the more assumptions it makes about how you want to use it, and consequently, the more inflexible it often becomes.  Yarc tries to be as bare-bones as possible.  If you want to write your own layer of software that wraps it, then by all means, do so.  All it does is expose the Redis protocol at a slightly higher level using a simple parser.  It then provides a simple synchronous or asynchronous request API.  And that's it!  That's all you need.  Keep it simple-stupid.

I'm sure there's room for improvement and extra features, but none of those will ever get in the way of its low-level nature.  Software should be simple, and it should just work.

Here is the hello-world of the Yarc library...

```C++
#include <yarc_client.h>
#include <yarc_data_types.h>
#include <stdio.h>

using namespace Yarc;

int main()
{
	ClientInterface* client = new SimpleClient();

	if (client->Connect("127.0.0.1", 6379))
	{
		client->MakeRequestAsync(DataType::ParseCommand("set a 1"), [](const DataType*) { return true; });

		DataType* responseData = nullptr;

		if (client->MakeRequestSync(DataType::ParseCommand("get a"), responeData))
		{
			Integer* integer = Cast<Integer>(responseData);
			if (integer)
				printf("a = %d\n", integer->GetNumber());
		
			delete responseData;
		}
		
		delete client;
	}
	
	return 0;
}
```

Redis Cluster support is obtained by simply replacing `SimpleClient` with `ClusterClient`.

My, perhaps, over-simplified understanding of Redis Cluster is as follows.  A cluster is made up of nodes.  These nodes collectively manage the database.  Conceptually, there is just one database, but it is stored across all the nodes.  Redundancy of the data across the nodes, if any, is an implimentation detail we need not concern ourselves with.  From the client's point of view, it can query any node for a response.  That node will either serve the query or redirect the client to the node in the cluster than can likely serve the query, provide it too does not recommend yet another redirection (which can happen in a few cases.)

As Redis is generally just a big key-value store (a shared map between processes), the key space is divided among the nodes of the cluster by hashing a key into one of 16384 slots, a range of which is delegated to each node of the cluster.  Clients should try to keep track of the slot ranges for which each node is responsible in order to minimize the number of redirections required for any query.  It can do this by caching redirections, but it is probably faster to query for the whole new cluster configuration whenever a redirection is required.  Before a client issues a query, it can compute the slot associated with the query (using the same rules performed by the cluster itself), and then use that slot number to determine the likely node that can handle the query.  A stable cluster and client state means that no redirections need occur.

All of the details of communicating with the cluster, of course, are hidden by the `ClusterClient` class.

Reference: [https://redis.io/topics/cluster-spec](https://redis.io/topics/cluster-spec)