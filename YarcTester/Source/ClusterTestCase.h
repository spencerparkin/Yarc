#pragma once

#include "TestCase.h"
#include <yarc_cluster.h>

class ClusterTestCase : public TestCase
{
public:

	ClusterTestCase(std::streambuf* givenLogStream);
	virtual ~ClusterTestCase();

	virtual bool Setup() override;
	virtual bool Shutdown() override;

	Yarc::Cluster* cluster;
};