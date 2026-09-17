#include "notifytables.h"

// See notifytables.h's top-of-file comment for the full provenance story.
// Every table below is transcribed directly from the real MUIBuilder
// v2.3 source (codenotifydefs.c's CACTxxx[]/CEVTxxx[] int arrays +
// initnotify.c's InitNotifyArrays()'s TYxxx[] assignments), index for
// index - NOT reordered to match either file's own ACTxxx[]/EVTxxx[]
// *display-string* ordering (which in at least one place, ColorField's
// Red/Blue/Green trigger-constant-variable triples, actually disagrees
// with CACTColorField's own physical layout - a real quirk of the
// original tool we deliberately do NOT "fix", since our job is to
// reproduce what CodeNotify() actually emits, not what the notify
// editor's listbox happens to label it).
//
// ObjType's own numeric values (muibobject.h) already equal the
// original's TY_* ids for every index used below (0=Appli ... 21=
// PopObject), with one deliberate renaming at 22/23: what the original
// calls TY_MENUSTRIP (22, "the whole menu bar") and TY_MENU (23, "one
// top-level/nested menu header") are ObjType::Menu (22) and
// ObjType::SubMenu (23) here - see muibobject.h's own enum comment. The
// tables below are laid out by our own ObjType names but at the
// original's numeric positions, so notifyActionsFor()/notifyEventsFor()
// can index straight off static_cast<int>(type) exactly like the
// original indexes off id_dest/id_src.

