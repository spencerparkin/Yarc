#include <yarc_cluster_client.h>
#include "ClusterTestCase.h"
#include "App.h"
#include <wx/stdpaths.h>
#include <wx/filename.h>
#include <wx/utils.h>
#include <wx/dir.h>

ClusterTestCase::ClusterTestCase(std::streambuf* givenLogStream) : TestCase(givenLogStream)
{
	this->cluster = nullptr;
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
