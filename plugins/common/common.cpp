///////////////////////////////////////////////////////////////////////////////
//
// wxFormBuilder - A Visual Dialog Editor for wxWidgets.
// Copyright (C) 2005 José Antonio Hurtado
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
//
// Written by
//   José Antonio Hurtado - joseantonio.hurtado@gmail.com
//   Juan Antonio Ortega  - jortegalalmolda@gmail.com
//
///////////////////////////////////////////////////////////////////////////////

#include <unordered_map>

#include <ticpp.h>
#include <wx/animate.h>
#include <wx/aui/auibar.h>
#include <wx/bmpcbox.h>
#include <wx/infobar.h>
#include <wx/listctrl.h>
#include <wx/statline.h>

#include <common/xmlutils.h>
#include <plugin_interface/plugin.h>
#include <plugin_interface/xrcconv.h>


// Custom status bar class for windows to prevent the status bar gripper from
// moving the entire wxFB window
#if defined(__WIN32__) && wxUSE_NATIVE_STATUSBAR
class wxIndependentStatusBar : public wxStatusBar
{
public:
    wxIndependentStatusBar(
      wxWindow* parent, wxWindowID id = wxID_ANY, long style = wxSTB_SIZEGRIP,
      const wxString& name = wxStatusBarNameStr) :
      wxStatusBar(parent, id, style, name)
    {
    }

    // override this virtual function to prevent the status bar from moving the main frame
    WXLRESULT MSWWindowProc(WXUINT nMsg, WXWPARAM wParam, WXLPARAM lParam) override
    {
        return wxStatusBarBase::MSWWindowProc(nMsg, wParam, lParam);
    }
};
#else
typedef wxStatusBar wxIndependentStatusBar;
#endif

// Custom Aui toolbar
class AuiToolBar : public wxAuiToolBar
{
    friend class AuiToolBarComponent;

public:
    AuiToolBar(wxWindow* win, IManager* manager, wxWindowID id, wxPoint pos, wxSize size, long style) :
      wxAuiToolBar(win, id, pos, size, style)
    {
        m_manager = manager;
    }

    void SetObject(int index, wxObject* pObject)
    {
        m_aObjects[index] = pObject;
    }

    wxObject* GetObject(int index)
    {
        return m_aObjects[index];
    }

protected:
    IManager* m_manager;

    wxMenu* GetMenuFromObject(IObject* menu)
    {
        int lastMenuId = wxID_HIGHEST + 1;
        wxMenu* menuWidget = new wxMenu();
        for (unsigned int j = 0; j < menu->GetChildCount(); j++) {
            auto* menuItem = menu->GetChildObject(j);
            if (menuItem->GetObjectTypeName() == wxT("submenu")) {
                menuWidget->Append(lastMenuId++, menuItem->GetPropertyAsString(wxT("label")), GetMenuFromObject(menuItem));
            } else if (menuItem->GetClassName() == wxT("separator")) {
                menuWidget->AppendSeparator();
            } else {
                wxString label = menuItem->GetPropertyAsString(wxT("label"));
                wxString shortcut = menuItem->GetPropertyAsString(wxT("shortcut"));
                if (!shortcut.IsEmpty()) {
                    label = label + wxChar('\t') + shortcut;
                }

                wxMenuItem* item = new wxMenuItem(
                menuWidget, lastMenuId++, label, menuItem->GetPropertyAsString(wxT("help")),
                (wxItemKind)menuItem->GetPropertyAsInteger(wxT("kind")));

                if (!menuItem->IsPropertyNull(wxT("bitmap"))) {
                    wxBitmap unchecked = wxNullBitmap;
                    if (!menuItem->IsPropertyNull(wxT("unchecked_bitmap"))) {
                        unchecked = menuItem->GetPropertyAsBitmap(wxT("unchecked_bitmap"));
                    }
    #ifdef __WXMSW__
                    item->SetBitmaps(menuItem->GetPropertyAsBitmap(wxT("bitmap")), unchecked);
    #elif defined(__WXGTK__)
                    item->SetBitmap(menuItem->GetPropertyAsBitmap(wxT("bitmap")));
    #endif
                } else {
                    if (!menuItem->IsPropertyNull(wxT("unchecked_bitmap"))) {
    #ifdef __WXMSW__
                        item->SetBitmaps(wxNullBitmap, menuItem->GetPropertyAsBitmap(wxT("unchecked_bitmap")));
    #endif
                    }
                }

                menuWidget->Append(item);

                if (item->GetKind() == wxITEM_CHECK && menuItem->GetPropertyAsInteger(wxT("checked")) != 0) {
                    item->Check(true);
                }

                item->Enable((menuItem->GetPropertyAsInteger(wxT("enabled")) != 0));
            }
        }

        return menuWidget;
    }

    void OnDropDownMenu([[maybe_unused]] wxAuiToolBarEvent& event)
    {
        wxAuiToolBar* tb = wxDynamicCast(event.GetEventObject(), wxAuiToolBar);

        if (tb) {
            wxAuiToolBarItem* item = tb->FindTool(event.GetId());

            if (item && item->HasDropDown()) {
                wxObject* wxobject = GetObject(item->GetUserData());

                if (wxobject) {
                    m_manager->SelectObject(wxobject);
                }

                tb->SetToolSticky(item->GetId(), true);
                wxRect rect = tb->GetToolRect(item->GetId());
                wxPoint pt = tb->ClientToScreen(rect.GetBottomLeft());
                pt = tb->ScreenToClient(pt);

                wxObject* child = m_manager->GetChild(wxobject, 0);
                if (child) {
                    tb->PopupMenu(GetMenuFromObject(m_manager->GetIObject(child)), pt);
                    tb->SetToolSticky(item->GetId(), false);
                }

                // TODO: For some strange reason, this event is occasionally propagated upwards even though it's been
                // handled here and there's a clash of IDs between that given a tool by wxAui and the IDs in mainframe.cpp
                // which sometimes results in a wxFormBuilder dialog being fired.
                // So StopPropagation() now, but those IDs need changing to avoid clashes.
                // event.StopPropagation();
            }
        }
    }

    void OnTool([[maybe_unused]] wxCommandEvent& event)
    {
        AuiToolBar* tb = wxDynamicCast(event.GetEventObject(), AuiToolBar);
        if (NULL == tb) {
            // very very strange
            return;
        }

        wxAuiToolBarItem* item = tb->FindTool(event.GetId());
        if (item) {
            wxObject* wxobject = GetObject(item->GetUserData());

            if (wxobject) {
                m_manager->SelectObject(wxobject);
            }
        }
    }

    wxDECLARE_EVENT_TABLE();

private:
    std::unordered_map<int, wxObject*> m_aObjects;
};

wxBEGIN_EVENT_TABLE(AuiToolBar, wxAuiToolBar)
EVT_AUITOOLBAR_TOOL_DROPDOWN(wxID_ANY, AuiToolBar::OnDropDownMenu)
EVT_TOOL(wxID_ANY, AuiToolBar::OnTool)
wxEND_EVENT_TABLE()

///////////////////////////////////////////////////////////////////////////////

/**
Event handler for events generated by controls in this plugin
*/
class ComponentEvtHandler : public wxEvtHandler
{
    friend class AuiToolBarComponent;

public:
    ComponentEvtHandler(wxWindow* win, IManager* manager) : m_window(win), m_manager(manager) {}

protected:
    void OnText([[maybe_unused]] wxCommandEvent& event)
    {
        wxTextCtrl* tc = wxDynamicCast(m_window, wxTextCtrl);
        if (tc != NULL) {
            m_manager->ModifyProperty(m_window, _("value"), tc->GetValue());
            tc->SetInsertionPointEnd();
            tc->SetFocus();
            return;
        }

        wxComboBox* cb = wxDynamicCast(m_window, wxComboBox);
        if (cb != NULL) {
            m_manager->ModifyProperty(m_window, _("value"), cb->GetValue());
            cb->SetInsertionPointEnd();
            cb->SetFocus();
            return;
        }
    }

    void OnChecked([[maybe_unused]] wxCommandEvent& event)
    {
        wxCheckBox* cb = wxDynamicCast(m_window, wxCheckBox);
        if (cb != NULL) {
            wxString cbValue;
            cbValue.Printf(wxT("%i"), cb->GetValue());
            m_manager->ModifyProperty(m_window, _("checked"), cbValue);
            cb->SetFocus();
        }
    }

    void OnChoice([[maybe_unused]] wxCommandEvent& event)
    {
        wxChoice* window = wxDynamicCast(m_window, wxChoice);
        if (window != NULL) {
            wxString value;
            value.Printf(wxT("%i"), window->GetSelection());
            m_manager->ModifyProperty(m_window, _("selection"), value);
            window->SetFocus();
        }
    }

    void OnComboBox([[maybe_unused]] wxCommandEvent& event)
    {
        wxComboBox* window = wxDynamicCast(m_window, wxComboBox);
        if (window != NULL) {
            wxString value;
            value.Printf(wxT("%i"), window->GetSelection());
            m_manager->ModifyProperty(m_window, _("selection"), value);
            window->SetFocus();
        }
    }

    void OnTool([[maybe_unused]] wxCommandEvent& event)
    {
        // FIXME: Same as above

        wxToolBar* tb = wxDynamicCast(event.GetEventObject(), wxToolBar);
        if (NULL == tb) {
            // very very strange
            return;
        }

        wxObject* wxobject = tb->GetToolClientData(event.GetId());
        if (NULL != wxobject) {
            m_manager->SelectObject(wxobject);
        }
    }

    void OnButton([[maybe_unused]] wxCommandEvent& event)
    {
        wxInfoBar* ib = wxDynamicCast(m_window, wxInfoBar);
        if (ib) {
            m_timer.SetOwner(this);
            m_timer.Start(ib->GetEffectDuration() + 1000, true);
        }

        event.Skip();
    }

    void OnTimer([[maybe_unused]] wxTimerEvent& event)
    {
        wxInfoBar* ib = wxDynamicCast(m_window, wxInfoBar);
        if (ib) {
            ib->ShowMessage(_("Message ..."));
        }
    }

private:
    wxWindow* m_window;
    IManager* m_manager;
    wxTimer m_timer;

    wxDECLARE_EVENT_TABLE();
};

wxBEGIN_EVENT_TABLE(ComponentEvtHandler, wxEvtHandler)
EVT_TEXT(wxID_ANY, ComponentEvtHandler::OnText)
EVT_CHECKBOX(wxID_ANY, ComponentEvtHandler::OnChecked)
EVT_CHOICE(wxID_ANY, ComponentEvtHandler::OnChoice)
EVT_COMBOBOX(wxID_ANY, ComponentEvtHandler::OnComboBox)

// Tools do not get click events, so this will help select them
EVT_TOOL(wxID_ANY, ComponentEvtHandler::OnTool)

// wxInfoBar related handlers
EVT_BUTTON(wxID_ANY, ComponentEvtHandler::OnButton)
EVT_TIMER(wxID_ANY, ComponentEvtHandler::OnTimer)
wxEND_EVENT_TABLE()

class wxLeftDownRedirect : public wxEvtHandler
{
private:
    wxWindow* m_window;
    IManager* m_manager;

    void OnLeftDown([[maybe_unused]] wxMouseEvent& event)
    {
        m_manager->SelectObject(m_window);
    }

public:
    wxLeftDownRedirect(wxWindow* win, IManager* manager) : m_window(win), m_manager(manager) {}

    wxDECLARE_EVENT_TABLE();
};

wxBEGIN_EVENT_TABLE(wxLeftDownRedirect, wxEvtHandler)
EVT_LEFT_DOWN(wxLeftDownRedirect::OnLeftDown)
wxEND_EVENT_TABLE()

///////////////////////////////////////////////////////////////////////////////

class ButtonComponent : public ComponentBase
{
public:
    wxObject* Create(IObject* obj, wxObject* parent) override
    {
        wxString label = obj->GetPropertyAsString(_("label"));
        wxButton* button = new wxButton(
          (wxWindow*)parent, wxID_ANY, label, obj->GetPropertyAsPoint(_("pos")), obj->GetPropertyAsSize(_("size")),
          obj->GetPropertyAsInteger(_("style")) | obj->GetPropertyAsInteger(_("window_style")));

        if (obj->GetPropertyAsInteger(_("markup")) != 0) {
            button->SetLabelMarkup(label);
        }

        if (obj->GetPropertyAsInteger(_("default")) != 0) {
            button->SetDefault();
        }

        if (obj->GetPropertyAsInteger(_("auth_needed")) != 0) {
            button->SetAuthNeeded();
        }

        if (!obj->IsPropertyNull(_("bitmap"))) {
            button->SetBitmap(obj->GetPropertyAsBitmap(_("bitmap")));
        }

        if (!obj->IsPropertyNull(_("disabled"))) {
            button->SetBitmapDisabled(obj->GetPropertyAsBitmap(_("disabled")));
        }

        if (!obj->IsPropertyNull(_("pressed"))) {
            button->SetBitmapPressed(obj->GetPropertyAsBitmap(_("pressed")));
        }

        if (!obj->IsPropertyNull(_("focus"))) {
            button->SetBitmapFocus(obj->GetPropertyAsBitmap(_("focus")));
        }

        if (!obj->IsPropertyNull(_("current"))) {
            button->SetBitmapCurrent(obj->GetPropertyAsBitmap(_("current")));
        }

        if (!obj->IsPropertyNull(_("position"))) {
            button->SetBitmapPosition(static_cast<wxDirection>(obj->GetPropertyAsInteger(_("position"))));
        }

        if (!obj->IsPropertyNull(_("margins"))) {
            button->SetBitmapMargins(obj->GetPropertyAsSize(_("margins")));
        }

        return button;
    }

