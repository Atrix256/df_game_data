#pragma once

#include <wx/wx.h>
#include <wx/propgrid/propgrid.h>
#include <wx/propgrid/advprops.h>

class ButtonRowProperty : public wxStringProperty
{
public:
    struct ButtonDef
    {
        wxString label;
        std::function<void()> onClick;
    };

    ButtonRowProperty(const wxString& label, const wxString& name,
                       const wxString& displayValue,
                       std::vector<ButtonDef> buttons)
        : wxStringProperty(label, name, displayValue)
        , m_buttons(std::move(buttons))
    {
        SetFlag(wxPG_PROP_READONLY);
    }

    const std::vector<ButtonDef>& GetButtons() const { return m_buttons; }

private:
    std::vector<ButtonDef> m_buttons;
};

class ButtonRowEditor : public wxPGTextCtrlEditor
{
public:
    wxString GetName() const override { return "ButtonRowEditor"; }

    wxPGWindowList CreateControls(wxPropertyGrid* pg, wxPGProperty* property,
        const wxPoint& pos, const wxSize& sz) const override
    {
        auto* btnProp = static_cast<ButtonRowProperty*>(property);

        // 1. Create buttons container first, sized against the full cell rect
        wxPGMultiButton* buttons = new wxPGMultiButton(pg, sz);
        for (const auto& def : btnProp->GetButtons())
            buttons->Add(def.label);

        // 2/3. Create the primary control using the space left after buttons
        wxWindow* primary = wxPGTextCtrlEditor::CreateControls(
            pg, property, pos, buttons->GetPrimarySize()).GetPrimary();

        // 4. Now position the buttons relative to the primary control
        buttons->Finalize(pg, pos);

        // 5. Primary goes in the window list as usual; buttons is tracked
        //    as the "secondary" window so OnEvent/GetEditorControlSecondary can find it
        return wxPGWindowList(primary, buttons);
    }

    bool OnEvent(wxPropertyGrid* pg, wxPGProperty* property, wxWindow* wnd,
        wxEvent& event) const override
    {
        if (event.GetEventType() == wxEVT_BUTTON)
        {
            auto* buttons = static_cast<wxPGMultiButton*>(pg->GetEditorControlSecondary());
            auto* btnProp = static_cast<ButtonRowProperty*>(property);

            if (buttons)
            {
                for (unsigned i = 0; i < buttons->GetCount(); ++i)
                {
                    if (buttons->GetButtonId(i) == event.GetId())
                    {
                        std::function<void()> cb = btnProp->GetButtons()[i].onClick;
                        if (cb)
                            cb();
                        return true;
                    }
                }
            }
        }
        return wxPGTextCtrlEditor::OnEvent(pg, property, wnd, event);
    }
};
