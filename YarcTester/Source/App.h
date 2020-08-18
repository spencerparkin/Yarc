#pragma once

#include <wx/setup.h>
#include <wx/app.h>
#include <wx/string.h>
#include <yarc_connection_pool.h>

class Frame;

class App : public wxApp
{
public:

	App();
	virtual ~App();

	virtual bool OnInit() override;
	virtual int OnExit() override;

	wxString GetRedisClientExecutablePath();
	wxString GetRedisServerExectuablePath();

	wxString redisBinDir;

private:

	Frame* frame;

	Yarc::ConnectionPool* connectionPool;
};

wxDECLARE_APP(App);