namespace {

using AV = QVector<NotifyActionDef>;
using EV = QVector<NotifyEventDef>;

NotifyActionDef act(int argCount, const char *method, const char *attr, const char *value, NotifyActionKind kind)
{
    return NotifyActionDef{ argCount, QString::fromLatin1(method), QString::fromLatin1(attr), QString::fromLatin1(value), kind };
}

NotifyEventDef evt(const char *attr, const char *value)
{
    return NotifyEventDef{ QString::fromLatin1(attr), QString::fromLatin1(value) };
}

using K = NotifyActionKind;

// --- Application (TY_APPLI = 0) ---
// ACTAppli: ReturnID / ReturnQuit / ApplicationSleep / ApplicationWake / CallFunction
const AV &actionsAppli()
{
    static const AV t = {
        act(2, "MUIM_Application_ReturnID", "", "", K::Unsupported),        // ReturnID - TY_ID (external constant), not modelled
        act(2, "MUIM_Application_ReturnID", "", "MUIV_Application_ReturnID_Quit", K::Nothing), // ReturnQuit
        act(3, "MUIM_Set", "MUIA_Application_Sleep", "TRUE", K::Nothing),
        act(3, "MUIM_Set", "MUIA_Application_Sleep", "FALSE", K::Nothing),
        act(2, "MUIM_CallHook", "", "", K::Unsupported),                    // CallFunction - TY_FUNCTION
    };
    return t;
}
const EV &eventsAppli()
{
    static const EV t = {
        evt("MUIA_Application_Iconified", "TRUE"),
        evt("MUIA_Application_Iconified", "FALSE"),
        evt("MUIA_Application_Active", "TRUE"),
        evt("MUIA_Application_Active", "FALSE"),
    };
    return t;
}

// --- Window (TY_WINDOW = 1) ---
const AV &actionsWindow()
{
    static const AV t = {
        act(3, "MUIM_Set", "MUIA_Window_Open", "TRUE", K::Nothing),
        act(3, "MUIM_Set", "MUIA_Window_Open", "FALSE", K::Nothing),
        act(3, "MUIM_Set", "MUIA_Window_Activate", "TRUE", K::Nothing),
        act(2, "MUIM_CallHook", "", "", K::Unsupported),
        act(3, "MUIM_Set", "MUIA_Window_ActiveObject", "MUIV_Window_ActiveObject_None", K::Nothing),
        act(3, "MUIM_Set", "MUIA_Window_ActiveObject", "MUIV_Window_ActiveObject_Next", K::Nothing),
        act(3, "MUIM_Set", "MUIA_Window_ActiveObject", "MUIV_Window_ActiveObject_Prev", K::Nothing),
    };
    return t;
}
const EV &eventsWindow()
{
    static const EV t = {
        evt("MUIA_Window_CloseRequest", "TRUE"),
        evt("MUIA_Window_Activate", "TRUE"),
        evt("MUIA_Window_Activate", "FALSE"),
    };
    return t;
}

// --- Button (TY_KEYBUTTON = 3) ---
const AV &actionsButton()
{
    static const AV t = {
        act(3, "MUIM_Set", "MUIA_ShowMe", "FALSE", K::Nothing),
        act(3, "MUIM_Set", "MUIA_ShowMe", "TRUE", K::Nothing),
        act(3, "MUIM_Set", "MUIA_Disabled", "TRUE", K::Nothing),
        act(3, "MUIM_Set", "MUIA_Disabled", "FALSE", K::Nothing),
        act(3, "MUIM_Set", "MUIA_Window_ActiveObject", "", K::Unsupported), // Activate - TY_WINOBJ
        act(3, "MUIM_Set", "MUIA_Text_Contents", "MUIV_TriggerValue", K::Nothing),
        act(3, "MUIM_Set", "MUIA_Text_Contents", "", K::ConsString),
        act(3, "MUIM_Set", "MUIA_Text_Contents", "", K::Unsupported), // PutVariableValue - TY_VARIABLE
        act(2, "MUIM_CallHook", "", "", K::Unsupported),
    };
    return t;
}
const EV &eventsButton()
{
    static const EV t = {
        evt("MUIA_Pressed", "TRUE"),
        evt("MUIA_Pressed", "FALSE"),
        evt("MUIA_ShowMe", "FALSE"),
        evt("MUIA_ShowMe", "TRUE"),
        evt("MUIA_Disabled", "TRUE"),
        evt("MUIA_Disabled", "FALSE"),
        evt("MUIA_Timer", "MUIV_EveryTime"),
    };
    return t;
}

// --- Group (TY_GROUP = 2) ---
const AV &actionsGroup()
{
    static const AV t = {
        act(3, "MUIM_Set", "MUIA_ShowMe", "FALSE", K::Nothing),
        act(3, "MUIM_Set", "MUIA_ShowMe", "TRUE", K::Nothing),
        act(3, "MUIM_Set", "MUIA_Disabled", "TRUE", K::Nothing),
        act(3, "MUIM_Set", "MUIA_Disabled", "FALSE", K::Nothing),
        act(3, "MUIM_Set", "MUIA_Window_ActiveObject", "", K::Unsupported), // Activate - TY_WINOBJ
        act(3, "MUIM_Set", "MUIA_Group_ActivePage", "MUIV_TriggerValue", K::Nothing),
        act(2, "MUIM_CallHook", "", "", K::Unsupported),
    };
    return t;
}
const EV &eventsGroup()
{
    static const EV t = {
        evt("MUIA_Group_ActivePage", "MUIV_EveryTime"),
        evt("MUIA_ShowMe", "FALSE"),
        evt("MUIA_ShowMe", "TRUE"),
        evt("MUIA_Disabled", "TRUE"),
        evt("MUIA_Disabled", "FALSE"),
    };
    return t;
}

// --- String (TY_STRING = 4) ---
const AV &actionsString()
{
    static const AV t = {
        act(3, "MUIM_Set", "MUIA_ShowMe", "FALSE", K::Nothing),
        act(3, "MUIM_Set", "MUIA_ShowMe", "TRUE", K::Nothing),
        act(3, "MUIM_Set", "MUIA_Disabled", "TRUE", K::Nothing),
        act(3, "MUIM_Set", "MUIA_Disabled", "FALSE", K::Nothing),
        act(3, "MUIM_Set", "MUIA_Window_ActiveObject", "", K::Unsupported),
        act(3, "MUIM_Set", "MUIA_String_Contents", "MUIV_TriggerValue", K::Nothing),
        act(3, "MUIM_Set", "MUIA_String_Contents", "", K::ConsString),
        act(3, "MUIM_Set", "MUIA_String_Contents", "", K::Unsupported),
        act(2, "MUIM_CallHook", "", "", K::Unsupported),
    };
    return t;
}
const EV &eventsString()
{
    static const EV t = {
        evt("MUIA_String_Acknowledge", "MUIV_EveryTime"),
        evt("MUIA_String_BufferPos", "MUIV_EveryTime"),
        evt("MUIA_String_Contents", "MUIV_EveryTime"),
        evt("MUIA_ShowMe", "FALSE"),
        evt("MUIA_ShowMe", "TRUE"),
        evt("MUIA_Disabled", "TRUE"),
        evt("MUIA_Disabled", "FALSE"),
    };
    return t;
}

// --- ListView (TY_LISTVIEW = 5; also reused for DirList = 17) ---
const AV &actionsListview()
{
    static const AV t = {
        act(3, "MUIM_Set", "MUIA_ShowMe", "FALSE", K::Nothing),
        act(3, "MUIM_Set", "MUIA_ShowMe", "TRUE", K::Nothing),
        act(3, "MUIM_Set", "MUIA_Disabled", "TRUE", K::Nothing),
        act(3, "MUIM_Set", "MUIA_Disabled", "FALSE", K::Nothing),
        act(3, "MUIM_Set", "MUIA_Window_ActiveObject", "", K::Unsupported),
        act(1, "MUIM_List_Clear", "", "", K::Nothing),
        act(2, "MUIM_List_Jump", "", "", K::ConsInt),
        act(2, "MUIM_List_Redraw", "", "MUIV_List_Redraw_All", K::Nothing),
        act(2, "MUIM_List_Redraw", "", "MUIV_List_Redraw_Active", K::Nothing),
        act(2, "MUIM_List_Remove", "", "MUIV_List_Remove_Active", K::Nothing),
        act(1, "MUIM_List_Sort", "", "", K::Nothing),
        act(2, "MUIM_CallHook", "", "", K::Unsupported),
    };
    return t;
}
const EV &eventsListview()
{
    static const EV t = {
        evt("MUIA_Listview_DoubleClick", "TRUE"),
        evt("MUIA_List_Active", "MUIV_EveryTime"),
        evt("MUIA_ShowMe", "FALSE"),
        evt("MUIA_ShowMe", "TRUE"),
        evt("MUIA_Disabled", "TRUE"),
        evt("MUIA_Disabled", "FALSE"),
    };
    return t;
}

// --- Gauge (TY_GAUGE = 6) ---
const AV &actionsGauge()
{
    static const AV t = {
        act(3, "MUIM_Set", "MUIA_ShowMe", "FALSE", K::Nothing),
        act(3, "MUIM_Set", "MUIA_ShowMe", "TRUE", K::Nothing),
        act(3, "MUIM_Set", "MUIA_Disabled", "TRUE", K::Nothing),
        act(3, "MUIM_Set", "MUIA_Disabled", "FALSE", K::Nothing),
        act(3, "MUIM_Set", "MUIA_Gauge_Current", "MUIV_TriggerValue", K::Nothing),
        act(3, "MUIM_Set", "MUIA_Gauge_Current", "", K::ConsInt),
        act(3, "MUIM_Set", "MUIA_Gauge_Current", "", K::Unsupported),
        act(2, "MUIM_CallHook", "", "", K::Unsupported),
    };
    return t;
}
const EV &eventsGauge()
{
    static const EV t = {
        evt("MUIA_Gauge_Current", "MUIV_EveryTime"),
        evt("MUIA_ShowMe", "FALSE"),
        evt("MUIA_ShowMe", "TRUE"),
        evt("MUIA_Disabled", "TRUE"),
        evt("MUIA_Disabled", "FALSE"),
    };
    return t;
}

// --- Cycle (TY_CYCLE = 7) ---
const AV &actionsCycle()
{
    static const AV t = {
        act(3, "MUIM_Set", "MUIA_ShowMe", "FALSE", K::Nothing),
        act(3, "MUIM_Set", "MUIA_ShowMe", "TRUE", K::Nothing),
        act(3, "MUIM_Set", "MUIA_Disabled", "TRUE", K::Nothing),
        act(3, "MUIM_Set", "MUIA_Disabled", "FALSE", K::Nothing),
        act(3, "MUIM_Set", "MUIA_Window_ActiveObject", "", K::Unsupported),
        act(3, "MUIM_Set", "MUIA_Cycle_Active", "MUIV_TriggerValue", K::Nothing),
        act(3, "MUIM_Set", "MUIA_Cycle_Active", "", K::ConsInt),
        act(3, "MUIM_Set", "MUIA_Cycle_Active", "", K::Unsupported),
        act(2, "MUIM_CallHook", "", "", K::Unsupported),
    };
    return t;
}
const EV &eventsCycle()
{
    static const EV t = {
        evt("MUIA_Cycle_Active", "MUIV_EveryTime"),
        evt("MUIA_ShowMe", "FALSE"),
        evt("MUIA_ShowMe", "TRUE"),
        evt("MUIA_Disabled", "TRUE"),
        evt("MUIA_Disabled", "FALSE"),
    };
    return t;
}

// --- Radio (TY_RADIO = 8) ---
const AV &actionsRadio()
{
    static const AV t = {
        act(3, "MUIM_Set", "MUIA_ShowMe", "FALSE", K::Nothing),
        act(3, "MUIM_Set", "MUIA_ShowMe", "TRUE", K::Nothing),
        act(3, "MUIM_Set", "MUIA_Disabled", "TRUE", K::Nothing),
        act(3, "MUIM_Set", "MUIA_Disabled", "FALSE", K::Nothing),
        act(3, "MUIM_Set", "MUIA_Window_ActiveObject", "", K::Unsupported),
        act(3, "MUIM_Set", "MUIA_Radio_Active", "MUIV_TriggerValue", K::Nothing),
        act(3, "MUIM_Set", "MUIA_Radio_Active", "", K::ConsInt),
        act(3, "MUIM_Set", "MUIA_Radio_Active", "", K::Unsupported),
        act(2, "MUIM_CallHook", "", "", K::Unsupported),
    };
    return t;
}
const EV &eventsRadio()
{
    static const EV t = {
        evt("MUIA_Radio_Active", "MUIV_EveryTime"),
        evt("MUIA_ShowMe", "FALSE"),
        evt("MUIA_ShowMe", "TRUE"),
        evt("MUIA_Disabled", "TRUE"),
        evt("MUIA_Disabled", "FALSE"),
    };
    return t;
}

// --- Label (TY_LABEL = 9) ---
const AV &actionsLabel()
{
    static const AV t = {
        act(3, "MUIM_Set", "MUIA_ShowMe", "FALSE", K::Nothing),
        act(3, "MUIM_Set", "MUIA_ShowMe", "TRUE", K::Nothing),
        act(3, "MUIM_Set", "MUIA_Disabled", "TRUE", K::Nothing),
        act(3, "MUIM_Set", "MUIA_Disabled", "FALSE", K::Nothing),
        act(3, "MUIM_Set", "MUIA_Text_Contents", "MUIV_TriggerValue", K::Nothing),
        act(3, "MUIM_Set", "MUIA_Text_Contents", "", K::ConsString),
        act(3, "MUIM_Set", "MUIA_Text_Contents", "", K::Unsupported),
        act(2, "MUIM_CallHook", "", "", K::Unsupported),
    };
    return t;
}
const EV &eventsLabel()
{
    static const EV t = {
        evt("MUIA_ShowMe", "FALSE"),
        evt("MUIA_ShowMe", "TRUE"),
        evt("MUIA_Disabled", "TRUE"),
        evt("MUIA_Disabled", "FALSE"),
    };
    return t;
}

// --- Space (TY_SPACE = 10; also reused for Rectangle = 18) ---
const AV &actionsSpace()
{
    static const AV t = {
        act(3, "MUIM_Set", "MUIA_ShowMe", "FALSE", K::Nothing),
        act(3, "MUIM_Set", "MUIA_ShowMe", "TRUE", K::Nothing),
        act(2, "MUIM_CallHook", "", "", K::Unsupported),
    };
    return t;
}
const EV &eventsSpace()
{
    static const EV t = {
        evt("MUIA_ShowMe", "FALSE"),
        evt("MUIA_ShowMe", "TRUE"),
    };
    return t;
}

// --- Check (TY_CHECK = 11) ---
const AV &actionsCheck()
{
    static const AV t = {
        act(3, "MUIM_Set", "MUIA_Selected", "TRUE", K::Nothing),
        act(3, "MUIM_Set", "MUIA_Selected", "FALSE", K::Nothing),
        act(3, "MUIM_Set", "MUIA_Selected", "MUIV_TriggerValue", K::Nothing),
        act(3, "MUIM_Set", "MUIA_Selected", "", K::Unsupported), // PutVariableValue - TY_VARIABLE
        act(3, "MUIM_Set", "MUIA_ShowMe", "FALSE", K::Nothing),
        act(3, "MUIM_Set", "MUIA_ShowMe", "TRUE", K::Nothing),
        act(3, "MUIM_Set", "MUIA_Disabled", "TRUE", K::Nothing),
        act(3, "MUIM_Set", "MUIA_Disabled", "FALSE", K::Nothing),
        act(3, "MUIM_Set", "MUIA_Window_ActiveObject", "", K::Unsupported),
        act(2, "MUIM_CallHook", "", "", K::Unsupported),
    };
    return t;
}
const EV &eventsCheck()
{
    static const EV t = {
        evt("MUIA_Selected", "TRUE"),
        evt("MUIA_Selected", "FALSE"),
        evt("MUIA_Selected", "MUIV_EveryTime"),
        evt("MUIA_ShowMe", "FALSE"),
        evt("MUIA_ShowMe", "TRUE"),
        evt("MUIA_Disabled", "TRUE"),
        evt("MUIA_Disabled", "FALSE"),
    };
    return t;
}

// --- Scale (TY_SCALE = 12) ---
const AV &actionsScale()
{
    static const AV t = {
        act(3, "MUIM_Set", "MUIA_ShowMe", "FALSE", K::Nothing),
        act(3, "MUIM_Set", "MUIA_ShowMe", "TRUE", K::Nothing),
        act(2, "MUIM_CallHook", "", "", K::Unsupported),
    };
    return t;
}
const EV &eventsScale()
{
    static const EV t = {
        evt("MUIA_ShowMe", "FALSE"),
        evt("MUIA_ShowMe", "TRUE"),
    };
    return t;
}

// --- Image (TY_IMAGE = 13) ---
const AV &actionsImage()
{
    static const AV t = {
        act(3, "MUIM_Set", "MUIA_ShowMe", "FALSE", K::Nothing),
        act(3, "MUIM_Set", "MUIA_ShowMe", "TRUE", K::Nothing),
        act(3, "MUIM_Set", "MUIA_Disabled", "TRUE", K::Nothing),
        act(3, "MUIM_Set", "MUIA_Disabled", "FALSE", K::Nothing),
        act(2, "MUIM_CallHook", "", "", K::Unsupported),
    };
    return t;
}
const EV &eventsImage()
{
    static const EV t = {
        evt("MUIA_Pressed", "TRUE"),
        evt("MUIA_Pressed", "FALSE"),
        evt("MUIA_ShowMe", "FALSE"),
        evt("MUIA_ShowMe", "TRUE"),
        evt("MUIA_Disabled", "TRUE"),
        evt("MUIA_Disabled", "FALSE"),
    };
    return t;
}

// --- Slider (TY_SLIDER = 14) ---
const AV &actionsSlider()
{
    static const AV t = {
        act(3, "MUIM_Set", "MUIA_Slider_Level", "MUIV_TriggerValue", K::Nothing),
        act(3, "MUIM_Set", "MUIA_Slider_Level", "", K::ConsInt),
        act(3, "MUIM_Set", "MUIA_Slider_Level", "", K::Unsupported),
        act(3, "MUIM_Set", "MUIA_ShowMe", "FALSE", K::Nothing),
        act(3, "MUIM_Set", "MUIA_ShowMe", "TRUE", K::Nothing),
        act(3, "MUIM_Set", "MUIA_Disabled", "TRUE", K::Nothing),
        act(3, "MUIM_Set", "MUIA_Disabled", "FALSE", K::Nothing),
        act(2, "MUIM_CallHook", "", "", K::Unsupported),
    };
    return t;
}
const EV &eventsSlider()
{
    static const EV t = {
        evt("MUIA_Slider_Level", "MUIV_EveryTime"),
        evt("MUIA_Slider_Level", "MUIV_EveryTime"),
        evt("MUIA_ShowMe", "FALSE"),
        evt("MUIA_ShowMe", "TRUE"),
        evt("MUIA_Disabled", "TRUE"),
        evt("MUIA_Disabled", "FALSE"),
    };
    return t;
}

// --- Text (TY_TEXT = 15) ---
const AV &actionsText()
{
    static const AV t = {
        act(3, "MUIM_Set", "MUIA_Text_Contents", "MUIV_TriggerValue", K::Nothing),
        act(3, "MUIM_Set", "MUIA_Text_Contents", "", K::ConsString),
        act(3, "MUIM_Set", "MUIA_Text_Contents", "", K::Unsupported),
        act(3, "MUIM_Set", "MUIA_ShowMe", "FALSE", K::Nothing),
        act(3, "MUIM_Set", "MUIA_ShowMe", "TRUE", K::Nothing),
        act(3, "MUIM_Set", "MUIA_Disabled", "TRUE", K::Nothing),
        act(3, "MUIM_Set", "MUIA_Disabled", "FALSE", K::Nothing),
        act(2, "MUIM_CallHook", "", "", K::Unsupported),
    };
    return t;
}
const EV &eventsText()
{
    static const EV t = {
        evt("MUIA_Text_Contents", "MUIV_EveryTime"),
        evt("MUIA_ShowMe", "FALSE"),
        evt("MUIA_ShowMe", "TRUE"),
        evt("MUIA_Disabled", "TRUE"),
        evt("MUIA_Disabled", "FALSE"),
    };
    return t;
}

// --- Prop (TY_PROP = 16) ---
const AV &actionsProp()
{
    static const AV t = {
        act(3, "MUIM_Set", "MUIA_Prop_First", "MUIV_TriggerValue", K::Nothing),
        act(3, "MUIM_Set", "MUIA_Prop_First", "", K::ConsInt),
        act(3, "MUIM_Set", "MUIA_Prop_First", "", K::Unsupported),
        act(3, "MUIM_Set", "MUIA_ShowMe", "FALSE", K::Nothing),
        act(3, "MUIM_Set", "MUIA_ShowMe", "TRUE", K::Nothing),
        act(3, "MUIM_Set", "MUIA_Disabled", "TRUE", K::Nothing),
        act(3, "MUIM_Set", "MUIA_Disabled", "FALSE", K::Nothing),
        act(2, "MUIM_CallHook", "", "", K::Unsupported),
    };
    return t;
}
const EV &eventsProp()
{
    static const EV t = {
        evt("MUIA_Prop_First", "MUIV_EveryTime"),
        evt("MUIA_Prop_Entries", "MUIV_EveryTime"),
        evt("MUIA_Prop_Visible", "MUIV_EveryTime"),
        evt("MUIA_ShowMe", "FALSE"),
        evt("MUIA_ShowMe", "TRUE"),
        evt("MUIA_Disabled", "TRUE"),
        evt("MUIA_Disabled", "FALSE"),
    };
    return t;
}

// --- ColorField (TY_COLORFIELD = 19) ---
// NB: the real CACTColorField[]'s physical Red/Blue/Green index order
// does not match the display-string ACTColorField[]'s Red/Green/Blue
// order (see this file's top comment) - reproduced as-is.
const AV &actionsColorField()
{
    static const AV t = {
        act(3, "MUIM_Set", "MUIA_Colorfield_Red", "MUIV_TriggerValue", K::Nothing),
        act(3, "MUIM_Set", "MUIA_Colorfield_Red", "", K::ConsInt),
        act(3, "MUIM_Set", "MUIA_Colorfield_Red", "", K::Unsupported),
        act(3, "MUIM_Set", "MUIA_Colorfield_Blue", "MUIV_TriggerValue", K::Nothing),
        act(3, "MUIM_Set", "MUIA_Colorfield_Blue", "", K::ConsInt),
        act(3, "MUIM_Set", "MUIA_Colorfield_Blue", "", K::Unsupported),
        act(3, "MUIM_Set", "MUIA_Colorfield_Green", "MUIV_TriggerValue", K::Nothing),
        act(3, "MUIM_Set", "MUIA_Colorfield_Green", "", K::ConsInt),
        act(3, "MUIM_Set", "MUIA_Colorfield_Green", "", K::Unsupported),
        act(3, "MUIM_Set", "MUIA_ShowMe", "FALSE", K::Nothing),
        act(3, "MUIM_Set", "MUIA_ShowMe", "TRUE", K::Nothing),
        act(3, "MUIM_Set", "MUIA_Disabled", "TRUE", K::Nothing),
        act(3, "MUIM_Set", "MUIA_Disabled", "FALSE", K::Nothing),
        act(3, "MUIM_Set", "MUIA_Window_ActiveObject", "", K::Unsupported),
        act(2, "MUIM_CallHook", "", "", K::Unsupported),
    };
    return t;
}
const EV &eventsColorField()
{
    static const EV t = {
        evt("MUIA_Colorfield_Red", "MUIV_EveryTime"),
        evt("MUIA_Colorfield_Blue", "MUIV_EveryTime"),
        evt("MUIA_Colorfield_Green", "MUIV_EveryTime"),
        evt("MUIA_ShowMe", "FALSE"),
        evt("MUIA_ShowMe", "TRUE"),
        evt("MUIA_Disabled", "TRUE"),
        evt("MUIA_Disabled", "FALSE"),
    };
    return t;
}

// --- PopAsl (TY_POPASL = 20) ---
const AV &actionsPopAsl()
{
    static const AV t = {
        act(3, "MUIM_Set", "MUIA_ShowMe", "FALSE", K::Nothing),
        act(3, "MUIM_Set", "MUIA_ShowMe", "TRUE", K::Nothing),
        act(3, "MUIM_Set", "MUIA_Disabled", "TRUE", K::Nothing),
        act(3, "MUIM_Set", "MUIA_Disabled", "FALSE", K::Nothing),
        act(1, "MUIM_Popstring_Open", "", "", K::Nothing),
        act(2, "MUIM_CallHook", "", "", K::Unsupported),
    };
    return t;
}
const EV &eventsPopAsl()
{
    static const EV t = {
        evt("MUIA_ShowMe", "FALSE"),
        evt("MUIA_ShowMe", "TRUE"),
        evt("MUIA_Disabled", "TRUE"),
        evt("MUIA_Disabled", "FALSE"),
    };
    return t;
}

// --- PopObject (TY_POPOBJECT = 21) ---
const AV &actionsPopObject()
{
    static const AV t = {
        act(3, "MUIM_Set", "MUIA_ShowMe", "FALSE", K::Nothing),
        act(3, "MUIM_Set", "MUIA_ShowMe", "TRUE", K::Nothing),
        act(3, "MUIM_Set", "MUIA_Disabled", "TRUE", K::Nothing),
        act(3, "MUIM_Set", "MUIA_Disabled", "FALSE", K::Nothing),
        act(3, "MUIM_Set", "MUIA_Window_ActiveObject", "", K::Unsupported),
        act(2, "MUIM_CallHook", "", "", K::Unsupported),
    };
    return t;
}
const EV &eventsPopObject()
{
    static const EV t = {
        evt("MUIA_ShowMe", "FALSE"),
        evt("MUIA_ShowMe", "TRUE"),
        evt("MUIA_Disabled", "TRUE"),
        evt("MUIA_Disabled", "FALSE"),
    };
    return t;
}

// --- "Menu" = original's TY_MENUSTRIP (22) - our ObjType::Menu (the
//     whole menu bar attached to a window) ---
const AV &actionsMenuStrip()
{
    static const AV t = {
        act(3, "MUIM_Set", "MUIA_Menustrip_Enabled", "TRUE", K::Nothing),
        act(3, "MUIM_Set", "MUIA_Menustrip_Enabled", "FALSE", K::Nothing),
        act(2, "MUIM_CallHook", "", "", K::Unsupported),
    };
    return t;
}
const EV &eventsMenuStrip()
{
    static const EV t = {
        evt("MUIA_Menustrip_Enabled", "TRUE"),
        evt("MUIA_Menustrip_Enabled", "FALSE"),
    };
    return t;
}

// --- "SubMenu" = original's TY_MENU (23) - one top-level/nested menu
//     header ---
const AV &actionsSubMenu()
{
    static const AV t = {
        act(3, "MUIM_Set", "MUIA_Menu_Enabled", "TRUE", K::Nothing),
        act(3, "MUIM_Set", "MUIA_Menu_Enabled", "FALSE", K::Nothing),
        // "0" here (not MUIV_TriggerValue) is reproduced exactly as the
        // real CACTMenu[]'s own literal data - a genuine quirk/likely
        // bug of the original (MenuItem's equivalent entries below DO
        // use MUIV_TriggerValue correctly) we deliberately do not "fix".
        act(3, "MUIM_Set", "MUIA_Menu_Title", "0", K::Nothing),
        act(3, "MUIM_Set", "MUIA_Menu_Title", "", K::Unsupported), // ChangeTitleVariable - TY_VARIABLE
        act(3, "MUIM_Set", "MUIA_Menu_Title", "", K::ConsString),  // ChangeTitleConstant
        act(2, "MUIM_CallHook", "", "", K::Unsupported),
    };
    return t;
}
const EV &eventsSubMenu()
{
    static const EV t = {
        evt("MUIA_Menu_Enabled", "TRUE"),
        evt("MUIA_Menu_Enabled", "FALSE"),
    };
    return t;
}

// --- MenuItem (TY_MENUITEM = 24) ---
const AV &actionsMenuItem()
{
    static const AV t = {
        act(3, "MUIM_Set", "MUIA_Menuitem_Enabled", "TRUE", K::Nothing),
        act(3, "MUIM_Set", "MUIA_Menuitem_Enabled", "FALSE", K::Nothing),
        act(3, "MUIM_Set", "MUIA_Menuitem_Checked", "TRUE", K::Nothing),
        act(3, "MUIM_Set", "MUIA_Menuitem_Checked", "FALSE", K::Nothing),
        act(3, "MUIM_Set", "MUIA_Menuitem_Checkit", "TRUE", K::Nothing),
        act(3, "MUIM_Set", "MUIA_Menuitem_Checkit", "FALSE", K::Nothing),
        act(3, "MUIM_Set", "MUIA_Menuitem_Title", "MUIV_TriggerValue", K::Nothing),
        act(3, "MUIM_Set", "MUIA_Menuitem_Title", "", K::Unsupported), // ChangeTitleVariable
        act(3, "MUIM_Set", "MUIA_Menuitem_Title", "", K::ConsString),  // ChangeTitleConstant
        act(3, "MUIM_Set", "MUIA_Menuitem_Shortcut", "MUIV_TriggerValue", K::Nothing),
        act(3, "MUIM_Set", "MUIA_Menuitem_Shortcut", "", K::Unsupported), // ChangeShortVariable
        act(3, "MUIM_Set", "MUIA_Menuitem_Shortcut", "", K::ConsString),  // ChangeShortConstant
        act(3, "MUIM_Set", "MUIA_Menuitem_Toggle", "TRUE", K::Nothing),
        act(3, "MUIM_Set", "MUIA_Menuitem_Toggle", "FALSE", K::Nothing),
        act(2, "MUIM_CallHook", "", "", K::Unsupported),
    };
    return t;
}
const EV &eventsMenuItem()
{
    static const EV t = {
        evt("MUIA_Menuitem_Trigger", "MUIV_EveryTime"),
        evt("MUIA_Menuitem_Enabled", "TRUE"),
        evt("MUIA_Menuitem_Enabled", "FALSE"),
        evt("MUIA_Menuitem_Checked", "TRUE"),
        evt("MUIA_Menuitem_Checked", "FALSE"),
        evt("MUIA_Menuitem_Checkit", "TRUE"),
        evt("MUIA_Menuitem_Checkit", "FALSE"),
    };
    return t;
}

const AV kEmptyActions;
const EV kEmptyEvents;

} // namespace

