// MuiBuilderQt GUI - small tree-mutation helpers shared by CanvasWidget,
// ObjectTreeView and MainWindow.
//
// Scope: the first group of helpers below operates on the Group/`children`
// layout tree (the part the Canvas/Palette edit) and on the top-level
// `MuibProject::windows` list. A second group (see "Menu-family tree
// helpers" further down) operates on a Window's `menu` (Menu/SubMenu/
// MenuItem, via `childs`) - added once menu editing itself was added to
// the GUI (previously out of scope; ObjectTreeView showed these read-only
// only). PopObject's `popObj` remains out of scope - a PopObject's popped-
// up sub-object isn't part of either tree and stays editable only via the
// loaded .MUIB file, as before.
#pragma once

#include "muibobject.h"
#include <QHash>
#include <vector>
#include <memory>

// Appends `child` to `group`'s children list, sets child->father = group,
// and returns the now-owned raw pointer (valid until the tree structure
// changes again). `group` must be an ObjType::Group object (asserts in
// debug builds); callers (CanvasWidget's drop handler) already guarantee
// this since only Group boxes accept drops.
MuibObject *appendChildToGroup(MuibObject *group, std::unique_ptr<MuibObject> child);

// Moves `obj` (which must currently be a direct child of some Group, i.e.
// obj->father->type == ObjType::Group) so it becomes child index
// `newIndex` of `newParent` (also must be a Group). `newIndex` is
// expressed against `newParent`'s children list AS IT STANDS BEFORE
// `obj` is removed - i.e. "insert before the object currently at this
// index", counting `obj` itself like any other sibling if it is already
// in that same list. That matches how ObjectBox computes a drop position
// (comparing the cursor to the still-on-screen sibling boxes), so callers
// never need to pre-adjust for the removal themselves; the same-group
// case internally shifts the index down by one when needed. Used for the
// Canvas's drag-to-reorder-within-a-group feature; currently only called
// with newParent == obj->father (see README's scope note on this
// feature), but works for moving into a different Group too. Returns
// false if `obj`/`newParent` don't satisfy the preconditions above.
bool moveObjectWithinGroup(MuibObject *obj, MuibObject *newParent, int newIndex);

// Removes `target` from wherever it sits in `proj`'s window/group tree
// (either a top-level entry in proj->windows, or somewhere in some
// group's `children`). Returns true if found and removed. The removed
// object itself is destroyed (along with its whole subtree) - there is
// no undo for this in the current pass, see README.md's "Bekannte
// Einschraenkungen" section.
bool removeObjectFromProject(MuibProject *proj, MuibObject *target);

// Collects every `label` currently used anywhere in the project (all
// windows/groups/widgets/menu entries), so createDefaultObject() /
// createDefaultWindow() can pick a guaranteed-unique one for a freshly
// dropped object.
QStringList collectAllLabels(const MuibProject *proj);

// Rewrites every project-wide reference to a label that got renamed, per
// `renames` (old label -> new label). Two kinds of reference exist in this
// object model, both by plain string, never by pointer (see NotifyEvent's
// own doc comment): every MuibObject's own `notify` vector's
// NotifyEvent::targetLabel (walked across proj->appMenu, every window's
// root-group AND menu subtree, and any PopObject's popped-up object - the
// exact same tree shape collectAllLabels()/collectLabels() already walk),
// plus MuibProject::notify itself (the application-level notify list -
// "appli1.notify" in the original, loaded/saved but not currently
// consumed by any BuildXxx() in muicodegen.cpp; rewritten anyway for
// faithful round-tripping even though nothing reads it today) and
// MuibProject::aboutBox's own `aboutLinkedMenuItem` (a MuiBuilderQt-only
// extension, not a NotifyEvent at all, but the same kind of
// label-by-string reference - see its own doc comment in muibobject.h).
//
// Added for MenuEditorDialog's user-editable menu-item "ID (intern)"
// field (see its own header doc comment / labelRenames()): renaming a
// menu entry's label inside that dialog only touches ONE window's own
// deep-cloned menu subtree, so nothing else in the project is renamed
// yet when the dialog returns - the caller (MainWindow::onMenuEditor())
// calls this afterwards so every notify wire and the AboutBox link keep
// pointing at the right object, project-wide, exactly as they did before
// the rename. A no-op (returns immediately) if `renames` is empty, so
// callers can call this unconditionally after every accepted edit
// without a manual emptiness check.
void applyLabelRenames(MuibProject *proj, const QHash<QString, QString> &renames);

