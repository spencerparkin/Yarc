#include "Frame.h"
#include "SimpleTestCase.h"
#include "ClusterTestCase.h"
#include <wx/menu.h>
#include <wx/aboutdlg.h>
#include <wx/sizer.h>
#include <yarc_data_types.h>

Frame::Frame(wxWindow* parent, const wxPoint& pos, const wxSize& size) : wxFrame(parent, wxID_ANY, "Yarc Tester", pos, size)
{
	this->testCase = nullptr;

	wxMenu* mainMenu = new wxMenu();
	mainMenu->Append(new wxMenuItem(mainMenu, ID_SimpleTestCase, "Simple Test Case", "Test Yarc's simple client.", wxITEM_CHECK));
	mainMenu->Append(new wxMenuItem(mainMenu, ID_ClusterTestCase, "Cluster Test Case", "Test Yarc's cluster client.", wxITEM_CHECK));
	mainMenu->AppendSeparator();
	mainMenu->Append(new wxMenuItem(mainMenu, ID_Exit, "Exit", "Exit this program."));

	wxMenu* helpMenu = new wxMenu();
	helpMenu->Append(new wxMenuItem(helpMenu, ID_About, "About", "Show the about-box."));

	wxMenuBar* menuBar = new wxMenuBar();
	menuBar->Append(mainMenu, "Tester");
	menuBar->Append(helpMenu, "Help");
	this->SetMenuBar(menuBar);

	this->Bind(wxEVT_MENU, &Frame::OnExit, this, ID_Exit);
	this->Bind(wxEVT_MENU, &Frame::OnAbout, this, ID_About);
	this->Bind(wxEVT_MENU, &Frame::OnSimpleTestCase, this, ID_SimpleTestCase);
	this->Bind(wxEVT_MENU, &Frame::OnClusterTestCase, this, ID_ClusterTestCase);
	this->Bind(wxEVT_UPDATE_UI, &Frame::OnUpdateMenuItemUI, this, ID_SimpleTestCase);
	this->Bind(wxEVT_UPDATE_UI, &Frame::OnUpdateMenuItemUI, this, ID_ClusterTestCase);

	this->SetStatusBar(new wxStatusBar(this));

	this->outputText = new wxTextCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxTE_MULTILINE | wxTE_READONLY | wxHSCROLL | wxTE_LEFT);
	this->inputText = new wxTextCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxTE_LEFT | wxTE_PROCESS_ENTER);

	wxFont font;
	font.SetFaceName("Courier New");
	font.SetFamily(wxFONTFAMILY_MODERN);
	this->inputText->SetFont(font);

	wxBoxSizer* boxSizer = new wxBoxSizer(wxVERTICAL);
	boxSizer->Add(this->outputText, 1, wxALL | wxGROW, 0);
	boxSizer->Add(this->inputText, 0, wxGROW);
	this->SetSizer(boxSizer);

	this->Bind(wxEVT_CHAR_HOOK, &Frame::OnCharHook, this);
}

/*virtual*/ Frame::~Frame()
{
	this->SetTestCase(nullptr);
}

void Frame::OnExit(wxCommandEvent& event)
{
	this->Close(true);
}

void Frame::OnSimpleTestCase(wxCommandEvent& event)
{
	this->SetTestCase(new SimpleTestCase());
}

void Frame::OnClusterTestCase(wxCommandEvent& event)
{
	this->SetTestCase(new ClusterTestCase());
}

void Frame::SetTestCase(TestCase* givenTestCase)
{
	if (this->testCase)
	{
		this->testCase->Shutdown();
		delete this->testCase;
		this->testCase = nullptr;
	}

	if (givenTestCase)
	{
		if (givenTestCase->Setup())
			this->testCase = givenTestCase;
		else
		{
			givenTestCase->Shutdown();
			delete givenTestCase;
		}
	}
}

void Frame::OnUpdateMenuItemUI(wxUpdateUIEvent& event)
{
	switch (event.GetId())
	{
		case ID_SimpleTestCase:
		{
			event.Check(this->testCase && dynamic_cast<SimpleTestCase*>(this->testCase));
			break;
		}
		case ID_ClusterTestCase:
		{
			event.Check(this->testCase && dynamic_cast<ClusterTestCase*>(this->testCase));
			break;
		}
	}
}

void Frame::OnCharHook(wxKeyEvent& event)
{
	switch (event.GetKeyCode())
	{
		case WXK_RETURN:
		{
			wxString redisCommand = this->inputText->GetValue();
			this->inputText->Clear();

			if (this->testCase)
			{
				Yarc::DataType* commandData = Yarc::DataType::ParseCommand(redisCommand.c_str());
				if (commandData)
				{
					Yarc::ClientInterface* client = this->testCase->GetClientInterface();
					if (client->MakeRequestAsync(commandData, [=](const Yarc::DataType* responseData) {
							uint8_t protocolData[10 * 1024];
							uint32_t protocolDataSize = sizeof(protocolData);
							responseData->Print(protocolData, protocolDataSize);
							wxString responseText = protocolData;
							this->outputText->AppendText(responseText);
							this->outputText->AppendText("\n\n");
							return true;
						}))
					{
						this->outputText->AppendText(redisCommand);
						this->outputText->AppendText("\n");
					}

					delete commandData;
				}
			}

			break;
		}
		default:
		{
			event.Skip();
			break;
		}
	}
}

void Frame::OnAbout(wxCommandEvent& event)
{
	wxAboutDialogInfo aboutDialogInfo;

	aboutDialogInfo.SetName("Yarc Tester");
	aboutDialogInfo.SetVersion("1.0");
	aboutDialogInfo.SetDescription("Test Yarc -- Yet Another Redis Client.");
	aboutDialogInfo.SetCopyright("Copyright (C) 2020 -- Spencer T. Parkin <SpencerTParkin@gmail.com>");

	wxAboutBox(aboutDialogInfo);
}