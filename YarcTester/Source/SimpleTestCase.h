#pragma once

#include "TestCase.h"

class SimpleTestCase : public TestCase
{
public:

	SimpleTestCase();
	virtual ~SimpleTestCase();

	virtual bool Setup() override;
	virtual bool Shutdown() override;
};