    ticpp::Element* ExportToXrc(IObject* obj) override
    {
        ObjectToXrcFilter xrc(GetLibrary(), obj, _("wxButton"), obj->GetPropertyAsString(_("name")));
        xrc.AddWindowProperties();
        xrc.AddProperty(_("label"), _("label"), XrcFilter::Type::Text);
        xrc.AddProperty(_("default"), _("default"), XrcFilter::Type::Bool);
        xrc.AddProperty(_("auth_needed"), _("auth_needed"), XrcFilter::Type::Bool);
        xrc.AddProperty(_("markup"), _("markup"), XrcFilter::Type::Bool);
        xrc.AddProperty(_("bitmap"), _("bitmap"), XrcFilter::Type::Bitmap);
        if (!obj->IsPropertyNull(_("disabled"))) {
            xrc.AddProperty(_("disabled"), _("disabled"), XrcFilter::Type::Bitmap);
        }
        if (!obj->IsPropertyNull(_("pressed"))) {
            xrc.AddProperty(_("pressed"), _("pressed"), XrcFilter::Type::Bitmap);
        }
        if (!obj->IsPropertyNull(_("focus"))) {
            xrc.AddProperty(_("focus"), _("focus"), XrcFilter::Type::Bitmap);
        }
        if (!obj->IsPropertyNull(_("current"))) {
            xrc.AddProperty(_("current"), _("current"), XrcFilter::Type::Bitmap);
        }
        if (!obj->IsPropertyNull(_("position"))) {
            xrc.AddProperty(_("position"), _("bitmapposition"), XrcFilter::Type::Text);
        }
        if (!obj->IsPropertyNull(_("margins"))) {
            xrc.AddProperty(_("margins"), _("margins"), XrcFilter::Type::Size);
        }
        return xrc.GetXrcObject();
    }
    tinyxml2::XMLElement* ExportToXrc(tinyxml2::XMLElement* xrc, const IObject* obj) override
    {
        ObjectToXrcFilter filter(xrc, GetLibrary(), obj);
        filter.AddWindowProperties();
        filter.AddProperty(XrcFilter::Type::Text, "label");
        filter.AddProperty(XrcFilter::Type::Bool, "default");
        filter.AddProperty(XrcFilter::Type::Bool, "auth_needed");
        filter.AddProperty(XrcFilter::Type::Bool, "markup");
        filter.AddProperty(XrcFilter::Type::Bitmap, "bitmap");
        if (!obj->IsPropertyNull("disabled")) {
            filter.AddProperty(XrcFilter::Type::Bitmap, "disabled");
        }
        if (!obj->IsPropertyNull("pressed")) {
            filter.AddProperty(XrcFilter::Type::Bitmap, "pressed");
        }
        if (!obj->IsPropertyNull("focus")) {
            filter.AddProperty(XrcFilter::Type::Bitmap, "focus");
        }
        if (!obj->IsPropertyNull("current")) {
            filter.AddProperty(XrcFilter::Type::Bitmap, "current");
        }
        if (!obj->IsPropertyNull("position")) {
            filter.AddProperty(XrcFilter::Type::Text, "position", "bitmapposition");
        }
        if (!obj->IsPropertyNull("margins")) {
            filter.AddProperty(XrcFilter::Type::Size, "margins");
        }
        return xrc;
    }

    ticpp::Element* ImportFromXrc(ticpp::Element* xrcObj) override
    {
        XrcToXfbFilter filter(GetLibrary(), xrcObj, _("wxButton"));
        filter.AddWindowProperties();
        filter.AddProperty(_("label"), _("label"), XrcFilter::Type::Text);
        filter.AddProperty(_("default"), _("default"), XrcFilter::Type::Bool);
        filter.AddProperty(_("auth_needed"), _("auth_needed"), XrcFilter::Type::Bool);
        filter.AddProperty(_("markup"), _("markup"), XrcFilter::Type::Bool);
        filter.AddProperty(_("bitmap"), _("bitmap"), XrcFilter::Type::Bitmap);
        filter.AddProperty(_("disabled"), _("disabled"), XrcFilter::Type::Bitmap);
        filter.AddProperty(_("pressed"), _("pressed"), XrcFilter::Type::Bitmap);
        filter.AddProperty(_("focus"), _("focus"), XrcFilter::Type::Bitmap);
        filter.AddProperty(_("current"), _("current"), XrcFilter::Type::Bitmap);
        filter.AddProperty(_("bitmapposition"), _("position"), XrcFilter::Type::Text);
        filter.AddProperty(_("margins"), _("margins"), XrcFilter::Type::Size);
        return filter.GetXfbObject();
    }
    tinyxml2::XMLElement* ImportFromXrc(tinyxml2::XMLElement* xfb, const tinyxml2::XMLElement* xrc) override
    {
        XrcToXfbFilter filter(xfb, GetLibrary(), xrc);
        filter.AddWindowProperties();
        filter.AddProperty(XrcFilter::Type::Text, "label");
        filter.AddProperty(XrcFilter::Type::Bool, "default");
        filter.AddProperty(XrcFilter::Type::Bool, "auth_needed");
        filter.AddProperty(XrcFilter::Type::Bool, "markup");
        filter.AddProperty(XrcFilter::Type::Bitmap, "bitmap");
        filter.AddProperty(XrcFilter::Type::Bitmap, "disabled");
        filter.AddProperty(XrcFilter::Type::Bitmap, "pressed");
        filter.AddProperty(XrcFilter::Type::Bitmap, "focus");
        filter.AddProperty(XrcFilter::Type::Bitmap, "current");
        filter.AddProperty(XrcFilter::Type::Text, "bitmapposition", "position");
        filter.AddProperty(XrcFilter::Type::Size, "margins");
        return xfb;
    }
};

class BitmapButtonComponent : public ComponentBase
{
public:
    wxObject* Create(IObject* obj, wxObject* parent) override
    {
        wxBitmapButton* button = new wxBitmapButton(
          (wxWindow*)parent, wxID_ANY, obj->GetPropertyAsBitmap(_("bitmap")), obj->GetPropertyAsPoint(_("pos")),
          obj->GetPropertyAsSize(_("size")),
          obj->GetPropertyAsInteger(_("style")) | obj->GetPropertyAsInteger(_("window_style")));

        // To stay in sync what the generator templates do apply the markup label here as well
        if (obj->GetPropertyAsInteger(_("markup")) != 0) {
            button->SetLabelMarkup(obj->GetPropertyAsString(_("label")));
        }

        if (obj->GetPropertyAsInteger(_("default")) != 0) {
            button->SetDefault();
        }

        if (obj->GetPropertyAsInteger(_("auth_needed")) != 0) {
            button->SetAuthNeeded();
        }

        if (!obj->IsPropertyNull(_("disabled"))) {
            button->SetBitmapDisabled(obj->GetPropertyAsBitmap(_("disabled")));
        }

        if (!obj->IsPropertyNull(_("pressed"))) {
            button->SetBitmapPressed(obj->GetPropertyAsBitmap(_("pressed")));
        }

        if (!obj->IsPropertyNull(_("focus"))) {
            button->SetBitmapFocus(obj->GetPropertyAsBitmap(_("focus")));
        }

        if (!obj->IsPropertyNull(_("current"))) {
            button->SetBitmapCurrent(obj->GetPropertyAsBitmap(_("current")));
        }

        if (!obj->IsPropertyNull(_("position"))) {
            button->SetBitmapPosition(static_cast<wxDirection>(obj->GetPropertyAsInteger(_("position"))));
        }

        if (!obj->IsPropertyNull(_("margins"))) {
            button->SetBitmapMargins(obj->GetPropertyAsSize(_("margins")));
        }

        return button;
    }

    ticpp::Element* ExportToXrc(IObject* obj) override
    {
        ObjectToXrcFilter xrc(GetLibrary(), obj, _("wxBitmapButton"), obj->GetPropertyAsString(_("name")));
        xrc.AddWindowProperties();
        xrc.AddProperty(_("bitmap"), _("bitmap"), XrcFilter::Type::Bitmap);
        if (!obj->IsPropertyNull(_("disabled"))) {
            xrc.AddProperty(_("disabled"), _("disabled"), XrcFilter::Type::Bitmap);
        }
        if (!obj->IsPropertyNull(_("pressed"))) {
            xrc.AddProperty(_("pressed"), _("pressed"), XrcFilter::Type::Bitmap);
        }
        if (!obj->IsPropertyNull(_("focus"))) {
            xrc.AddProperty(_("focus"), _("focus"), XrcFilter::Type::Bitmap);
        }
        if (!obj->IsPropertyNull(_("current"))) {
            xrc.AddProperty(_("current"), _("current"), XrcFilter::Type::Bitmap);
        }
        if (!obj->IsPropertyNull(_("position"))) {
            xrc.AddProperty(_("position"), _("bitmapposition"), XrcFilter::Type::Text);
        }
        if (!obj->IsPropertyNull(_("margins"))) {
            xrc.AddProperty(_("margins"), _("margins"), XrcFilter::Type::Size);
        }
        xrc.AddProperty(_("default"), _("default"), XrcFilter::Type::Bool);
        xrc.AddProperty(_("auth_needed"), _("auth_needed"), XrcFilter::Type::Bool);
        return xrc.GetXrcObject();
    }
    tinyxml2::XMLElement* ExportToXrc(tinyxml2::XMLElement* xrc, const IObject* obj) override
    {
        ObjectToXrcFilter filter(xrc, GetLibrary(), obj);
        filter.AddWindowProperties();
        filter.AddProperty(XrcFilter::Type::Bool, "default");
        filter.AddProperty(XrcFilter::Type::Bool, "auth_needed");
        filter.AddProperty(XrcFilter::Type::Bitmap, "bitmap");
        if (!obj->IsPropertyNull("disabled")) {
            filter.AddProperty(XrcFilter::Type::Bitmap, "disabled");
        }
        if (!obj->IsPropertyNull("pressed")) {
            filter.AddProperty(XrcFilter::Type::Bitmap, "pressed");
        }
        if (!obj->IsPropertyNull("focus")) {
            filter.AddProperty(XrcFilter::Type::Bitmap, "focus");
        }
        if (!obj->IsPropertyNull("current")) {
            filter.AddProperty(XrcFilter::Type::Bitmap, "current");
        }
        if (!obj->IsPropertyNull("position")) {
            filter.AddProperty(XrcFilter::Type::Text, "position", "bitmapposition");
        }
        if (!obj->IsPropertyNull("margins")) {
            filter.AddProperty(XrcFilter::Type::Size, "margins");
        }
        return xrc;
    }

    ticpp::Element* ImportFromXrc(ticpp::Element* xrcObj) override
    {
        XrcToXfbFilter filter(GetLibrary(), xrcObj, _("wxBitmapButton"));
        filter.AddWindowProperties();
        filter.AddProperty(_("bitmap"), _("bitmap"), XrcFilter::Type::Bitmap);
        filter.AddProperty(_("disabled"), _("disabled"), XrcFilter::Type::Bitmap);
        filter.AddProperty(_("pressed"), _("pressed"), XrcFilter::Type::Bitmap);
        filter.AddProperty(_("focus"), _("focus"), XrcFilter::Type::Bitmap);
        filter.AddProperty(_("current"), _("current"), XrcFilter::Type::Bitmap);
        filter.AddProperty(_("bitmapposition"), _("position"), XrcFilter::Type::Text);
        filter.AddProperty(_("margins"), _("margins"), XrcFilter::Type::Size);
        filter.AddProperty(_("default"), _("default"), XrcFilter::Type::Bool);
        filter.AddProperty(_("auth_needed"), _("auth_needed"), XrcFilter::Type::Bool);
        return filter.GetXfbObject();
    }
    tinyxml2::XMLElement* ImportFromXrc(tinyxml2::XMLElement* xfb, const tinyxml2::XMLElement* xrc) override
    {
        XrcToXfbFilter filter(xfb, GetLibrary(), xrc);
        filter.AddWindowProperties();
        filter.AddProperty(XrcFilter::Type::Bool, "default");
        filter.AddProperty(XrcFilter::Type::Bool, "auth_needed");
        filter.AddProperty(XrcFilter::Type::Bitmap, "bitmap");
        filter.AddProperty(XrcFilter::Type::Bitmap, "disabled");
        filter.AddProperty(XrcFilter::Type::Bitmap, "pressed");
        filter.AddProperty(XrcFilter::Type::Bitmap, "focus");
        filter.AddProperty(XrcFilter::Type::Bitmap, "current");
        filter.AddProperty(XrcFilter::Type::Text, "bitmapposition", "position");
        filter.AddProperty(XrcFilter::Type::Size, "margins");
        return xfb;
    }
};

