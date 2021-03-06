#include "yarc_cluster.h"
#include "yarc_simple_client.h"
#include "yarc_protocol_data.h"
#include "yarc_misc.h"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdio.h>
#if defined __LINUX__
#	include <unistd.h>
#endif

namespace Yarc
{
	//------------------------------ Cluster ------------------------------

	Cluster::Cluster()
	{
		this->numMasters = 3;
		this->numSlavesPerMaster = 1;
		this->redisBinDir = new std::string;
		this->clusterRootDir = new std::string;
		this->nodeArray = new DynamicArray<Node>();
		this->migrationList = new ReductionObjectList;
	}

	/*virtual*/ Cluster::~Cluster()
	{
		delete this->redisBinDir;
		delete this->clusterRootDir;
		delete this->nodeArray;
		DeleteList<ReductionObject*>(*this->migrationList);
		delete this->migrationList;
	}

	bool Cluster::Setup(void)
	{
		char buffer[1024];

		try
		{
			if (this->nodeArray->GetCount() > 0)
				return false;

			if (this->numMasters == 0)
				return false;

			if (this->numSlavesPerMaster == 0)
				return false;

			//
			// STEP 1: Make sure we can locate the redis-server executable.
			//


			std::filesystem::path redisServerPath = *this->redisBinDir;
#if defined __WINDOWS__
			redisServerPath = redisServerPath / std::string("redis-server.exe");
#elif defined __LINUX__
			redisServerPath = redisServerPath / std::string("redis-server");
#endif
			if (!std::filesystem::exists(redisServerPath))
				return false;

			//
			// STEP 2: Blow away the root folder where we'll run the cluster servers.
			//

			std::filesystem::path clusterRootPath = *this->clusterRootDir;
			std::filesystem::remove_all(clusterRootPath);
			if(!std::filesystem::create_directory(clusterRootPath))
				return false;

			//
			// STEP 3: Bring up all the cluster node servers in cluster mode.
			//

			uint32_t numNodes = this->numMasters + this->numSlavesPerMaster * this->numMasters;

			for (uint32_t i = 0; i < numNodes; i++)
			{
				uint16_t port = i + 7000;

				sprintf(buffer, "%04d", port);

				std::filesystem::path nodeFolderPath = clusterRootPath / std::string(buffer);
				if(!std::filesystem::create_directory(nodeFolderPath))
					return false;

				std::filesystem::path redisConfFilePath = nodeFolderPath / std::string("redis.conf");
				std::ofstream redisConfStream;
				redisConfStream.open(redisConfFilePath, std::ios::out);
				if(redisConfStream.rdstate() & std::ios::failbit)
					return false;

				sprintf(buffer, "port %04d", port);
				redisConfStream << std::string(buffer) << std::endl;
				redisConfStream << "cluster-enabled yes" << std::endl;
				redisConfStream << "cluster-node-timeout 5000" << std::endl;
				redisConfStream << "appendonly yes" << std::endl;
				redisConfStream.close();

#if defined __WINDOWS__
				const wchar_t* nodeFolderPathCStr = nodeFolderPath.c_str();
				if (0 != ::_wchdir(nodeFolderPathCStr))
					return false;
#elif defined __LINUX__
				if(0 != ::chdir(nodeFolderPath.c_str()))
					return false;
#endif

				Process* process = new Process();
				std::string command = redisServerPath.generic_string() + std::string(" redis.conf");
				if(!process->Spawn(command))
				{
					delete process;
					return false;
				}

				Node node;
				node.process = process;
				node.client = new SimpleClient();
				node.client->address.SetIPAddress("127.0.0.1");
				node.client->address.port = port;
				this->nodeArray->SetCount(i + 1);
				(*this->nodeArray)[i] = node;
			}

			//
			// STEP 4: Join the cluster nodes together in the gossip protocol.
			//

			ProtocolData* commandData = nullptr;
			ProtocolData* responseData = nullptr;

			for (uint32_t i = 0; i < nodeArray->GetCount() - 1; i++)
			{
				uint16_t port = i + 7001;
				sprintf(buffer, "CLUSTER MEET 127.0.0.1 %04d", port);
				commandData = ProtocolData::ParseCommand(buffer);
				(*this->nodeArray)[i].client->MakeRequestAsync(commandData, [](const ProtocolData* responseData) {
					return true;
				});
			}

			this->FlushAllNodes();

			// We have to wait a bit for knowledge of who's who to propagate/gossip across the cluster.
#if defined __WINDOWS__
			::Sleep(5000);
#elif defined __LINUX__
			::sleep(5);
#endif

			//
			// STEP 5: Identify the cluster view from an arbitrary node in the cluster.
			//

			commandData = ProtocolData::ParseCommand("CLUSTER NODES");
			if (!(*this->nodeArray)[0].client->MakeRequestSync(commandData, responseData))
				return false;

			BlobStringData* nodeInfoStringData = Cast<BlobStringData>(responseData);
			if (nodeInfoStringData)
			{
				std::string csvData = nodeInfoStringData->GetValue();
				std::istringstream lineStream(csvData);
				std::string line;
				while (std::getline(lineStream, line, '\n'))
				{
					std::string nodeId, nodeAddr;
					uint32_t i = 0;
					std::istringstream wordStream(line);
					std::string word;
					while (std::getline(wordStream, word, ' '))
					{
						if (i == 0)
							nodeId = word;
						else if (i == 1)
							nodeAddr = word;
						i++;
					}

					i = (uint32_t)nodeAddr.find(':');
					if (i != uint32_t(-1))
					{
						std::string portStr = nodeAddr.substr(i + 1, nodeAddr.length() - i - 1);
						uint16_t port = ::atoi(portStr.c_str());
						i = port - 7000;
						Node& node = (*this->nodeArray)[i];
						strcpy(node.id, nodeId.c_str());
					}
				}
			}

			delete responseData;

			//
			// STEP 6: Evenly distribute the slot-space over the master nodes in the cluster.
			//

			uint32_t totalSlots = 16384;
			uint32_t slotsPerMaster = totalSlots / this->numMasters;
			uint32_t slotBase = 0;
			
			for (uint32_t i = 0; i < this->nodeArray->GetCount(); i++)
			{
				if ((i % (1 + this->numSlavesPerMaster)) == 0)
				{
					Node& masterNode = (*this->nodeArray)[i];
					this->AddSlots(masterNode.client, slotBase, slotBase + slotsPerMaster - 1);
					slotBase += slotsPerMaster;
				}
			}

			if (slotBase < totalSlots)
			{
				Node& masterNode = (*this->nodeArray)[0];
				this->AddSlots(masterNode.client, slotBase, totalSlots - 1);
			}

			this->FlushAllNodes();

			//
			// STEP 7: Lastly, slave the slave nodes to their masters.
			//

			for (uint32_t i = 0; i < this->nodeArray->GetCount(); i++)
			{
				if ((i % (1 + this->numSlavesPerMaster)) != 0)
				{
					uint32_t j = i - (i % (1 + this->numSlavesPerMaster));

					Node& slaveNode = (*this->nodeArray)[i];
					Node& masterNode = (*this->nodeArray)[j];

					sprintf(buffer, "CLUSTER REPLICATE %s", masterNode.id);
					commandData = ProtocolData::ParseCommand(buffer);
					slaveNode.client->MakeRequestAsync(commandData, [](const ProtocolData*) { return true; });
				}
			}

			this->FlushAllNodes();
		}
		catch (std::exception&)
		{
			return false;
		}

		return true;
	}

