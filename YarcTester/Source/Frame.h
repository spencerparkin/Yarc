#pragma once

#include <wx/frame.h>
#include <wx/textctrl.h>
#include <wx/timer.h>

class TestCase;

class Frame : public wxFrame
{
public:

	Frame(wxWindow* parent, const wxPoint& pos, const wxSize& size);
	virtual ~Frame();

	void SetTestCase(TestCase* givenTestCase);
	TestCase* GetTestCase() { return this->testCase; }

private:

	enum
	{
		ID_Exit = wxID_HIGHEST,
		ID_About,
		ID_SimpleTestCase,
		ID_ClusterTestCase,
		ID_Timer,
		ID_LocateRedisBinDir
	};

	void OnExit(wxCommandEvent& event);
	void OnAbout(wxCommandEvent& event);
	void OnSimpleTestCase(wxCommandEvent& event);
	void OnClusterTestCase(wxCommandEvent& event);
	void OnLocateRedisBinDir(wxCommandEvent& event);
	void OnCharHook(wxKeyEvent& event);
	void OnUpdateMenuItemUI(wxUpdateUIEvent& event);
	void OnTimer(wxTimerEvent& event);

	wxTextCtrl* inputText;
	wxTextCtrl* outputText;

	TestCase* testCase;
	
	wxTimer timer;
};