class TextCtrlComponent : public ComponentBase
{
public:
    wxObject* Create(IObject* obj, wxObject* parent) override
    {
        wxTextCtrl* tc = new wxTextCtrl(
          (wxWindow*)parent, wxID_ANY, obj->GetPropertyAsString(_("value")), obj->GetPropertyAsPoint(_("pos")),
          obj->GetPropertyAsSize(_("size")),
          obj->GetPropertyAsInteger(_("style")) | obj->GetPropertyAsInteger(_("window_style")));

        if (!obj->IsPropertyNull(_("maxlength"))) {
            tc->SetMaxLength(obj->GetPropertyAsInteger(_("maxlength")));
        }

        tc->PushEventHandler(new ComponentEvtHandler(tc, GetManager()));

        return tc;
    }

    void Cleanup(wxObject* obj) override
    {
        auto* window = wxDynamicCast(obj, wxTextCtrl);
        if (window) {
            window->PopEventHandler(true);
        }
    }

    ticpp::Element* ExportToXrc(IObject* obj) override
    {
        ObjectToXrcFilter xrc(GetLibrary(), obj, _("wxTextCtrl"), obj->GetPropertyAsString(_("name")));
        xrc.AddWindowProperties();
        xrc.AddProperty(_("value"), _("value"), XrcFilter::Type::Text);
        if (!obj->IsPropertyNull(_("maxlength")))
            xrc.AddProperty(_("maxlength"), _("maxlength"), XrcFilter::Type::Integer);
        return xrc.GetXrcObject();
    }
    tinyxml2::XMLElement* ExportToXrc(tinyxml2::XMLElement* xrc, const IObject* obj) override
    {
        ObjectToXrcFilter filter(xrc, GetLibrary(), obj);
        filter.AddWindowProperties();
        filter.AddProperty(XrcFilter::Type::Text, "value");
        if (!obj->IsPropertyNull("maxlength")) {
            filter.AddProperty(XrcFilter::Type::Integer, "maxlength");
        }
        return xrc;
    }

    ticpp::Element* ImportFromXrc(ticpp::Element* xrcObj) override
    {
        XrcToXfbFilter filter(GetLibrary(), xrcObj, _("wxTextCtrl"));
        filter.AddWindowProperties();
        filter.AddProperty(_("value"), _("value"), XrcFilter::Type::Text);
        filter.AddProperty(_("maxlength"), _("maxlength"), XrcFilter::Type::Integer);
        return filter.GetXfbObject();
    }
    tinyxml2::XMLElement* ImportFromXrc(tinyxml2::XMLElement* xfb, const tinyxml2::XMLElement* xrc) override
    {
        XrcToXfbFilter filter(xfb, GetLibrary(), xrc);
        filter.AddWindowProperties();
        filter.AddProperty(XrcFilter::Type::Text, "value");
        filter.AddProperty(XrcFilter::Type::Integer, "maxlength");
        return xfb;
    }
};

class StaticTextComponent : public ComponentBase
{
public:
    wxObject* Create(IObject* obj, wxObject* parent) override
    {
        wxString label = obj->GetPropertyAsString(_("label"));
        wxStaticText* st = new wxStaticText(
          (wxWindow*)parent, wxID_ANY, label, obj->GetPropertyAsPoint(_("pos")), obj->GetPropertyAsSize(_("size")),
          obj->GetPropertyAsInteger(_("style")) | obj->GetPropertyAsInteger(_("window_style")));

        st->Wrap(obj->GetPropertyAsInteger(_("wrap")));

        if (obj->GetPropertyAsInteger(_("markup")) != 0) {
            st->SetLabelMarkup(label);
        }

        return st;
    }

    ticpp::Element* ExportToXrc(IObject* obj) override
    {
        wxString name = obj->GetPropertyAsString(_("name"));
        ObjectToXrcFilter xrc(GetLibrary(), obj, _("wxStaticText"), name);
        xrc.AddWindowProperties();
        xrc.AddProperty(_("label"), _("label"), XrcFilter::Type::Text);
        xrc.AddProperty(_("wrap"), _("wrap"), XrcFilter::Type::Integer);
        return xrc.GetXrcObject();
    }
    tinyxml2::XMLElement* ExportToXrc(tinyxml2::XMLElement* xrc, const IObject* obj) override
    {
        ObjectToXrcFilter filter(xrc, GetLibrary(), obj);
        filter.AddWindowProperties();
        filter.AddProperty(XrcFilter::Type::Text, "label");
        filter.AddProperty(XrcFilter::Type::Integer, "wrap");
        return xrc;
    }

    ticpp::Element* ImportFromXrc(ticpp::Element* xrcObj) override
    {
        XrcToXfbFilter filter(GetLibrary(), xrcObj, _("wxStaticText"));
        filter.AddWindowProperties();
        filter.AddProperty(_("label"), _("label"), XrcFilter::Type::Text);
        filter.AddProperty(_("wrap"), _("wrap"), XrcFilter::Type::Integer);
        return filter.GetXfbObject();
    }
    tinyxml2::XMLElement* ImportFromXrc(tinyxml2::XMLElement* xfb, const tinyxml2::XMLElement* xrc) override
    {
        XrcToXfbFilter filter(xfb, GetLibrary(), xrc);
        filter.AddWindowProperties();
        filter.AddProperty(XrcFilter::Type::Text, "label");
        filter.AddProperty(XrcFilter::Type::Integer, "wrap");
        return xfb;
    }
};

class ComboBoxComponent : public ComponentBase
{
public:
    wxObject* Create(IObject* obj, wxObject* parent) override
    {
        wxComboBox* combo = new wxComboBox(
          (wxWindow*)parent, wxID_ANY, obj->GetPropertyAsString(_("value")), obj->GetPropertyAsPoint(_("pos")),
          obj->GetPropertyAsSize(_("size")), 0, NULL,
          obj->GetPropertyAsInteger(_("style")) | obj->GetPropertyAsInteger(_("window_style")));

        // choices
        wxArrayString choices = obj->GetPropertyAsArrayString(_("choices"));
        for (unsigned int i = 0; i < choices.GetCount(); i++) combo->Append(choices[i]);

        int sel = obj->GetPropertyAsInteger(_("selection"));
        if (sel > -1 && sel < (int)choices.GetCount())
            combo->SetSelection(sel);

        combo->PushEventHandler(new ComponentEvtHandler(combo, GetManager()));

        return combo;
    }

    void Cleanup(wxObject* obj) override
    {
        auto* window = wxDynamicCast(obj, wxComboBox);
        if (window) {
            window->PopEventHandler(true);
        }
    }

    ticpp::Element* ExportToXrc(IObject* obj) override
    {
        ObjectToXrcFilter xrc(GetLibrary(), obj, _("wxComboBox"), obj->GetPropertyAsString(_("name")));
        xrc.AddWindowProperties();
        xrc.AddProperty(_("value"), _("value"), XrcFilter::Type::Text);
        xrc.AddProperty(_("choices"), _("content"), XrcFilter::Type::StringList);
        return xrc.GetXrcObject();
    }
    tinyxml2::XMLElement* ExportToXrc(tinyxml2::XMLElement* xrc, const IObject* obj) override
    {
        ObjectToXrcFilter filter(xrc, GetLibrary(), obj);
        filter.AddWindowProperties();
        filter.AddProperty(XrcFilter::Type::Text, "value");
        filter.AddProperty(XrcFilter::Type::StringList, "choices", "content");
        return xrc;
    }

    ticpp::Element* ImportFromXrc(ticpp::Element* xrcObj) override
    {
        XrcToXfbFilter filter(GetLibrary(), xrcObj, _("wxComboBox"));
        filter.AddWindowProperties();
        filter.AddProperty(_("value"), _("value"), XrcFilter::Type::Text);
        filter.AddProperty(_("content"), _("choices"), XrcFilter::Type::StringList);
        return filter.GetXfbObject();
    }
    tinyxml2::XMLElement* ImportFromXrc(tinyxml2::XMLElement* xfb, const tinyxml2::XMLElement* xrc) override
    {
        XrcToXfbFilter filter(xfb, GetLibrary(), xrc);
        filter.AddWindowProperties();
        filter.AddProperty(XrcFilter::Type::Text, "value");
        filter.AddProperty(XrcFilter::Type::StringList, "content", "choices");
        return xfb;
    }
};

class BitmapComboBoxComponent : public ComponentBase
{
public:
    wxObject* Create(IObject* obj, wxObject* parent) override
    {
        wxBitmapComboBox* bcombo = new wxBitmapComboBox(
          (wxWindow*)parent, wxID_ANY, obj->GetPropertyAsString(_("value")), obj->GetPropertyAsPoint(_("pos")),
          obj->GetPropertyAsSize(_("size")), 0, NULL,
          obj->GetPropertyAsInteger(_("style")) | obj->GetPropertyAsInteger(_("window_style")));

        // choices
        wxArrayString choices = obj->GetPropertyAsArrayString(_("choices"));
        for (unsigned int i = 0; i < choices.GetCount(); i++) {
            wxImage img(choices[i].BeforeFirst(wxChar(58)));
            bcombo->Append(choices[i].AfterFirst(wxChar(58)), wxBitmap(img));
        }

        int sel = obj->GetPropertyAsInteger(_("selection"));
        if (sel > -1 && sel < (int)choices.GetCount())
            bcombo->SetSelection(sel);

        bcombo->PushEventHandler(new ComponentEvtHandler(bcombo, GetManager()));

        return bcombo;
    }

    void Cleanup(wxObject* obj) override
    {
        auto* window = wxDynamicCast(obj, wxBitmapComboBox);
        if (window) {
            window->PopEventHandler(true);
        }
    }

    ticpp::Element* ExportToXrc(IObject* obj) override
    {
        ObjectToXrcFilter xrc(GetLibrary(), obj, _("wxBitmapComboBox"), obj->GetPropertyAsString(_("name")));
        xrc.AddWindowProperties();
        xrc.AddProperty(_("value"), _("value"), XrcFilter::Type::Text);
        xrc.AddProperty(_("choices"), _("content"), XrcFilter::Type::StringList);
        return xrc.GetXrcObject();
    }
    tinyxml2::XMLElement* ExportToXrc(tinyxml2::XMLElement* xrc, const IObject* obj) override
    {
        ObjectToXrcFilter filter(xrc, GetLibrary(), obj);
        filter.AddWindowProperties();
        filter.AddProperty(XrcFilter::Type::Text, "value");
        filter.AddProperty(XrcFilter::Type::StringList, "choices", "content");
        return xrc;
    }

    ticpp::Element* ImportFromXrc(ticpp::Element* xrcObj) override
    {
        XrcToXfbFilter filter(GetLibrary(), xrcObj, _("wxBitmapComboBox"));
        filter.AddWindowProperties();
        filter.AddProperty(_("value"), _("value"), XrcFilter::Type::Text);
        filter.AddProperty(_("content"), _("choices"), XrcFilter::Type::StringList);
        return filter.GetXfbObject();
    }
    tinyxml2::XMLElement* ImportFromXrc(tinyxml2::XMLElement* xfb, const tinyxml2::XMLElement* xrc) override
    {
        XrcToXfbFilter filter(xfb, GetLibrary(), xrc);
        filter.AddWindowProperties();
        filter.AddProperty(XrcFilter::Type::Text, "value");
        filter.AddProperty(XrcFilter::Type::StringList, "content", "choices");
        return xfb;
    }
};

class CheckBoxComponent : public ComponentBase
{
public:
    wxObject* Create(IObject* obj, wxObject* parent) override
    {
        wxCheckBox* res = new wxCheckBox(
          (wxWindow*)parent, wxID_ANY, obj->GetPropertyAsString(_("label")), obj->GetPropertyAsPoint(_("pos")),
          obj->GetPropertyAsSize(_("size")),
          obj->GetPropertyAsInteger(_("window_style")) | obj->GetPropertyAsInteger(_T("style")));
        res->SetValue(obj->GetPropertyAsInteger(_T("checked")) != 0);

        res->PushEventHandler(new ComponentEvtHandler(res, GetManager()));

        return res;
    }

    void Cleanup(wxObject* obj) override
    {
        auto* window = wxDynamicCast(obj, wxCheckBox);
        if (window) {
            window->PopEventHandler(true);
        }
    }

    ticpp::Element* ExportToXrc(IObject* obj) override
    {
        ObjectToXrcFilter xrc(GetLibrary(), obj, _("wxCheckBox"), obj->GetPropertyAsString(_("name")));
        xrc.AddWindowProperties();
        xrc.AddProperty(_("label"), _("label"), XrcFilter::Type::Text);
        xrc.AddProperty(_("checked"), _("checked"), XrcFilter::Type::Bool);
        return xrc.GetXrcObject();
    }
    tinyxml2::XMLElement* ExportToXrc(tinyxml2::XMLElement* xrc, const IObject* obj) override
    {
        ObjectToXrcFilter filter(xrc, GetLibrary(), obj);
        filter.AddWindowProperties();
        filter.AddProperty(XrcFilter::Type::Text, "label");
        filter.AddProperty(XrcFilter::Type::Bool, "checked");
        return xrc;
    }

    ticpp::Element* ImportFromXrc(ticpp::Element* xrcObj) override
    {
        XrcToXfbFilter filter(GetLibrary(), xrcObj, _("wxCheckBox"));
        filter.AddWindowProperties();
        filter.AddProperty(_("label"), _("label"), XrcFilter::Type::Text);
        filter.AddProperty(_("checked"), _("checked"), XrcFilter::Type::Bool);
        return filter.GetXfbObject();
    }
    tinyxml2::XMLElement* ImportFromXrc(tinyxml2::XMLElement* xfb, const tinyxml2::XMLElement* xrc) override
    {
        XrcToXfbFilter filter(xfb, GetLibrary(), xrc);
        filter.AddWindowProperties();
        filter.AddProperty(XrcFilter::Type::Text, "label");
        filter.AddProperty(XrcFilter::Type::Bool, "checked");
        return xfb;
    }
};