	void Cluster::AddSlots(SimpleClient* client, uint32_t minSlot, uint32_t maxSlot)
	{
		char commandBuffer[10 * 1024];

		uint32_t slot = minSlot;
		while (slot <= maxSlot)
		{
			strcpy(commandBuffer, "CLUSTER ADDSLOTS");

			for (uint32_t i = 0; i < 100 && slot <= maxSlot; i++)
			{
				char slotBuffer[32];
				sprintf(slotBuffer, " %d", slot++);
				strcat(commandBuffer, slotBuffer);
			}

			ProtocolData* commandData = ProtocolData::ParseCommand(commandBuffer);
			client->MakeRequestAsync(commandData, [](const ProtocolData*) { return true; });

			// Not sure why we have to do this, but as the saying goes...flush early, flush often.
			client->Flush();
		}
	}

	bool Cluster::Shutdown(void)
	{
		try
		{
			if (this->nodeArray->GetCount() == 0)
				return false;

			//
			// STEP 1: Signal the cluster nodes to shutdown.
			//

			ProtocolData* commandData = nullptr;

			for (uint32_t i = 0; i < this->nodeArray->GetCount(); i++)
			{
				Node& node = (*this->nodeArray)[i];
				commandData = ProtocolData::ParseCommand("SHUTDOWN NOSAVE");
				node.client->MakeRequestAsync(commandData, [](const ProtocolData*) { return true; });
			}

			this->FlushAllNodes();

			//
			// STEP 2: Wait for all server processes to quit.
			//

			for (uint32_t i = 0; i < this->nodeArray->GetCount(); i++)
			{
				Node& node = (*this->nodeArray)[i];
				node.process->WaitForExit();
				delete node.process;
			}

			//
			// STEP 3: Delete our clients.
			//

			for (uint32_t i = 0; i < this->nodeArray->GetCount(); i++)
			{
				Node& node = (*this->nodeArray)[i];
				delete node.client;
			}

			this->nodeArray->SetCount(0);
		}
		catch (std::exception&)
		{
			return false;
		}

		return true;
	}

