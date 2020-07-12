#include "App.h"
#include "Frame.h"

wxIMPLEMENT_APP(App);

App::App()
{
	this->frame = nullptr;
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