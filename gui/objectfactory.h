// MuiBuilderQt GUI - factory helpers for objects created interactively
// (dragged from the WidgetPalette, or via a menu action), as opposed to
// objects loaded from a .MUIB file (MuibLoader already builds those with
// every field taken from the file).
//
// Scope decision: the palette only offers the 20 "real widget" object
// types (Group plus every leaf control). Window is created via the
// File/Edit menu ("New Window"), not dragged onto the canvas - a window
// is a top-level project entry, not something you drop into another
// object. The Menu/SubMenu/MenuItem family is also NOT in the palette
// (menu strips are a linear strip/submenu structure, not something that
// nests into the widget layout tree, so they're never dragged onto the
// Canvas) - but createDefaultObject() below DOES support these three
// types, used by ObjectTreeView's menu-editing context menu (see
// objecttreeutil.h's "Menu-family tree helpers") rather than the palette.
#pragma once

#include "muibobject.h"
#include <QString>
#include <QVector>
#include <memory>

// MIME type used to drag a new-object type (as its ObjType int, in the
// payload bytes) from WidgetPalette onto an ObjectBox on the Canvas.
inline constexpr const char *kMuibObjectTypeMime = "application/x-muibuilderqt-objecttype";

// MIME type used to drag an EXISTING object (already a child of some
// Group) over its siblings to reorder it within that same Group. The
// payload is the source MuibObject* itself, copied byte-for-byte - valid
// only within this same process/address space, which is always true here
// since ObjectBox/CanvasWidget render the very MuibProject tree they drag
// from (nothing is ever serialized across a process boundary for this).
inline constexpr const char *kMuibObjectReorderMime = "application/x-muibuilderqt-objectreorder";

struct PaletteEntry
{
    ObjType type;
    QString displayName;   // shown in the palette and as the default label prefix
    QString category;      // groups entries in the palette (Container/Steuerelemente/Anzeige/Layout)
};

// The fixed, ordered list of types offered in the WidgetPalette.
const QVector<PaletteEntry> &paletteEntries();

// Creates a new MuibObject of `type` with sensible, immediately-usable
// default field values (a freshly dropped "Button" already has a visible
// title, a freshly dropped "Cycle" already has two entries, etc.) and an
// auto-generated, so-far-unique `label` (e.g. "Button", "Button_2", ...
// - checked against `existingLabels` so two drops in the same project
// never collide, which would make notifications/code-gen ambiguous).
std::unique_ptr<MuibObject> createDefaultObject(ObjType type, const QStringList &existingLabels);

// Creates a brand-new Window, complete with its mandatory root Group
// (isRoot=true) - a MUI window always needs a root layout object, so
// "New Window" hands back something immediately droppable-into rather
// than a window with a null ->root that every other piece of code would
// have to null-check.
std::unique_ptr<MuibObject> createDefaultWindow(const QStringList &existingLabels);

// True if `type`'s `area` field (AreaAttrs) is actually meaningful and
// should be shown in the PropertyInspector - ports the muibobject.h
// comment verbatim ("most - not all - Space and the Appli/Window/Group/
// Menu 'header' objects don't have one").
bool typeHasAreaAttrs(ObjType type);

// Human-readable name for any ObjType, including the ones NOT in the
// palette (Window, Appli, Menu/SubMenu/MenuItem) - used by ObjectBox's
// header, ObjectTreeView's item text, and PropertyInspector's title.
QString displayNameForType(ObjType type);