class StaticBitmapComponent : public ComponentBase
{
public:
    wxObject* Create(IObject* obj, wxObject* parent) override
    {
        return new wxStaticBitmap(
          (wxWindow*)parent, wxID_ANY, obj->GetPropertyAsBitmap(_("bitmap")), obj->GetPropertyAsPoint(_("pos")),
          obj->GetPropertyAsSize(_("size")), obj->GetPropertyAsInteger(_("window_style")));
    }

    ticpp::Element* ExportToXrc(IObject* obj) override
    {
        ObjectToXrcFilter xrc(GetLibrary(), obj, _("wxStaticBitmap"), obj->GetPropertyAsString(_("name")));
        xrc.AddWindowProperties();
        xrc.AddProperty(_("bitmap"), _("bitmap"), XrcFilter::Type::Bitmap);
        return xrc.GetXrcObject();
    }
    tinyxml2::XMLElement* ExportToXrc(tinyxml2::XMLElement* xrc, const IObject* obj) override
    {
        ObjectToXrcFilter filter(xrc, GetLibrary(), obj);
        filter.AddWindowProperties();
        filter.AddProperty(XrcFilter::Type::Bitmap, "bitmap");
        return xrc;
    }

    ticpp::Element* ImportFromXrc(ticpp::Element* xrcObj) override
    {
        XrcToXfbFilter filter(GetLibrary(), xrcObj, _("wxStaticBitmap"));
        filter.AddWindowProperties();
        filter.AddProperty(_("bitmap"), _("bitmap"), XrcFilter::Type::Bitmap);
        return filter.GetXfbObject();
    }
    tinyxml2::XMLElement* ImportFromXrc(tinyxml2::XMLElement* xfb, const tinyxml2::XMLElement* xrc) override
    {
        XrcToXfbFilter filter(xfb, GetLibrary(), xrc);
        filter.AddWindowProperties();
        filter.AddProperty(XrcFilter::Type::Bitmap, "bitmap");
        return xfb;
    }
};

class XpmStaticBitmapComponent : public StaticBitmapComponent
{
};

class StaticLineComponent : public ComponentBase
{
public:
    wxObject* Create(IObject* obj, wxObject* parent) override
    {
        return new wxStaticLine(
          (wxWindow*)parent, wxID_ANY, obj->GetPropertyAsPoint(_("pos")), obj->GetPropertyAsSize(_("size")),
          obj->GetPropertyAsInteger(_("style")) | obj->GetPropertyAsInteger(_("window_style")));
    }

    ticpp::Element* ExportToXrc(IObject* obj) override
    {
        ObjectToXrcFilter xrc(GetLibrary(), obj, _("wxStaticLine"), obj->GetPropertyAsString(_("name")));
        xrc.AddWindowProperties();
        return xrc.GetXrcObject();
    }
    tinyxml2::XMLElement* ExportToXrc(tinyxml2::XMLElement* xrc, const IObject* obj) override
    {
        ObjectToXrcFilter filter(xrc, GetLibrary(), obj);
        filter.AddWindowProperties();
        return xrc;
    }

    ticpp::Element* ImportFromXrc(ticpp::Element* xrcObj) override
    {
        XrcToXfbFilter filter(GetLibrary(), xrcObj, _("wxStaticLine"));
        filter.AddWindowProperties();
        return filter.GetXfbObject();
    }
    tinyxml2::XMLElement* ImportFromXrc(tinyxml2::XMLElement* xfb, const tinyxml2::XMLElement* xrc) override
    {
        XrcToXfbFilter filter(xfb, GetLibrary(), xrc);
        filter.AddWindowProperties();
        return xfb;
    }
};

class ListCtrlComponent : public ComponentBase
{
public:
    wxObject* Create(IObject* obj, wxObject* parent) override
    {
        wxListCtrl* lc = new wxListCtrl(
          (wxWindow*)parent, wxID_ANY, obj->GetPropertyAsPoint(_("pos")), obj->GetPropertyAsSize(_("size")),
          (obj->GetPropertyAsInteger(_("style")) | obj->GetPropertyAsInteger(_("window_style"))) & ~wxLC_VIRTUAL);


        // Refilling
        int i, j;
        wxString buf;
        if ((lc->GetWindowStyle() & wxLC_REPORT) != 0) {
            for (i = 0; i < 4; i++) {
                buf.Printf(wxT("Label %d"), i);
                lc->InsertColumn(i, buf, wxLIST_FORMAT_LEFT, 80);
            }
        }

        for (j = 0; j < 10; j++) {
            long temp;
            buf.Printf(wxT("Cell (0,%d)"), j);
            temp = lc->InsertItem(j, buf);
            if ((lc->GetWindowStyle() & wxLC_REPORT) != 0) {
                for (i = 1; i < 4; i++) {
                    buf.Printf(wxT("Cell (%d,%d)"), i, j);
                    lc->SetItem(temp, i, buf);
                }
            }
        }

        return lc;
    }

    ticpp::Element* ExportToXrc(IObject* obj) override
    {
        ObjectToXrcFilter xrc(GetLibrary(), obj, _("wxListCtrl"), obj->GetPropertyAsString(_("name")));
        xrc.AddWindowProperties();
        return xrc.GetXrcObject();
    }
    tinyxml2::XMLElement* ExportToXrc(tinyxml2::XMLElement* xrc, const IObject* obj) override
    {
        ObjectToXrcFilter filter(xrc, GetLibrary(), obj);
        filter.AddWindowProperties();
        return xrc;
    }

    ticpp::Element* ImportFromXrc(ticpp::Element* xrcObj) override
    {
        XrcToXfbFilter filter(GetLibrary(), xrcObj, _("wxListCtrl"));
        filter.AddWindowProperties();
        return filter.GetXfbObject();
    }
    tinyxml2::XMLElement* ImportFromXrc(tinyxml2::XMLElement* xfb, const tinyxml2::XMLElement* xrc) override
    {
        XrcToXfbFilter filter(xfb, GetLibrary(), xrc);
        filter.AddWindowProperties();
        return xfb;
    }
};

class ListBoxComponent : public ComponentBase
{
public:
    wxObject* Create(IObject* obj, wxObject* parent) override
    {
        wxListBox* listbox = new wxListBox(
          (wxWindow*)parent, wxID_ANY, obj->GetPropertyAsPoint(_("pos")), obj->GetPropertyAsSize(_("size")), 0, NULL,
          obj->GetPropertyAsInteger(_("style")) | obj->GetPropertyAsInteger(_("window_style")));

        // choices
        wxArrayString choices = obj->GetPropertyAsArrayString(_("choices"));
        for (unsigned int i = 0; i < choices.Count(); i++) listbox->Append(choices[i]);

        return listbox;
    }

    ticpp::Element* ExportToXrc(IObject* obj) override
    {
        ObjectToXrcFilter xrc(GetLibrary(), obj, _("wxListBox"), obj->GetPropertyAsString(_("name")));
        xrc.AddWindowProperties();
        xrc.AddProperty(_("choices"), _("content"), XrcFilter::Type::StringList);
        return xrc.GetXrcObject();
    }
    tinyxml2::XMLElement* ExportToXrc(tinyxml2::XMLElement* xrc, const IObject* obj) override
    {
        ObjectToXrcFilter filter(xrc, GetLibrary(), obj);
        filter.AddWindowProperties();
        filter.AddProperty(XrcFilter::Type::StringList, "choices", "content");
        return xrc;
    }

    ticpp::Element* ImportFromXrc(ticpp::Element* xrcObj) override
    {
        XrcToXfbFilter filter(GetLibrary(), xrcObj, _("wxListBox"));
        filter.AddWindowProperties();
        filter.AddProperty(_("content"), _("choices"), XrcFilter::Type::StringList);
        return filter.GetXfbObject();
    }
    tinyxml2::XMLElement* ImportFromXrc(tinyxml2::XMLElement* xfb, const tinyxml2::XMLElement* xrc) override
    {
        XrcToXfbFilter filter(xfb, GetLibrary(), xrc);
        filter.AddWindowProperties();
        filter.AddProperty(XrcFilter::Type::StringList, "content", "choices");
        return xfb;
    }
};

class RadioBoxComponent : public ComponentBase, public wxEvtHandler
{
public:
    wxObject* Create(IObject* obj, wxObject* parent) override
    {
        wxArrayString choices = obj->GetPropertyAsArrayString(_("choices"));
        int count = choices.Count();
        if (0 == count) {
            choices.Add(_("wxRadioBox must have at least one choice"));
            count = 1;
        }

        int majorDim = obj->GetPropertyAsInteger(_("majorDimension"));
        if (majorDim < 1) {
            wxLogWarning(_("majorDimension must be greater than zero."));
            majorDim = 1;
        }

        wxRadioBox* radiobox = new wxRadioBox(
          (wxWindow*)parent, wxID_ANY, obj->GetPropertyAsString(_("label")), obj->GetPropertyAsPoint(_("pos")),
          obj->GetPropertyAsSize(_("size")), choices, majorDim,
          obj->GetPropertyAsInteger(_("style")) | obj->GetPropertyAsInteger(_("window_style")));

        int selection = obj->GetPropertyAsInteger(_("selection"));
        if (selection < count) {
            radiobox->SetSelection(selection);
        }

        radiobox->Connect(
          wxEVT_COMMAND_RADIOBOX_SELECTED, wxCommandEventHandler(RadioBoxComponent::OnRadioBox), NULL, this);

        return radiobox;
    }

    void OnRadioBox(wxCommandEvent& event)
    {
        wxRadioBox* window = dynamic_cast<wxRadioBox*>(event.GetEventObject());
        if (0 != window) {
            wxString value;
            value.Printf(wxT("%i"), window->GetSelection());
            GetManager()->ModifyProperty(window, _("selection"), value);
            window->SetFocus();

            GetManager()->SelectObject(window);
        }
    }

    void Cleanup(wxObject* obj) override
    {
        wxRadioBox* window = dynamic_cast<wxRadioBox*>(obj);
        if (0 != window) {
            window->Disconnect(
              wxEVT_COMMAND_RADIOBOX_SELECTED, wxCommandEventHandler(RadioBoxComponent::OnRadioBox), NULL, this);
        }
        ComponentBase::Cleanup(obj);
    }

    ticpp::Element* ExportToXrc(IObject* obj) override
    {
        ObjectToXrcFilter xrc(GetLibrary(), obj, _("wxRadioBox"), obj->GetPropertyAsString(_("name")));
        xrc.AddWindowProperties();
        xrc.AddProperty(_("label"), _("label"), XrcFilter::Type::Text);
        xrc.AddProperty(_("selection"), _("selection"), XrcFilter::Type::Integer);
        xrc.AddProperty(_("choices"), _("content"), XrcFilter::Type::StringList);
        xrc.AddProperty(_("majorDimension"), _("dimension"), XrcFilter::Type::Integer);
        return xrc.GetXrcObject();
    }
    tinyxml2::XMLElement* ExportToXrc(tinyxml2::XMLElement* xrc, const IObject* obj) override
    {
        ObjectToXrcFilter filter(xrc, GetLibrary(), obj);
        filter.AddWindowProperties();
        filter.AddProperty(XrcFilter::Type::Text, "label");
        filter.AddProperty(XrcFilter::Type::Integer, "selection");
        filter.AddProperty(XrcFilter::Type::StringList, "choices", "content");
        filter.AddProperty(XrcFilter::Type::Integer, "majorDimension", "dimension");
        return xrc;
    }

    ticpp::Element* ImportFromXrc(ticpp::Element* xrcObj) override
    {
        XrcToXfbFilter filter(GetLibrary(), xrcObj, _("wxRadioBox"));
        filter.AddWindowProperties();
        filter.AddProperty(_("label"), _("label"), XrcFilter::Type::Text);
        filter.AddProperty(_("selection"), _("selection"), XrcFilter::Type::Integer);
        filter.AddProperty(_("content"), _("choices"), XrcFilter::Type::StringList);
        filter.AddProperty(_("dimension"), _("majorDimension"), XrcFilter::Type::Integer);
        return filter.GetXfbObject();
    }
    tinyxml2::XMLElement* ImportFromXrc(tinyxml2::XMLElement* xfb, const tinyxml2::XMLElement* xrc) override
    {
        XrcToXfbFilter filter(xfb, GetLibrary(), xrc);
        filter.AddWindowProperties();
        filter.AddProperty(XrcFilter::Type::Text, "label");
        filter.AddProperty(XrcFilter::Type::Integer, "selection");
        filter.AddProperty(XrcFilter::Type::StringList, "content", "choices");
        filter.AddProperty(XrcFilter::Type::Integer, "dimension", "majorDimension");
        return xfb;
    }
};

class RadioButtonComponent : public ComponentBase
{
public:
    wxObject* Create(IObject* obj, wxObject* parent) override
    {
        wxRadioButton* rb = new wxRadioButton(
          (wxWindow*)parent, wxID_ANY, obj->GetPropertyAsString(_("label")), obj->GetPropertyAsPoint(_("pos")),
          obj->GetPropertyAsSize(_("size")),
          obj->GetPropertyAsInteger(_("style")) | obj->GetPropertyAsInteger(_("window_style")));

        rb->SetValue((obj->GetPropertyAsInteger(_("value")) != 0));
        return rb;
    }

