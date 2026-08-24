#include <wx/wx.h>
#include <wx/propgrid/propgrid.h>
#include <wx/propgrid/advprops.h>
#include <wx/filedlg.h>

#include "loader.h"

class DataApp : public wxApp
{
public:
    virtual bool OnInit() override;
};

class DataFrame : public wxFrame
{
public:
    DataFrame();

private:
    void OnOpenFile(wxCommandEvent& event);
    void OnButtonClick(wxCommandEvent& event);
    void OnExit(wxCommandEvent& event);

    wxPropertyGrid* m_propGrid = nullptr;
    wxTextCtrl* m_textCtrl = nullptr;
};

enum
{
    ID_OpenFile = wxID_HIGHEST + 1
};

DataFrame::DataFrame()
    : wxFrame(nullptr, wxID_ANY, "Property Editor", wxDefaultPosition, wxSize(500, 400))
{
    // --- Menu bar ---
    wxMenu* fileMenu = new wxMenu;
    fileMenu->Append(ID_OpenFile, "&Open...\tCtrl+O");
    fileMenu->AppendSeparator();
    fileMenu->Append(wxID_EXIT);

    wxMenuBar* menuBar = new wxMenuBar;
    menuBar->Append(fileMenu, "&File");
    SetMenuBar(menuBar);

    // --- Main panel and sizer-based layout ---
    wxPanel* panel = new wxPanel(this);
    wxBoxSizer* mainSizer = new wxBoxSizer(wxVERTICAL);

    // A text control
    m_textCtrl = new wxTextCtrl(panel, wxID_ANY, "");
    mainSizer->Add(m_textCtrl, 0, wxEXPAND | wxALL, 5);

    // A button
    wxButton* button = new wxButton(panel, wxID_ANY, "Click Me");
    mainSizer->Add(button, 0, wxALL, 5);

    // The property grid itself
    m_propGrid = new wxPropertyGrid(panel, wxID_ANY, wxDefaultPosition,
        wxDefaultSize, wxPG_SPLITTER_AUTO_CENTER);

    m_propGrid->Append(new wxStringProperty("Name", wxPG_LABEL, "MyObject"));
    m_propGrid->Append(new wxIntProperty("Count", wxPG_LABEL, 10));
    m_propGrid->Append(new wxFloatProperty("Scale", wxPG_LABEL, 1.0));
    m_propGrid->Append(new wxBoolProperty("Enabled", wxPG_LABEL, true));
    m_propGrid->Append(new wxColourProperty("Color", wxPG_LABEL, *wxRED));

    // wxEnumProperty for dropdown/choice-style values
    wxArrayString choices;
    choices.Add("Low");
    choices.Add("Medium");
    choices.Add("High");
    m_propGrid->Append(new wxEnumProperty("Priority", wxPG_LABEL, choices));

    mainSizer->Add(m_propGrid, 1, wxEXPAND | wxALL, 5);

    panel->SetSizer(mainSizer);

    // --- Event bindings ---
    // Bind() is the modern approach; ties events to member functions directly
    Bind(wxEVT_MENU, &DataFrame::OnOpenFile, this, ID_OpenFile);
    Bind(wxEVT_MENU, &DataFrame::OnExit, this, wxID_EXIT);
    button->Bind(wxEVT_BUTTON, &DataFrame::OnButtonClick, this);

    CreateStatusBar();
    SetStatusText("Ready");
}

void DataFrame::OnOpenFile(wxCommandEvent& /*event*/)
{
    wxFileDialog openFileDialog(this, "Open File", "", "",
        "Text files (*.txt)|*.txt|All files (*.*)|*.*",
        wxFD_OPEN | wxFD_FILE_MUST_EXIST);

    if (openFileDialog.ShowModal() == wxID_CANCEL)
        return; // user cancelled

    wxString path = openFileDialog.GetPath();

    // Read it, e.g. into the text control
    wxFile file(path);
    wxString contents;
    if (file.IsOpened())
    {
        file.ReadAll(&contents);
        m_textCtrl->SetValue(contents);
        SetStatusText("Opened: " + path);
    }
}

void DataFrame::OnButtonClick(wxCommandEvent& /*event*/)
{
    // Pull current values out of the property grid
    wxVariant nameVal = m_propGrid->GetPropertyValue("Name");
    wxString name = nameVal.GetString();

    wxMessageBox("Name property is: " + name, "Info", wxOK | wxICON_INFORMATION);
}

void DataFrame::OnExit(wxCommandEvent& /*event*/)
{
    Close(true);
}

wxIMPLEMENT_APP(DataApp);

bool DataApp::OnInit()
{
    DataFrame* frame = new DataFrame();
    frame->Show(true);
    return true;
}
