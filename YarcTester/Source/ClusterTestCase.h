#pragma once

#include "TestCase.h"

class ClusterTestCase : public TestCase
{
public:

	ClusterTestCase();
	virtual ~ClusterTestCase();

	virtual bool Setup() override;
	virtual bool Shutdown() override;
};