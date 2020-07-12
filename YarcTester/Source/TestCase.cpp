#include "TestCase.h"

TestCase::TestCase(std::streambuf* givenLogStream) : logStream(givenLogStream)
{
	this->client = nullptr;
}

/*virtual*/ TestCase::~TestCase()
{
	delete this->client;
}