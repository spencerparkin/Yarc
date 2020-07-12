#include "SimpleTestCase.h"
#include <yarc_simple_client.h>

SimpleTestCase::SimpleTestCase()
{
}

/*virtual*/ SimpleTestCase::~SimpleTestCase()
{
}

/*virtual*/ bool SimpleTestCase::Setup()
{
	this->client = new Yarc::SimpleClient();

	// TODO: If we fail to connect, start a redis server?  Or start one then connect to it?
	return this->client->Connect("127.0.0.1", 6379);
}

/*virtual*/ bool SimpleTestCase::Shutdown()
{
	this->client->Disconnect();
	delete this->client;
	this->client = nullptr;

	return true;
}