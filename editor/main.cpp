#pragma warning(push)
#pragma warning(disable: 4996)
#include <wx/wx.h>
#include <wx/propgrid/propgrid.h>
#include <wx/propgrid/advprops.h>
#include <wx/filedlg.h>
#include <wx/splitter.h>
#include <wx/sysopt.h>
#include <wx/stattext.h>
#pragma warning(pop)

#include "../loader/loader.h"

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

    wxChoice* m_dbDropDown = nullptr;

    DBRoot m_database;

    // TODO: delete this when no longer needed
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

#if 1

    wxBoxSizer* topSizer = new wxBoxSizer(wxVERTICAL);

    // Database table drop down
    {
        wxBoxSizer* sizer = new wxBoxSizer(wxHORIZONTAL);

        wxStaticText* label = new wxStaticText(this, wxID_ANY, "Table:");
        sizer->Add(label, 0, wxALL | wxALIGN_LEFT, 8);

        wxArrayString choices;
        m_dbDropDown = new wxChoice(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, choices);
        m_dbDropDown->SetSelection(0);
        sizer->Add(m_dbDropDown, 0, wxALL | wxEXPAND, 8);

        topSizer->Add(sizer, 0, wxALL | wxEXPAND, 0);
    }


    wxSplitterWindow* splitter = new wxSplitterWindow(
        this, wxID_ANY, wxDefaultPosition, wxDefaultSize,
        wxSP_LIVE_UPDATE | wxSP_3D
    );

    // minimum size for each panel
    splitter->SetMinimumPaneSize(150);

    //splitter->SetBackgroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_BTNFACE));
    splitter->SetBackgroundColour(wxColour(192, 192, 192));

    // --- Left panel ---
    wxPanel* leftPanel = new wxPanel(splitter, wxID_ANY);
    wxBoxSizer* leftSizer = new wxBoxSizer(wxVERTICAL);


    // 1. Create standard non-editable text
    wxStaticText* plainText1 = new wxStaticText(
        leftPanel,
        wxID_ANY,
        "Clickable Plain Text"
    );
    leftSizer->Add(plainText1, 0, wxALL | wxALIGN_LEFT);
    wxStaticText* plainText2 = new wxStaticText(
        leftPanel,
        wxID_ANY,
        "Clickable Plain Text"
    );
    leftSizer->Add(plainText2, 0, wxALL | wxALIGN_LEFT);

    // 2. Bind the left-mouse-button release event
    plainText1->Bind(wxEVT_LEFT_UP, [this](wxMouseEvent& event) {
        wxMessageBox("You clicked the plain text! 1");
        event.Skip();
        });

    plainText2->Bind(wxEVT_LEFT_UP, [this](wxMouseEvent& event) {
        wxMessageBox("You clicked the plain text! 2");
        event.Skip();
        });

    leftPanel->SetSizer(leftSizer);

    // --- Right panel ---
    wxPanel* rightPanel = new wxPanel(splitter, wxID_ANY);
    wxBoxSizer* rightSizer = new wxBoxSizer(wxVERTICAL);
    wxButton* rightButton = new wxButton(rightPanel, wxID_ANY, "Right Button");
    rightSizer->Add(rightButton, 0, wxALL | wxALIGN_CENTER, 10);
    rightPanel->SetSizer(rightSizer);

    // Third argument is where the sash should start
    splitter->SplitVertically(leftPanel, rightPanel, 150);



    // CHANGE: splitter now goes into topSizer instead of being set directly
    topSizer->Add(splitter, 1, wxEXPAND | wxALL, 8);

    // CHANGE: SetSizer(splitter-related sizer) becomes SetSizer(topSizer)
    SetSizer(topSizer);

    // OPTIONAL: react to dropdown selection
    m_dbDropDown->Bind(wxEVT_CHOICE, [this](wxCommandEvent&) {
        wxLogMessage("Selected: %s", m_dbDropDown->GetStringSelection());
        });

#else

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
    button->Bind(wxEVT_BUTTON, &DataFrame::OnButtonClick, this);

#endif

    Bind(wxEVT_MENU, &DataFrame::OnOpenFile, this, ID_OpenFile);
    Bind(wxEVT_MENU, &DataFrame::OnExit, this, wxID_EXIT);

    CreateStatusBar();
    SetStatusText("Ready");
}

void DataFrame::OnOpenFile(wxCommandEvent& /*event*/)
{
    wxFileDialog openFileDialog(this, "Open File", "", "",
        "DBRoot files (*.dbroot)|*.dbroot",
        wxFD_OPEN | wxFD_FILE_MUST_EXIST);

    if (openFileDialog.ShowModal() == wxID_CANCEL)
        return; // user cancelled

    wxString path = openFileDialog.GetPath();

    if(!m_database.Load(path.ToUTF8().data()))
    {
        wxMessageBox("Failed to load database file.", "Error", wxOK | wxICON_ERROR);
        return;
    }

    wxArrayString choices;
    for (const std::string& table : m_database.GetTablePaths())
        choices.Add(table);

    m_dbDropDown->Clear();
    m_dbDropDown->Set(choices);
    m_dbDropDown->SetSelection(0);

    // Read it, e.g. into the text control
    if (m_textCtrl)
    {
        wxFile file(path);
        wxString contents;
        if (file.IsOpened())
        {
            file.ReadAll(&contents);
            m_textCtrl->SetValue(contents);
            SetStatusText("Opened: " + path);
        }
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

/*

TODO: for flat buffer schemas
* root must be an array of struct (tables have more chaser pointing etc)
* struct must contain a string name
? do we want more control, like the user specifies a limited schema in json and we turn that into a flat buffer schema?

TODO:
* open recent
* save? or automatic save on close / when changing records and databases?
* file watch for files changing on disk
? what if you change the schema? and like data versioning?

*/