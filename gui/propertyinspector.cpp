#include "propertyinspector.h"
#include "objectfactory.h"

#include <QFormLayout>
#include <QVBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QCheckBox>
#include <QComboBox>
#include <QSpinBox>
#include <QPlainTextEdit>
#include <QScrollArea>
#include <QFrame>
#include <QFocusEvent>
#include <QTimer>
#include <functional>

// A QPlainTextEdit that commits on focus-out (QPlainTextEdit has no
// built-in editingFinished(), unlike QLineEdit/QSpinBox) - used for the
// multi-line fields (Cycle/Radio entries, Help content) so a full
// PropertyInspector row-rebuild only ever happens once per edit, not
// once per keystroke. Local to this .cpp (see the "#include
// propertyinspector.moc" at the bottom) since nothing outside this file
// needs it.
class FocusOutPlainTextEdit : public QPlainTextEdit
{
    Q_OBJECT
public:
    using QPlainTextEdit::QPlainTextEdit;
signals:
    void editingFinished();
protected:
    void focusOutEvent(QFocusEvent *event) override
    {
        QPlainTextEdit::focusOutEvent(event);
        emit editingFinished();
    }
};

PropertyInspector::PropertyInspector(QWidget *parent)
    : QWidget(parent)
{
    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);

    m_titleLabel = new QLabel(this);
    m_titleLabel->setStyleSheet(QStringLiteral("font-weight: bold; padding: 6px;"));
    outer->addWidget(m_titleLabel);

    m_placeholder = new QLabel(tr("No object selected."), this);
    m_placeholder->setStyleSheet(QStringLiteral("color: #808080; font-style: italic; padding: 6px;"));
    outer->addWidget(m_placeholder);

    m_scroll = new QScrollArea(this);
    m_scroll->setWidgetResizable(true);
    m_scroll->setFrameShape(QFrame::NoFrame);
    m_formHost = new QWidget(m_scroll);
    m_form = new QFormLayout(m_formHost);
    m_form->setRowWrapPolicy(QFormLayout::WrapLongRows);
    m_scroll->setWidget(m_formHost);
    outer->addWidget(m_scroll, 1);

    showObject(nullptr);
}

void PropertyInspector::retranslate()
{
    m_placeholder->setText(tr("No object selected."));
    showObject(m_obj);   // rebuilds every row fresh - see the header's own doc comment
}

// --- small row-adding helpers -------------------------------------------
// Each takes the notify callback (a lambda the caller supplies that
// emits fieldChanged(obj, structural)) so these free functions never
// need to know about PropertyInspector/QObject signals themselves.

static void addStringRow(QFormLayout *form, const QString &label, const QString &initial,
                          bool structural, const std::function<void(const QString &)> &setter,
                          const std::function<void(bool)> &notify)
{
    auto *edit = new QLineEdit(initial);
    edit->setMinimumWidth(200);
    QObject::connect(edit, &QLineEdit::editingFinished, edit, [edit, setter, structural, notify]() {
        setter(edit->text());
        notify(structural);
    });
    form->addRow(label, edit);
}

static void addMultilineRow(QFormLayout *form, const QString &label, const QString &initial,
                             const std::function<void(const QString &)> &setter,
                             const std::function<void(bool)> &notify)
{
    auto *edit = new FocusOutPlainTextEdit(initial);
    edit->setFixedHeight(60);
    edit->setMinimumWidth(200);
    QObject::connect(edit, &FocusOutPlainTextEdit::editingFinished, edit, [edit, setter, notify]() {
        setter(edit->toPlainText());
        notify(false);
    });
    form->addRow(label, edit);
}

static void addStringListRow(QFormLayout *form, const QString &label, const QStringList &initial,
                              const std::function<void(const QStringList &)> &setter,
                              const std::function<void(bool)> &notify)
{
    auto *edit = new FocusOutPlainTextEdit(initial.join(QLatin1Char('\n')));
    edit->setFixedHeight(70);
    edit->setToolTip(QObject::tr("One entry per line"));
    QObject::connect(edit, &FocusOutPlainTextEdit::editingFinished, edit, [edit, setter, notify]() {
        setter(edit->toPlainText().split(QLatin1Char('\n'), Qt::SkipEmptyParts));
        notify(true);   // entry count can change -> header preview + (for Cycle/Radio) nothing structural on canvas, but tree/preview needs refresh; treated as structural-safe no-op for canvas since Cycle/Radio aren't Groups
    });
    form->addRow(label, edit);
}

