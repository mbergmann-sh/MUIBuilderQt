// MuiBuilderQt - ground-truth notification action/event tables.
//
// Ported from the REAL MUIBuilder v2.3 source's own notification-codegen
// data files - NOT invented, NOT a "best effort" mapping:
//   - Modules/../src/codenotifydefs.c: CACTxxx[]/CEVTxxx[]/ArgEVTxxx[]
//     (int arrays - the actual per-object-type "what MUIM_*/MUIA_* call
//     does this notify action turn into" tables CodeNotify()/WriteNotify()
//     use at code-generation time, keyed by the on-disk event/action
//     integer index that a NotifyEvent's srcType/destType already is).
//   - Modules/../src/initnotify.c: TYxxx[] (per-object-type "what KIND of
//     action is this" classification array - TY_NOTHING/TY_CONS_INT/
//     TY_CONS_CHAR/TY_CONS_STRING/TY_CONS_BOOL/TY_WINOBJ/TY_FUNCTION/
//     TY_ID/TY_VARIABLE, see builder.h's #defines) - read alongside the
//     ACTxxx[]/EVTxxx[] *display strings* (also assigned in
//     InitNotifyArrays(), for the original's own notify-editor GUI - not
//     needed here since we don't (yet) have a notify-editing UI, only
//     preserve+regenerate what a loaded real .MUIB file already stored).
//
// Both files use raw MB_MUIM_*/MB_MUIA_*/MB_MUIV_* integer tokens
// (#defined in muib_file.h, also in our own source tree) rather than the
// real MUI symbol strings - but since every MB_* macro name IS the real
// name with an "MB_" prefix (confirmed directly: "#define
// MB_MUIM_Application_ReturnID 238" in muib_file.h - the 238 is never
// needed, the *name* already tells us the answer), this file simply
// writes out the real stripped names directly as string literals instead
// of reproducing a numeric token table - there is no missing/unrecoverable
// lookup step here (unlike the *separate*, genuinely-unavailable
// GenCodeC.c/MUIStrings[] table muicodegen.h's top-of-file comment
// documents - that one is about rendering the WIDGET TREE itself, not
// notifications, and remains a real gap).
//
// Scope/limitation that IS real and unavoidable with our current object
// model: TY_WINOBJ (references a second, independent target object -
// "activate window object X" - the original's evt->arguments field,
// which our own NotifyEvent does not capture), TY_FUNCTION/TY_ID/
// TY_VARIABLE (reference the project's external Functions/Idents/
// Variables name lists by pointer, also not captured) cannot be
// rendered symbolically - emitNotifications() in muicodegen.cpp keeps
// its previous raw-integer "verify by hand" fallback for exactly these
// four classifications (NotifyActionKind::Unsupported below), same as
// for any destType/srcType index the real table doesn't define at all
// (out-of-range, or targeting an object type - e.g. a raw AboutBox -
// that was never a valid notification destination in the original).
#pragma once

#include "muibobject.h"
#include <QString>
#include <QVector>

enum class NotifyActionKind
{
    Nothing,     // TY_NOTHING - fully fixed/static, render literally
    ConsInt,     // TY_CONS_INT - value is evt.argString, rendered as an integer literal
    ConsChar,    // TY_CONS_CHAR - value is evt.argString's first character, as a C char literal
    ConsString,  // TY_CONS_STRING - value is evt.argString, as a C string literal
    ConsBool,    // TY_CONS_BOOL - value is evt.argString ("1"/other), rendered TRUE/FALSE
    Unsupported, // TY_WINOBJ/TY_FUNCTION/TY_ID/TY_VARIABLE - needs data our
                 // NotifyEvent doesn't capture; caller keeps the old raw fallback
};

// One destination-side action definition - mirrors one 4-int record of
// the original's CACTxxx[] tables (argCount, method, attr, value) plus
// the matching TYxxx[] classification for that same index.
struct NotifyActionDef
{
    int argCount = 0;    // 1, 2 or 3, exactly as CACTxxx[4*i+0]
    QString method;      // e.g. "MUIM_Application_ReturnID", "MUIM_Set", "MUIM_List_Clear", "MUIM_CallHook"
    QString attr;         // e.g. "MUIA_Selected" - only meaningful/emitted when argCount == 3
    QString value;        // literal text for the trailing operand of a Nothing action
                           // (argCount==3: the attribute's value, e.g. "TRUE"/"MUIV_TriggerValue";
                           //  argCount==2: the value in the "attr" slot instead, e.g.
                           //  "MUIV_Application_ReturnID_Quit"; argCount==1: unused).
                           // Ignored for the four Cons* kinds (the real value comes from
                           // the NotifyEvent's own argString at render time instead).
    NotifyActionKind kind = NotifyActionKind::Unsupported;
};

// One source-side event definition - mirrors one 2-int record of the
// original's CEVTxxx[] tables (attr, value), both already fully fixed/
// literal (unlike the destination side, no source event in the real
// tool ever takes a user-supplied runtime value - matches ArgEVTxxx[]
// only ever gating the notify-editor GUI's own input widget, not
// anything CodeNotify() itself needed).
struct NotifyEventDef
{
    QString attr;   // e.g. "MUIA_Pressed", "MUIA_Menuitem_Trigger", "MUIA_Window_CloseRequest"
    QString value;  // e.g. "TRUE", "FALSE", "MUIV_EveryTime"
};

// Returns object type `type`'s destination-action table (CACTxxx[]+
// TYxxx[] combined), indexed by NotifyEvent::targetTypeId is NOT it -
// index with the notification's actual destType (dest_type/evt->id
// stored per-event) instead. Empty for a type that was never a valid
// notification destination in the original (e.g. Space/Rectangle/
// AboutBox) or isn't modelled here.
const QVector<NotifyActionDef> &notifyActionsFor(ObjType type);

// Returns object type `type`'s source-event table (CEVTxxx[]), indexed
// by the notification's srcType. Empty for a type that was never a
// valid notification source in the original.
const QVector<NotifyEventDef> &notifyEventsFor(ObjType type);
