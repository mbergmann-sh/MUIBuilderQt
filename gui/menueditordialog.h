// MuiBuilderQt GUI - self-contained "Menue-Editor" dialog for one
// Window's whole menu strip (Menu/SubMenu/MenuItem tree, via `childs`).
//
// Replaces the previous right-click-context-menu-based menu editing in
// ObjectTreeView entirely, at the Chefentwickler's own explicit request -
// he sent a screenshot of a comparable "Menu Setup" dialog (a flat
// outline list of the whole menu plus Name/CommKey/Type fields,
// Add/Delete/Modify/Move Up/Move Down/OK/Cancel) as a UX blueprint. That
// screenshot's own controls map almost 1:1 onto this project's existing
// Menu/SubMenu/MenuItem model - see the field-by-field comments below for
// the one real adaptation this port makes (the "Type" concept) and the
// "Mutual Group" field's own honesty note.
//
// Editing semantics: operates on a DEEP COPY (objecttreeutil.h's
// cloneMenuTree()) of the window's current menu strip, so Cancel can
// discard every change made during the dialog session - the window's
// real `menu` is only replaced (via objecttreeutil.h's
// replaceWindowMenu()) by the caller if the dialog is accepted (OK). A
// window with no menu yet starts the dialog on an empty tree; accepting
// with at least one top-level entry creates the menu strip, accepting
// with none removes it (if it had one) - there is no separate "add/
// remove whole menu" action anymore, unlike the old context-menu editor.
//
// "Type" concept, REVISED after the Chefentwickler's own bug report
// (first shipped version: selecting a SubMenu and clicking "Hinzufuegen"
// always added the new entry as ITS child, so merely having the wrong
// item selected - e.g. a MenuItem the combo had defaulted to "Untermenue"
// for right after selecting its own future parent - silently nested
// every further entry one level too deep; see the screenshot in the bug
// report, where "Tabs" and a separator both ended up as children of
// "About" instead of its siblings under "File"). The real underlying
// model only has two KINDS (ObjType::SubMenu = container-capable,
// ObjType::MenuItem = always a leaf - see muicodegen.cpp's own comments),
// but the classic 3-level Amiga menu shape (Title/Item/Sub-Item, exactly
// what the screenshot's own Menu Type column enumerates minus its
// separate "Menu Bar" entry, which this dialog already covers via the
// "Menue-Bar" checkbox instead) is a property of TREE DEPTH, not of
// which combo value happened to be chosen for some ancestor. So the combo
// now offers the 3 real depth levels directly - "Menue" (index 0, a new
// top-level SubMenu, always appended directly under the tree root, no
// matter what is selected), "Menuepunkt" (index 1, a new leaf MenuItem
// under the RELEVANT top-level Menue - found by climbing up from whatever
// is currently selected to its own depth-1 ancestor) and
// "Untermenuepunkt" (index 2, a new leaf MenuItem one level below an
// EXISTING Menuepunkt - found by climbing up from the current selection
// to its depth-2 ancestor). Each choice has exactly one placement rule
// that depends only on DEPTH, never on the selected item's own kind, so
// accidentally having the "wrong" thing selected can no longer nest an
// entry too deep - at worst climbing up lands on the wrong Menue/
// Menuepunkt, which is immediately visible in the tree and easy to
// correct with "Nach oben"/"Nach unten" or by reselecting first.
// Selecting an existing entry updates the combo to reflect ITS OWN depth
// (a sensible default for the next Add), but "Aendern" (Modify) never
// reads the combo at all - see applyFieldsTo()'s own comment. The one
// exception to "Add never changes an existing entry's kind": adding an
// Untermenuepunkt under a Menuepunkt that is still a plain leaf MenuItem
// converts that Menuepunkt into a SubMenu right there (in onAdd() only,
// nowhere else) so it has somewhere to put the new child - always safe,
// since a childless SubMenu already renders identically to a plain
// MenuItem leaf (muicodegen.cpp), so nothing already-generated changes
// until the new child is actually added. onDelete() reverses this
// automatically when a Menuepunkt's last child is removed, so an emptied
// container goes back to being a plain leaf rather than lingering as a
// SubMenu with nothing in it.
//
// "Aktion" (notify wiring), added after the Chefentwickler's own report
// that there was no way at all to give a menu item (specifically "Quit")
// an action - true project-wide, not just here: obj->notify (the same
// srcType/destType-based wiring muicodegen.cpp already regenerates
// faithfully for whatever a REAL .MUIB file happens to already store -
// see notifytables.h) had no GUI to CREATE it from scratch anywhere in
// MuiBuilderQt before this. This dialog adds a small, deliberately
// CURATED "Aktion" combo for MenuItem leaves only - not a fully general
// "pick any target object + any of its real actions" editor, which would
// need this dialog to resolve arbitrary target object TYPES (it is only
// ever given `project` for enumerating windows, see below) and is
// left as a separate, larger future feature. What IS offered is real,
// ground-truth-verified (notifytables.h's actionsAppli()/actionsWindow()
// tables, ported directly from the original's own codenotifydefs.c) and
// covers the two overwhelmingly common cases: "Programm beenden (Quit)"
// (the literal reported gap) and opening/closing/activating another
// Window in the project (e.g. a "Settings..." item). Calling an
// arbitrary external Function is NOT offered - real MUIBuilder's
// TY_FUNCTION notify actions reference the project's own Functions name
// list by pointer, data this port's NotifyEvent struct has never
// captured (see notifytables.h's own top-of-file comment) - inventing a
// plausible-looking call here without that plumbing would violate this
// project's own core rule, so it stays a documented, known gap instead.
//
// Because a real, already-loaded .MUIB file can carry notify wiring this
// small curated combo cannot represent (multiple events on one item, or
// a target/action pair outside the two cases above), loadFieldsFrom()
// NEVER silently destroys what it can't show: an unrecognized
// obj->notify selects a dedicated "Unveraendert lassen" sentinel entry,
// and applyFieldsTo()/onAdd() only ever touch obj->notify when the combo
// is on something OTHER than that sentinel - so simply opening this
// dialog and clicking "Aendern" without touching the Aktion combo can
// never lose pre-existing, unrepresented wiring.
//
// "ID (intern)", added after the Chefentwickler's own report that the
// auto-generated, purely order-based default labels ("MenuItem_5",
// "MenuItem_2", ...) give no clue which real, visible entry they belong
// to - he once hand-edited generated code for the wrong menu item as a
// direct result (see muicodegen.cpp's debugStubDisplayTextFor() comment
// for the full story). That fix added a read-only comment to the
// generated code; this one lets the label itself be renamed to something
// self-documenting ("MI_Quit") right in the dialog, in a QLineEdit
// pre-filled with the current/default value, exactly as requested. Also
// shown as its own tree column (updated automatically on every
// rebuildTree(), i.e. after every Add/Aendern), per his own follow-up
// request, so the ID is visible without opening a Modify at all.
//
// A label is not just decoration - `identOf()`/`rawIdent()`
// (muicodegen.cpp) use it directly as the generated C identifier, and
// NotifyEvent::targetLabel / MuibProject::aboutBox's own
// aboutLinkedMenuItem reference OTHER objects by this exact string,
// project-wide, not just within this window's own menu. Since this
// dialog only ever sees one window's own (cloned) menu tree, renaming a
// label here cannot safely rewrite those other references itself - it
// only enforces that the new value is a valid identifier and, against
// `m_existingLabels` (already project-wide, see the constructor's own
// comment), unique. The CALLER (MainWindow::onMenuEditor()) is
// responsible for actually cascading each rename project-wide via
// labelRenames() (see its own comment) once the dialog is accepted -
// exactly the same split of responsibility takeEditedMenu()/
// replaceWindowMenu() already use for applying the edited tree itself.
#pragma once

