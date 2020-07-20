#include <yarc_cluster_client.h>
#include <yarc_data_types.h>
#include <yarc_misc.h>
#include "ClusterTestCase.h"
#include "App.h"
#include <wx/stdpaths.h>
#include <wx/filename.h>
#include <wx/utils.h>
#include <wx/dir.h>
#include <stdlib.h>

ClusterTestCase::ClusterTestCase(std::streambuf* givenLogStream) : TestCase(givenLogStream)
{
	this->cluster = nullptr;

	this->testKeyArray.push_back("apples");
	this->testKeyArray.push_back("bannanas");
	this->testKeyArray.push_back("cherries");
	this->testKeyArray.push_back("oranges");
	this->testKeyArray.push_back("strawberries");
	this->testKeyArray.push_back("mangos");
	this->testKeyArray.push_back("kiwies");

	::srand(0);
}

/*virtual*/ ClusterTestCase::~ClusterTestCase()
{
	delete this->cluster;
}

/*virtual*/ bool ClusterTestCase::Setup()
{
	if (!wxFileExists(wxGetApp().GetRedisServerExectuablePath()))
	{
		this->logStream << "Failed to locate redis-server executable!" << std::endl;
		return false;
	}

	if (!this->cluster)
	{
		this->cluster = new Yarc::Cluster();
		this->cluster->numMasters = 3;
		this->cluster->numSlavesPerMaster = 1;
		*this->cluster->redisBinDir = wxGetApp().redisBinDir.c_str();
		*this->cluster->clusterRootDir = wxGetApp().redisBinDir + "\\cluster";

		if (!this->cluster->Setup())
		{
			this->logStream << "Failed to setup local cluster!" << std::endl;
			return false;
		}
	}

	this->logStream << "Cluster up and running!" << std::endl;

	if (!this->client)
	{
		this->client = new Yarc::ClusterClient();

		// We can connect to the cluster by connecting to any node in the cluster.
		const char* address = (*this->cluster->nodeArray)[0].client->GetAddress();
		uint16_t port = (*this->cluster->nodeArray)[0].client->GetPort();

		if (!this->client->Connect(address, port))
		{
			this->logStream << "Failed to connect client to cluster!" << std::endl;
			return false;
		}
	}

	this->logStream << "Connected to cluster!" << std::endl;
	return true;
}

/*virtual*/ bool ClusterTestCase::Shutdown()
{
	if (this->client)
	{
		this->client->Disconnect();
		delete this->client;
		this->client = nullptr;
	}

	if (this->cluster)
	{
		this->cluster->Shutdown();
		delete this->cluster;
		this->cluster = nullptr;
	}

	return true;
}

/*virtual*/ bool ClusterTestCase::PerformAutomatedTesting()
{
	/*if (this->cluster->migrationList->GetCount() < 2)
	{
		uint32_t i = Yarc::RandomNumber(0, this->testKeyArray.size() - 1);
		std::string testKey = this->testKeyArray[i];
		uint16_t hashSlot = Yarc::DataType::CalcKeyHashSlot(testKey);
		Yarc::Cluster::Migration* migration = this->cluster->CreateRandomMigrationForHashSlot(hashSlot);
		if (migration)
			this->cluster->migrationList->AddTail(migration);
	}

	this->cluster->Update();*/

	for (uint32_t i = 0; i < this->testKeyArray.size(); i++)
	{
		std::string testKey = this->testKeyArray[i];
		std::map<std::string, uint32_t>::iterator iter = this->testKeyMap.find(testKey);
		if (iter != this->testKeyMap.end())
		{
			if (iter->second > 0)
				continue;

			this->testKeyMap.erase(iter);
		}

		uint32_t number = Yarc::RandomNumber(1, 1000);
		this->testKeyMap.insert(std::pair<std::string, uint32_t>(testKey, number));

		char setCommand[128];
		sprintf_s(setCommand, sizeof(setCommand), "SET %s %d", testKey.c_str(), number);

		this->client->MakeRequestAsync(Yarc::DataType::ParseCommand(setCommand), [=](const Yarc::DataType* setCommandResponse) {
			const Yarc::Error* error = Yarc::Cast<Yarc::Error>(setCommandResponse);
			if (error)
				this->logStream << "SET command got error response: " << error->GetString() << std::endl;
			else
			{
				char getCommand[128];
				sprintf_s(getCommand, sizeof(getCommand), "GET %s", testKey.c_str());

				this->client->MakeRequestAsync(Yarc::DataType::ParseCommand(getCommand), [=](const Yarc::DataType* getCommandResponse) {
					const Yarc::Error* error = Yarc::Cast<Yarc::Error>(getCommandResponse);
					if (error)
						this->logStream << "GET command got error response: " << error->GetString() << std::endl;
					else
					{
						std::map<std::string, uint32_t>::iterator iter = this->testKeyMap.find(testKey);
						if (iter != this->testKeyMap.end())
						{
							const Yarc::Integer* integerData = Yarc::Cast<Yarc::Integer>(getCommandResponse);
							if (integerData)
							{
								if (iter->second != integerData->GetNumber())
									this->logStream << "Key " << testKey << " failed round-trip!" << std::endl;
								else
									iter->second = 0;
							}

							const Yarc::BulkString* stringData = Yarc::Cast<Yarc::BulkString>(getCommandResponse);
							if (stringData)
							{
								char buffer[32];
								stringData->GetString((uint8_t*)buffer, sizeof(buffer));
								if (iter->second != ::atoi(buffer))
									this->logStream << "Key " << testKey << " failed round-trip!" << std::endl;
								else
									iter->second = 0;
							}
						}
					}

					return true;
				});
			}
			return true;
		});
	}

	return true;
}