	void Cluster::FlushAllNodes(void)
	{
		for (uint32_t i = 0; i < nodeArray->GetCount(); i++)
			(*this->nodeArray)[i].client->Flush();
	}

	bool Cluster::Update(void)
	{
		for (uint32_t i = 0; i < nodeArray->GetCount(); i++)
			(*this->nodeArray)[i].client->Update();

		ReductionObject::ReduceList(this->migrationList);

		return true;
	}

	Cluster::Migration* Cluster::CreateRandomMigration(void)
	{
		if (this->nodeArray->GetCount() == 0)
			return nullptr;

		ProtocolData* responseData = nullptr;
		if (!(*this->nodeArray)[0].client->MakeRequestSync(ProtocolData::ParseCommand("CLUSTER SLOTS"), responseData))
			return nullptr;

		ArrayData* arrayData = Cast<ArrayData>(responseData);
		if (!arrayData || arrayData->GetCount() == 0)
		{
			delete responseData;
			return nullptr;
		}

		uint32_t i = RandomNumber(0, arrayData->GetCount() - 1);

		ArrayData* entryData = Cast<ArrayData>(arrayData->GetElement(i));

		uint16_t minHashSlot = (uint16_t)Cast<NumberData>(entryData->GetElement(0))->GetValue();
		uint16_t maxHashSlot = (uint16_t)Cast<NumberData>(entryData->GetElement(1))->GetValue();
		uint16_t hashSlot = RandomNumber(minHashSlot, maxHashSlot);

		std::string sourceID = Cast<BlobStringData>(Cast<ArrayData>(entryData->GetElement(2))->GetElement(2))->GetValue();

		uint32_t j = RandomNumber(0, arrayData->GetCount() - 1);
		while (j == i)
			j = RandomNumber(0, arrayData->GetCount() - 1);

		entryData = Cast<ArrayData>(arrayData->GetElement(j));

		std::string destinationID = Cast<BlobStringData>(Cast<ArrayData>(entryData->GetElement(2))->GetElement(2))->GetValue();

		delete responseData;

		Node* sourceNode = this->FindNodeWithID(sourceID.c_str());
		Node* destinationNode = this->FindNodeWithID(destinationID.c_str());

		if (!(sourceNode && destinationNode))
			return nullptr;

		return new Migration(sourceNode, destinationNode, hashSlot);
	}

	Cluster::Migration* Cluster::CreateRandomMigrationForHashSlot(uint16_t hashSlot)
	{
		if (this->nodeArray->GetCount() == 0)
			return nullptr;

		ProtocolData* responseData = nullptr;
		if (!(*this->nodeArray)[0].client->MakeRequestSync(ProtocolData::ParseCommand("CLUSTER SLOTS"), responseData))
			return nullptr;

		ArrayData* arrayData = Cast<ArrayData>(responseData);
		if (!arrayData || arrayData->GetCount() == 0)
		{
			delete responseData;
			return nullptr;
		}

		Node* sourceNode = nullptr;
		Node* destinationNode = nullptr;

		uint32_t i;
		for (i = 0; i < arrayData->GetCount(); i++)
		{
			ArrayData* entryData = Cast<ArrayData>(arrayData->GetElement(i));

			uint16_t minHashSlot = (uint16_t)Cast<NumberData>(entryData->GetElement(0))->GetValue();
			uint16_t maxHashSlot = (uint16_t)Cast<NumberData>(entryData->GetElement(1))->GetValue();

			if (minHashSlot <= hashSlot && hashSlot <= maxHashSlot)
			{
				ArrayData* addressArrayData = Cast<ArrayData>(entryData->GetElement(2));
				if (addressArrayData && addressArrayData->GetCount() >= 3)
				{
					std::string sourceID = Cast<BlobStringData>(addressArrayData->GetElement(2))->GetValue();
					sourceNode = this->FindNodeWithID(sourceID.c_str());
					break;
				}
			}
		}

		if (!sourceNode)
		{
			delete responseData;
			return nullptr;
		}

		while (!destinationNode || destinationNode == sourceNode)
		{
			i = RandomNumber(0, arrayData->GetCount() - 1);
			ArrayData* entryData = Cast<ArrayData>(arrayData->GetElement(i));

			std::string destinationID = Cast<BlobStringData>(Cast<ArrayData>(entryData->GetElement(2))->GetElement(2))->GetValue();
			destinationNode = this->FindNodeWithID(destinationID.c_str());
		}

		delete responseData;
		return new Migration(sourceNode, destinationNode, hashSlot);
	}

	Cluster::Node* Cluster::FindNodeWithID(const char* id)
	{
		for (uint32_t i = 0; i < this->nodeArray->GetCount(); i++)
			if (strcmp((*this->nodeArray)[i].id, id) == 0)
				return &(*this->nodeArray)[i];

		return nullptr;
	}

