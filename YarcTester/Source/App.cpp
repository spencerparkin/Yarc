#include "App.h"
#include "Frame.h"
#include <wx/filename.h>

wxIMPLEMENT_APP(App);

App::App()
{
	this->frame = nullptr;
	this->redisBinDir = wxT("/home/sparkin/git_repos/redis/src");
	this->connectionPool = new Yarc::ConnectionPool();
	Yarc::SetConnectionPool(this->connectionPool);
}

/*virtual*/ App::~App()
{
	delete this->connectionPool;
}

/*virtual*/ bool App::OnInit()
{
	this->frame = new Frame(nullptr, wxDefaultPosition, wxSize(800, 600));
	this->frame->Show();

	return true;
}

/*virtual*/ int App::OnExit()
{
	return 0;
}

wxString App::GetRedisClientExecutablePath()
{
	wxFileName clientExePath;
#if defined __WINDOWS__
	clientExePath.Assign(this->redisBinDir, wxT("redis-cli.exe"));
#elif defined __LINUX__
	clientExePath.Assign(this->redisBinDir, wxT("redis-cli"));
#endif
	return clientExePath.GetFullPath();
}

wxString App::GetRedisServerExectuablePath()
{
	wxFileName serverExePath;
#if defined __WINDOWS__
	serverExePath.Assign(this->redisBinDir, wxT("redis-server.exe"));
#elif defined __LINUX__
	serverExePath.Assign(this->redisBinDir, wxT("redis-server"));
#endif
	return serverExePath.GetFullPath();
}