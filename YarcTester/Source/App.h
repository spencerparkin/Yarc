#pragma once

#include <wx/setup.h>
#include <wx/app.h>

class Frame;

class App : public wxApp
{
public:

	App();
	virtual ~App();

	virtual bool OnInit() override;
	virtual int OnExit() override;

private:

	Frame* frame;
};

wxDECLARE_APP(App);