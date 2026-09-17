#include "objectbox.h"
#include "objectfactory.h"

#include <QLabel>
#include <QBoxLayout>
#include <QMouseEvent>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDragLeaveEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QPainter>
#include <QDrag>
#include <QApplication>
#include <cstring>
#include <algorithm>

// Packs/unpacks a MuibObject* as the payload of a kMuibObjectReorderMime
// drag - valid only within this same process (see objectfactory.h), which
// is always the case here since the drag both starts and ends inside this
// one running MuiBuilderQt instance's own in-memory MuibProject tree.
static QByteArray encodeReorderPayload(MuibObject *obj)
{
    QByteArray data(sizeof(MuibObject *), Qt::Uninitialized);
    std::memcpy(data.data(), &obj, sizeof(obj));
    return data;
}

static MuibObject *decodeReorderPayload(const QMimeData *mime)
{
    const QByteArray data = mime->data(QLatin1String(kMuibObjectReorderMime));
    if (data.size() != static_cast<int>(sizeof(MuibObject *)))
        return nullptr;
    MuibObject *obj = nullptr;
    std::memcpy(&obj, data.constData(), sizeof(obj));
    return obj;
}

// One background/border colour per palette category, applied to every
// box of a type in that category - lets the canvas read at a glance
// (layout containers vs. interactive controls vs. display-only vs.
// popups) without having to read every header label. Deliberately muted/
// pastel so the (dark-on-light by default, Qt Fusion default palette)
// header text stays legible on top.
static QColor colorForType(ObjType type)
{
    if (type == ObjType::Window)
        return QColor("#f0c987");   // window header - warm amber, stands out from every child

    for (const auto &e : paletteEntries())
    {
        if (e.type != type)
            continue;
        if (e.category == QLatin1String("Layout"))
            return QColor("#d8d8d8");
        if (e.category == QLatin1String("Steuerelemente"))
            return QColor("#bcd8f2");
        if (e.category == QLatin1String("Anzeige"))
            return QColor("#c7e8c0");
        if (e.category == QLatin1String("Popup"))
            return QColor("#dcc7ec");
    }
    return QColor("#e0e0e0");
}

ObjectBox::ObjectBox(MuibObject *obj, QWidget *parent)
    : QFrame(parent), m_obj(obj)
{
    setFrameShape(QFrame::Box);
    setLineWidth(1);
    setAutoFillBackground(true);
    // Every box is a potential reorder-drop target (see the class-level
    // comment in objectbox.h); dragEnterEvent() below still decides
    // per-box, per-MIME-type whether a given drag is actually welcome.
    setAcceptDrops(true);

    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(6, 4, 6, 6);
    outer->setSpacing(4);

    // Bug report (Chefentwickler, with Dark-theme screenshots): the
    // canvas became hard to read under the dark synthetic themes. Root
    // cause: colorForType() below always returns a light pastel colour
    // (by design - see its own comment - these mimic a light, Amiga-
    // style mockup and are meant to stay readable regardless of the
    // app's own chrome theme), but the header text's colour was left to
    // simple QPalette inheritance, which normally would pick up the dark
    // text this box's own QPalette::Window override implies - EXCEPT once
    // any ancestor widget (here: MainWindow, for 3 of the 4 synthetic
    // themes' own stylesheets - see applyApplicationStyle()) has an
    // active stylesheet, Qt's CSS engine resolves an unstyled label's
    // "inherited" colour from the application-wide palette instead of the
    // immediate parent's runtime palette - e.g. Dark's light #d4d4d4.
    // Light text on a light pastel background is exactly the "hard to
    // read" reported. Baking the colour directly into m_headerLabel's OWN
    // stylesheet below sidesteps that ambiguity entirely, independent of
    // whatever stylesheet state any ancestor happens to be in.
    m_headerLabel = new QLabel(this);
    m_headerLabel->setStyleSheet(QStringLiteral("font-weight: bold; color: #202020;"));
    outer->addWidget(m_headerLabel);
    refreshHeader();

    QPalette pal = palette();
    pal.setColor(QPalette::Window, colorForType(m_obj->type));
    pal.setColor(QPalette::WindowText, QColor("#202020"));   // defensive fallback for any future unstyled child text
    setPalette(pal);

    if (m_obj->type == ObjType::Group)
    {
        m_childContainer = new QWidget(this);
        m_childLayout = m_obj->horizontal
                            ? static_cast<QBoxLayout *>(new QHBoxLayout(m_childContainer))
                            : static_cast<QBoxLayout *>(new QVBoxLayout(m_childContainer));
        m_childLayout->setContentsMargins(4, 4, 4, 4);
        m_childLayout->setSpacing(4);
        outer->addWidget(m_childContainer);

        m_emptyHint = new QLabel(tr("Drag widgets from the palette here"), m_childContainer);
        m_emptyHint->setStyleSheet(QStringLiteral("color: #808080; font-style: italic;"));
        m_childLayout->addWidget(m_emptyHint);

        for (auto &child : m_obj->children)
        {
            auto *box = new ObjectBox(child.get(), m_childContainer);
            connect(box, &ObjectBox::objectClicked, this, &ObjectBox::objectClicked);
            connect(box, &ObjectBox::objectDropped, this, &ObjectBox::objectDropped);
            connect(box, &ObjectBox::objectReordered, this, &ObjectBox::objectReordered);
            m_childLayout->addWidget(box);
        }
        m_emptyHint->setVisible(m_obj->children.empty());
    }
    else if (m_obj->type == ObjType::Window)
    {
        // A Window box never accepts drops itself (setAcceptDrops stays
        // false) - it just nests its mandatory root Group's own box,
        // which does. This lets CanvasWidget treat "the whole canvas" as
        // a single top-level ObjectBox for the Window, reusing every bit
        // of the recursive rendering/selection/drop logic below instead
        // of duplicating it for the outermost level.
        m_childContainer = new QWidget(this);
        m_childLayout = new QVBoxLayout(m_childContainer);
        m_childLayout->setContentsMargins(4, 4, 4, 4);
        outer->addWidget(m_childContainer);

        if (m_obj->root)
        {
            auto *rootBox = new ObjectBox(m_obj->root.get(), m_childContainer);
            connect(rootBox, &ObjectBox::objectClicked, this, &ObjectBox::objectClicked);
            connect(rootBox, &ObjectBox::objectDropped, this, &ObjectBox::objectDropped);
            connect(rootBox, &ObjectBox::objectReordered, this, &ObjectBox::objectReordered);
            m_childLayout->addWidget(rootBox);
        }
    }
}