#include <QDialog>
#include <QHash>
#include "muibobject.h"
#include "notifytables.h"
#include <memory>
#include <vector>

class QTreeWidget;
class QTreeWidgetItem;
class QLineEdit;
class QComboBox;
class QCheckBox;
class QSpinBox;
class QPushButton;
class QLabel;

class MenuEditorDialog : public QDialog
{
    Q_OBJECT
public:
    // `window` must be an ObjType::Window object (asserts in debug
    // builds); its CURRENT `menu` is only read here (cloned), never
    // mutated - the caller applies takeEditedMenu()'s result via
    // objecttreeutil.h's replaceWindowMenu() only after exec() returns
    // QDialog::Accepted. `existingLabels` must already include every
    // label anywhere in the project (this window's own current menu
    // labels among them - that's fine, see the .cpp) so newly added
    // entries get truly project-wide-unique labels. `project` is used
    // read-only, purely to enumerate the other Windows in it for the
    // "Aktion" combo's Fenster-oeffnen/schliessen/aktivieren choices (see
    // the class doc comment's own "Aktion" section) - may be nullptr, in
    // which case the combo simply offers no Window-targeting choices.
    explicit MenuEditorDialog(MuibObject *window, QStringList existingLabels,
                               const MuibProject *project, QWidget *parent = nullptr);

    // Only meaningful after exec() returned QDialog::Accepted - hands
    // over ownership of the edited tree's root Menu object (nullptr if
    // the user deleted every top-level entry, meaning "the window should
    // end up with no menu at all"). Moves out of the dialog's own
    // storage, so this can only usefully be called once.
    std::unique_ptr<MuibObject> takeEditedMenu();

