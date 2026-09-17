#include "objecttreeview.h"
#include "objectfactory.h"

// MuibObject* stored/retrieved via a plain reinterpret_cast round-trip
// through qulonglong - avoids registering a custom QVariant metatype for
// what is always used strictly within one process's lifetime (never
// serialized), matching the pragmatic style already used elsewhere in
// this port (e.g. muicodegen.cpp's QHash<const MuibObject*, QString>).
static QVariant ptrToVariant(MuibObject *p)
{
    return QVariant::fromValue<qulonglong>(reinterpret_cast<qulonglong>(p));
}
static MuibObject *variantToPtr(const QVariant &v)
{
    return reinterpret_cast<MuibObject *>(v.toULongLong());
}

ObjectTreeView::ObjectTreeView(QWidget *parent)
    : QTreeWidget(parent)
{
    setHeaderHidden(true);
    setSelectionMode(QAbstractItemView::SingleSelection);
    connect(this, &QTreeWidget::currentItemChanged, this, &ObjectTreeView::onCurrentItemChanged);
}

QTreeWidgetItem *ObjectTreeView::addObjectItem(QTreeWidgetItem *parent, MuibObject *obj)
{
    auto *item = new QTreeWidgetItem(parent);
    QString text = QStringLiteral("%1: %2").arg(displayNameForType(obj->type),
                                                  obj->label.isEmpty() ? tr("(no label)") : obj->label);
    const bool isMenuFamily = (obj->type == ObjType::Menu || obj->type == ObjType::SubMenu || obj->type == ObjType::MenuItem);
    item->setText(0, text);
    item->setData(0, Qt::UserRole, ptrToVariant(obj));

    if (obj->type == ObjType::Group)
    {
        for (auto &c : obj->children)
            addObjectItem(item, c.get());
    }
    else if (isMenuFamily)
    {
        for (auto &c : obj->childs)
            addObjectItem(item, c.get());
    }
    return item;
}

void ObjectTreeView::setProject(MuibProject *proj)
{
    m_project = proj;
    m_suppressSelectionSignal = true;
    clear();

    if (m_project)
    {
        for (auto &win : m_project->windows)
        {
            auto *winItem = new QTreeWidgetItem(this);
            winItem->setText(0, QStringLiteral("%1: %2").arg(tr("Window"), win->label.isEmpty() ? win->title : win->label));
            winItem->setData(0, Qt::UserRole, ptrToVariant(win.get()));

            if (win->menu)
                addObjectItem(winItem, win->menu.get());
            if (win->root)
                addObjectItem(winItem, win->root.get());
        }
    }
    expandAll();
    m_suppressSelectionSignal = false;
}

QTreeWidgetItem *ObjectTreeView::findItemFor(MuibObject *obj, QTreeWidgetItem *from) const
{
    const int count = from ? from->childCount() : topLevelItemCount();
    for (int i = 0; i < count; ++i)
    {
        QTreeWidgetItem *item = from ? from->child(i) : topLevelItem(i);
        if (variantToPtr(item->data(0, Qt::UserRole)) == obj)
            return item;
        if (auto *found = findItemFor(obj, item))
            return found;
    }
    return nullptr;
}

void ObjectTreeView::selectObject(MuibObject *obj)
{
    m_suppressSelectionSignal = true;
    if (QTreeWidgetItem *item = obj ? findItemFor(obj) : nullptr)
        setCurrentItem(item);
    else
        clearSelection();
    m_suppressSelectionSignal = false;
}

void ObjectTreeView::onCurrentItemChanged(QTreeWidgetItem *current, QTreeWidgetItem *)
{
    if (m_suppressSelectionSignal || !current)
        return;

    MuibObject *obj = variantToPtr(current->data(0, Qt::UserRole));
    if (!obj)
        return;

    if (obj->type == ObjType::Window)
        emit windowActivated(obj);
    emit objectSelected(obj);
}
