#pragma once

#include "yarc_api.h"
#include "yarc_dynamic_array.h"
#include "yarc_linked_list.h"
#include "yarc_reducer.h"
#include <stdint.h>
#include <string>

namespace Yarc
{
	class SimpleClient;

	// This class is primarily for testing purposes.  It is designed to be a convenient way
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
		bool Update(void);

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

		Node* FindNodeWithID(const char* id);

		void FlushAllNodes(void);
		void AddSlots(SimpleClient* client, uint32_t minSlot, uint32_t maxSlot);

		// Unlike a production-style migration, here we're going to be lazy
		// about the process, because we want plenty of opportunity to exercise
		// the client during various stages of the migration.
		class Migration : public ReductionObject
		{
		public:

			Migration(Node* givenSourceNode, Node* givenDestinationNode, uint16_t givenHashSlot);
			virtual ~Migration();

			virtual ReductionResult Reduce() override;

			enum class State
			{
				MARK_IMPORTING,
				MARK_MIGRATING,
				MIGRATING_KEYS,
				WAITING,
				UNMARK,
				WAIT_FOR_GOSSIP,
				BAIL
			};

			State state;
			Node* sourceNode;
			Node* destinationNode;
			uint16_t hashSlot;
			uint32_t gossipCountdown;
		};

		ReductionObjectList* migrationList;

		Migration* CreateRandomMigration(void);
		Migration* CreateRandomMigrationForHashSlot(uint16_t hashSlot);
	};
}