QString ObjectBox::headerText() const
{
    const QString typeName = displayNameForType(m_obj->type);

    // A short, type-appropriate "content preview" appended after the
    // label, so a glance at the canvas already shows what a Button says
    // or what a String's current value is, without opening the inspector.
    QString preview;
    switch (m_obj->type)
    {
    case ObjType::Window:   preview = m_obj->title; break;
    case ObjType::Button:   preview = m_obj->title; break;
    case ObjType::Label:    preview = m_obj->title; break;
    case ObjType::Check:    preview = m_obj->title; break;
    case ObjType::Text:     preview = m_obj->content; break;
    case ObjType::String:   preview = m_obj->title; break;
    default: break;
    }

    QString text = QStringLiteral("%1: %2").arg(typeName, m_obj->label.isEmpty() ? tr("(no label)") : m_obj->label);
    if (!preview.isEmpty())
        text += QStringLiteral("  — „%1“").arg(preview);
    return text;
}

void ObjectBox::refreshHeader()
{
    m_headerLabel->setText(headerText());
}

void ObjectBox::refreshHeaderRecursive()
{
    refreshHeader();
    if (m_childLayout)
    {
        for (int i = 0; i < m_childLayout->count(); ++i)
        {
            if (auto *box = qobject_cast<ObjectBox *>(m_childLayout->itemAt(i)->widget()))
                box->refreshHeaderRecursive();
        }
    }
}

void ObjectBox::applySelection(MuibObject *selected)
{
    m_selected = (m_obj == selected);
    setFrameShadow(m_selected ? QFrame::Sunken : QFrame::Raised);
    setLineWidth(m_selected ? 2 : 1);
    update();

    if (m_childLayout)
    {
        for (int i = 0; i < m_childLayout->count(); ++i)
        {
            if (auto *box = qobject_cast<ObjectBox *>(m_childLayout->itemAt(i)->widget()))
                box->applySelection(selected);
        }
    }
}

void ObjectBox::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton)
        m_dragStartPos = event->pos();
    emit objectClicked(m_obj);
    event->accept();
    // Deliberately NOT calling QFrame::mousePressEvent()/propagating up:
    // a click anywhere inside a box (including inside its child area's
    // empty padding, outside any nested child box) should select THIS
    // object, not bubble up to whichever ancestor group happens to be
    // the parent widget.
}

bool ObjectBox::isReorderable() const
{
    return m_obj->father && m_obj->father->type == ObjType::Group;
}

