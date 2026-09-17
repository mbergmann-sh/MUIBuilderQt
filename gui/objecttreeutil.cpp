#include "objecttreeutil.h"
#include <QtGlobal>
#include <algorithm>

MuibObject *appendChildToGroup(MuibObject *group, std::unique_ptr<MuibObject> child)
{
    Q_ASSERT(group && group->type == ObjType::Group);
    child->father = group;
    MuibObject *raw = child.get();
    group->children.push_back(std::move(child));
    return raw;
}

bool moveObjectWithinGroup(MuibObject *obj, MuibObject *newParent, int newIndex)
{
    if (!obj || !newParent || newParent->type != ObjType::Group)
        return false;
    MuibObject *oldParent = obj->father;
    if (!oldParent || oldParent->type != ObjType::Group)
        return false;

    auto &oldChildren = oldParent->children;
    auto it = std::find_if(oldChildren.begin(), oldChildren.end(),
                            [obj](const std::unique_ptr<MuibObject> &c) { return c.get() == obj; });
    if (it == oldChildren.end())
        return false;
    const int oldIndex = static_cast<int>(std::distance(oldChildren.begin(), it));

    // `newIndex` is expressed against the list as it stood BEFORE `obj`
    // is removed. When moving within the SAME group to a position past
    // its own original slot, removing `obj` first shifts every later
    // sibling down by one - compensate so the object still lands where
    // the caller (and the user, visually) intended.
    if (oldParent == newParent && newIndex > oldIndex)
        --newIndex;

    std::unique_ptr<MuibObject> owned = std::move(*it);
    oldChildren.erase(it);

    auto &newChildren = newParent->children;
    if (newIndex < 0)
        newIndex = 0;
    if (newIndex > static_cast<int>(newChildren.size()))
        newIndex = static_cast<int>(newChildren.size());

    owned->father = newParent;
    newChildren.insert(newChildren.begin() + newIndex, std::move(owned));
    return true;
}

// Recursively searches `children` (and nested groups' children) for
// `target`; on match, erases it from the vector it was found in and
// returns true.
static bool removeFromChildren(std::vector<std::unique_ptr<MuibObject>> &children, MuibObject *target)
{
    for (auto it = children.begin(); it != children.end(); ++it)
    {
        if (it->get() == target)
        {
            children.erase(it);
            return true;
        }
        if ((*it)->type == ObjType::Group)
        {
            if (removeFromChildren((*it)->children, target))
                return true;
        }
    }
    return false;
}

bool removeObjectFromProject(MuibProject *proj, MuibObject *target)
{
    if (!proj || !target)
        return false;

    // Top-level window?
    for (auto it = proj->windows.begin(); it != proj->windows.end(); ++it)
    {
        if (it->get() == target)
        {
            proj->windows.erase(it);
            return true;
        }
    }

    // Somewhere inside a window's root-group subtree?
    for (auto &win : proj->windows)
    {
        if (win->root && win->root.get() == target)
        {
            // Deleting the whole root group would leave the window with
            // no layout - not offered by the UI (root group boxes are
            // not individually removable, only their children are), but
            // guard against it defensively rather than crash.
            return false;
        }
        if (win->root && removeFromChildren(win->root->children, target))
            return true;
    }

    return false;
}

static void collectLabels(const MuibObject *obj, QStringList &out)
{
    if (!obj)
        return;
    if (!obj->label.isEmpty())
        out << obj->label;
    if (obj->root)
        collectLabels(obj->root.get(), out);
    if (obj->menu)
        collectLabels(obj->menu.get(), out);
    if (obj->popObj)
        collectLabels(obj->popObj.get(), out);
    for (const auto &c : obj->children)
        collectLabels(c.get(), out);
    for (const auto &c : obj->childs)
        collectLabels(c.get(), out);
}

QStringList collectAllLabels(const MuibProject *proj)
{
    QStringList out;
    if (!proj)
        return out;
    if (proj->appMenu)
        collectLabels(proj->appMenu.get(), out);
    for (const auto &w : proj->windows)
        collectLabels(w.get(), out);
    return out;
}

