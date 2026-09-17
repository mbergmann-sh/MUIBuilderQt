#include "objectfactory.h"

static const QVector<PaletteEntry> g_palette = {
    { ObjType::Group,      QStringLiteral("Group"),       QStringLiteral("Layout") },
    { ObjType::Space,      QStringLiteral("Space"),       QStringLiteral("Layout") },
    { ObjType::Rectangle,  QStringLiteral("Rectangle"),   QStringLiteral("Layout") },

    { ObjType::Button,     QStringLiteral("Button"),      QStringLiteral("Controls") },
    { ObjType::String,     QStringLiteral("String"),      QStringLiteral("Controls") },
    { ObjType::Cycle,      QStringLiteral("Cycle"),       QStringLiteral("Controls") },
    { ObjType::Radio,      QStringLiteral("Radio"),       QStringLiteral("Controls") },
    { ObjType::Check,      QStringLiteral("Check"),       QStringLiteral("Controls") },
    { ObjType::Slider,     QStringLiteral("Slider"),      QStringLiteral("Controls") },
    { ObjType::Scale,      QStringLiteral("Scale"),       QStringLiteral("Controls") },
    { ObjType::Gauge,      QStringLiteral("Gauge"),       QStringLiteral("Controls") },
    { ObjType::Prop,       QStringLiteral("Prop"),        QStringLiteral("Controls") },
    { ObjType::ColorField, QStringLiteral("ColorField"),  QStringLiteral("Controls") },

    { ObjType::Text,       QStringLiteral("Text"),        QStringLiteral("Display") },
    { ObjType::Label,      QStringLiteral("Label"),       QStringLiteral("Display") },
    { ObjType::Image,      QStringLiteral("Image"),       QStringLiteral("Display") },
    { ObjType::ListView,   QStringLiteral("Listview"),    QStringLiteral("Display") },
    { ObjType::DirList,    QStringLiteral("DirList"),     QStringLiteral("Display") },

    { ObjType::PopAsl,     QStringLiteral("PopAsl"),      QStringLiteral("Popup") },
    { ObjType::PopObject,  QStringLiteral("PopObject"),   QStringLiteral("Popup") },
};

const QVector<PaletteEntry> &paletteEntries()
{
    return g_palette;
}

static QString uniqueLabel(const QString &base, const QStringList &existingLabels)
{
    if (!existingLabels.contains(base))
        return base;
    int n = 2;
    while (existingLabels.contains(base + QStringLiteral("_%1").arg(n)))
        ++n;
    return base + QStringLiteral("_%1").arg(n);
}

bool typeHasAreaAttrs(ObjType type)
{
    switch (type)
    {
    case ObjType::Appli:
    case ObjType::Window:
    case ObjType::Space:
    case ObjType::Menu:
    case ObjType::SubMenu:
    case ObjType::MenuItem:
        return false;
    // Group WAS listed here (returning false), but the original tool's
    // own struct group1 (builder.h) embeds a real "area Area;" field
    // just like every other widget type, and its CodeArea() call site
    // (code.c, TY_GROUP case) passes Frame=TRUE and TitleFrame=TRUE -
    // i.e. a real MUIBuilder Group DOES support "Rahmen" (MUIA_Frame)
    // and "Rahmentext" (MUIA_FrameTitle). MuiCodeGen::emitObject()'s
    // own Group case already unconditionally calls emitArea() (see
    // muicodegen.cpp) - only the property-inspector GUI was wrongly
    // hiding the Area section for Group. Falls through to "default:
    // return true" below.
    default:
        return true;
    }
}

