#include "App.h"
#include "Frame.h"
#include <wx/filename.h>

wxIMPLEMENT_APP(App);

App::App()
{
	this->frame = nullptr;
	this->redisBinDir = wxT("E:\\redis");
}

/*virtual*/ App::~App()
{
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
	clientExePath.Assign(this->redisBinDir, wxT("redis-cli.exe"));
	return clientExePath.GetFullPath();
}

wxString App::GetRedisServerExectuablePath()
{
	wxFileName clientExePath;
	clientExePath.Assign(this->redisBinDir, wxT("redis-server.exe"));
	return clientExePath.GetFullPath();
}