# Yarc -- Yet Another Redis Client

I looked around for a C++ Redis client library for a while, but none of them satisfied me.  Either the API was stupid, or it just didn't make sense to me.  Yarc is fantastic because it's super simple and minimalistic.  I find that the smarter a library tries to be, the more assumptions it makes about how you want to use it, and consequently, the more inflexible it often becomes.  Yarc tries to be as bare-bones as possible.  If you want to write your own layer of software that wraps it, then by all means, do so.  All it does is expose the Redis protocol as a slightly higher level using a simple parser.  It then provides a simple synchronous or asynchronous request API.  And that's it!  That's all you need.  Keep it simple stupid.

I'm sure there's room for improvement and extra features, but none of those will ever get in the way of its low-level nature.  Software should be simple and should just work.

Here is the hello-world of the Yarc library...

```C++
#include <yarc_client.h>
#include <yarc_data_types.h>
#include <stdio.h>

using namespace Yarc;

int main()
{
	Client* client = new Client();

	if (client->Connect("127.0.0.1", 6379))
	{
		DataType* requestData = DataType::ParseCommand("set a 1");
		client->MakeRequestAsync(requestData, [](const DataType*) { return true; });

		requestData = DataType::ParseCommand("get a");
		DataType* responseData = nullptr;
		if (client->MakeRequestSync(requestData, responeData))
		{
			Integer* integer = Cast<Integer>(responseData);
			if (integer)
				printf("a = %d\n", integer->GetNumber());
		}
	
		client->Disconnect();
		delete client;
	}
	
	return 0;
}
```