// MuiBuilderQt GUI - the central editing surface. Shows one Window's
// object tree as nested ObjectBoxes (see objectbox.h) inside a
// QScrollArea, and re-emits selection/drop events from wherever in that
// tree they occurred so MainWindow doesn't need to know about ObjectBox
// at all.
#pragma once

#include <QScrollArea>
#include "muibobject.h"

class ObjectBox;
class QLabel;
class QVBoxLayout;

class CanvasWidget : public QScrollArea
{
    Q_OBJECT
public:
    explicit CanvasWidget(QWidget *parent = nullptr);

    // Switches the canvas to show `win` (may be nullptr to show the
    // "no window" placeholder, e.g. right after project creation/load
    // if it has zero windows, or after the last window was removed).
    // Always tears down and rebuilds every ObjectBox - simplest correct
    // approach given the tree sizes involved (a MUI dialog's widget
    // count, not thousands of nodes) and the only strategy that can't
    // drift out of sync with the underlying MuibObject tree.
    void setWindow(MuibObject *win);
    MuibObject *currentWindow() const { return m_window; }

    // Re-runs the full rebuild for the currently-set window (call after
    // any structural change: add/delete object, or Group horizontal
    // flag flipped so its layout direction needs to change). Preserves
    // the current selection if that object still exists in the rebuilt
    // tree, otherwise falls back to selecting the window itself.
    void rebuild();

    // Updates the highlight only, no rebuild - call after a
    // non-structural PropertyInspector edit, or after selection changed
    // via the ObjectTreeView.
    void setSelected(MuibObject *obj);
    MuibObject *selected() const { return m_selected; }

    // Updates which object counts as "selected" WITHOUT touching any
    // ObjectBox widget or emitting selectionChanged() - for the one case
    // where that matters: MainWindow must repoint the selection away
    // from an object it is about to delete (so this canvas never holds a
    // dangling MuibObject* even momentarily) before the doomed object's
    // ObjectBox widget is torn down by the rebuild() that follows.
    void setSelectedSilently(MuibObject *obj) { m_selected = obj; }

    // Refreshes just the header text of every box (label/title/content
    // previews) without a full rebuild - call after a PropertyInspector
    // edit that doesn't change tree structure or layout direction.
    void refreshHeaders();

    // Re-applies the "no window open" placeholder's tr()-wrapped text
    // after a runtime GUI-language change - like WidgetPalette/
    // PropertyInspector (see their own doc comments), this label is
    // created once in the constructor and only ever toggled visible/
    // invisible afterwards (setWindow()/rebuild()), never recreated, so
    // without this it would keep showing the old language until the next
    // full application restart. ObjectBox's own labels need no such
    // handling - they're torn down and rebuilt fresh by rebuild() itself.
    // Called from MainWindow::retranslateUi().
    void retranslate();

signals:
    void selectionChanged(MuibObject *obj);
    void objectAdded(MuibObject *targetGroup, ObjType type);
    // Re-emitted from ObjectBox::objectReordered - see objecttreeutil.h's
    // moveObjectWithinGroup(), which this is meant to feed directly.
    void objectReordered(MuibObject *movedObj, MuibObject *targetGroup, int newIndex);

private:
    MuibObject *m_window = nullptr;
    MuibObject *m_selected = nullptr;

    QWidget *m_content = nullptr;
    QVBoxLayout *m_contentLayout = nullptr;
    ObjectBox *m_rootBox = nullptr;   // the single top-level box for m_window, or nullptr
    QLabel *m_placeholder = nullptr;
};
