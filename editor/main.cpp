#include "../loader/loader.h"

#pragma warning(push)
#pragma warning(disable: 4996)
#include <wx/wx.h>
#include <wx/propgrid/propgrid.h>
#include <wx/propgrid/advprops.h>
#include <wx/filedlg.h>
#include <wx/splitter.h>
#include <wx/sysopt.h>
#include <wx/stattext.h>
#include "FormBuilder/main/DataEditor.h"
#pragma warning(pop)

class DataApp : public wxApp
{
public:
    virtual bool OnInit() override;
};

class MainWindow : public Main
{
public:
    MainWindow()
        : Main(nullptr, wxID_ANY, "Data Editor", wxDefaultPosition, wxSize(500, 400))
    { }

    void OnFileExit(wxCommandEvent& /*event*/) override final
    {
        Close(true);
    }

    void OnFileOpen(wxCommandEvent& /*event*/) override final
    {
        wxFileDialog openFileDialog(this, "Open File", "", "",
            "DBRoot files (*.dbroot)|*.dbroot",
            wxFD_OPEN | wxFD_FILE_MUST_EXIST);

        if (openFileDialog.ShowModal() == wxID_CANCEL)
            return; // user cancelledm_database
        wxString path = openFileDialog.GetPath();

        if (!m_database.Load(path.ToUTF8().data()))
        {
            wxMessageBox(m_database.GetErrorText(), "Error", wxOK | wxICON_ERROR);
            return;
        }

        // show warnings if we should
        std::string warningText = m_database.GetErrorText();
        if (!warningText.empty())
        {
            wxMessageBox(m_database.GetErrorText(), "Warning", wxOK | wxICON_WARNING);
        }

        wxArrayString choices;
        for (auto& table : m_database.m_tables)
            choices.Add(table.GetName());

        m_tableChoice->Clear();
        m_tableChoice->Set(choices);
        m_tableChoice->SetSelection(0);

        PopulateDataChoices();
    }

    void OnTableViewChange(wxCommandEvent& /*event*/) override final
    {
        PopulateDataChoices();
    }

    void PopulateDataChoices()
    {
        m_dataChoice->DeleteAllItems();

        int tableIndex = m_tableChoice->GetSelection();
        DBTable& table = m_database.m_tables[tableIndex];

        for (auto& it : table.m_data)
            m_dataChoice->InsertItem(m_dataChoice->GetItemCount(), it.first.c_str());
    }

private:
    DBRoot m_database;
};

wxIMPLEMENT_APP(DataApp);

bool DataApp::OnInit()
{
    MainWindow* frame = new MainWindow();
    frame->Show(true);
    return true;
}

/*
TODO: Next
* start showing UI?

* use "documentation" field as tooltips in editor
* have an enum in the example schema, and make editor have you choose which type it is.
* have an example C++ program that loads the example data.
* rename the .dbroot file to like dbroot.txt maybe? the rest of the file extensions are plain. maybe not
* if there's an error during loading, clear the UI and filename
* show the filename in the title bar, and a * when it's dirty. make a "save all" option. or call it save but it does a save all.
* this json library is aparently really good at handling undo. look into it so you can do undo


TODO: flatbuffers say the schema of tables can "evolve", so that binary files are forward and backwards compatible.
 When our source data is json, we don't really need that.
 Maybe have the root type be a struct in your example data.

TODO: for flat buffer schemas
* root must be a table that contains an array of struct (tables have more chaser pointing etc)
* struct must contain a string name
? do we want more control, like the user specifies a limited schema in json and we turn that into a flat buffer schema?

? what types should we allow?
 * pod, string, bool -> with default values
 * structs -> can we have override default values for the entire struct?
 * can we do links to other records? like for a link, specify the database name, and it can be any db that is part of the current project

? could maybe go to yaml if schema is sufficiently complicated
 * if so, could have the dbroot file be yaml too.
 * dbName1: "relative/path/to/db1.yaml"
 * dbName2: "relative/path/to/db2.yaml"
 * etc.

! maybe do json for schema, because the data files will be in json?

TODO:
* make the db table be full sized again
* ask someone for help with wxwidgets looking bad, not like a native app
* open recent
* save? or automatic save on close / when changing records and databases?
* file watch for files changing on disk. implement the file watcher
? what if you change the schema? and like data versioning?

*/


/*
TODO:
* maybe m_tables should be a map instead of a vector
* how do we support schema changes? we need a resave of all the data.
 * could maybe have a "convert" option from one known type to another?
 * angel script? idk.
 * source data maybe should have schema? (object names, hash, ??)
* TODOs in this file and other files
* support singular items, so when you open the db there is one record only, not a dictionary of them.
* note that there is a (key) field in flatbuffers, which apparently sorts by that field for binary search lookup.
* make sure you support unicode file names. run everything from within a unicode path.
* file / directory watching
 * what to do if a schema file changes? reload that table? what if there is unsaved data records?
 * what to do if a data file changes? 
 * what if a schema file is added or removed?
 * what if a data file is added or removed?
* handle renaming of data items. make sure it's a unique name and rename the file
*/

/*
TODO:
Hot reloading:
Have a record reference for each type. Those can survive a reload(*). Maybe store name internally and a load version #. It can relook up when used, and db load version is different.

Dont cache anything off from the recods (**)

* - deleting a data record makes it be default valued.
** - you can, at your own risk. But you get a callback on hot reload so can update whatever you want in response.

Have same interface for non hot reload version. Just, data records etc are simpler.
*/

/*
TODO: Example data:
* have at least 2 tables
* use enums
* use unions
* use various types
* use links to other tables

! note that it scans the folder where the schema lives, recursively, for all .json files and tries to load them as table entries
! note that comments become tooltips and show in example data
! note the custom attribute for (root_type)
! note how to use links. a string with attribute (link:"tablename") inventory:[string] (link:"Item");.  It gives you the record name
 TODO: should it be an integer type for index instead? or let either work?
*/
