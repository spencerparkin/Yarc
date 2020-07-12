#pragma once

#include <yarc_client_iface.h>

class TestCase
{
public:

	TestCase();
	virtual ~TestCase();

	virtual bool Setup() = 0;
	virtual bool Shutdown() = 0;
	
	Yarc::ClientInterface* GetClientInterface() { return this->client; }

protected:

	Yarc::ClientInterface* client;
};