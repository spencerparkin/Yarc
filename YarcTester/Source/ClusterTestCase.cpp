#include "ClusterTestCase.h"

ClusterTestCase::ClusterTestCase()
{
}

/*virtual*/ ClusterTestCase::~ClusterTestCase()
{
}

/*virtual*/ bool ClusterTestCase::Setup()
{
	// TODO: Setup the cluster nodes, join them, then try to connect to one of them.

	return true;
}

/*virtual*/ bool ClusterTestCase::Shutdown()
{
	// TODO: Disconnect from the cluster, then bring the cluster down.

	return true;
}