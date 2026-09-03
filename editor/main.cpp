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

#include "TypesUI.h"

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
        for (auto& it : m_database.m_tables)
            choices.Add(it.first.c_str());

        m_tableChoice->Clear();
        m_tableChoice->Set(choices);
        m_tableChoice->SetSelection(0);

        PopulateDataChoices();
        PopulateDataEditUI();
    }

    void OnTableViewChange(wxCommandEvent& /*event*/) override final
    {
        PopulateDataChoices();
    }

    void OnDataChoiceRenamed(wxListEvent& event) override final
    {
        if (event.IsEditCancelled())
            return;

        long itemIndex = event.GetIndex();
        wxString oldName = m_dataChoice->GetItemText(itemIndex);
        wxString newName = event.GetLabel();

        // If the name didn't change, nothing to do
        if (oldName == newName)
            return;

        // Don't allow characters which are not valid to be part of filename
        static const std::string_view illegalChars = "<>:\"/\\|?*";
        bool hasIllegal = std::any_of(
            newName.begin(), newName.end(), [](char c)
            {
                // don't allow characters < 32, or the illegal characters
                return (static_cast<unsigned char>(c) < 32) || (illegalChars.find(c) != std::string_view::npos);
            }
        );
        if (hasIllegal)
            return;

        // If the name is already taken by something else, don't do it
        int totalItems = m_dataChoice->GetItemCount();
        for (int index = 0; index < totalItems; ++index)
        {
            if (index == itemIndex)
                continue;

            if (m_dataChoice->GetItemText(itemIndex) == newName)
                return;
        }

        // rename the file on disk
        std::string tableName = m_tableChoice->GetStringSelection().utf8_string();
        DBTable& table = *m_database.m_tables[tableName].get();
        std::unique_ptr<DBTable::JSONData> data = std::move(table.m_data[oldName.utf8_string()]);
        std::string oldFileName = data->m_path;
        std::string newFileName = (std::filesystem::path(oldFileName).remove_filename() / newName.utf8_string()).replace_extension(".json").string();
        std::filesystem::rename(oldFileName.c_str(), newFileName.c_str());
        data->m_path = newFileName;

        // change the key where this data is, from the old name to the new
        table.m_data.erase(oldName.utf8_string());
        table.m_data[newName.utf8_string()] = std::move(data);

        // rename the item in the UI
        m_dataChoice->SetItemText(itemIndex, newName);
    }

    void OnDataChoiceDelete(wxListEvent& event) override final
    {
        long itemIndex = event.GetIndex();
        if (itemIndex < 0 || itemIndex >= m_dataChoice->GetItemCount())
            return;

        std::string tableName = m_tableChoice->GetStringSelection().utf8_string();
        DBTable& table = *m_database.m_tables[tableName].get();
        wxString name = m_dataChoice->GetItemText(itemIndex);

        // delete file from disk
        std::filesystem::remove(table.m_data[name.utf8_string()]->m_path);

        // delete from the table
        table.m_data.erase(name.utf8_string());
    }

    void OnRightClickItem(wxListEvent& event) override final
    {
        long itemIndex = event.GetIndex();

        // select the item that was right clicked
        m_dataChoice->SetItemState(itemIndex, wxLIST_STATE_SELECTED, wxLIST_STATE_SELECTED);

        wxMenu menu;

        wxMenuItem* menuItemNewEntry = new wxMenuItem(m_menu1, wxID_ANY, wxString(_("New Entry")), wxEmptyString, wxITEM_NORMAL);
        menu.Append(menuItemNewEntry);

        menu.AppendSeparator();

        wxMenuItem* menuItemDuplicate = new wxMenuItem(m_menu1, wxID_ANY, wxString(_("Duplicate")), wxEmptyString, wxITEM_NORMAL);
        menu.Append(menuItemDuplicate);
        wxMenuItem* menuItemRename = new wxMenuItem(m_menu1, wxID_ANY, wxString(_("Rename")), wxEmptyString, wxITEM_NORMAL);
        menu.Append(menuItemRename);
        wxMenuItem* menuItemDelete = new wxMenuItem(m_menu1, wxID_ANY, wxString(_("Delete")), wxEmptyString, wxITEM_NORMAL);
        menu.Append(menuItemDelete);

        // bind fnuctions to the menu options
        menu.Bind(wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler(MainWindow::OnDataChoiceNewEntry), this, menuItemNewEntry->GetId());
        menu.Bind(wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler(MainWindow::OnDataChoiceDuplicate), this, menuItemDuplicate->GetId());
        menu.Bind(wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler(MainWindow::OnDataChoiceRename), this, menuItemRename->GetId());
        menu.Bind(wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler(MainWindow::OnDataChoiceDelete), this, menuItemDelete->GetId());

        // pop up where they clicked
        wxPoint pos = event.GetPoint();
        m_dataChoice->PopupMenu(&menu, pos);
    }

    std::string GetUniqueDataEntryName(const char* baseName)
    {
        DBTable& table = *m_database.m_tables[m_tableChoice->GetStringSelection().utf8_string()].get();
        if (table.m_data.count(baseName) == 0)
            return baseName;

        int index = 0;
        char buffer[1024];

        while (1)
        {
            index++;
            sprintf_s(buffer, "%s_%i", baseName, index);

            if (table.m_data.count(buffer) == 0)
                return buffer;
        }
    }

    void MakeNewDataChoiceEntry()
    {
        DBTable& table = *m_database.m_tables[m_tableChoice->GetStringSelection().utf8_string()].get();
        std::string itemName = GetUniqueDataEntryName("NewEntry");
        std::filesystem::path fileName = (std::filesystem::path(table.GetPath()).remove_filename() / itemName).replace_extension(".json");

        // make the file
        {
            FILE* file = nullptr;
            fopen_s(&file, fileName.string().c_str(), "wb");
            if (!file)
                return;

            fprintf(file, "{\n}\n");
            fclose(file);
        }

        // make the entry in the data
        table.LoadFile(fileName.string().c_str());

        // make the entry in the UI and select it
        m_dataChoice->InsertItem(m_dataChoice->GetItemCount(), itemName.c_str());
        m_dataChoice->SetItemState(m_dataChoice->GetItemCount() - 1, wxLIST_STATE_SELECTED, wxLIST_STATE_SELECTED);
    }

    void OnDataChoiceNewEntry(wxCommandEvent& event)
    {
        MakeNewDataChoiceEntry();
    }

    void OnDataChoiceDuplicate(wxCommandEvent& event)
    {
        DBTable& table = *m_database.m_tables[m_tableChoice->GetStringSelection().utf8_string()].get();

        long selectedIndex = m_dataChoice->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
        if (selectedIndex == -1)
            return;

        std::string oldName = m_dataChoice->GetItemText(selectedIndex).utf8_string();
        std::string newName = GetUniqueDataEntryName(oldName.c_str());

        std::filesystem::path oldFileName = (std::filesystem::path(table.GetPath()).remove_filename() / oldName).replace_extension(".json");
        std::filesystem::path newFileName = (std::filesystem::path(table.GetPath()).remove_filename() / newName).replace_extension(".json");

        // Copy the file
        std::error_code ec;
        std::filesystem::copy_file(oldFileName, newFileName, std::filesystem::copy_options::overwrite_existing, ec);

        // make the entry in the data
        table.LoadFile(newFileName.string().c_str());

        // make the entry in the UI and select it
        m_dataChoice->InsertItem(m_dataChoice->GetItemCount(), newName.c_str());
        m_dataChoice->SetItemState(m_dataChoice->GetItemCount() - 1, wxLIST_STATE_SELECTED, wxLIST_STATE_SELECTED);
    }

    void OnDataChoiceRename(wxCommandEvent& event)
    {
        int selectedIndex = m_dataChoice->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
        if (selectedIndex != -1)
            m_dataChoice->EditLabel(selectedIndex);
    }

    void OnDataChoiceDelete(wxCommandEvent& event)
    {
        int selectedIndex = m_dataChoice->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
        m_dataChoice->DeleteItem(selectedIndex);
    }

    void OnDataChoiceButtonNew(wxCommandEvent& event)
    {
        MakeNewDataChoiceEntry();
    }
    void OnDataChoiceButtonDelete(wxCommandEvent & event)
    {
        int selectedIndex = m_dataChoice->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
        if (selectedIndex != -1)
            m_dataChoice->DeleteItem(selectedIndex);
    }

    void PopulateDataEditUI()
    {
        // Remove any UI there already
        m_editPanel->Freeze();
        m_editPanel->DestroyChildren();
        m_editSizer->Clear(true);

        // Refresh UI
        m_editPanel->Layout();
        m_editPanel->Thaw();

        long selectedIndex = m_dataChoice->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
        if (selectedIndex == -1)
            return;

        DBTable& table = *m_database.m_tables[m_tableChoice->GetStringSelection().utf8_string()].get();
        std::string name = m_dataChoice->GetItemText(selectedIndex).utf8_string();
        DBTable::JSONData& data = *table.m_data[name].get();

        const flatbuffers::Parser& parser = table.GetParser();

        AddUIForType(parser, *parser.root_struct_def_, m_editPanel, m_editSizer);

        m_editPanel->Layout();
        m_editSizer->Fit(m_editPanel);
    }

    void OnDataChoiceSelect(wxListEvent& /*event*/) override final
    {
        PopulateDataEditUI();
    }

    void OnDataChoiceKeyDown(wxKeyEvent& event) override final
    {
        switch (event.GetKeyCode())
        {
            case WXK_F2:
            {
                int selectedIndex = m_dataChoice->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
                if (selectedIndex != -1)
                    m_dataChoice->EditLabel(selectedIndex);
                break;
            }
            case WXK_DELETE:
            {
                int selectedIndex = m_dataChoice->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
                m_dataChoice->DeleteItem(selectedIndex);
                break;
            }
            default:
            {
                event.Skip();
                break;
            }
        }
    }

    void PopulateDataChoices()
    {
        m_dataChoice->DeleteAllItems();

        DBTable& table = *m_database.m_tables[m_tableChoice->GetStringSelection().utf8_string()].get();

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

* it's ok to use the actual root_type statement instead of the custom attrbutes. we can remove those lines when we combine them.

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
! note how to use links. a string with attribute (link:"tablename") inventory:[string] (link:"Item");.  It gives you the record name
 TODO: should it be an integer type for index instead? or let either work?
*/