    // Only meaningful after exec() returned QDialog::Accepted - every
    // entry still present in the edited tree whose label differs from
    // what it was when this dialog was constructed, keyed by the OLD
    // (real, still-live-elsewhere-in-the-project) label, valued by the
    // NEW one - see the class doc comment's own "ID (intern)" section.
    // Unlike takeEditedMenu(), this can be called any number of times
    // (it recomputes from m_root, doesn't move anything) and is cheap to
    // call even when nothing was renamed (returns an empty hash). The
    // caller must apply this BEFORE or AFTER replaceWindowMenu() (order
    // doesn't matter, the two trees are disjoint) by rewriting every
    // NotifyEvent::targetLabel and MuibProject::aboutBox's own
    // aboutLinkedMenuItem project-wide that matches an old key.
    QHash<QString, QString> labelRenames() const;

private slots:
    void onSelectionChanged();
    void onAdd();
    void onModify();
    void onDelete();
    void onMoveUp();
    void onMoveDown();
    void onSeparatorToggled(bool checked);

private:
    void rebuildTree();
    QTreeWidgetItem *addTreeItem(QTreeWidgetItem *parent, MuibObject *obj);
    MuibObject *selectedObject() const;
    void loadFieldsFrom(MuibObject *obj);
    void clearFieldsForNewEntry();
    void updateButtonStates();
    void updateFieldEnablement();
    QString makeUniqueLabel(const QString &base);
    // Applies every field (including the ID, see applyIdFieldTo()) to an
    // EXISTING object - Modify's own logic, factored out. Returns false
    // (having already shown the user a QMessageBox) and applies NOTHING
    // at all when the ID field holds an empty or already-taken value, so
    // "Aendern" is all-or-nothing, never a partial, silently-broken edit.
    bool applyFieldsTo(MuibObject *obj);
    // Validates and applies m_idEdit's current text as `obj`'s label
    // (used by both applyFieldsTo() for an existing object and onAdd()
    // for a brand-new one) - non-empty, a valid C-identifier shape (the
    // QLineEdit's own validator already restricts input to this, this is
    // the defensive server-side re-check), and unique against
    // m_existingLabels (excluding `obj`'s own current label, so leaving
    // the field unchanged is always accepted). On success, updates
    // m_existingLabels to replace the old label with the new one, if any
    // actually changed. Returns false (having shown a QMessageBox) on
    // any validation failure, in which case `obj` is left untouched.
    bool applyIdFieldTo(MuibObject *obj);
    // Refills m_idEdit with a live preview of what onAdd() would
    // currently assign as the new entry's default label - only while no
    // object is selected (i.e. actually in "new entry" mode) and only
    // when the user hasn't already hand-edited the field for the entry
    // about to be added (see m_idFieldUserEdited) - changing the Type
    // combo before clicking "Hinzufuegen" must not silently clobber an
    // ID the user already typed.
    void refreshIdPreviewForNewEntry();
    void buildActionChoices();
    void applyActionChoiceTo(MuibObject *obj);   // writes obj->notify from m_actionCombo, unless on the "preserve" sentinel

    // One selectable "Aktion" combo entry - see the class doc comment's
    // "Aktion" section. `isPreserveExisting` marks the one sentinel
    // choice that means "don't touch obj->notify at all", never a real,
    // applicable target/action pair.
    struct ActionChoice
    {
        QString label;
        QString targetLabel;          // "" only for "Keine"/the sentinel
        int srcType = 0;               // index into notifyEventsFor(ObjType::MenuItem)
        int destType = -1;             // index into notifyActionsFor(<target's real type>); -1 for "Keine"/sentinel
        bool isPreserveExisting = false;
    };

    MuibObject *m_window = nullptr;               // reference only (window title) - never mutated
    const MuibProject *m_project = nullptr;        // reference only, for buildActionChoices()'s Window enumeration
    std::unique_ptr<MuibObject> m_root;            // the working copy - an ObjType::Menu, or nullptr if starting empty
    QStringList m_existingLabels;                  // grows as new entries are added, for makeUniqueLabel()
    std::vector<ActionChoice> m_actionChoices;     // built once in the constructor; see buildActionChoices()
    // Snapshot of every m_root object's label AS OF CONSTRUCTION (i.e.
    // the real, currently-saved project label) - see labelRenames()'s
    // own comment. Keyed by object identity (not by label) so repeated
    // renames of the same entry within one dialog session are tracked
    // correctly no matter how many times the user changes their mind.
    QHash<const MuibObject *, QString> m_originalLabelOf;
    bool m_idFieldUserEdited = false;              // see refreshIdPreviewForNewEntry()'s own comment

    QTreeWidget *m_tree = nullptr;
    QLineEdit *m_nameEdit = nullptr;
    QLineEdit *m_idEdit = nullptr;                 // see the class doc comment's "ID (intern)" section
    QLineEdit *m_commKeyEdit = nullptr;
    QComboBox *m_typeCombo = nullptr;
    QCheckBox *m_menuBarCheck = nullptr;    // separator (name == "BarLabel")
    QCheckBox *m_checkCheck = nullptr;      // check_enable
    QCheckBox *m_toggleCheck = nullptr;     // toggleMenu
    QCheckBox *m_checkedCheck = nullptr;    // check_state
    QCheckBox *m_enabledCheck = nullptr;    // menu_enable
    QSpinBox *m_mutualGroupSpin = nullptr;  // excludeGroup
    QLabel *m_mutualGroupLabel = nullptr;
    QComboBox *m_actionCombo = nullptr;     // see ActionChoice/buildActionChoices()
    QLabel *m_actionLabel = nullptr;

    QPushButton *m_addButton = nullptr;
    QPushButton *m_modifyButton = nullptr;
    QPushButton *m_deleteButton = nullptr;
    QPushButton *m_moveUpButton = nullptr;
    QPushButton *m_moveDownButton = nullptr;
};