std::unique_ptr<MuibObject> createDefaultObject(ObjType type, const QStringList &existingLabels)
{
    // Find the palette entry for the display name used as the label
    // prefix; for a type not in the palette (Menu/SubMenu/MenuItem -
    // see the header's scope note - or, in principle, Window/Appli)
    // fall back to the same displayNameForType() every other piece of
    // this GUI already uses for these types, rather than a generic
    // "Object" - this project's own established label convention (as
    // opposed to the original tool's own distinct "MN_label_N" scheme
    // for freshly-created menu entries, which this port deliberately
    // does not replicate, for consistency with every other type here).
    QString base = displayNameForType(type);
    for (const auto &e : paletteEntries())
    {
        if (e.type == type) { base = e.displayName; break; }
    }

    auto obj = std::make_unique<MuibObject>(type);
    obj->label = uniqueLabel(base, existingLabels);

    switch (type)
    {
    case ObjType::Group:
        obj->horizontal = false;
        break;
    case ObjType::Button:
        obj->title = QStringLiteral("Button");
        break;
    case ObjType::String:
        obj->title = QString();
        obj->maxlen = 255;
        break;
    case ObjType::Cycle:
        obj->entries = { QStringLiteral("Item 1"), QStringLiteral("Item 2") };
        break;
    case ObjType::Radio:
        obj->entries = { QStringLiteral("Item 1"), QStringLiteral("Item 2") };
        break;
    case ObjType::Check:
        obj->title = QStringLiteral("Check");
        obj->title_exist = true;
        break;
    case ObjType::Slider:
        obj->sliderMin = 0;
        obj->sliderMax = 100;
        obj->init = 0;
        break;
    case ObjType::Scale:
        obj->scaleHoriz = true;
        break;
    case ObjType::Gauge:
        obj->gaugeMax = 100;
        obj->infotext = QStringLiteral("%ld%%");
        break;
    case ObjType::Text:
        obj->content = QStringLiteral("Text");
        break;
    case ObjType::Label:
        obj->title = QStringLiteral("Label");
        break;
    case ObjType::ColorField:
        obj->red = 0x8000; obj->green = 0x8000; obj->blue = 0x8000;
        break;
    case ObjType::ListView:
        obj->format = QStringLiteral("BAR,BAR,");
        break;
    case ObjType::Space:
        obj->spaceType = 0;
        break;
    case ObjType::Menu:
        // Real default (menu.c's InitMenu(): menu_enable = TRUE) - the
        // root Menu object doesn't actually use menu_enable itself, but
        // set it for consistency since MuibObject is shared across all
        // three Menu-family types.
        obj->menu_enable = true;
        break;
    case ObjType::SubMenu:
        obj->menu_enable = true;
        obj->name = QStringLiteral("New Menu");
        break;
    case ObjType::MenuItem:
        obj->menu_enable = true;
        obj->name = QStringLiteral("New Item");
        break;
    default:
        break;
    }

    return obj;
}

QString displayNameForType(ObjType type)
{
    for (const auto &e : paletteEntries())
    {
        if (e.type == type)
            return e.displayName;
    }
    switch (type)
    {
    case ObjType::Window:   return QStringLiteral("Window");
    case ObjType::Appli:    return QStringLiteral("Application");
    case ObjType::Menu:     return QStringLiteral("Menu");
    case ObjType::SubMenu:  return QStringLiteral("SubMenu");
    case ObjType::MenuItem: return QStringLiteral("MenuItem");
    default:                return QStringLiteral("Object");
    }
}

std::unique_ptr<MuibObject> createDefaultWindow(const QStringList &existingLabels)
{
    auto win = std::make_unique<MuibObject>(ObjType::Window);
    win->label = uniqueLabel(QStringLiteral("Window"), existingLabels);
    win->title = QStringLiteral("New Window");
    win->closegadget = true;
    win->depthgadget = true;
    win->dragbar = true;
    win->sizegadget = true;

    auto root = std::make_unique<MuibObject>(ObjType::Group);
    root->isRoot = true;
    root->label = uniqueLabel(win->label + QStringLiteral("_Root"), existingLabels);
    root->horizontal = false;
    win->root = std::move(root);

    return win;
}
