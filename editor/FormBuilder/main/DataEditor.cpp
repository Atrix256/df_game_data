///////////////////////////////////////////////////////////////////////////
// C++ code generated with wxFormBuilder (version 4.2.1-0-g80c4cb6)
// http://www.wxformbuilder.org/
//
// PLEASE DO *NOT* EDIT THIS FILE!
///////////////////////////////////////////////////////////////////////////

#include "DataEditor.h"

///////////////////////////////////////////////////////////////////////////

Main::Main( wxWindow* parent, wxWindowID id, const wxString& title, const wxPoint& pos, const wxSize& size, long style ) : wxFrame( parent, id, title, pos, size, style )
{
	this->SetSizeHints( wxDefaultSize, wxDefaultSize );

	m_menubar3 = new wxMenuBar( 0 );
	m_menu1 = new wxMenu();
	wxMenuItem* m_menuItem1;
	m_menuItem1 = new wxMenuItem( m_menu1, wxID_ANY, wxString( _("Open") ) , wxEmptyString, wxITEM_NORMAL );
	m_menu1->Append( m_menuItem1 );

	wxMenuItem* m_menuItem2;
	m_menuItem2 = new wxMenuItem( m_menu1, wxID_ANY, wxString( _("Exit") ) , wxEmptyString, wxITEM_NORMAL );
	m_menu1->Append( m_menuItem2 );

	m_menubar3->Append( m_menu1, _("File") );

	this->SetMenuBar( m_menubar3 );

	wxBoxSizer* bSizer1;
	bSizer1 = new wxBoxSizer( wxVERTICAL );

	wxBoxSizer* bSizer2;
	bSizer2 = new wxBoxSizer( wxHORIZONTAL );

	m_staticText1 = new wxStaticText( this, wxID_ANY, _("Table:"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticText1->Wrap( -1 );
	bSizer2->Add( m_staticText1, 0, wxALL, 5 );

	wxArrayString m_tableChoiceChoices;
	m_tableChoice = new wxChoice( this, wxID_ANY, wxDefaultPosition, wxDefaultSize, m_tableChoiceChoices, wxCB_SORT );
	m_tableChoice->SetSelection( 0 );
	bSizer2->Add( m_tableChoice, 1, wxALL, 1 );


	bSizer1->Add( bSizer2, 0, wxEXPAND, 5 );

	wxBoxSizer* bSizer3;
	bSizer3 = new wxBoxSizer( wxHORIZONTAL );

	m_dataChoice = new wxListCtrl( this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLC_EDIT_LABELS|wxLC_ICON|wxLC_SINGLE_SEL );
	bSizer3->Add( m_dataChoice, 0, wxALL|wxEXPAND, 1 );

	MyPanel1 = new wxPanel( this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL );
	m_editSizer = new wxBoxSizer( wxVERTICAL );


	MyPanel1->SetSizer( m_editSizer );
	MyPanel1->Layout();
	m_editSizer->Fit( MyPanel1 );
	bSizer3->Add( MyPanel1, 1, wxEXPAND | wxALL, 1 );


	bSizer1->Add( bSizer3, 1, wxEXPAND, 1 );


	this->SetSizer( bSizer1 );
	this->Layout();

	this->Centre( wxBOTH );

	// Connect Events
	m_menu1->Bind(wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler( Main::OnFileOpen ), this, m_menuItem1->GetId());
	m_menu1->Bind(wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler( Main::OnFileExit ), this, m_menuItem2->GetId());
	m_tableChoice->Connect( wxEVT_COMMAND_CHOICE_SELECTED, wxCommandEventHandler( Main::OnTableViewChange ), NULL, this );
}

Main::~Main()
{
}