static void addBoolRow(QFormLayout *form, const QString &label, bool initial,
                        bool structural, const std::function<void(bool)> &setter,
                        const std::function<void(bool)> &notify)
{
    auto *box = new QCheckBox;
    box->setChecked(initial);
    QObject::connect(box, &QCheckBox::toggled, box, [setter, structural, notify](bool checked) {
        setter(checked);
        notify(structural);
    });
    form->addRow(label, box);
}

static void addIntRow(QFormLayout *form, const QString &label, int initial, int lo, int hi,
                       const std::function<void(int)> &setter,
                       const std::function<void(bool)> &notify)
{
    auto *spin = new QSpinBox;
    spin->setRange(lo, hi);
    spin->setValue(initial);
    QObject::connect(spin, &QSpinBox::editingFinished, spin, [spin, setter, notify]() {
        setter(spin->value());
        notify(false);
    });
    form->addRow(label, spin);
}

// A fixed-choice dropdown - currently only used for Window's
// "Fensterposition" (WindowPositionMode), see the Window case below.
static void addComboRow(QFormLayout *form, const QString &label, const QStringList &options,
                         int initialIndex, const std::function<void(int)> &setter,
                         const std::function<void(bool)> &notify)
{
    auto *combo = new QComboBox;
    combo->addItems(options);
    combo->setCurrentIndex(initialIndex);
    QObject::connect(combo, QOverload<int>::of(&QComboBox::currentIndexChanged), combo,
                      [setter, notify](int index) {
        setter(index);
        notify(false);
    });
    form->addRow(label, combo);
}

static void addSectionLabel(QFormLayout *form, const QString &text)
{
    auto *lbl = new QLabel(QStringLiteral("<b>%1</b>").arg(text));
    form->addRow(lbl);
}

