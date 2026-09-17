// MuiBuilderQt GUI - hierarchical read-through view of the whole project
// (every window, each one's root-group widget tree, AND its menu strip
// if it has one). The Canvas only visualizes/edits the widget-layout
// part (see objectfactory.h's scope note on Menu family); a window's
// menu structure itself is edited via the dedicated MenuEditorDialog
// (gui/menueditordialog.h, opened from MainWindow's "Menue bearbeiten..."
// action) - this tree shows the menu subtree read-only, for browsing/
// selection only. It previously also offered a right-click context menu
// for structural menu edits (add/move/delete); that was fully replaced
// by MenuEditorDialog at the Chefentwickler's own request once it
// existed, so this view no longer mutates the project at all.
#pragma once

#include <QTreeWidget>
#include "muibobject.h"

class ObjectTreeView : public QTreeWidget
{
    Q_OBJECT
public:
    explicit ObjectTreeView(QWidget *parent = nullptr);

    // Rebuilds the whole tree from `proj` (call after New/Open/window
    // add/remove/object add/remove - any structural change).
    void setProject(MuibProject *proj);

    // Programmatically highlights the item for `obj` (call after the
    // Canvas's selection changes) without re-emitting objectSelected().
    void selectObject(MuibObject *obj);

signals:
    void objectSelected(MuibObject *obj);
    // Emitted when the user picks a different WINDOW item - MainWindow
    // switches the Canvas to show it.
    void windowActivated(MuibObject *win);

private slots:
    void onCurrentItemChanged(QTreeWidgetItem *current, QTreeWidgetItem *previous);

private:
    QTreeWidgetItem *addObjectItem(QTreeWidgetItem *parent, MuibObject *obj);
    QTreeWidgetItem *findItemFor(MuibObject *obj, QTreeWidgetItem *from = nullptr) const;

    MuibProject *m_project = nullptr;
    bool m_suppressSelectionSignal = false;
};
