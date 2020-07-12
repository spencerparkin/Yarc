#include "TestCase.h"

TestCase::TestCase()
{
	this->client = nullptr;
}

/*virtual*/ TestCase::~TestCase()
{
	delete this->client;
}