    ticpp::Element* ExportToXrc(IObject* obj) override
    {
        ObjectToXrcFilter xrc(GetLibrary(), obj, _("wxRadioButton"), obj->GetPropertyAsString(_("name")));
        xrc.AddWindowProperties();
        xrc.AddProperty(_("label"), _("label"), XrcFilter::Type::Text);
        xrc.AddProperty(_("value"), _("value"), XrcFilter::Type::Bool);
        return xrc.GetXrcObject();
    }
    tinyxml2::XMLElement* ExportToXrc(tinyxml2::XMLElement* xrc, const IObject* obj) override
    {
        ObjectToXrcFilter filter(xrc, GetLibrary(), obj);
        filter.AddWindowProperties();
        filter.AddProperty(XrcFilter::Type::Text, "label");
        filter.AddProperty(XrcFilter::Type::Bool, "value");
        return xrc;
    }

    ticpp::Element* ImportFromXrc(ticpp::Element* xrcObj) override
    {
        XrcToXfbFilter filter(GetLibrary(), xrcObj, _("wxRadioButton"));
        filter.AddWindowProperties();
        filter.AddProperty(_("label"), _("label"), XrcFilter::Type::Text);
        filter.AddProperty(_("value"), _("value"), XrcFilter::Type::Bool);
        return filter.GetXfbObject();
    }
    tinyxml2::XMLElement* ImportFromXrc(tinyxml2::XMLElement* xfb, const tinyxml2::XMLElement* xrc) override
    {
        XrcToXfbFilter filter(xfb, GetLibrary(), xrc);
        filter.AddWindowProperties();
        filter.AddProperty(XrcFilter::Type::Text, "label");
        filter.AddProperty(XrcFilter::Type::Bool, "value");
        return xfb;
    }
};

class StatusBarComponent : public ComponentBase
{
public:
    wxObject* Create(IObject* obj, wxObject* parent) override
    {
        wxStatusBar* sb = new wxIndependentStatusBar(
          (wxWindow*)parent, wxID_ANY,
          obj->GetPropertyAsInteger(_("style")) | obj->GetPropertyAsInteger(_("window_style")));
        sb->SetFieldsCount(obj->GetPropertyAsInteger(_("fields")));

#ifndef __WXMSW__
        sb->PushEventHandler(new wxLeftDownRedirect(sb, GetManager()));
#endif
        return sb;
    }

#ifndef __WXMSW__
    void Cleanup(wxObject* obj) override
    {
        auto* window = wxDynamicCast(obj, wxStatusBar);
        if (window) {
            window->PopEventHandler(true);
        }
    }
#endif

    ticpp::Element* ExportToXrc(IObject* obj) override
    {
        ObjectToXrcFilter xrc(GetLibrary(), obj, _("wxStatusBar"), obj->GetPropertyAsString(_("name")));
        xrc.AddWindowProperties();
        xrc.AddProperty(_("fields"), _("fields"), XrcFilter::Type::Integer);
        return xrc.GetXrcObject();
    }
    tinyxml2::XMLElement* ExportToXrc(tinyxml2::XMLElement* xrc, const IObject* obj) override
    {
        ObjectToXrcFilter filter(xrc, GetLibrary(), obj);
        filter.AddWindowProperties();
        filter.AddProperty(XrcFilter::Type::Integer, "fields");
        return xrc;
    }

    ticpp::Element* ImportFromXrc(ticpp::Element* xrcObj) override
    {
        XrcToXfbFilter filter(GetLibrary(), xrcObj, _("wxStatusBar"));
        filter.AddWindowProperties();
        filter.AddProperty(_("fields"), _("fields"), XrcFilter::Type::Integer);
        return filter.GetXfbObject();
    }
    tinyxml2::XMLElement* ImportFromXrc(tinyxml2::XMLElement* xfb, const tinyxml2::XMLElement* xrc) override
    {
        XrcToXfbFilter filter(xfb, GetLibrary(), xrc);
        filter.AddWindowProperties();
        filter.AddProperty(XrcFilter::Type::Integer, "fields");
        return xfb;
    }
};

class MenuBarComponent : public ComponentBase
{
public:
    wxObject* Create(IObject* obj, wxObject* /*parent*/) override
    {
        wxMenuBar* mb =
          new wxMenuBar(obj->GetPropertyAsInteger(_("style")) | obj->GetPropertyAsInteger(_("window_style")));
        return mb;
    }

    ticpp::Element* ExportToXrc(IObject* obj) override
    {
        ObjectToXrcFilter xrc(GetLibrary(), obj, _("wxMenuBar"), obj->GetPropertyAsString(_("name")));
        return xrc.GetXrcObject();
    }
    tinyxml2::XMLElement* ExportToXrc(tinyxml2::XMLElement* xrc, const IObject* obj) override
    {
        ObjectToXrcFilter filter(xrc, GetLibrary(), obj);
        return xrc;
    }

    ticpp::Element* ImportFromXrc(ticpp::Element* xrcObj) override
    {
        XrcToXfbFilter filter(GetLibrary(), xrcObj, _("wxMenuBar"));
        return filter.GetXfbObject();
    }
    tinyxml2::XMLElement* ImportFromXrc(tinyxml2::XMLElement* xfb, const tinyxml2::XMLElement* xrc) override
    {
        XrcToXfbFilter filter(xfb, GetLibrary(), xrc);
        return xfb;
    }
};

class MenuComponent : public ComponentBase
{
public:
    ticpp::Element* ExportToXrc(IObject* obj) override
    {
        ObjectToXrcFilter xrc(GetLibrary(), obj, _("wxMenu"), obj->GetPropertyAsString(_("name")));
        xrc.AddProperty(_("label"), _("label"), XrcFilter::Type::Text);
        return xrc.GetXrcObject();
    }
    tinyxml2::XMLElement* ExportToXrc(tinyxml2::XMLElement* xrc, const IObject* obj) override
    {
        ObjectToXrcFilter filter(xrc, GetLibrary(), obj);
        filter.AddProperty(XrcFilter::Type::Text, "label");
        return xrc;
    }

    ticpp::Element* ImportFromXrc(ticpp::Element* xrcObj) override
    {
        XrcToXfbFilter filter(GetLibrary(), xrcObj, _("wxMenu"));
        filter.AddProperty(_("label"), _("label"), XrcFilter::Type::Text);
        return filter.GetXfbObject();
    }
    tinyxml2::XMLElement* ImportFromXrc(tinyxml2::XMLElement* xfb, const tinyxml2::XMLElement* xrc) override
    {
        XrcToXfbFilter filter(xfb, GetLibrary(), xrc);
        filter.AddProperty(XrcFilter::Type::Text, "label");
        return xfb;
    }
};

class SubMenuComponent : public ComponentBase
{
public:
    ticpp::Element* ExportToXrc(IObject* obj) override
    {
        ObjectToXrcFilter xrc(GetLibrary(), obj, _("wxMenu"), obj->GetPropertyAsString(_("name")));
        xrc.AddProperty(_("label"), _("label"), XrcFilter::Type::Text);
        return xrc.GetXrcObject();
    }
    tinyxml2::XMLElement* ExportToXrc(tinyxml2::XMLElement* xrc, const IObject* obj) override
    {
        ObjectToXrcFilter filter(xrc, GetLibrary(), obj, "wxMenu");
        filter.AddProperty(XrcFilter::Type::Text, "label");
        return xrc;
    }

    ticpp::Element* ImportFromXrc(ticpp::Element* xrcObj) override
    {
        XrcToXfbFilter filter(GetLibrary(), xrcObj, _("submenu"));
        filter.AddProperty(_("label"), _("label"), XrcFilter::Type::Text);
        return filter.GetXfbObject();
    }
    tinyxml2::XMLElement* ImportFromXrc(tinyxml2::XMLElement* xfb, const tinyxml2::XMLElement* xrc) override
    {
        XrcToXfbFilter filter(xfb, GetLibrary(), xrc, "submenu");
        filter.AddProperty(XrcFilter::Type::Text, "label");
        return xfb;
    }
};

class MenuItemComponent : public ComponentBase
{
public:
    ticpp::Element* ExportToXrc(IObject* obj) override
    {
        ObjectToXrcFilter xrc(GetLibrary(), obj, _("wxMenuItem"), obj->GetPropertyAsString(_("name")));
        wxString shortcut = obj->GetPropertyAsString(_("shortcut"));
        wxString label;
        if (shortcut.IsEmpty())
            label = obj->GetPropertyAsString(_("label"));
        else
            label = obj->GetPropertyAsString(_("label")) + wxT("\t") + shortcut;

        xrc.AddPropertyValue(_("label"), label, true);
        xrc.AddProperty(_("help"), _("help"), XrcFilter::Type::Text);

        if (!obj->IsPropertyNull(_("bitmap")))
            xrc.AddProperty(_("bitmap"), _("bitmap"), XrcFilter::Type::Bitmap);

        int kind = obj->GetPropertyAsInteger(_("kind"));

        if (obj->GetPropertyAsInteger(_("checked")) != 0 && (kind == wxITEM_RADIO || kind == wxITEM_CHECK)) {
            xrc.AddProperty(_("checked"), _("checked"), XrcFilter::Type::Bool);
        }
        if (obj->GetPropertyAsInteger(_("enabled")) == 0)
            xrc.AddProperty(_("enabled"), _("enabled"), XrcFilter::Type::Bool);

        switch (kind) {
            case wxITEM_CHECK:
                xrc.AddPropertyValue(_("checkable"), _("1"));
                break;
            case wxITEM_RADIO:
                xrc.AddPropertyValue(_("radio"), _("1"));
                break;
        }

        return xrc.GetXrcObject();
    }
    tinyxml2::XMLElement* ExportToXrc(tinyxml2::XMLElement* xrc, const IObject* obj) override
    {
        ObjectToXrcFilter filter(xrc, GetLibrary(), obj);
        filter.AddProperty(XrcFilter::Type::Text, "label");
        filter.AddProperty(XrcFilter::Type::Text, "shortcut", "accel");
        filter.AddProperty(XrcFilter::Type::Text, "help");
        if (!obj->IsPropertyNull("bitmap")) {
            filter.AddProperty(XrcFilter::Type::Bitmap, "bitmap");
        }
        if (obj->GetPropertyAsInteger("enabled") == 0) {
            filter.AddProperty(XrcFilter::Type::Bool, "enabled");
        }
        auto kind = obj->GetPropertyAsInteger("kind");
        switch (kind) {
            case wxITEM_CHECK:
                filter.AddPropertyValue("checkable", "1");
                break;
            case wxITEM_RADIO:
                filter.AddPropertyValue("radio", "1");
                break;
        }
        if ((kind == wxITEM_CHECK || kind == wxITEM_RADIO) && obj->GetPropertyAsInteger("checked") != 0) {
            filter.AddProperty(XrcFilter::Type::Bool, "checked");
        }
        return xrc;
    }

    ticpp::Element* ImportFromXrc(ticpp::Element* xrcObj) override
    {
        XrcToXfbFilter filter(GetLibrary(), xrcObj, _("wxMenuItem"));

        try {
            ticpp::Element* labelElement = xrcObj->FirstChildElement("label");
            wxString label(labelElement->GetText().c_str(), wxConvUTF8);

            wxString shortcut;
            int pos = label.Find(wxT("\\t"));
            if (pos >= 0) {
                shortcut = label.Mid(pos + 2);
                label = label.Left(pos);
            }

            filter.AddPropertyValue(_("label"), label, true);
            filter.AddPropertyValue(_("shortcut"), shortcut);
        } catch (ticpp::Exception&) {
        }

        filter.AddProperty(_("help"), _("help"), XrcFilter::Type::Text);
        filter.AddProperty(_("bitmap"), _("bitmap"), XrcFilter::Type::Bitmap);
        return filter.GetXfbObject();
    }
    tinyxml2::XMLElement* ImportFromXrc(tinyxml2::XMLElement* xfb, const tinyxml2::XMLElement* xrc) override
    {
        XrcToXfbFilter filter(xfb, GetLibrary(), xrc);
        filter.AddProperty(XrcFilter::Type::Text, "label");
        filter.AddProperty(XrcFilter::Type::Text, "accel", "shortcut");
        filter.AddProperty(XrcFilter::Type::Text, "help");
        filter.AddProperty(XrcFilter::Type::Bitmap, "bitmap");
        filter.AddProperty(XrcFilter::Type::Bool, "enabled");
        wxString kind = "wxITEM_NORMAL";
        if (const auto* checkableElement = xrc->FirstChildElement("checkable"); checkableElement && checkableElement->IntText() != 0) {
            kind = "wxITEM_CHECK";
        } else if (const auto* radioElement = xrc->FirstChildElement("radio"); radioElement && radioElement->IntText() != 0) {
            kind = "wxITEM_RADIO";
        }
        filter.AddPropertyValue("kind", kind);
        if (kind == "wxITEM_CHECK" || kind == "wxITEM_RADIO") {
            filter.AddProperty(XrcFilter::Type::Bool, "checked");
        }
        return xfb;
    }
};