// --- Menu-family tree helpers ------------------------------------------
// `window` must be an ObjType::Window object; `parent` for
// appendChildToMenu() must be ObjType::Menu or ObjType::SubMenu (the only
// two Menu-family types that may have children - MenuItem is always a
// leaf). Unlike the Group helpers above, these never need to search the
// project from the root: the loader already sets every Menu-family
// object's `father` (including the root Menu's, to its owning Window -
// see muibloader.cpp's loadWindow()), so a target object alone is always
// enough to find and mutate its owning list.
//
// (This section used to also have addMenuToWindow()/removeMenuFromWindow()
// for the old right-click-context-menu editor's "Menue hinzufuegen"/
// "Menue entfernen" actions - removed once MenuEditorDialog replaced that
// editor entirely, since replaceWindowMenu() below now covers both cases
// generically, whether or not the window already had a menu.)

// Appends `child` (a freshly created SubMenu/MenuItem) to `parent`'s
// `childs` list, sets child->father = parent, and returns the now-owned
// raw pointer. `parent` must be ObjType::Menu or ObjType::SubMenu (asserts
// in debug builds).
MuibObject *appendChildToMenu(MuibObject *parent, std::unique_ptr<MuibObject> child);

// Moves `obj` (any Menu/SubMenu/MenuItem with a father) one slot earlier
// (moveUp=true) or later (moveUp=false) among its father's `childs`.
// Returns false if `obj` has no father, or is already at that end of the
// list.
bool moveMenuChildUpDown(MuibObject *obj, bool moveUp);

// Removes `obj` (any Menu/SubMenu/MenuItem with a father, i.e. NOT the
// root Menu - use removeMenuFromWindow() for that) from its father's
// `childs` list, destroying its whole subtree. Returns false if `obj` has
// no father or isn't found there.
bool removeMenuChild(MuibObject *obj);

// Deep-clones a Menu-family subtree (`src` must be ObjType::Menu/SubMenu/
// MenuItem) - every field the Menu family actually uses (label,
// generated, help, notify, name, menuKey, menu_enable, check_enable,
// check_state, toggleMenu, excludeGroup) plus a recursive clone of
// `childs`, with each clone's `father` fixed up to point at its new
// parent clone (`src`'s own father is NOT carried over - the caller
// decides where the clone attaches, if anywhere). Returns nullptr if
// `src` is nullptr. NOT a generic MuibObject::clone() - this project
// deliberately has none (see muibobject.h's own "kitchen sink" object
// design-decision comment) - scoped tightly to exactly what MenuEditorDialog
// needs to edit a provisional COPY of a window's menu (so Cancel can
// discard every change made during the dialog session).
std::unique_ptr<MuibObject> cloneMenuTree(const MuibObject *src);

// Replaces `window`'s entire menu strip with `newMenuRoot` (may be
// nullptr, meaning "this window should end up with no menu at all" -
// unlike addMenuToWindow()/removeMenuFromWindow() above, this works
// whether or not `window` already had one, and is what MenuEditorDialog
// commits its edited copy through on OK). `window` must be an
// ObjType::Window object (asserts in debug builds). A non-null
// `newMenuRoot` must be an ObjType::Menu object; its `father` is set to
// `window`.
void replaceWindowMenu(MuibObject *window, std::unique_ptr<MuibObject> newMenuRoot);
