#pragma once

#include "yarc_api.h"
#include "yarc_dynamic_array.h"
#include <stdint.h>
#include <string>

namespace Yarc
{
	class SimpleClient;

	// This class is mainly for testing purposes.  It is designed to be a convenient way
	// to bring up and tear down a mini redis cluster on the local machine.  As this is
	// for testing and debugging purposes, limited error handling is performed since most
	// such errors will be dealt with when a debugger is attached.
	class YARC_API Cluster
	{
	public:

		Cluster();
		virtual ~Cluster();

		bool Setup(void);
		bool Shutdown(void);

		uint32_t numMasters;
		uint32_t numSlavesPerMaster;
		std::string* redisBinDir;
		std::string* clusterRootDir;

		struct Node
		{
			SimpleClient* client;
			uint64_t processHandle;
			char id[256];
		};

		DynamicArray<Node>* nodeArray;

		void FlushAllNodes(void);
	};
}