#pragma once

#include "TestCase.h"
#include <yarc_cluster.h>
#include <vector>
#include <map>

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
	std::map<std::string, uint32_t> testKeyMap;

	bool keySettingGettingEnabled;
	bool keySlotMigratingEnabled;
};