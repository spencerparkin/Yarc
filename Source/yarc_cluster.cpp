#include "yarc_cluster.h"
#include "yarc_simple_client.h"
#include "yarc_data_types.h"
#include "yarc_misc.h"
#include <filesystem>
#include <fstream>
#include <sstream>

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
		DeleteList<ReductionObject>(*this->migrationList);
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
			redisServerPath = redisServerPath / std::string("redis-server.exe");
			if (!std::filesystem::exists(redisServerPath))
				return false;

			//
			// STEP 2: Blow away the root folder where we'll run the cluster servers.
			//

			std::filesystem::path clusetrRootPath = *this->clusterRootDir;
			std::filesystem::remove_all(clusetrRootPath);
			std::filesystem::create_directory(clusetrRootPath);

			//
			// STEP 3: Bring up all the cluster node servers in cluster mode.
			//

			uint32_t numNodes = this->numMasters + this->numSlavesPerMaster * this->numMasters;

			for (uint32_t i = 0; i < numNodes; i++)
			{
				uint16_t port = i + 7000;

				sprintf_s(buffer, sizeof(buffer), "%04d", port);

				std::filesystem::path nodeFolderPath = clusetrRootPath / std::string(buffer);
				std::filesystem::create_directory(nodeFolderPath);

				std::filesystem::path redisConfFilePath = nodeFolderPath / std::string("redis.conf");
				std::ofstream redisConfStream;
				redisConfStream.open(redisConfFilePath, std::ios::out);

				sprintf_s(buffer, sizeof(buffer), "port %04d", port);
				redisConfStream << std::string(buffer) << std::endl;
				redisConfStream << "cluster-enabled yes" << std::endl;
				redisConfStream << "cluster-node-timeout 5000" << std::endl;
				redisConfStream << "appendonly yes" << std::endl;
				redisConfStream.close();

				PROCESS_INFORMATION procInfo;
				::ZeroMemory(&procInfo, sizeof(procInfo));

				STARTUPINFO startupInfo;
				::ZeroMemory(&startupInfo, sizeof(startupInfo));
				startupInfo.cb = sizeof(startupInfo);
				
				const wchar_t* nodeFolderPathCStr = nodeFolderPath.c_str();
				if (0 != ::_wchdir(nodeFolderPathCStr))
					return false;

				std::wstring command = redisServerPath.c_str();
				command += std::wstring(L" redis.conf");
				if (!::CreateProcessW(nullptr, (LPWSTR)command.c_str(), nullptr, nullptr, FALSE, 0, nullptr, nodeFolderPathCStr, &startupInfo, &procInfo))
					return false;

				Node node;
				node.processHandle = (uint64_t)procInfo.hProcess;
				node.client = new Yarc::SimpleClient();
				this->nodeArray->SetCount(i + 1);
				(*this->nodeArray)[i] = node;
				if (!node.client->Connect("127.0.0.1", port))
					return false;
			}

			//
			// STEP 4: Join the cluster nodes together in the gossip protocol.
			//

			DataType* commandData = nullptr;
			DataType* responseData = nullptr;

			for (uint32_t i = 0; i < nodeArray->GetCount() - 1; i++)
			{
				uint16_t port = i + 7001;
				sprintf_s(buffer, sizeof(buffer), "CLUSTER MEET 127.0.0.1 %04d", port);
				commandData = DataType::ParseCommand(buffer);
				(*this->nodeArray)[i].client->MakeRequestAsync(commandData, [](const DataType* responseData) {
					return true;
				});
			}

			this->FlushAllNodes();

			// We have to wait a bit for knowledge of who's who to propagate/gossip across the cluster.
			::Sleep(5000);

			//
			// STEP 5: Identify the cluster view from an arbitrary node in the cluster.
			//

			commandData = DataType::ParseCommand("CLUSTER NODES");
			if (!(*this->nodeArray)[0].client->MakeRequestSync(commandData, responseData))
				return false;

			BulkString* nodeInfoString = Cast<BulkString>(responseData);
			if (nodeInfoString)
			{
				nodeInfoString->GetString((uint8_t*)buffer, sizeof(buffer));
				std::string csvData = buffer;
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

					i = nodeAddr.find(':');
					if (i != -1)
					{
						std::string portStr = nodeAddr.substr(i + 1, nodeAddr.length() - i - 1);
						uint16_t port = ::atoi(portStr.c_str());
						i = port - 7000;
						Node& node = (*this->nodeArray)[i];
						strcpy_s(node.id, sizeof(node.id), nodeId.c_str());
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

					sprintf_s(buffer, sizeof(buffer), "CLUSTER REPLICATE %s", masterNode.id);
					commandData = DataType::ParseCommand(buffer);
					slaveNode.client->MakeRequestAsync(commandData, [](const DataType*) { return true; });
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
			strcpy_s(commandBuffer, sizeof(commandBuffer), "CLUSTER ADDSLOTS");

			for (uint32_t i = 0; i < 100 && slot <= maxSlot; i++)
			{
				char slotBuffer[32];
				sprintf_s(slotBuffer, sizeof(slotBuffer), " %d", slot++);
				strcat_s(commandBuffer, sizeof(commandBuffer), slotBuffer);
			}

			DataType* commandData = DataType::ParseCommand(commandBuffer);
			client->MakeRequestAsync(commandData, [](const DataType*) { return true; });

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

			DataType* commandData = nullptr;

			for (uint32_t i = 0; i < this->nodeArray->GetCount(); i++)
			{
				Node& node = (*this->nodeArray)[i];
				if (node.client->IsConnected())
				{
					commandData = DataType::ParseCommand("SHUTDOWN NOSAVE");
					node.client->MakeRequestAsync(commandData, [](const DataType*) { return true; });
				}
			}

			this->FlushAllNodes();

			//
			// STEP 2: Wait for all server processes to quit.
			//

			for (uint32_t i = 0; i < this->nodeArray->GetCount(); i++)
			{
				Node& node = (*this->nodeArray)[i];
				::WaitForSingleObject((HANDLE)node.processHandle, INFINITE);
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
			(*this->nodeArray)[i].client->Update(false);

		ReductionObject::ReduceList(this->migrationList);

		return true;
	}

	Cluster::Migration* Cluster::CreateRandomMigration(void)
	{
		if (this->nodeArray->GetCount() == 0)
			return nullptr;

		DataType* responseData = nullptr;
		if (!(*this->nodeArray)[0].client->MakeRequestSync(DataType::ParseCommand("CLUSTER SLOTS"), responseData))
			return nullptr;

		Array* arrayData = Cast<Array>(responseData);
		if (!arrayData || arrayData->GetSize() == 0)
			return nullptr;

		uint32_t i = RandomNumber(0, arrayData->GetSize() - 1);

		Array* entryData = Cast<Array>(arrayData->GetElement(i));

		uint16_t minHashSlot = Cast<Integer>(entryData->GetElement(0))->GetNumber();
		uint16_t maxHashSlot = Cast<Integer>(entryData->GetElement(1))->GetNumber();
		uint16_t hashSlot = RandomNumber(minHashSlot, maxHashSlot);

		char sourceID[256];
		Cast<BulkString>(Cast<Array>(entryData->GetElement(2))->GetElement(2))->GetString((uint8_t*)sourceID, sizeof(sourceID));

		uint32_t j = RandomNumber(0, arrayData->GetSize() - 1);
		while (j == i)
			j = RandomNumber(0, arrayData->GetSize() - 1);

		entryData = Cast<Array>(arrayData->GetElement(j));

		char destinationID[256];
		Cast<BulkString>(Cast<Array>(entryData->GetElement(2))->GetElement(2))->GetString((uint8_t*)destinationID, sizeof(destinationID));

		delete responseData;

		Node* sourceNode = this->FindNodeWithID(sourceID);
		Node* destinationNode = this->FindNodeWithID(destinationID);

		if (!(sourceNode && destinationNode))
			return nullptr;

		return new Migration(sourceNode, destinationNode, hashSlot);
	}

	Cluster::Migration* Cluster::CreateRandomMigrationForHashSlot(uint16_t hashSlot)
	{
		if (this->nodeArray->GetCount() == 0)
			return nullptr;

		DataType* responseData = nullptr;
		if (!(*this->nodeArray)[0].client->MakeRequestSync(DataType::ParseCommand("CLUSTER SLOTS"), responseData))
			return nullptr;

		Array* arrayData = Cast<Array>(responseData);
		if (!arrayData || arrayData->GetSize() == 0)
			return nullptr;

		Node* sourceNode = nullptr;
		Node* destinationNode = nullptr;

		uint32_t i;
		for (i = 0; i < arrayData->GetSize(); i++)
		{
			Array* entryData = Cast<Array>(arrayData->GetElement(i));

			uint16_t minHashSlot = Cast<Integer>(entryData->GetElement(0))->GetNumber();
			uint16_t maxHashSlot = Cast<Integer>(entryData->GetElement(1))->GetNumber();

			if (minHashSlot <= hashSlot && hashSlot <= maxHashSlot)
			{
				char sourceID[256];
				Cast<BulkString>(Cast<Array>(entryData->GetElement(2))->GetElement(2))->GetString((uint8_t*)sourceID, sizeof(sourceID));
				sourceNode = this->FindNodeWithID(sourceID);
				break;
			}
		}

		if (!sourceNode)
			return nullptr;

		while (!destinationNode || destinationNode == sourceNode)
		{
			i = RandomNumber(0, arrayData->GetSize() - 1);
			Array* entryData = Cast<Array>(arrayData->GetElement(i));

			char destinationID[256];
			Cast<BulkString>(Cast<Array>(entryData->GetElement(2))->GetElement(2))->GetString((uint8_t*)destinationID, sizeof(destinationID));
			destinationNode = this->FindNodeWithID(destinationID);
		}

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
		this->lazyCount = 512;
		this->state = State::MARK_IMPORTING;
	}

	/*virtual*/ Cluster::Migration::~Migration()
	{
	}

	/*virtual*/ ReductionObject::ReductionResult Cluster::Migration::Reduce()
	{
		ReductionResult result = RESULT_NONE;

		switch (state)
		{
			case State::MARK_IMPORTING:
			{
				char command[512];
				sprintf_s(command, sizeof(command), "CLUSTER SETSLOT %d IMPORTING %s", this->hashSlot, this->sourceNode->id);

				if (!this->destinationNode->client->MakeRequestAsync(DataType::ParseCommand(command), [=](const DataType* responseData) {
					const Error* error = Cast<Error>(responseData);
					if (error)
						this->state = State::BAIL;
					else
						this->state = State::MARK_MIGRATING;
					return true;
				}))
				{
					this->state = State::BAIL;
				}
				else
				{
					this->state = State::WAITING;
				}

				break;
			}
			case State::MARK_MIGRATING:
			{
				char command[512];
				sprintf_s(command, sizeof(command), "CLUSTER SETSLOT %d MIGRATING %s", this->hashSlot, this->destinationNode->id);

				DataType* responseData = nullptr;
				if (!this->sourceNode->client->MakeRequestAsync(DataType::ParseCommand(command), [=](const DataType* responseData) {
					const Error* error = Cast<Error>(responseData);
					if (error)
						this->state = State::BAIL;
					else
						this->state = State::BE_LAZY;
					return true;
				}))
				{
					this->state = State::BAIL;
				}
				else
				{
					this->state = State::WAITING;
				}

				break;
			}
			case State::BE_LAZY:
			{
				if (this->lazyCount == 0)
					this->state = State::MIGRATING_KEYS;
				else
					this->lazyCount--;
				break;
			}
			case State::MIGRATING_KEYS:
			{
				// Migrate just one key at a time to give the client more opportunities to redirect.
				// Of course, this is innefficient, but the whole point here is to test the client.

				char command[512];
				sprintf_s(command, sizeof(command), "CLUSTER GETKEYSINSLOT %d 1", this->hashSlot);

				DataType* getKeysResponseData = nullptr;
				if (!this->sourceNode->client->MakeRequestSync(DataType::ParseCommand(command), getKeysResponseData))
					this->state = State::BAIL;
				else
				{
					Array* arrayData = Cast<Array>(getKeysResponseData);
					if (arrayData)
					{
						if (arrayData->GetSize() == 0)
							this->state = State::UNMARK;	// All keys migrated.
						else
						{
							BulkString* stringData = Cast<BulkString>(arrayData->GetElement(0));
							if (stringData)
							{
								char keyBuffer[512];
								stringData->GetString((uint8_t*)keyBuffer, sizeof(keyBuffer));
								sprintf_s(command, sizeof(command), "MIGRATE %s %d %s 0 5000", this->destinationNode->client->GetAddress(), this->destinationNode->client->GetPort(), keyBuffer);

								DataType* migrateKeysResponseData = nullptr;
								if (!this->sourceNode->client->MakeRequestSync(DataType::ParseCommand(command), migrateKeysResponseData))
									this->state = State::BAIL;
								else
								{
									Error* error = Cast<Error>(migrateKeysResponseData);
									if (error)
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
				sprintf_s(command, sizeof(command), "CLUSTER SETSLOT %d NODE %s", this->hashSlot, this->destinationNode->id);

				DataType* responseData = nullptr;

				this->destinationNode->client->MakeRequestSync(DataType::ParseCommand(command), responseData);
				delete responseData;

				this->sourceNode->client->MakeRequestSync(DataType::ParseCommand(command), responseData);
				delete responseData;

				this->state = State::BAIL;

				break;
			}
			case State::WAITING:
			{
				// Wait for async request to finish.
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