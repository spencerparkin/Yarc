#pragma once

#include "TestCase.h"

class ClusterTestCase : public TestCase
{
public:

	ClusterTestCase(std::streambuf* givenLogStream);
	virtual ~ClusterTestCase();

	virtual bool Setup() override;
	virtual bool Shutdown() override;
};