void PropertyInspector::showObject(MuibObject *obj)
{
    m_obj = obj;

    // Notify callback shared by every row this call creates - captures
    // `this` (the widget's own lifetime already bounds every row's
    // lambda, since rebuilding the form deletes the old rows/lambdas
    // together with their widgets) and `obj` (valid for as long as this
    // form is showing it; a fresh showObject() call is always what
    // invalidates it, which also tears down these very rows first).
    auto notify = [this, obj](bool structural) { emit fieldChanged(obj, structural); };

    // Tear down the previous form's rows. QFormLayout::takeAt() logs a
    // qWarning() on an out-of-range index rather than just returning
    // nullptr like QLayout's other implementations, so the loop
    // condition checks count() first instead of relying on a nullptr
    // return to stop.
    while (m_form->count() > 0)
    {
        QLayoutItem *child = m_form->takeAt(0);
        delete child->widget();
        delete child;
    }

    if (!obj)
    {
        m_titleLabel->setText(QString());
        m_placeholder->setVisible(true);
        m_scroll->setVisible(false);
        return;
    }
    m_placeholder->setVisible(false);
    m_scroll->setVisible(true);
    m_titleLabel->setText(displayNameForType(obj->type));

    addStringRow(m_form, tr("Label"), obj->label, false,
                 [obj](const QString &v) { obj->label = v; }, notify);

    switch (obj->type)
    {
    case ObjType::Window:
        addStringRow(m_form, tr("Title"), obj->title, false, [obj](const QString &v) { obj->title = v; }, notify);
        // MuiBuilderQt-only feature (see WindowPositionMode's own comment
        // in muibobject.h) - the original tool has no such field at all,
        // a window always opens at MUI's own default (centered) position.
        addComboRow(m_form, tr("Window Position"),
                    { tr("Centered (default)"), tr("Under mouse pointer"), tr("Manual (X/Y)") },
                    static_cast<int>(obj->posMode),
                    [this, obj](int idx) {
                        obj->posMode = static_cast<WindowPositionMode>(idx);
                        // Deferred: showObject() below tears down and
                        // rebuilds every row, which would delete the very
                        // QComboBox currently emitting this signal if
                        // called synchronously. QTimer::singleShot(0, ...)
                        // runs it right after this signal emission has
                        // fully returned instead, so the X/Y fields can
                        // appear/disappear immediately without a crash.
                        QTimer::singleShot(0, this, [this, obj]() { showObject(obj); });
                    },
                    notify);
        if (obj->posMode == WindowPositionMode::Manual)
        {
            addIntRow(m_form, tr("Position X"), obj->posX, 0, 4096, [obj](int v) { obj->posX = v; }, notify);
            addIntRow(m_form, tr("Position Y"), obj->posY, 0, 4096, [obj](int v) { obj->posY = v; }, notify);
        }
        // MuiBuilderQt-only feature (see the winWidth/... comment right
        // next to posMode in muibobject.h) - a user-reported gap ("das
        // Fenster bleibt initial sehr klein"): the original tool has no
        // such fields either, a window's size has always come purely
        // from MUI's own automatic layout. "0 = nicht gesetzt" follows
        // this same dialog family's existing convention (compare the
        // Menu-Editor's own "Mutual-Exclude-Gruppe (0 = keine)" spin box).
        addIntRow(m_form, tr("Width (0 = not set)"), obj->winWidth, 0, 100000, [obj](int v) { obj->winWidth = v; }, notify);
        addIntRow(m_form, tr("Height (0 = not set)"), obj->winHeight, 0, 100000, [obj](int v) { obj->winHeight = v; }, notify);
        addIntRow(m_form, tr("Min. Width (0 = not set)"), obj->winMinWidth, 0, 100000, [obj](int v) { obj->winMinWidth = v; }, notify);
        addIntRow(m_form, tr("Min. Height (0 = not set)"), obj->winMinHeight, 0, 100000, [obj](int v) { obj->winMinHeight = v; }, notify);
        addIntRow(m_form, tr("Max. Width (0 = not set)"), obj->winMaxWidth, 0, 100000, [obj](int v) { obj->winMaxWidth = v; }, notify);
        addIntRow(m_form, tr("Max. Height (0 = not set)"), obj->winMaxHeight, 0, 100000, [obj](int v) { obj->winMaxHeight = v; }, notify);
        addBoolRow(m_form, tr("Draggable (title bar)"), obj->dragbar, false, [obj](bool v) { obj->dragbar = v; }, notify);
        addBoolRow(m_form, tr("CloseGadget"), obj->closegadget, false, [obj](bool v) { obj->closegadget = v; }, notify);
        addBoolRow(m_form, tr("DepthGadget"), obj->depthgadget, false, [obj](bool v) { obj->depthgadget = v; }, notify);
        addBoolRow(m_form, tr("SizeGadget"), obj->sizegadget, false, [obj](bool v) { obj->sizegadget = v; }, notify);
        addBoolRow(m_form, tr("Backdrop"), obj->backdrop, false, [obj](bool v) { obj->backdrop = v; }, notify);
        addBoolRow(m_form, tr("Borderless"), obj->borderless, false, [obj](bool v) { obj->borderless = v; }, notify);
        addBoolRow(m_form, tr("Initially open"), obj->initopen, false, [obj](bool v) { obj->initopen = v; }, notify);
        break;

    case ObjType::Group:
        addBoolRow(m_form, tr("Horizontal"), obj->horizontal, true, [obj](bool v) { obj->horizontal = v; }, notify);
        addBoolRow(m_form, tr("Equal Width"), obj->samewidth, false, [obj](bool v) { obj->samewidth = v; }, notify);
        addBoolRow(m_form, tr("Equal Height"), obj->sameheight, false, [obj](bool v) { obj->sameheight = v; }, notify);
        addBoolRow(m_form, tr("Equal Size"), obj->samesize, false, [obj](bool v) { obj->samesize = v; }, notify);
        addBoolRow(m_form, tr("Virtual (scrollbar)"), obj->isVirtual, false, [obj](bool v) { obj->isVirtual = v; }, notify);
        break;

    case ObjType::Button:
        addStringRow(m_form, tr("Label Text"), obj->title, false, [obj](const QString &v) { obj->title = v; }, notify);
        break;

    case ObjType::Label:
        addStringRow(m_form, tr("Text"), obj->title, false, [obj](const QString &v) { obj->title = v; }, notify);
        break;

    case ObjType::Text:
        addMultilineRow(m_form, tr("Content"), obj->content, [obj](const QString &v) { obj->content = v; }, notify);
        addStringRow(m_form, tr("Preparse"), obj->preparse, false, [obj](const QString &v) { obj->preparse = v; }, notify);
        break;

    case ObjType::String:
        addStringRow(m_form, tr("Default Text"), obj->title, false, [obj](const QString &v) { obj->title = v; }, notify);
        addIntRow(m_form, tr("Max. Length"), obj->maxlen, 0, 32767, [obj](int v) { obj->maxlen = v; }, notify);
        addBoolRow(m_form, tr("Numeric Only"), obj->integer, false, [obj](bool v) { obj->integer = v; }, notify);
        addBoolRow(m_form, tr("Password Field (secret)"), obj->secret, false, [obj](bool v) { obj->secret = v; }, notify);
        break;

    case ObjType::Check:
        // The setter also mirrors title_exist (real original field,
        // core/muibobject.h's MuibObject::title_exist, loaded from real
        // .MUIB files by loadCheck() - core/muibloader.cpp) from whether
        // there's any text at all: muicodegen.cpp's emitObject() only
        // wraps the checkbox with its visible label (the real
        // Label2()/KeyLabel2()-based two-column group, see there) when
        // title_exist is set, exactly like the original tool's own
        // check.c dialog (an empty title there means no label - the
        // simplest faithful mapping, without a separate, easy-to-forget
        // "has label" checkbox in this form).
        addStringRow(m_form, tr("Label Text"), obj->title, false,
                     [obj](const QString &v) { obj->title = v; obj->title_exist = !v.isEmpty(); }, notify);
        addBoolRow(m_form, tr("Initial State"), obj->init_state, false, [obj](bool v) { obj->init_state = v; }, notify);
        break;

    case ObjType::Cycle:
    case ObjType::Radio:
        addStringListRow(m_form, tr("Entries"), obj->entries, [obj](const QStringList &v) { obj->entries = v; }, notify);
        break;

    case ObjType::Slider:
        addIntRow(m_form, tr("Minimum"), obj->sliderMin, -1000000, 1000000, [obj](int v) { obj->sliderMin = v; }, notify);
        addIntRow(m_form, tr("Maximum"), obj->sliderMax, -1000000, 1000000, [obj](int v) { obj->sliderMax = v; }, notify);
        addIntRow(m_form, tr("Initial Value"), obj->init, -1000000, 1000000, [obj](int v) { obj->init = v; }, notify);
        addBoolRow(m_form, tr("Horizontal"), obj->horizontal, false, [obj](bool v) { obj->horizontal = v; }, notify);
        addBoolRow(m_form, tr("Reversed (reverse)"), obj->reverse, false, [obj](bool v) { obj->reverse = v; }, notify);
        break;

    case ObjType::Scale:
        addBoolRow(m_form, tr("Horizontal"), obj->scaleHoriz, false, [obj](bool v) { obj->scaleHoriz = v; }, notify);
        break;

    case ObjType::Gauge:
        addIntRow(m_form, tr("Maximum"), obj->gaugeMax, 0, 1000000, [obj](int v) { obj->gaugeMax = v; }, notify);
        addStringRow(m_form, tr("Info Text"), obj->infotext, false, [obj](const QString &v) { obj->infotext = v; }, notify);
        addBoolRow(m_form, tr("Horizontal"), obj->horizontal, false, [obj](bool v) { obj->horizontal = v; }, notify);
        break;

    case ObjType::Prop:
        addIntRow(m_form, tr("Entries"), obj->propEntries, 0, 1000000, [obj](int v) { obj->propEntries = v; }, notify);
        addIntRow(m_form, tr("First"), obj->first, 0, 1000000, [obj](int v) { obj->first = v; }, notify);
        addIntRow(m_form, tr("Visible"), obj->visible, 0, 1000000, [obj](int v) { obj->visible = v; }, notify);
        break;

    case ObjType::Image:
        addStringRow(m_form, tr("Spec (Image Path/Name)"), obj->spec, false, [obj](const QString &v) { obj->spec = v; }, notify);
        addIntRow(m_form, tr("Width"), obj->width, 0, 100000, [obj](int v) { obj->width = v; }, notify);
        addIntRow(m_form, tr("Height"), obj->height, 0, 100000, [obj](int v) { obj->height = v; }, notify);
        break;

    case ObjType::Rectangle:
        addIntRow(m_form, tr("Rect Type"), obj->rectType, 0, 100, [obj](int v) { obj->rectType = v; }, notify);
        break;

    case ObjType::ColorField:
        addIntRow(m_form, tr("Red"), static_cast<int>(obj->red), 0, 65535, [obj](int v) { obj->red = static_cast<unsigned int>(v); }, notify);
        addIntRow(m_form, tr("Green"), static_cast<int>(obj->green), 0, 65535, [obj](int v) { obj->green = static_cast<unsigned int>(v); }, notify);
        addIntRow(m_form, tr("Blue"), static_cast<int>(obj->blue), 0, 65535, [obj](int v) { obj->blue = static_cast<unsigned int>(v); }, notify);
        break;

    case ObjType::ListView:
        addStringRow(m_form, tr("Format"), obj->format, false, [obj](const QString &v) { obj->format = v; }, notify);
        addBoolRow(m_form, tr("Multi-Selection"), obj->multiselect, false, [obj](bool v) { obj->multiselect = v; }, notify);
        // Real MUIA_Listview_DoubleClick toggle (same field/semantics as
        // the original MUIBuilder tool's own ListView editor dialog,
        // "Doppelklick" there too - see code.c/listview.c) - MuiBuilderQt
        // was already loading/saving this field from the .MUIB file
        // format, and the code generator now emits/wires it (see
        // muicodegen.cpp's ObjType::ListView cases), but there was no way
        // to actually turn it ON for a gadget built here from scratch
        // until now.
        addBoolRow(m_form, tr("Double-Click (enables notification)"), obj->doubleclick, false, [obj](bool v) { obj->doubleclick = v; }, notify);
        break;

    case ObjType::DirList:
        addStringRow(m_form, tr("Directory"), obj->directory, false, [obj](const QString &v) { obj->directory = v; }, notify);
        addBoolRow(m_form, tr("Drawers"), obj->drawers, false, [obj](bool v) { obj->drawers = v; }, notify);
        addBoolRow(m_form, tr("Files"), obj->files, false, [obj](bool v) { obj->files = v; }, notify);
        break;

    case ObjType::Space:
        addIntRow(m_form, tr("Space Type"), obj->spaceType, 0, 100, [obj](int v) { obj->spaceType = v; }, notify);
        addIntRow(m_form, tr("Spacing"), obj->spacing, 0, 1000, [obj](int v) { obj->spacing = v; }, notify);
        break;

    case ObjType::PopAsl:
        addIntRow(m_form, tr("Pop Type"), obj->popType, 0, 100, [obj](int v) { obj->popType = v; }, notify);
        break;

    case ObjType::PopObject:
        addStringRow(m_form, tr("Open-Hook"), obj->openhook, false, [obj](const QString &v) { obj->openhook = v; }, notify);
        addStringRow(m_form, tr("Close-Hook"), obj->closehook, false, [obj](const QString &v) { obj->closehook = v; }, notify);
        break;

    case ObjType::Menu:
    case ObjType::SubMenu:
    case ObjType::MenuItem:
        // The whole Menu family (root menu strip, pulldown/flyout titles,
        // leaf items, separators) is edited exclusively through the
        // dedicated MenuEditorDialog now (Datei > Menue bearbeiten... -
        // see menueditordialog.h) rather than here or via a right-click
        // context menu (both existed at various earlier points in this
        // project - this inspector's own field rows for SubMenu/MenuItem
        // were the last remnant, removed once that dialog covered
        // everything they did, including a "Mutual-Exclude-Gruppe" field
        // they never had). Selecting a Menu-family object in the
        // Projektbaum still works (for browsing/AboutBox-linking
        // purposes), it just no longer opens an edit form here.
        {
            auto *hint = new QLabel(tr("Menus are edited via \"File > Edit\n"
                                        "Menu...\" (with a window\n"
                                        "selected)."));
            hint->setStyleSheet(QStringLiteral("color: #808080; font-style: italic;"));
            m_form->addRow(hint);
        }
        break;

    default:
        break;
    }

    if (typeHasAreaAttrs(obj->type))
    {
        addSectionLabel(m_form, tr("Area"));
        addBoolRow(m_form, tr("Hide"), obj->area.Hide, false, [obj](bool v) { obj->area.Hide = v; }, notify);
        addBoolRow(m_form, tr("Disable"), obj->area.Disable, false, [obj](bool v) { obj->area.Disable = v; }, notify);
        addIntRow(m_form, tr("Weight"), obj->area.Weight, 0, 1000, [obj](int v) { obj->area.Weight = v; }, notify);
        addIntRow(m_form, tr("Frame"), obj->area.Frame, 0, 100, [obj](int v) { obj->area.Frame = v; }, notify);
        addIntRow(m_form, tr("Background"), static_cast<int>(obj->area.Background), 0, 0xFFFFFF,
                  [obj](int v) { obj->area.Background = static_cast<unsigned int>(v); }, notify);
        addStringRow(m_form, tr("Frame Title"), obj->area.TitleFrame, false,
                     [obj](const QString &v) { obj->area.TitleFrame = v; }, notify);
    }

    addSectionLabel(m_form, tr("Help"));
    addStringRow(m_form, tr("Help Title"), obj->help.title, false, [obj](const QString &v) { obj->help.title = v; }, notify);
    addMultilineRow(m_form, tr("Help Text"), QString::fromUtf8(obj->help.content),
                     [obj](const QString &v) { obj->help.content = v.toUtf8(); obj->help.nb_char = obj->help.content.size(); },
                     notify);
}

#include "propertyinspector.moc"
