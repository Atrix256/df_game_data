#pragma once

#include <wx/wx.h>
#include <wx/propgrid/propgrid.h>
#include <wx/propgrid/advprops.h>

class ButtonRowProperty : public wxPGProperty
{
public:
    struct ButtonDef { wxString label; std::function<void()> onClick; };

    ButtonRowProperty(const wxString& label, const wxString& name,
        std::vector<ButtonDef> buttons)
        : wxPGProperty(label, name), m_buttons(std::move(buttons))
    {
        SetFlag(wxPG_PROP_READONLY);
    }

    const std::vector<ButtonDef>& GetButtons() const { return m_buttons; }
    wxString ValueToString(wxVariant&, int) const override { return wxEmptyString; }

private:
    std::vector<ButtonDef> m_buttons;
};

class ButtonRowEditor : public wxPGEditor
{
public:
    wxString GetName() const override { return "ButtonRowEditor"; }

    wxPGWindowList CreateControls(wxPropertyGrid* pg, wxPGProperty* property,
        const wxPoint& pos, const wxSize& sz) const override
    {
        auto* btnProp = static_cast<ButtonRowProperty*>(property);

        wxPGMultiButton* buttons = new wxPGMultiButton(pg, sz);
        for (const auto& def : btnProp->GetButtons())
            buttons->Add(def.label);

        buttons->Finalize(pg, pos);

        return wxPGWindowList(buttons); // buttons ARE the primary window now
    }

    // No value to sync back and forth — these are no-ops
    void UpdateControl(wxPGProperty*, wxWindow*) const override {}
    bool GetValueFromControl(wxVariant&, wxPGProperty*, wxWindow*) const override { return false; }

    bool OnEvent(wxPropertyGrid* pg, wxPGProperty* property, wxWindow*,
        wxEvent& event) const override
    {
        if (event.GetEventType() == wxEVT_BUTTON)
        {
            auto* buttons = static_cast<wxPGMultiButton*>(pg->GetEditorControl()); // now primary
            auto* btnProp = static_cast<ButtonRowProperty*>(property);
            if (buttons)
            {
                for (unsigned i = 0; i < buttons->GetCount(); ++i)
                {
                    if (buttons->GetButtonId(i) == event.GetId())
                    {
                        std::function<void()> cb = btnProp->GetButtons()[i].onClick;
                        if (cb) cb();
                        return true;
                    }
                }
            }
        }
        return false;
    }
};