class SeparatorComponent : public ComponentBase
{
public:
    ticpp::Element* ExportToXrc(IObject* obj) override
    {
        ObjectToXrcFilter xrc(GetLibrary(), obj, _("separator"));
        return xrc.GetXrcObject();
    }
    tinyxml2::XMLElement* ExportToXrc(tinyxml2::XMLElement* xrc, const IObject* obj) override
    {
        ObjectToXrcFilter filter(xrc, GetLibrary(), obj, std::nullopt, "");
        return xrc;
    }

    ticpp::Element* ImportFromXrc(ticpp::Element* xrcObj) override
    {
        XrcToXfbFilter filter(GetLibrary(), xrcObj, _("separator"));
        return filter.GetXfbObject();
    }
    tinyxml2::XMLElement* ImportFromXrc(tinyxml2::XMLElement* xfb, const tinyxml2::XMLElement* xrc) override
    {
        XrcToXfbFilter filter(xfb, GetLibrary(), xrc, std::nullopt, "");
        return xfb;
    }
};

class ToolBarComponent : public ComponentBase
{
public:
    wxObject* Create(IObject* obj, wxObject* parent) override
    {
        wxToolBar* tb = new wxToolBar(
          (wxWindow*)parent, wxID_ANY, obj->GetPropertyAsPoint(_("pos")), obj->GetPropertyAsSize(_("size")),
          obj->GetPropertyAsInteger(_("style")) | obj->GetPropertyAsInteger(_("window_style")) | wxTB_NOALIGN |
            wxTB_NODIVIDER | wxNO_BORDER);

        if (!obj->IsPropertyNull(_("bitmapsize")))
            tb->SetToolBitmapSize(obj->GetPropertyAsSize(_("bitmapsize")));
        if (!obj->IsPropertyNull(_("margins"))) {
            wxSize margins(obj->GetPropertyAsSize(_("margins")));
            tb->SetMargins(margins.GetWidth(), margins.GetHeight());
        }
        if (!obj->IsPropertyNull(_("packing")))
            tb->SetToolPacking(obj->GetPropertyAsInteger(_("packing")));
        if (!obj->IsPropertyNull(_("separation")))
            tb->SetToolSeparation(obj->GetPropertyAsInteger(_("separation")));

        tb->PushEventHandler(new ComponentEvtHandler(tb, GetManager()));

        return tb;
    }

    void Cleanup(wxObject* obj) override
    {
        auto* window = wxDynamicCast(obj, wxToolBar);
        if (window) {
            window->PopEventHandler(true);
        }
    }

    void OnCreated(wxObject* wxobject, wxWindow* /*wxparent*/) override
    {
        wxToolBar* tb = wxDynamicCast(wxobject, wxToolBar);
        if (NULL == tb) {
            // very very strange
            return;
        }

        size_t count = GetManager()->GetChildCount(wxobject);
        for (size_t i = 0; i < count; ++i) {
            wxObject* child = GetManager()->GetChild(wxobject, i);
            IObject* childObj = GetManager()->GetIObject(child);
            if (wxT("tool") == childObj->GetClassName()) {
                tb->AddTool(
                  wxID_ANY, childObj->GetPropertyAsString(_("label")), childObj->GetPropertyAsBitmap(_("bitmap")),
                  wxNullBitmap, (wxItemKind)childObj->GetPropertyAsInteger(_("kind")),
                  childObj->GetPropertyAsString(_("help")), wxEmptyString, child);
            } else if (wxT("toolSeparator") == childObj->GetClassName()) {
                tb->AddSeparator();
            } else {
                wxControl* control = wxDynamicCast(child, wxControl);
                if (NULL != control) {
                    tb->AddControl(control);
                }
            }
        }

        tb->Realize();
    }

    ticpp::Element* ExportToXrc(IObject* obj) override
    {
        ObjectToXrcFilter xrc(GetLibrary(), obj, _("wxToolBar"), obj->GetPropertyAsString(_("name")));
        xrc.AddWindowProperties();
        xrc.AddProperty(_("bitmapsize"), _("bitmapsize"), XrcFilter::Type::Size);
        xrc.AddProperty(_("margins"), _("margins"), XrcFilter::Type::Size);
        xrc.AddProperty(_("packing"), _("packing"), XrcFilter::Type::Integer);
        xrc.AddProperty(_("separation"), _("separation"), XrcFilter::Type::Integer);
        return xrc.GetXrcObject();
    }
    tinyxml2::XMLElement* ExportToXrc(tinyxml2::XMLElement* xrc, const IObject* obj) override
    {
        ObjectToXrcFilter filter(xrc, GetLibrary(), obj);
        filter.AddWindowProperties();
        filter.AddProperty(XrcFilter::Type::Size, "bitmapsize");
        filter.AddProperty(XrcFilter::Type::Size, "margins");
        filter.AddProperty(XrcFilter::Type::Integer, "packing");
        filter.AddProperty(XrcFilter::Type::Integer, "separation");
        return xrc;
    }

    ticpp::Element* ImportFromXrc(ticpp::Element* xrcObj) override
    {
        XrcToXfbFilter filter(GetLibrary(), xrcObj, _("wxToolBar"));
        filter.AddWindowProperties();
        filter.AddProperty(_("bitmapsize"), _("bitmapsize"), XrcFilter::Type::Size);
        filter.AddProperty(_("margins"), _("margins"), XrcFilter::Type::Size);
        filter.AddProperty(_("packing"), _("packing"), XrcFilter::Type::Integer);
        filter.AddProperty(_("separation"), _("separation"), XrcFilter::Type::Integer);
        return filter.GetXfbObject();
    }
    tinyxml2::XMLElement* ImportFromXrc(tinyxml2::XMLElement* xfb, const tinyxml2::XMLElement* xrc) override
    {
        XrcToXfbFilter filter(xfb, GetLibrary(), xrc);
        filter.AddWindowProperties();
        filter.AddProperty(XrcFilter::Type::Size, "bitmapsize");
        filter.AddProperty(XrcFilter::Type::Size, "margins");
        filter.AddProperty(XrcFilter::Type::Integer, "packing");
        filter.AddProperty(XrcFilter::Type::Integer, "separation");
        return xfb;
    }
};

class AuiToolBarComponent : public ComponentBase
{
public:
    wxObject* Create(IObject* obj, wxObject* parent) override
    {
        AuiToolBar* tb = new AuiToolBar(
          (wxWindow*)parent, GetManager(), wxID_ANY, obj->GetPropertyAsPoint(_("pos")),
          obj->GetPropertyAsSize(_("size")),
          obj->GetPropertyAsInteger(_("style")));  // | obj->GetPropertyAsInteger(_("window_style")) | wxTB_NOALIGN |
                                                   // wxTB_NODIVIDER | wxNO_BORDER);

        if (!obj->IsPropertyNull(_("bitmapsize")))
            tb->SetToolBitmapSize(obj->GetPropertyAsSize(_("bitmapsize")));
        if (!obj->IsPropertyNull(_("margins"))) {
            wxSize margins(obj->GetPropertyAsSize(_("margins")));
            tb->SetMargins(margins.GetWidth(), margins.GetHeight());
        }
        if (!obj->IsPropertyNull(_("packing")))
            tb->SetToolPacking(obj->GetPropertyAsInteger(_("packing")));
        if (!obj->IsPropertyNull(_("separation")))
            tb->SetToolSeparation(obj->GetPropertyAsInteger(_("separation")));

        return tb;
    }

    void OnCreated(wxObject* wxobject, wxWindow* /*wxparent*/) override
    {
        AuiToolBar* tb = wxDynamicCast(wxobject, AuiToolBar);
        if (NULL == tb) {
            // very very strange
            return;
        }
        size_t count = GetManager()->GetChildCount(wxobject);
        for (size_t i = 0; i < count; ++i) {
            wxObject* child = GetManager()->GetChild(wxobject, i);
            IObject* childObj = GetManager()->GetIObject(child);
            if (wxT("tool") == childObj->GetClassName()) {
                tb->AddTool(
                  wxID_ANY, childObj->GetPropertyAsString(_("label")), childObj->GetPropertyAsBitmap(_("bitmap")),
                  wxNullBitmap, (wxItemKind)childObj->GetPropertyAsInteger(_("kind")),
                  childObj->GetPropertyAsString(_("help")), wxEmptyString, child);
                wxAuiToolBarItem* itm = tb->FindToolByIndex(i);
                wxASSERT(itm);
                itm->SetUserData(i);
                tb->SetObject(i, child);
                if (childObj->GetPropertyAsInteger(_("context_menu")) == 1 && !itm->HasDropDown())
                    tb->SetToolDropDown(itm->GetId(), true);
                else if (childObj->GetPropertyAsInteger(_("context_menu")) == 0 && itm->HasDropDown())
                    tb->SetToolDropDown(itm->GetId(), false);
            } else if (wxT("toolSeparator") == childObj->GetClassName()) {
                tb->AddSeparator();
            } else {
                wxControl* control = wxDynamicCast(child, wxControl);
                if (NULL != control) {
                    tb->AddControl(control);
                }
            }
        }
        tb->Realize();
    }
    /*
            ticpp::Element* ExportToXrc(IObject *obj) override
            {
                    ObjectToXrcFilter xrc(GetLibrary(), obj, _("wxAuiToolBar"), obj->GetPropertyAsString(_("name")));
                    xrc.AddWindowProperties();
                    xrc.AddProperty(_("bitmapsize"), _("bitmapsize"), XrcFilter::Type::Size);
                    xrc.AddProperty(_("margins"), _("margins"), XrcFilter::Type::Size);
                    xrc.AddProperty(_("packing"), _("packing"), XrcFilter::Type::Integer);
                    xrc.AddProperty(_("separation"), _("separation"), XrcFilter::Type::Integer);
                    return xrc.GetXrcObject();
            }
            tinyxml2::XMLElement* ExportToXrc(tinyxml2::XMLElement* xrc, const IObject* obj) override
            {
                ObjectToXrcFilter filter(xrc, GetLibrary(), obj);
                filter.AddWindowProperties();
                filter.AddProperty(XrcFilter::Type::Size, "bitmapsize");
                filter.AddProperty(XrcFilter::Type::Size, "margins");
                filter.AddProperty(XrcFilter::Type::Integer, "packing");
                filter.AddProperty(XrcFilter::Type::Integer, "separation");
                return xrc;
            }

            ticpp::Element* ImportFromXrc( ticpp::Element* xrcObj )
            {
                    XrcToXfbFilter filter(GetLibrary(), xrcObj, _("wxAuiToolBar"));
                    filter.AddWindowProperties();
                    filter.AddProperty(_("bitmapsize"), _("bitmapsize"), XrcFilter::Type::Size);
                    filter.AddProperty(_("margins"), _("margins"), XrcFilter::Type::Size);
                    filter.AddProperty(_("packing"), _("packing"), XrcFilter::Type::Integer);
                    filter.AddProperty(_("separation"), _("separation"), XrcFilter::Type::Integer);
                    return filter.GetXfbObject();
            }
            tinyxml2::XMLElement* ImportFromXrc(tinyxml2::XMLElement* xfb, const tinyxml2::XMLElement* xrc) override
            {
                XrcToXfbFilter filter(xfb, GetLibrary(), xrc);
                filter.AddWindowProperties();
                filter.AddProperty(XrcFilter::Type::Size, "bitmapsize");
                filter.AddProperty(XrcFilter::Type::Size, "margins");
                filter.AddProperty(XrcFilter::Type::Integer, "packing");
                filter.AddProperty(XrcFilter::Type::Integer, "separation");
                return xfb;
            }
    */
};

class ToolComponent : public ComponentBase
{
public:
    ticpp::Element* ExportToXrc(IObject* obj) override
    {
        ObjectToXrcFilter xrc(GetLibrary(), obj, _("tool"), obj->GetPropertyAsString(_("name")));
        xrc.AddProperty(_("label"), _("label"), XrcFilter::Type::Text);
        xrc.AddProperty(_("tooltip"), _("tooltip"), XrcFilter::Type::Text);
        xrc.AddProperty(_("statusbar"), _("longhelp"), XrcFilter::Type::Text);
        xrc.AddProperty(_("bitmap"), _("bitmap"), XrcFilter::Type::Bitmap);

        wxItemKind kind = (wxItemKind)obj->GetPropertyAsInteger(_("kind"));
        if (wxITEM_CHECK == kind) {
            xrc.AddPropertyValue(wxT("toggle"), wxT("1"));
        } else if (wxITEM_RADIO == kind) {
            xrc.AddPropertyValue(wxT("radio"), wxT("1"));
        }
        return xrc.GetXrcObject();
    }
    tinyxml2::XMLElement* ExportToXrc(tinyxml2::XMLElement* xrc, const IObject* obj) override
    {
        ObjectToXrcFilter filter(xrc, GetLibrary(), obj);
        filter.AddProperty(XrcFilter::Type::Text, "label");
        filter.AddProperty(XrcFilter::Type::Text, "tooltip");
        filter.AddProperty(XrcFilter::Type::Text, "statusbar", "longhelp");
        filter.AddProperty(XrcFilter::Type::Bitmap, "bitmap");
        auto kind = obj->GetPropertyAsInteger("kind");
        switch (kind) {
            case wxITEM_CHECK:
                filter.AddPropertyValue("toggle", "1");
                break;
            case wxITEM_RADIO:
                filter.AddPropertyValue("radio", "1");
                break;
        }
        // FIXME: dropdown, disabled, checked is current not part of the data model
        return xrc;
    }