// Same tree shape as collectLabels() just above - mirrored deliberately
// rather than merged with it, since this one MUTATES (rewrites matching
// NotifyEvent::targetLabel entries in place) instead of collecting.
static void rewriteNotifyLabels(MuibObject *obj, const QHash<QString, QString> &renames)
{
    if (!obj)
        return;
    for (NotifyEvent &evt : obj->notify)
    {
        auto it = renames.constFind(evt.targetLabel);
        if (it != renames.constEnd())
            evt.targetLabel = it.value();
    }
    if (obj->root)
        rewriteNotifyLabels(obj->root.get(), renames);
    if (obj->menu)
        rewriteNotifyLabels(obj->menu.get(), renames);
    if (obj->popObj)
        rewriteNotifyLabels(obj->popObj.get(), renames);
    for (auto &c : obj->children)
        rewriteNotifyLabels(c.get(), renames);
    for (auto &c : obj->childs)
        rewriteNotifyLabels(c.get(), renames);
}

void applyLabelRenames(MuibProject *proj, const QHash<QString, QString> &renames)
{
    if (!proj || renames.isEmpty())
        return;

    for (NotifyEvent &evt : proj->notify)
    {
        auto it = renames.constFind(evt.targetLabel);
        if (it != renames.constEnd())
            evt.targetLabel = it.value();
    }

    if (proj->appMenu)
        rewriteNotifyLabels(proj->appMenu.get(), renames);
    for (auto &w : proj->windows)
        rewriteNotifyLabels(w.get(), renames);

    if (proj->aboutBox)
    {
        auto it = renames.constFind(proj->aboutBox->aboutLinkedMenuItem);
        if (it != renames.constEnd())
            proj->aboutBox->aboutLinkedMenuItem = it.value();
    }
}

// --- Menu-family tree helpers -------------------------------------------

MuibObject *appendChildToMenu(MuibObject *parent, std::unique_ptr<MuibObject> child)
{
    Q_ASSERT(parent && (parent->type == ObjType::Menu || parent->type == ObjType::SubMenu));
    child->father = parent;
    MuibObject *raw = child.get();
    parent->childs.push_back(std::move(child));
    return raw;
}

bool moveMenuChildUpDown(MuibObject *obj, bool moveUp)
{
    if (!obj || !obj->father)
        return false;
    auto &siblings = obj->father->childs;
    auto it = std::find_if(siblings.begin(), siblings.end(),
                            [obj](const std::unique_ptr<MuibObject> &c) { return c.get() == obj; });
    if (it == siblings.end())
        return false;
    const int idx = static_cast<int>(std::distance(siblings.begin(), it));
    const int newIdx = moveUp ? idx - 1 : idx + 1;
    if (newIdx < 0 || newIdx >= static_cast<int>(siblings.size()))
        return false;
    std::swap(siblings[idx], siblings[newIdx]);
    return true;
}

bool removeMenuChild(MuibObject *obj)
{
    if (!obj || !obj->father)
        return false;
    auto &siblings = obj->father->childs;
    auto it = std::find_if(siblings.begin(), siblings.end(),
                            [obj](const std::unique_ptr<MuibObject> &c) { return c.get() == obj; });
    if (it == siblings.end())
        return false;
    siblings.erase(it);
    return true;
}

std::unique_ptr<MuibObject> cloneMenuTree(const MuibObject *src)
{
    if (!src)
        return nullptr;
    Q_ASSERT(src->type == ObjType::Menu || src->type == ObjType::SubMenu || src->type == ObjType::MenuItem);

    auto copy = std::make_unique<MuibObject>(src->type);
    copy->label = src->label;
    copy->generated = src->generated;
    copy->help = src->help;
    copy->notify = src->notify;
    copy->name = src->name;
    copy->menuKey = src->menuKey;
    copy->menu_enable = src->menu_enable;
    copy->check_enable = src->check_enable;
    copy->check_state = src->check_state;
    copy->toggleMenu = src->toggleMenu;
    copy->excludeGroup = src->excludeGroup;

    for (const auto &c : src->childs)
    {
        auto childCopy = cloneMenuTree(c.get());
        childCopy->father = copy.get();
        copy->childs.push_back(std::move(childCopy));
    }
    return copy;
}

void replaceWindowMenu(MuibObject *window, std::unique_ptr<MuibObject> newMenuRoot)
{
    Q_ASSERT(window && window->type == ObjType::Window);
    if (newMenuRoot)
    {
        Q_ASSERT(newMenuRoot->type == ObjType::Menu);
        newMenuRoot->father = window;
    }
    window->menu = std::move(newMenuRoot);
}
