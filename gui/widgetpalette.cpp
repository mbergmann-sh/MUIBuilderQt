#include "widgetpalette.h"
#include "objectfactory.h"

#include <QMimeData>
#include <QFont>

// PaletteEntry::category (objectfactory.cpp) is a stable, untranslated
// English key used both for display AND to detect where one category's
// run of entries ends and the next begins (the `!=` check below) - it
// must never itself be translated, or that grouping would silently break
// whenever German is active. This maps each key to its own tr()'d
// display text instead, so the header text follows the GUI language
// while the grouping key stays fixed. Individual object type names
// (entry.displayName, e.g. "Button"/"String"/...) are deliberately left
// untranslated in both languages - the project's established convention,
// same as the View > Theme menu's native style names/synthetic theme
// names (see mainwindow.cpp's own buildThemeMenu()) - a widget type name
// reads as an identifier here, not as translatable prose.
static QString categoryDisplayName(const QString &category)
{
    if (category == QLatin1String("Layout"))
        return WidgetPalette::tr("Layout");
    if (category == QLatin1String("Controls"))
        return WidgetPalette::tr("Controls");
    if (category == QLatin1String("Display"))
        return WidgetPalette::tr("Display");
    if (category == QLatin1String("Popup"))
        return WidgetPalette::tr("Popup");
    return category;   // defensive fallback - every real category above is listed explicitly
}

WidgetPalette::WidgetPalette(QWidget *parent)
    : QListWidget(parent)
{
    setDragEnabled(true);
    setSelectionMode(QAbstractItemView::SingleSelection);
    setDragDropMode(QAbstractItemView::DragOnly);

    populate();
}

void WidgetPalette::populate()
{
    QString lastCategory;
    for (const auto &entry : paletteEntries())
    {
        if (entry.category != lastCategory)
        {
            auto *header = new QListWidgetItem(categoryDisplayName(entry.category), this);
            QFont f = header->font();
            f.setBold(true);
            header->setFont(f);
            header->setFlags(Qt::NoItemFlags);   // category header: not selectable/draggable
            header->setBackground(QColor("#e8e8e8"));
            addItem(header);
            lastCategory = entry.category;
        }

        auto *item = new QListWidgetItem(entry.displayName, this);
        item->setData(Qt::UserRole, static_cast<int>(entry.type));
        item->setToolTip(QObject::tr("Drag onto the canvas to create a new %1 object").arg(entry.displayName));
        addItem(item);
    }
}

void WidgetPalette::retranslate()
{
    clear();
    populate();
}

QMimeData *WidgetPalette::mimeData(const QList<QListWidgetItem *> &items) const
{
    if (items.isEmpty())
        return nullptr;

    const int typeInt = items.first()->data(Qt::UserRole).toInt();
    auto *mime = new QMimeData;
    mime->setData(QLatin1String(kMuibObjectTypeMime), QByteArray::number(typeInt));
    return mime;
}