    ticpp::Element* ImportFromXrc(ticpp::Element* xrcObj) override
    {
        XrcToXfbFilter filter(GetLibrary(), xrcObj, _("tool"));
        filter.AddProperty(_("longhelp"), _("statusbar"), XrcFilter::Type::Text);
        filter.AddProperty(_("tooltip"), _("tooltip"), XrcFilter::Type::Text);
        filter.AddProperty(_("label"), _("label"), XrcFilter::Type::Text);
        filter.AddProperty(_("bitmap"), _("bitmap"), XrcFilter::Type::Bitmap);
        bool gotToggle = false;
        bool gotRadio = false;
        ticpp::Element* toggle = xrcObj->FirstChildElement("toggle", false);
        if (toggle) {
            toggle->GetTextOrDefault(&gotToggle, false);
            if (gotToggle) {
                filter.AddPropertyValue(_("kind"), wxT("wxITEM_CHECK"));
            }
        }
        if (!gotToggle) {
            ticpp::Element* radio = xrcObj->FirstChildElement("radio", false);
            if (radio) {
                radio->GetTextOrDefault(&gotRadio, false);
                if (gotRadio) {
                    filter.AddPropertyValue(_("kind"), wxT("wxITEM_RADIO"));
                }
            }
        }
        if (!(gotToggle || gotRadio)) {
            filter.AddPropertyValue(_("kind"), wxT("wxITEM_NORMAL"));
        }

        return filter.GetXfbObject();
    }
    tinyxml2::XMLElement* ImportFromXrc(tinyxml2::XMLElement* xfb, const tinyxml2::XMLElement* xrc) override
    {
        XrcToXfbFilter filter(xfb, GetLibrary(), xrc);
        filter.AddProperty(XrcFilter::Type::Text, "label");
        filter.AddProperty(XrcFilter::Type::Text, "tooltip");
        filter.AddProperty(XrcFilter::Type::Text, "longhelp", "statusbar");
        filter.AddProperty(XrcFilter::Type::Bitmap, "bitmap");
        wxString kind = "wxITEM_NORMAL";
        if (const auto* checkableElement = xrc->FirstChildElement("toggle"); checkableElement && checkableElement->IntText() != 0) {
            kind = "wxITEM_CHECK";
        } else if (const auto* radioElement = xrc->FirstChildElement("radio"); radioElement && radioElement->IntText() != 0) {
            kind = "wxITEM_RADIO";
        }
        filter.AddPropertyValue("kind", kind);
        // FIXME: dropdown, disabled, checked is current not part of the data model
        return xfb;
    }
};

class ToolSeparatorComponent : public ComponentBase
{
public:
    ticpp::Element* ExportToXrc(IObject* obj) override
    {
        ObjectToXrcFilter xrc(GetLibrary(), obj, _("separator"));
        return xrc.GetXrcObject();
    }
    tinyxml2::XMLElement* ExportToXrc(tinyxml2::XMLElement* xrc, const IObject* obj) override
    {
        ObjectToXrcFilter filter(xrc, GetLibrary(), obj, "separator", "");
        return xrc;
    }

    ticpp::Element* ImportFromXrc(ticpp::Element* xrcObj) override
    {
        XrcToXfbFilter filter(GetLibrary(), xrcObj, _("toolSeparator"));
        return filter.GetXfbObject();
    }
    tinyxml2::XMLElement* ImportFromXrc(tinyxml2::XMLElement* xfb, const tinyxml2::XMLElement* xrc) override
    {
        XrcToXfbFilter filter(xfb, GetLibrary(), xrc, "toolSeparator", "");
        return xfb;
    }
};

class ChoiceComponent : public ComponentBase
{
public:
    wxObject* Create(IObject* obj, wxObject* parent) override
    {
        wxArrayString choices = obj->GetPropertyAsArrayString(_("choices"));
        wxString* strings = new wxString[choices.GetCount()];
        for (unsigned int i = 0; i < choices.GetCount(); i++) strings[i] = choices[i];

        wxChoice* choice = new wxChoice(
          (wxWindow*)parent, wxID_ANY, obj->GetPropertyAsPoint(_("pos")), obj->GetPropertyAsSize(_("size")),
          (int)choices.Count(), strings, obj->GetPropertyAsInteger(_("window_style")));

        int sel = obj->GetPropertyAsInteger(_("selection"));
        if (sel < (int)choices.GetCount())
            choice->SetSelection(sel);

        delete[] strings;

        choice->PushEventHandler(new ComponentEvtHandler(choice, GetManager()));

        return choice;
    }

    void Cleanup(wxObject* obj) override
    {
        auto* window = wxDynamicCast(obj, wxChoice);
        if (window) {
            window->PopEventHandler(true);
        }
    }

    ticpp::Element* ExportToXrc(IObject* obj) override
    {
        ObjectToXrcFilter xrc(GetLibrary(), obj, _("wxChoice"), obj->GetPropertyAsString(_("name")));
        xrc.AddWindowProperties();
        xrc.AddProperty(_("selection"), _("selection"), XrcFilter::Type::Integer);
        xrc.AddProperty(_("choices"), _("content"), XrcFilter::Type::StringList);
        return xrc.GetXrcObject();
    }
    tinyxml2::XMLElement* ExportToXrc(tinyxml2::XMLElement* xrc, const IObject* obj) override
    {
        ObjectToXrcFilter filter(xrc, GetLibrary(), obj);
        filter.AddWindowProperties();
        filter.AddProperty(XrcFilter::Type::Integer, "selection");
        filter.AddProperty(XrcFilter::Type::StringList, "choices", "content");
        return xrc;
    }

    ticpp::Element* ImportFromXrc(ticpp::Element* xrcObj) override
    {
        XrcToXfbFilter filter(GetLibrary(), xrcObj, _("wxChoice"));
        filter.AddWindowProperties();
        filter.AddProperty(_("selection"), _("selection"), XrcFilter::Type::Integer);
        filter.AddProperty(_("content"), _("choices"), XrcFilter::Type::StringList);
        return filter.GetXfbObject();
    }
    tinyxml2::XMLElement* ImportFromXrc(tinyxml2::XMLElement* xfb, const tinyxml2::XMLElement* xrc) override
    {
        XrcToXfbFilter filter(xfb, GetLibrary(), xrc);
        filter.AddWindowProperties();
        filter.AddProperty(XrcFilter::Type::Integer, "selection");
        filter.AddProperty(XrcFilter::Type::StringList, "content", "choices");
        return xfb;
    }
};

class SliderComponent : public ComponentBase
{
public:
    wxObject* Create(IObject* obj, wxObject* parent) override
    {
        return new wxSlider(
          (wxWindow*)parent, wxID_ANY, obj->GetPropertyAsInteger(_("value")), obj->GetPropertyAsInteger(_("minValue")),
          obj->GetPropertyAsInteger(_("maxValue")), obj->GetPropertyAsPoint(_("pos")),
          obj->GetPropertyAsSize(_("size")),
          obj->GetPropertyAsInteger(_("style")) | obj->GetPropertyAsInteger(_("window_style")) |
            obj->GetPropertyAsInteger(_("window_style")));
    }

    ticpp::Element* ExportToXrc(IObject* obj) override
    {
        ObjectToXrcFilter xrc(GetLibrary(), obj, _("wxSlider"), obj->GetPropertyAsString(_("name")));
        xrc.AddWindowProperties();
        xrc.AddProperty(_("value"), _("value"), XrcFilter::Type::Integer);
        xrc.AddProperty(_("minValue"), _("min"), XrcFilter::Type::Integer);
        xrc.AddProperty(_("maxValue"), _("max"), XrcFilter::Type::Integer);
        return xrc.GetXrcObject();
    }
    tinyxml2::XMLElement* ExportToXrc(tinyxml2::XMLElement* xrc, const IObject* obj) override
    {
        ObjectToXrcFilter filter(xrc, GetLibrary(), obj);
        filter.AddWindowProperties();
        filter.AddProperty(XrcFilter::Type::Integer, "value");
        filter.AddProperty(XrcFilter::Type::Integer, "minValue", "min");
        filter.AddProperty(XrcFilter::Type::Integer, "maxValue", "max");
        return xrc;
    }

    ticpp::Element* ImportFromXrc(ticpp::Element* xrcObj) override
    {
        XrcToXfbFilter filter(GetLibrary(), xrcObj, _("wxSlider"));
        filter.AddWindowProperties();
        filter.AddProperty(_("value"), _("value"), XrcFilter::Type::Integer);
        filter.AddProperty(_("min"), _("minValue"), XrcFilter::Type::Integer);
        filter.AddProperty(_("max"), _("maxValue"), XrcFilter::Type::Integer);
        return filter.GetXfbObject();
    }
    tinyxml2::XMLElement* ImportFromXrc(tinyxml2::XMLElement* xfb, const tinyxml2::XMLElement* xrc) override
    {
        XrcToXfbFilter filter(xfb, GetLibrary(), xrc);
        filter.AddWindowProperties();
        filter.AddProperty(XrcFilter::Type::Integer, "value");
        filter.AddProperty(XrcFilter::Type::Integer, "min", "minValue");
        filter.AddProperty(XrcFilter::Type::Integer, "max", "maxValue");
        return xfb;
    }
};

class GaugeComponent : public ComponentBase
{
public:
    wxObject* Create(IObject* obj, wxObject* parent) override
    {
        wxGauge* gauge = new wxGauge(
          (wxWindow*)parent, wxID_ANY, obj->GetPropertyAsInteger(_("range")), obj->GetPropertyAsPoint(_("pos")),
          obj->GetPropertyAsSize(_("size")),
          obj->GetPropertyAsInteger(_("style")) | obj->GetPropertyAsInteger(_("window_style")));
        gauge->SetValue(obj->GetPropertyAsInteger(_("value")));
        return gauge;
    }

    ticpp::Element* ExportToXrc(IObject* obj) override
    {
        ObjectToXrcFilter xrc(GetLibrary(), obj, _("wxGauge"), obj->GetPropertyAsString(_("name")));
        xrc.AddWindowProperties();
        xrc.AddProperty(_("range"), _("range"), XrcFilter::Type::Integer);
        xrc.AddProperty(_("value"), _("value"), XrcFilter::Type::Integer);
        return xrc.GetXrcObject();
    }
    tinyxml2::XMLElement* ExportToXrc(tinyxml2::XMLElement* xrc, const IObject* obj) override
    {
        ObjectToXrcFilter filter(xrc, GetLibrary(), obj);
        filter.AddWindowProperties();
        filter.AddProperty(XrcFilter::Type::Integer, "range");
        filter.AddProperty(XrcFilter::Type::Integer, "value");
        return xrc;
    }

    ticpp::Element* ImportFromXrc(ticpp::Element* xrcObj) override
    {
        XrcToXfbFilter filter(GetLibrary(), xrcObj, _("wxGauge"));
        filter.AddWindowProperties();
        filter.AddProperty(_("range"), _("range"), XrcFilter::Type::Integer);
        filter.AddProperty(_("value"), _("value"), XrcFilter::Type::Integer);
        return filter.GetXfbObject();
    }
    tinyxml2::XMLElement* ImportFromXrc(tinyxml2::XMLElement* xfb, const tinyxml2::XMLElement* xrc) override
    {
        XrcToXfbFilter filter(xfb, GetLibrary(), xrc);
        filter.AddWindowProperties();
        filter.AddProperty(XrcFilter::Type::Integer, "range");
        filter.AddProperty(XrcFilter::Type::Integer, "value");
        return xfb;
    }
};

class AnimCtrlComponent : public ComponentBase
{
public:
    wxObject* Create(IObject* obj, wxObject* parent) override
    {
        wxAnimationCtrl* ac = new wxAnimationCtrl(
          (wxWindow*)parent, wxID_ANY, wxNullAnimation, obj->GetPropertyAsPoint(_("pos")),
          obj->GetPropertyAsSize(_("size")),
          obj->GetPropertyAsInteger(_("style")) | obj->GetPropertyAsInteger(_("window_style")));

        if (!obj->IsPropertyNull(_("animation"))) {
            if (ac->LoadFile(obj->GetPropertyAsString(_("animation")))) {
                if (!obj->IsPropertyNull(_("play")) && (obj->GetPropertyAsInteger(_("play")) == 1))
                    ac->Play();
                else
                    ac->Stop();
            }
        }

        if (!obj->IsPropertyNull(_("inactive_bitmap"))) {
            wxBitmap bmp = obj->GetPropertyAsBitmap(_("inactive_bitmap"));
            if (bmp.IsOk())
                ac->SetInactiveBitmap(bmp);
            else
                ac->SetInactiveBitmap(wxNullBitmap);
        }

        ac->PushEventHandler(new ComponentEvtHandler(ac, GetManager()));

        return ac;
    }

    void Cleanup(wxObject* obj) override
    {
        auto* window = wxDynamicCast(obj, wxAnimationCtrl);
        if (window) {
            window->PopEventHandler(true);
        }
    }

