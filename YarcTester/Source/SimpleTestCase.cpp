#include "SimpleTestCase.h"
#include <yarc_simple_client.h>

SimpleTestCase::SimpleTestCase(std::streambuf* givenLogStream) : TestCase(givenLogStream)
{
}

/*virtual*/ SimpleTestCase::~SimpleTestCase()
{
}

/*virtual*/ bool SimpleTestCase::Setup()
{
	this->client = new Yarc::SimpleClient();

	// TODO: If we fail to connect, start a redis server?  Or start one then connect to it?
	if (!this->client->Connect("127.0.0.1", 6379))
	{
		this->logStream << "Failed to connect to Redis server." << std::endl;
		return false;
	}

	this->logStream << "Connected to Redis server!" << std::endl;
	return true;
}

/*virtual*/ bool SimpleTestCase::Shutdown()
{
	this->client->Disconnect();
	delete this->client;
	this->client = nullptr;

	this->logStream << "Disconnected from Redis server!" << std::endl;

	return true;
}