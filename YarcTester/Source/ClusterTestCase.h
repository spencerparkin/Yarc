#pragma once

#include "TestCase.h"
#include <yarc_cluster.h>
#include <vector>

class ClusterTestCase : public TestCase
{
public:

	ClusterTestCase(std::streambuf* givenLogStream);
	virtual ~ClusterTestCase();

	virtual bool Setup() override;
	virtual bool Shutdown() override;
	virtual bool PerformAutomatedTesting() override;

	Yarc::Cluster* cluster;

	std::vector<std::string> testKeyArray;
};