    ticpp::Element* ExportToXrc(IObject* obj) override
    {
        ObjectToXrcFilter xrc(GetLibrary(), obj, _("wxAnimationCtrl"), obj->GetPropertyAsString(_("name")));
        xrc.AddWindowProperties();
        xrc.AddProperty(_("animation"), _("animation"), XrcFilter::Type::Text);
        return xrc.GetXrcObject();
    }
    tinyxml2::XMLElement* ExportToXrc(tinyxml2::XMLElement* xrc, const IObject* obj) override
    {
        ObjectToXrcFilter filter(xrc, GetLibrary(), obj);
        filter.AddWindowProperties();
        filter.AddProperty(XrcFilter::Type::Text, "animation");
        return xrc;
    }

    ticpp::Element* ImportFromXrc(ticpp::Element* xrcObj) override
    {
        XrcToXfbFilter filter(GetLibrary(), xrcObj, _("wxAnimationCtrl"));
        filter.AddWindowProperties();
        filter.AddProperty(_("animation"), _("animation"), XrcFilter::Type::Text);
        return filter.GetXfbObject();
    }
    tinyxml2::XMLElement* ImportFromXrc(tinyxml2::XMLElement* xfb, const tinyxml2::XMLElement* xrc) override
    {
        XrcToXfbFilter filter(xfb, GetLibrary(), xrc);
        filter.AddWindowProperties();
        filter.AddProperty(XrcFilter::Type::Text, "animation");
        return xfb;
    }
};

class InfoBarComponent : public ComponentBase
{
public:
    wxObject* Create(IObject* obj, wxObject* parent) override
    {
        wxInfoBar* ib = new wxInfoBar((wxWindow*)parent);

        ib->SetShowHideEffects(
          (wxShowEffect)obj->GetPropertyAsInteger(_("show_effect")),
          (wxShowEffect)obj->GetPropertyAsInteger(_("hide_effect")));
        ib->SetEffectDuration(obj->GetPropertyAsInteger(_("duration")));
        ib->ShowMessage(wxT("Message ..."), wxICON_INFORMATION);

        ib->PushEventHandler(new ComponentEvtHandler(ib, GetManager()));

        return ib;
    }

    void Cleanup(wxObject* obj) override
    {
        auto* window = wxDynamicCast(obj, wxInfoBar);
        if (window) {
            window->PopEventHandler(true);
        }
    }

    ticpp::Element* ExportToXrc(IObject* obj) override
    {
        ObjectToXrcFilter xrc(GetLibrary(), obj, _("unknown"), obj->GetPropertyAsString(_("name")));

        /*ObjectToXrcFilter xrc(GetLibrary(), obj, _("wxInfoBar"), obj->GetPropertyAsString(_("name")));
        xrc.AddWindowProperties();*/

        return xrc.GetXrcObject();
    }
    tinyxml2::XMLElement* ExportToXrc(tinyxml2::XMLElement* xrc, const IObject* obj) override
    {
        ObjectToXrcFilter filter(xrc, GetLibrary(), obj);
        filter.AddWindowProperties();
        filter.AddProperty(XrcFilter::Type::Text, "show_effect", "showeffect");
        filter.AddProperty(XrcFilter::Type::Text, "hide_effect", "hideeffect");
        filter.AddProperty(XrcFilter::Type::Integer, "duration", "effectduration");
        // FIXME: button is currently not part of the data model
        return xrc;
    }

    /*ticpp::Element* ImportFromXrc( ticpp::Element* xrcObj )
    {
            XrcToXfbFilter filter(GetLibrary(), xrcObj, _("wxInfoBar"));
            filter.AddWindowProperties();
            return filter.GetXfbObject();
    }*/
    tinyxml2::XMLElement* ImportFromXrc(tinyxml2::XMLElement* xfb, const tinyxml2::XMLElement* xrc) override
    {
        XrcToXfbFilter filter(xfb, GetLibrary(), xrc);
        filter.AddWindowProperties();
        filter.AddProperty(XrcFilter::Type::Text, "showeffect", "show_effect");
        filter.AddProperty(XrcFilter::Type::Text, "hideeffect", "hide_effect");
        filter.AddProperty(XrcFilter::Type::Integer, "effectduration", "duration");
        // FIXME: button is currently not part of the data model
        return xfb;
    }
};

///////////////////////////////////////////////////////////////////////////////

BEGIN_LIBRARY()

WINDOW_COMPONENT("wxButton", ButtonComponent)
WINDOW_COMPONENT("wxBitmapButton", BitmapButtonComponent)
WINDOW_COMPONENT("wxTextCtrl", TextCtrlComponent)
WINDOW_COMPONENT("wxStaticText", StaticTextComponent)
WINDOW_COMPONENT("wxComboBox", ComboBoxComponent)
WINDOW_COMPONENT("wxBitmapComboBox", BitmapComboBoxComponent)
WINDOW_COMPONENT("wxListBox", ListBoxComponent)
WINDOW_COMPONENT("wxRadioBox", RadioBoxComponent)
WINDOW_COMPONENT("wxRadioButton", RadioButtonComponent)
WINDOW_COMPONENT("wxCheckBox", CheckBoxComponent)
WINDOW_COMPONENT("wxStaticBitmap", StaticBitmapComponent)
WINDOW_COMPONENT("wxStaticLine", StaticLineComponent)
WINDOW_COMPONENT("wxMenuBar", MenuBarComponent)
ABSTRACT_COMPONENT("wxMenu", MenuComponent)
ABSTRACT_COMPONENT("submenu", SubMenuComponent)
ABSTRACT_COMPONENT("wxMenuItem", MenuItemComponent)
ABSTRACT_COMPONENT("separator", SeparatorComponent)
WINDOW_COMPONENT("wxListCtrl", ListCtrlComponent)
WINDOW_COMPONENT("wxStatusBar", StatusBarComponent)
WINDOW_COMPONENT("wxToolBar", ToolBarComponent)
WINDOW_COMPONENT("wxAuiToolBar", AuiToolBarComponent)
ABSTRACT_COMPONENT("tool", ToolComponent)
ABSTRACT_COMPONENT("toolSeparator", ToolSeparatorComponent)
WINDOW_COMPONENT("wxChoice", ChoiceComponent)
WINDOW_COMPONENT("wxSlider", SliderComponent)
WINDOW_COMPONENT("wxGauge", GaugeComponent)
WINDOW_COMPONENT("wxAnimationCtrl", AnimCtrlComponent)
WINDOW_COMPONENT("wxInfoBar", InfoBarComponent)

// wxButton
MACRO(wxBU_LEFT)
MACRO(wxBU_TOP)
MACRO(wxBU_RIGHT)
MACRO(wxBU_BOTTOM)
MACRO(wxBU_EXACTFIT)
MACRO(wxBU_NOTEXT)
MACRO(wxLEFT)
MACRO(wxRIGHT)
MACRO(wxTOP)
MACRO(wxBOTTOM)

// wxStaticText
MACRO(wxALIGN_LEFT)
MACRO(wxALIGN_CENTER_HORIZONTAL)
MACRO(wxALIGN_RIGHT)
MACRO(wxST_NO_AUTORESIZE)
MACRO(wxST_ELLIPSIZE_START)
MACRO(wxST_ELLIPSIZE_MIDDLE)
MACRO(wxST_ELLIPSIZE_END)

// wxTextCtrl
MACRO(wxTE_MULTILINE)
MACRO(wxTE_READONLY)
MACRO(wxTE_RICH)
MACRO(wxTE_AUTO_URL)
MACRO(wxTE_CAPITALIZE)
MACRO(wxTE_CENTER)
MACRO(wxTE_CHARWRAP)
MACRO(wxTE_DONTWRAP)
MACRO(wxTE_LEFT)
MACRO(wxTE_NOHIDESEL)
MACRO(wxTE_PASSWORD)
MACRO(wxTE_PROCESS_ENTER)
MACRO(wxTE_PROCESS_TAB)
MACRO(wxTE_RICH2)
MACRO(wxTE_RIGHT)
MACRO(wxTE_WORDWRAP)
MACRO(wxTE_BESTWRAP)
MACRO(wxTE_NO_VSCROLL)

// wxStaticLine
MACRO(wxLI_HORIZONTAL)
MACRO(wxLI_VERTICAL)

// wxListCtrl
MACRO(wxLC_LIST)
MACRO(wxLC_REPORT)
MACRO(wxLC_VIRTUAL)
MACRO(wxLC_ICON)
MACRO(wxLC_SMALL_ICON)
MACRO(wxLC_ALIGN_TOP)
MACRO(wxLC_ALIGN_LEFT)
MACRO(wxLC_AUTOARRANGE)
MACRO(wxLC_EDIT_LABELS)
MACRO(wxLC_NO_SORT_HEADER)
MACRO(wxLC_NO_HEADER)
MACRO(wxLC_SINGLE_SEL)
MACRO(wxLC_SORT_ASCENDING)
MACRO(wxLC_SORT_DESCENDING)
MACRO(wxLC_HRULES)
MACRO(wxLC_VRULES)

// wxListBox
MACRO(wxLB_SINGLE)
MACRO(wxLB_MULTIPLE)
MACRO(wxLB_EXTENDED)
MACRO(wxLB_HSCROLL)
MACRO(wxLB_ALWAYS_SB)
MACRO(wxLB_NEEDED_SB)
MACRO(wxLB_NO_SB)
MACRO(wxLB_SORT)

// wxRadioBox
MACRO(wxRA_SPECIFY_ROWS)
MACRO(wxRA_SPECIFY_COLS)

// wxRadioButton
MACRO(wxRB_GROUP)
MACRO(wxRB_SINGLE)

// wxStatusBar
MACRO(wxSTB_SIZEGRIP)
MACRO(wxSTB_SHOW_TIPS)
MACRO(wxSTB_ELLIPSIZE_START)
MACRO(wxSTB_ELLIPSIZE_MIDDLE)
MACRO(wxSTB_ELLIPSIZE_END)
MACRO(wxSTB_DEFAULT_STYLE)

// wxMenuItem & wxTool
MACRO(wxITEM_NORMAL)
MACRO(wxITEM_CHECK)
MACRO(wxITEM_RADIO)

// wxToolBar
MACRO(wxTB_FLAT)
MACRO(wxTB_DOCKABLE)
MACRO(wxTB_HORIZONTAL)
MACRO(wxTB_VERTICAL)
MACRO(wxTB_TEXT)
MACRO(wxTB_NOICONS)
MACRO(wxTB_NODIVIDER)
MACRO(wxTB_NOALIGN)
MACRO(wxTB_HORZ_LAYOUT)
MACRO(wxTB_HORZ_TEXT)
MACRO(wxTB_NO_TOOLTIPS)
MACRO(wxTB_BOTTOM)
MACRO(wxTB_RIGHT)
MACRO(wxTB_DEFAULT_STYLE)

// wxAuiToolBar
MACRO(wxAUI_TB_TEXT)
MACRO(wxAUI_TB_NO_TOOLTIPS)
MACRO(wxAUI_TB_NO_AUTORESIZE)
MACRO(wxAUI_TB_GRIPPER)
MACRO(wxAUI_TB_OVERFLOW)
MACRO(wxAUI_TB_VERTICAL)
MACRO(wxAUI_TB_HORZ_LAYOUT)
MACRO(wxAUI_TB_HORIZONTAL)
MACRO(wxAUI_TB_PLAIN_BACKGROUND)
MACRO(wxAUI_TB_HORZ_TEXT)
MACRO(wxAUI_TB_DEFAULT_STYLE)

// wxSlider
MACRO(wxSL_AUTOTICKS)
MACRO(wxSL_BOTTOM)
MACRO(wxSL_HORIZONTAL)
MACRO(wxSL_INVERSE)
MACRO(wxSL_MIN_MAX_LABELS)
MACRO(wxSL_VALUE_LABEL)
MACRO(wxSL_LABELS)
MACRO(wxSL_LEFT)
MACRO(wxSL_RIGHT)
MACRO(wxSL_SELRANGE)
MACRO(wxSL_TOP)
MACRO(wxSL_VERTICAL)
MACRO(wxSL_BOTH)

// wxComboBox
MACRO(wxCB_DROPDOWN)
MACRO(wxCB_READONLY)
MACRO(wxCB_SIMPLE)
MACRO(wxCB_SORT)

// wxCheckBox
MACRO(wxCHK_2STATE)
MACRO(wxCHK_3STATE)
MACRO(wxCHK_ALLOW_3RD_STATE_FOR_USER)

// wxGauge
MACRO(wxGA_HORIZONTAL)
MACRO(wxGA_SMOOTH)
MACRO(wxGA_VERTICAL)

// wxAnimationCtrl
MACRO(wxAC_DEFAULT_STYLE)
MACRO(wxAC_NO_AUTORESIZE)

// wxInfoBar
MACRO(wxSHOW_EFFECT_NONE)
MACRO(wxSHOW_EFFECT_ROLL_TO_LEFT)
MACRO(wxSHOW_EFFECT_ROLL_TO_RIGHT)
MACRO(wxSHOW_EFFECT_ROLL_TO_TOP)
MACRO(wxSHOW_EFFECT_ROLL_TO_BOTTOM)
MACRO(wxSHOW_EFFECT_SLIDE_TO_LEFT)
MACRO(wxSHOW_EFFECT_SLIDE_TO_RIGHT)
MACRO(wxSHOW_EFFECT_SLIDE_TO_TOP)
MACRO(wxSHOW_EFFECT_SLIDE_TO_BOTTOM)
MACRO(wxSHOW_EFFECT_BLEND)
MACRO(wxSHOW_EFFECT_EXPAND)

END_LIBRARY()
