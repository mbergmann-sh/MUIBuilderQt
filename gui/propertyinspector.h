// MuiBuilderQt GUI - the right-hand dock showing/editing the fields of
// the currently selected MuibObject.
//
// Scope decision (see also objectfactory.h and README.md): given the
// "kitchen sink" MuibObject class (every type's fields as plain members,
// see core/muibobject.h's own design-decision comment), this inspector
// does NOT expose every single field of all 24 types - that would be a
// huge surface for marginal benefit in a first interactive-GUI pass. It
// exposes label + the fields that meaningfully define each type's
// appearance/behaviour (e.g. Button/Label/Check's title text, String's
// max length, Cycle/Radio's entries, Slider/Gauge's min/max, Group's
// layout direction, ...) plus the common AreaAttrs (Hide/Disable/Weight/
// Background/Frame) for types that have them, plus the Help title/text.
// Less common fields (ListView's hook function names, DirList's filter
// settings, PopAsl/PopObject's hooks, notifications themselves) are
// intentionally left to a follow-up pass - loading/saving them is
// already lossless (MuibLoader/MuibSaver round-trip every field
// regardless of what this inspector shows), only hand-editing them is
// not yet possible from the GUI.
#pragma once

#include <QWidget>
#include "muibobject.h"

class QFormLayout;
class QLabel;
class QScrollArea;

class PropertyInspector : public QWidget
{
    Q_OBJECT
public:
    explicit PropertyInspector(QWidget *parent = nullptr);

    // Shows the editable fields for `obj` (nullptr shows a "nothing
    // selected" placeholder). Always fully rebuilds the form - simplest
    // correct approach since the set of fields differs per ObjType.
    void showObject(MuibObject *obj);

    // Re-applies every tr()-wrapped string on the CURRENTLY shown form
    // after a runtime GUI-language change - unlike ObjectTreeView/
    // ObjectBox, this inspector's rows are only rebuilt when the
    // selection changes (showObject()), not on every language switch, so
    // without this the field labels ("Label"/"Width"/...) of whatever is
    // selected right now would keep showing the old language until the
    // user clicks a different object. Mirrors WidgetPalette::retranslate()
    // (see its own doc comment) - the placeholder text needs the same
    // treatment since it too is normally set once and never revisited.
    // Called from MainWindow::retranslateUi().
    void retranslate();

signals:
    // Emitted after any field edit is committed (line edit losing focus
    // or Enter, checkbox toggled, spin box losing focus...). `structural`
    // is true only for edits that change the Canvas's box NESTING/LAYOUT
    // (currently: Group's "Horizontal" flag) and therefore need a full
    // CanvasWidget::rebuild() rather than just refreshHeaders().
    void fieldChanged(MuibObject *obj, bool structural);

private:
    MuibObject *m_obj = nullptr;
    QScrollArea *m_scroll = nullptr;
    QWidget *m_formHost = nullptr;
    QFormLayout *m_form = nullptr;
    QLabel *m_titleLabel = nullptr;
    QLabel *m_placeholder = nullptr;
};
