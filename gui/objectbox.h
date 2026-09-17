// MuiBuilderQt GUI - one visual box on the Canvas representing a single
// MuibObject. A Group's box lays its children out with a real QHBoxLayout/
// QVBoxLayout (chosen from `horizontal`) containing one child ObjectBox
// per entry in `children` - so the canvas is a literal, recursively
// nested rendering of the object tree, not a separate diagram of it.
//
// Drops: every box accepts drops (setAcceptDrops(true) unconditionally),
// but what a given box actually accepts differs by role -
//   - a Group box accepts a new-object-from-palette drop (unchanged from
//     before) AND a reorder drop landing in its own empty padding/gaps;
//   - ANY box whose object is itself a child of some Group (i.e. has a
//     reorderable sibling set) accepts a reorder drop landing directly on
//     it, used to drop "next to this sibling" - the common case, since a
//     sibling's own box normally covers most of the space between gaps.
// Both reorder cases only fire when the dragged object's current father
// is that same Group (see objecttreeutil.h's moveObjectWithinGroup scope
// note - this first pass is deliberately "innerhalb einer Gruppe" only,
// no cross-group drag-moves).
//
// Drag source: any box whose object is a child of some Group is also a
// drag SOURCE for that same reorder MIME type, started from a mouse-press
// + move-past-threshold gesture (mousePressEvent still fires the existing
// immediate-select-on-click behaviour either way).
#pragma once

#include <QFrame>
#include "muibobject.h"

class QLabel;
class QBoxLayout;

class ObjectBox : public QFrame
{
    Q_OBJECT
public:
    explicit ObjectBox(MuibObject *obj, QWidget *parent = nullptr);

    MuibObject *object() const { return m_obj; }

    // Recursively applies the current selection to this box and every
    // descendant box (highlight border on the match, plain on everyone
    // else). Called from CanvasWidget after any click or tree-view
    // selection change.
    void applySelection(MuibObject *selected);

    // Rebuilds this box's header text (label/type/title-ish summary) -
    // called after a PropertyInspector edit changes a field that shows
    // up in the header (label, title/content, entries...), without
    // needing a full canvas rebuild.
    void refreshHeader();

    // refreshHeader() for this box and every descendant box.
    void refreshHeaderRecursive();

signals:
    void objectClicked(MuibObject *obj);
    void objectDropped(MuibObject *targetGroup, ObjType newType);
    // `movedObj` should end up as child index `newIndex` of `targetGroup`
    // (index counted against targetGroup's children as they stand right
    // now, i.e. still including movedObj if it's already one of them -
    // see objecttreeutil.h's moveObjectWithinGroup, which this is written
    // to feed directly).
    void objectReordered(MuibObject *movedObj, MuibObject *targetGroup, int newIndex);

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dragMoveEvent(QDragMoveEvent *event) override;
    void dragLeaveEvent(QDragLeaveEvent *event) override;
    void dropEvent(QDropEvent *event) override;
    void paintEvent(QPaintEvent *event) override;

private:
    QString headerText() const;
    // True if this box's object is a direct child of some Group, i.e. it
    // has siblings it could be reordered among (excludes a Window's root
    // Group, whose "father" is the Window, and the Window box itself).
    bool isReorderable() const;
    // Computes an insertion index (against `m_obj->children` as they
    // stand right now) for a reorder drop landing at `posInThis` (this
    // box's own local coordinates) - only meaningful when this box is a
    // Group. Walks the real child boxes in m_childLayout, comparing the
    // position along the group's layout direction to each child box's
    // midpoint.
    int insertionIndexForPos(const QPoint &posInThis) const;

    MuibObject *m_obj;
    QLabel *m_headerLabel = nullptr;
    QWidget *m_childContainer = nullptr;   // only for Group; holds nested ObjectBoxes
    QBoxLayout *m_childLayout = nullptr;
    QLabel *m_emptyHint = nullptr;         // "Drop widgets here" placeholder for an empty Group
    bool m_selected = false;
    bool m_dragHover = false;
    QPoint m_dragStartPos;                 // set on left mouse-press, used for the drag threshold
};
