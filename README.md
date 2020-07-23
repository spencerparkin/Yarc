# Yarc -- Yet Another Redis Client

I looked around for a C++ Redis client library for a while, but I didn't like any of them.  Either the API was stupid, or it just didn't make sense to me.  So I wrote my own Redis client, Yarc.  Yarc is better for me, because it's super simple and minimalistic.  I find that the smarter a library tries to be, the more assumptions it makes about how you want to use it; and consequently, the more inflexible it often becomes.  Yarc tries to be as bare-bones as possible.  If you want to write your own layer of software that wraps it, then by all means, do so.  All Yarc does is expose the Redis protocol at a slightly higher level using a simple parser.  It then provides a simple asynchronous request API, and that's it!  That's all you need.  Keep it simple-stupid.

I'm sure there's room for improvement and extra features, but none of those will ever get in the way of Yarc's low-level nature.  Software should be simple, and it should just work.

Here is the hello-world of the Yarc library...

```C++
#include <yarc_simple_client.h>
#include <yarc_data_types.h>
#include <stdio.h>

using namespace Yarc;

int main()
{
	ClientInterface* client = new SimpleClient();

	if (client->Connect("127.0.0.1", 6379))
	{
		client->MakeRequestAsync(DataType::ParseCommand("set a 1"));

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

Redis Cluster support is obtained by simply replacing `SimpleClient` with `ClusterClient`, and then replacing `#include <yarc_simple_client.h>` with `#include <yarc_cluster_client.h>`.

As of this writing, the cluster client has been thrashed against a mini local cluster of 6 nodes of 3 masters and 3 slaves, 1 slave per master; all while performing live resharding of hash slots involving the keys in question being continually set and queried, then checked against an in-memory map to verify the round-trip.  If the cluster client can get some real-world use out of it, whatever remaining bugs or difficiencies in it can get eliminated, and then become a reliable work-horse.  Presently I have no real-world use-case for a cluster client, though, because I just wrote it for fun.

Also note that this client library has only been tested on Windows, and only compiles on that platform.  It probably wouldn't be hard to port it to Linux or iOS, however, as it doesn't make use of any OS-specific calls, and the testing application was developed using the cross-platform GUI-toolkit wxWidgets.