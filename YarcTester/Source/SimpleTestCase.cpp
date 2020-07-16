#include <yarc_simple_client.h>
#include "SimpleTestCase.h"
#include "App.h"
#include <wx/utils.h>

SimpleTestCase::SimpleTestCase(std::streambuf* givenLogStream) : TestCase(givenLogStream)
{
}

/*virtual*/ SimpleTestCase::~SimpleTestCase()
{
}

/*virtual*/ bool SimpleTestCase::Setup()
{
	this->client = new Yarc::SimpleClient();

	if (!this->client->Connect("127.0.0.1", 6379))
	{
		this->logStream << "Failed to connect to Redis server.  Trying to start a server..." << std::endl;

		wxString command = wxGetApp().GetRedisServerExectuablePath();
		long pid = wxExecute(command);
		if (pid == 0)
		{
			this->logStream << "Failed to start local Redis server." << std::endl;
			return false;
		}
		else if (!this->client->Connect("127.0.0.1", 6379))
		{
			this->logStream << "Failed to connect to Redis server.  Giving up!" << std::endl;
			return false;
		}
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