void ObjectBox::mouseMoveEvent(QMouseEvent *event)
{
    if (!(event->buttons() & Qt::LeftButton) || !isReorderable())
        return;
    if ((event->pos() - m_dragStartPos).manhattanLength() < QApplication::startDragDistance())
        return;

    auto *drag = new QDrag(this);
    auto *mime = new QMimeData;
    mime->setData(QLatin1String(kMuibObjectReorderMime), encodeReorderPayload(m_obj));
    drag->setMimeData(mime);
    if (!size().isEmpty())
        drag->setPixmap(grab().scaled(size() / 2, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    drag->exec(Qt::MoveAction);
}

int ObjectBox::insertionIndexForPos(const QPoint &posInThis) const
{
    if (!m_childLayout || !m_childContainer)
        return 0;
    const QPoint posInContainer = m_childContainer->mapFrom(const_cast<ObjectBox *>(this), posInThis);
    const bool horiz = m_obj->horizontal;
    int index = 0;
    for (int i = 0; i < m_childLayout->count(); ++i)
    {
        auto *box = qobject_cast<ObjectBox *>(m_childLayout->itemAt(i)->widget());
        if (!box)
            continue;   // skip the "drop widgets here" empty-hint label
        const QRect g = box->geometry();
        const int mid = horiz ? g.center().x() : g.center().y();
        const int pos = horiz ? posInContainer.x() : posInContainer.y();
        if (pos < mid)
            break;
        ++index;
    }
    return index;
}

void ObjectBox::dragEnterEvent(QDragEnterEvent *event)
{
    const QMimeData *mime = event->mimeData();
    if (m_obj->type == ObjType::Group && mime->hasFormat(QLatin1String(kMuibObjectTypeMime)))
    {
        m_dragHover = true;
        setFrameShadow(QFrame::Sunken);
        event->acceptProposedAction();
        return;
    }
    if (mime->hasFormat(QLatin1String(kMuibObjectReorderMime)))
    {
        MuibObject *dragged = decodeReorderPayload(mime);
        // Same-group reorder only (see objectbox.h's class comment): a
        // Group box accepts it if the dragged object is one of ITS OWN
        // children; any other box accepts it if it shares the same
        // father as the dragged object (dropping "next to" a sibling).
        const bool onOwnGroup = (m_obj->type == ObjType::Group && dragged && dragged->father == m_obj);
        const bool onSibling = (dragged && dragged->father == m_obj->father && isReorderable());
        if (onOwnGroup || onSibling)
        {
            m_dragHover = true;
            setFrameShadow(QFrame::Sunken);
            event->acceptProposedAction();
        }
    }
}

void ObjectBox::dragMoveEvent(QDragMoveEvent *event)
{
    const QMimeData *mime = event->mimeData();
    if (m_obj->type == ObjType::Group && mime->hasFormat(QLatin1String(kMuibObjectTypeMime)))
    {
        event->acceptProposedAction();
        return;
    }
    if (mime->hasFormat(QLatin1String(kMuibObjectReorderMime)))
    {
        MuibObject *dragged = decodeReorderPayload(mime);
        const bool onOwnGroup = (m_obj->type == ObjType::Group && dragged && dragged->father == m_obj);
        const bool onSibling = (dragged && dragged->father == m_obj->father && isReorderable());
        if (onOwnGroup || onSibling)
            event->acceptProposedAction();
    }
}

void ObjectBox::dragLeaveEvent(QDragLeaveEvent *)
{
    m_dragHover = false;
    setFrameShadow(m_selected ? QFrame::Sunken : QFrame::Raised);
}

void ObjectBox::dropEvent(QDropEvent *event)
{
    m_dragHover = false;
    setFrameShadow(m_selected ? QFrame::Sunken : QFrame::Raised);

    const QMimeData *mime = event->mimeData();

    if (m_obj->type == ObjType::Group && mime->hasFormat(QLatin1String(kMuibObjectTypeMime)))
    {
        const QByteArray payload = mime->data(QLatin1String(kMuibObjectTypeMime));
        bool ok = false;
        const int typeInt = payload.toInt(&ok);
        if (ok)
        {
            emit objectDropped(m_obj, static_cast<ObjType>(typeInt));
            event->acceptProposedAction();
        }
        return;
    }

    if (mime->hasFormat(QLatin1String(kMuibObjectReorderMime)))
    {
        MuibObject *dragged = decodeReorderPayload(mime);
        if (!dragged)
            return;

        if (m_obj->type == ObjType::Group && dragged->father == m_obj)
        {
            // Dropped in this Group's own empty padding/gaps - compute
            // the index from the cursor's position among the real child
            // boxes (skips the empty-hint label).
            const int index = insertionIndexForPos(event->position().toPoint());
            emit objectReordered(dragged, m_obj, index);
            event->acceptProposedAction();
        }
        else if (dragged->father == m_obj->father && isReorderable())
        {
            // Dropped directly on a sibling box - before/after it,
            // depending on which half of ITS OWN rect the cursor is in
            // (along the shared parent Group's layout direction).
            MuibObject *parent = m_obj->father;
            auto &siblings = parent->children;
            const auto it = std::find_if(siblings.begin(), siblings.end(),
                                          [this](const std::unique_ptr<MuibObject> &c) { return c.get() == m_obj; });
            if (it != siblings.end())
            {
                const int ownIndex = static_cast<int>(std::distance(siblings.begin(), it));
                const bool horiz = parent->horizontal;
                const QPoint pos = event->position().toPoint();
                const int mid = horiz ? width() / 2 : height() / 2;
                const int cursorPos = horiz ? pos.x() : pos.y();
                const int index = ownIndex + (cursorPos < mid ? 0 : 1);
                emit objectReordered(dragged, parent, index);
                event->acceptProposedAction();
            }
        }
    }
}

void ObjectBox::paintEvent(QPaintEvent *event)
{
    QFrame::paintEvent(event);
    if (m_selected)
    {
        QPainter p(this);
        QPen pen(QColor("#1a5fb4"));
        pen.setWidth(2);
        p.setPen(pen);
        p.drawRect(rect().adjusted(1, 1, -1, -1));
    }
}
