#include <yarc_cluster_client.h>
#include "ClusterTestCase.h"
#include "App.h"
#include <wx/stdpaths.h>
#include <wx/filename.h>
#include <wx/utils.h>
#include <wx/dir.h>

ClusterTestCase::ClusterTestCase(std::streambuf* givenLogStream) : TestCase(givenLogStream)
{
	this->numMasterNodes = 3;
	this->numSlavesPerMaster = 1;
}

/*virtual*/ ClusterTestCase::~ClusterTestCase()
{
}

/*virtual*/ bool ClusterTestCase::Setup()
{
	wxString command;
	long pid = 0;

	this->clusterRootDir.AssignDir(wxStandardPaths::Get().GetTempDir());
	this->clusterRootDir.AppendDir(wxT("YarcTestCluster"));

	if (!this->clusterRootDir.DirExists())
	{
		if (!wxMkdir(this->clusterRootDir.GetFullPath()))
		{
			this->logStream << "Failed to create cluster root directory: " << this->clusterRootDir.GetFullPath() << std::endl;
			return false;
		}
	}

	for (int i = 0; i < this->NumNodes(); i++)
	{
		int port = i + 7000;

		wxFileName nodeDir;
		nodeDir.AssignDir(this->clusterRootDir.GetFullPath());
		nodeDir.AppendDir(wxString::Format("%04d", port));

		if (!nodeDir.DirExists())
		{
			if (!wxMkdir(nodeDir.GetFullPath()))
			{
				this->logStream << "Failed to create node directory: " << nodeDir.GetFullPath() << std::endl;
				return false;
			}
		}

		wxString configContents = wxString::Format(
			"port %04d\n"
			"cluster-enabled yes\n"
			"cluster-config-file nodes.conf\n"
			"cluster-node-timeout 5000\n"
			"appendonly yes\n",
			port);

		wxFileName redisConfigFile;
		redisConfigFile.Assign(nodeDir.GetFullPath(), wxT("redis.conf"));

		wxFile configFile(redisConfigFile.GetFullPath(), wxFile::write);
		if (!configFile.Write(configContents))
		{
			this->logStream << "Failed to write config contents for: " << redisConfigFile.GetFullPath() << std::endl;
			return false;
		}

		command = wxGetApp().GetRedisServerExectuablePath() + wxT(" ") + redisConfigFile.GetFullPath();
		wxExecuteEnv env;
		env.cwd = nodeDir.GetFullPath();
		pid = ::wxExecute(command, 0, nullptr, &env);
		if (pid == 0)
		{
			this->logStream << "Failed to launch Redis node server for port " << i << std::endl;
			return false;
		}

		this->nodePidArray.push_back(pid);
	}

	// TODO: The --cluster option is, apparently, not supported on windows.  See...
	//       https://medium.com/@michaelmaier_32241/redis-cluster-in-windows-a0dfe6e85fa
	//       ...for what may be some steps to making a cluster on windows.  I'll have
	//       to refactor my code quite a bit.  It's 1:00 a.m. now, so time for bed!!
	//       Note that slaves will need a different config than masters.  See, for example,
	//       the "slaveof <masterip> <masterport>" option.  See also...
	//       https://redis.io/commands/cluster-meet
	//       https://redis.io/commands/cluster-replicate
	//       https://redis.io/commands/cluster-addslots
	//       Go to bed!!!!!

	command = wxGetApp().GetRedisClientExecutablePath() + wxT(" --cluster create");
	for (int i = 0; i < this->NumNodes(); i++)
		command += wxString::Format(" 127.0.0.1:%04d", i + 7000);
	
	command += wxString::Format(" --cluster-replicas %d", this->numSlavesPerMaster);
	if (!this->ExecuteCommand(command))
		return false;

	this->client = new Yarc::ClusterClient();
	if (!this->client->Connect("127.0.0.1", 7000))
	{
		this->logStream << "Failed to connect client to cluster!" << std::endl;
		return false;
	}

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

	for (int i = 0; i < this->NumNodes(); i++)
	{
		int port = i + 7000;
		wxString command = wxGetApp().GetRedisClientExecutablePath();
		command += wxString::Format(" -p %04d shutdown nosave", port);
		if (!this->ExecuteCommand(command))
		{
			this->logStream << "Shutdown command failed, trying to kill..." << std::endl;
			long pid = this->nodePidArray[i];
			if (0 != ::wxKill(pid))
				this->logStream << "Kill failed!" << std::endl;
		}
	}

	this->RemoveDir(this->clusterRootDir.GetFullPath());

	return true;
}

void ClusterTestCase::RemoveDir(const wxString& rootDir)
{
	if (wxDir::Exists(rootDir))
	{
		wxDir dir(rootDir);
		wxString file;
		if (dir.GetFirst(&file))
		{
			do
			{
				wxFileName fileName;
				fileName.Assign(rootDir, file);

				if (fileName.IsDir())
					this->RemoveDir(fileName.GetFullPath());
				else
					wxRemoveFile(fileName.GetFullPath());

			} while (dir.GetNext(&file));
		}

		wxDir::Remove(rootDir);
	}
}

bool ClusterTestCase::ExecuteCommand(const wxString& command)
{
	this->logStream << "Executing: " << command << std::endl;
	wxArrayString commandResultArray;
	long exitCode = ::wxExecute(command, commandResultArray, wxEXEC_BLOCK);
	if (exitCode != 0)
		this->logStream << "Execution failed!" << std::endl;
	
	for (int i = 0; i < (signed)commandResultArray.GetCount(); i++)
		this->logStream << commandResultArray[i] << std::endl;

	return exitCode == 0;
}