const QVector<NotifyActionDef> &notifyActionsFor(ObjType type)
{
    switch (type)
    {
    case ObjType::Appli: return actionsAppli();
    case ObjType::Window: return actionsWindow();
    case ObjType::Group: return actionsGroup();
    case ObjType::Button: return actionsButton();
    case ObjType::String: return actionsString();
    case ObjType::ListView: case ObjType::DirList: return actionsListview();
    case ObjType::Gauge: return actionsGauge();
    case ObjType::Cycle: return actionsCycle();
    case ObjType::Radio: return actionsRadio();
    case ObjType::Label: return actionsLabel();
    case ObjType::Space: case ObjType::Rectangle: return actionsSpace();
    case ObjType::Check: return actionsCheck();
    case ObjType::Scale: return actionsScale();
    case ObjType::Image: return actionsImage();
    case ObjType::Slider: return actionsSlider();
    case ObjType::Text: return actionsText();
    case ObjType::Prop: return actionsProp();
    case ObjType::ColorField: return actionsColorField();
    case ObjType::PopAsl: return actionsPopAsl();
    case ObjType::PopObject: return actionsPopObject();
    case ObjType::Menu: return actionsMenuStrip();
    case ObjType::SubMenu: return actionsSubMenu();
    case ObjType::MenuItem: return actionsMenuItem();
    default: return kEmptyActions;
    }
}

