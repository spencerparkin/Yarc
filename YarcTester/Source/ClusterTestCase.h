#pragma once

#include "TestCase.h"
#include <wx/string.h>
#include <wx/filename.h>
#include <vector>

class ClusterTestCase : public TestCase
{
public:

	ClusterTestCase(std::streambuf* givenLogStream);
	virtual ~ClusterTestCase();

	virtual bool Setup() override;
	virtual bool Shutdown() override;

	bool ExecuteCommand(const wxString& command);

	int NumNodes() { return this->numMasterNodes + this->numMasterNodes * this->numSlavesPerMaster; }

	void RemoveDir(const wxString& rootDir);

	wxFileName clusterRootDir;
	int numMasterNodes;
	int numSlavesPerMaster;
	std::vector<long> nodePidArray;
};