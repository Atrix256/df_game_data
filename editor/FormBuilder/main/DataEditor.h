///////////////////////////////////////////////////////////////////////////
// C++ code generated with wxFormBuilder (version 4.2.1-0-g80c4cb6)
// http://www.wxformbuilder.org/
//
// PLEASE DO *NOT* EDIT THIS FILE!
///////////////////////////////////////////////////////////////////////////

#pragma once

#include <wx/artprov.h>
#include <wx/xrc/xmlres.h>
#include <wx/intl.h>
#include <wx/string.h>
#include <wx/bitmap.h>
#include <wx/image.h>
#include <wx/icon.h>
#include <wx/menu.h>
#include <wx/gdicmn.h>
#include <wx/font.h>
#include <wx/colour.h>
#include <wx/settings.h>
#include <wx/stattext.h>
#include <wx/choice.h>
#include <wx/sizer.h>
#include <wx/listctrl.h>
#include <wx/button.h>
#include <wx/panel.h>
#include <wx/frame.h>

///////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
/// Class Main
///////////////////////////////////////////////////////////////////////////////
class Main : public wxFrame
{
	private:

	protected:
		wxMenuBar* m_menubar3;
		wxMenu* m_menu1;
		wxStaticText* m_staticText1;
		wxChoice* m_tableChoice;
		wxListCtrl* m_dataChoice;
		wxButton* m_button2;
		wxButton* m_button1;
		wxPanel* m_editPanel;
		wxBoxSizer* m_editSizer;

		// Virtual event handlers, override them in your derived class
		virtual void OnFileOpen( wxCommandEvent& event ) { event.Skip(); }
		virtual void OnFileExit( wxCommandEvent& event ) { event.Skip(); }
		virtual void OnTableViewChange( wxCommandEvent& event ) { event.Skip(); }
		virtual void OnDataChoiceKeyDown( wxKeyEvent& event ) { event.Skip(); }
		virtual void OnDataChoiceDelete( wxListEvent& event ) { event.Skip(); }
		virtual void OnDataChoiceRenamed( wxListEvent& event ) { event.Skip(); }
		virtual void OnRightClickItem( wxListEvent& event ) { event.Skip(); }
		virtual void OnDataChoiceSelect( wxListEvent& event ) { event.Skip(); }
		virtual void OnDataChoiceButtonNew( wxCommandEvent& event ) { event.Skip(); }
		virtual void OnDataChoiceButtonDelete( wxCommandEvent& event ) { event.Skip(); }


	public:

		Main( wxWindow* parent, wxWindowID id = wxID_ANY, const wxString& title = _("Data Editor"), const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxSize( 500,300 ), long style = wxDEFAULT_FRAME_STYLE|wxTAB_TRAVERSAL );

		~Main();

};