const QVector<NotifyEventDef> &notifyEventsFor(ObjType type)
{
    switch (type)
    {
    case ObjType::Appli: return eventsAppli();
    case ObjType::Window: return eventsWindow();
    case ObjType::Group: return eventsGroup();
    case ObjType::Button: return eventsButton();
    case ObjType::String: return eventsString();
    case ObjType::ListView: case ObjType::DirList: return eventsListview();
    case ObjType::Gauge: return eventsGauge();
    case ObjType::Cycle: return eventsCycle();
    case ObjType::Radio: return eventsRadio();
    case ObjType::Label: return eventsLabel();
    case ObjType::Space: case ObjType::Rectangle: return eventsSpace();
    case ObjType::Check: return eventsCheck();
    case ObjType::Scale: return eventsScale();
    case ObjType::Image: return eventsImage();
    case ObjType::Slider: return eventsSlider();
    case ObjType::Text: return eventsText();
    case ObjType::Prop: return eventsProp();
    case ObjType::ColorField: return eventsColorField();
    case ObjType::PopAsl: return eventsPopAsl();
    case ObjType::PopObject: return eventsPopObject();
    case ObjType::Menu: return eventsMenuStrip();
    case ObjType::SubMenu: return eventsSubMenu();
    case ObjType::MenuItem: return eventsMenuItem();
    default: return kEmptyEvents;
    }
}