	//------------------------------ Cluster::Migration ------------------------------

	Cluster::Migration::Migration(Node* givenSourceNode, Node* givenDestinationNode, uint16_t givenHashSlot)
	{
		this->sourceNode = givenSourceNode;
		this->destinationNode = givenDestinationNode;
		this->hashSlot = givenHashSlot;
		this->state = State::MARK_IMPORTING;
		this->gossipCountdown = 512;
	}

	/*virtual*/ Cluster::Migration::~Migration()
	{
	}

	/*virtual*/ ReductionObject::ReductionResult Cluster::Migration::Reduce(void* userData)
	{
		ReductionResult result = RESULT_NONE;

		switch (state)
		{
			case State::MARK_IMPORTING:
			{
				char command[512];
				sprintf(command, "CLUSTER SETSLOT %d IMPORTING %s", this->hashSlot, this->sourceNode->id);

				this->destinationNode->client->MakeRequestAsync(ProtocolData::ParseCommand(command), [this](const ProtocolData* responseData) {
					const SimpleErrorData* errorData = Cast<SimpleErrorData>(responseData);
					if (errorData)
						this->state = State::BAIL;
					else
						this->state = State::MARK_MIGRATING;
					return true;
				});
				
				this->state = State::WAITING;
				break;
			}
			case State::MARK_MIGRATING:
			{
				char command[512];
				sprintf(command, "CLUSTER SETSLOT %d MIGRATING %s", this->hashSlot, this->destinationNode->id);

				this->sourceNode->client->MakeRequestAsync(ProtocolData::ParseCommand(command), [this](const ProtocolData* responseData) {
					if (responseData->IsError())
						this->state = State::BAIL;
					else
						this->state = State::MIGRATING_KEYS;
					return true;
				});
				
				this->state = State::WAITING;
				break;
			}
			case State::MIGRATING_KEYS:
			{
				// Migrate just one key at a time to give the client more opportunities to redirect.
				// Of course, this is innefficient, but the whole point here is to test the client.

				char command[512];
				sprintf(command, "CLUSTER GETKEYSINSLOT %d 1", this->hashSlot);

				ProtocolData* getKeysResponseData = nullptr;
				if (!this->sourceNode->client->MakeRequestSync(ProtocolData::ParseCommand(command), getKeysResponseData))
					this->state = State::BAIL;
				else
				{
					ArrayData* arrayData = Cast<ArrayData>(getKeysResponseData);
					if (arrayData)
					{
						if (arrayData->GetCount() == 0)
							this->state = State::UNMARK;	// All keys migrated.
						else
						{
							BlobStringData* stringData = Cast<BlobStringData>(arrayData->GetElement(0));
							if (stringData)
							{
								std::string keyBuffer = stringData->GetValue();
								const Address& address = this->destinationNode->client->GetSocketStream()->GetAddress();
								sprintf(command, "MIGRATE %s %d %s 0 5000", address.ipAddress, address.port, keyBuffer.c_str());

								ProtocolData* migrateKeysResponseData = nullptr;
								if (!this->sourceNode->client->MakeRequestSync(ProtocolData::ParseCommand(command), migrateKeysResponseData))
									this->state = State::BAIL;
								else
								{
									if (migrateKeysResponseData->IsError())
										this->state = State::BAIL;

									delete migrateKeysResponseData;
								}
							}
						}
					}

					delete getKeysResponseData;
				}

				break;
			}
			case State::UNMARK:
			{
				// Note that it is good practice to set this on all nodes of the cluster, but not necessary.
				// It will propagate across the cluster using the gossip protocol.

				char command[512];
				sprintf(command, "CLUSTER SETSLOT %d NODE %s", this->hashSlot, this->destinationNode->id);

				ProtocolData* responseData = nullptr;

				this->destinationNode->client->MakeRequestSync(ProtocolData::ParseCommand(command), responseData);
				delete responseData;

				this->sourceNode->client->MakeRequestSync(ProtocolData::ParseCommand(command), responseData);
				delete responseData;

				this->state = State::WAIT_FOR_GOSSIP;

				break;
			}
			case State::WAITING:
			{
				// Wait for async request to finish.
				break;
			}
			case State::WAIT_FOR_GOSSIP:
			{
				// If we don't do this, we can get into an odd state where a key is redirected
				// between two nodes forever, and each time we keep regetting the same cluster
				// configuration.  I'm not sure what's really going on, but this seems to fix it.
				if (this->gossipCountdown == 0)
					this->state = State::BAIL;
				else
					this->gossipCountdown--;

				break;
			}
			case State::BAIL:
			{
				result = RESULT_DELETE;
				break;
			}
		}

		return result;
	}
}