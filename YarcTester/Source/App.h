#pragma once

#include <wx/setup.h>
#include <wx/app.h>
#include <wx/string.h>

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
};

wxDECLARE_APP(App);