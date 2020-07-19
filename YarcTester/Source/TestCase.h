#pragma once

#include <yarc_client_iface.h>
#include <streambuf>
#include <ostream>

class TestCase
{
public:

	TestCase(std::streambuf* givenLogStream);
	virtual ~TestCase();

	virtual bool Setup() = 0;
	virtual bool Shutdown() = 0;
	virtual bool PerformAutomatedTesting() { return false; }
	
	Yarc::ClientInterface* GetClientInterface() { return this->client; }

protected:

	Yarc::ClientInterface* client;
	std::ostream logStream;
};