#pragma once

#include <wx/panel.h>
#include <wx/aui/auibook.h>

// A panel appears on a note-book control that is docked to the main window.
// Panels can be moved within a note-book control, or from one to another.
class YarcTesterPanel : public wxPanel
{
public:

	wxDECLARE_DYNAMIC_CLASS(YarcTesterPanel);

	YarcTesterPanel();
	virtual ~YarcTesterPanel();

	virtual wxString GetTitle();
};