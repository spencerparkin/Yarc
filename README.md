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
		client->MakeRequestAsync(DataType::ParseCommand("SET greeting \"Hello, world!\""));

		DataType* responseData = nullptr;

		if (client->MakeRequestSync(DataType::ParseCommand("GET greeting"), responeData))
		{
			BulkString* bulkString = Cast<BulkString>(responseData);
			if (bulkString)
			{
				char buffer[32];
				bulkString->GetString((uint8_t*)buffer, sizeof(buffer));
				printf("greeting = %s\n", buffer);
			}
		
			delete responseData;
		}
		
		delete client;
	}
	
	return 0;
}
```

Redis Cluster support is obtained by simply replacing `SimpleClient` with `ClusterClient`, and then replacing `#include <yarc_simple_client.h>` with `#include <yarc_cluster_client.h>`.

Since Redis commands are just arrays of bulk strings, it is easy to send raw binary data to the database server as follows.

```C++
// Send raw binary data to Redis...
struct { int a, b, c; } data = { 0, 1, 2 };
Yarc::Array* commandArray = new Yarc::Array();
commandArray->SetSize(3);
commandArray->SetElement(0, new Yarc::BulkString("SET"));
commandArray->SetElement(1, new Yarc::BulkString("key"));
commandArray->SetElement(2, new Yarc::BulkString(&data, sizeof(data));
client->MakeRequestAsync(commandArray);
```

This, however, will require byte-swapping when retrieved on platforms with a differing endianness than the machine that sent the data.  To avoid this problem, you could send and retrieve JSON blobs.

```C++
// Send a JSON blob to Redis...
Yarc::Array* commandArray = new Yarc::Array();
commandArray->SetSize(3);
commandArray->SetElement(0, new Yarc::BulkString("SET"));
commandArray->SetElement(1, new Yarc::BulkString("key"));
commandArray->SetElement(2, new Yarc::BulkString("{ "list": [ 0 1 ] }");
client->MakeRequestAsync(commandArray);
```

Use a nice JSON library to parse and print JSON strings.

Note that convenience routines are provided to make the above tasks much easier.  In the first case, we could more easily write it as...

```C++
struct { int a, b, c; } data = { 0, 1, 2 };
Yarc::DataType* commandData = Yarc::Command::Set("key", &data, sizeof(data));
```

...and in the second case, write...

```C++
Yarc::DataType* commandData = Yarc::Command::Set("key", "{ "list": [ 0 1 ] }");
```

Convenience routines are provided for many Redis commands, but there is nothing to stop you from putting together your own protocol tree, and then sending that to the server.