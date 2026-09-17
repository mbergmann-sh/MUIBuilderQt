// MuiBuilderQt GUI - the left-hand dock listing every draggable object
// type (see objectfactory.h's paletteEntries()), grouped by category.
// A plain QListWidget with Qt's built-in drag support (setDragEnabled)
// rather than a custom drag source - the payload is just the ObjType int
// under kMuibObjectTypeMime, decoded by ObjectBox::dropEvent().
#pragma once

#include <QListWidget>

class WidgetPalette : public QListWidget
{
    Q_OBJECT
public:
    explicit WidgetPalette(QWidget *parent = nullptr);

    // Rebuilds the list from scratch (see populate()) - unlike
    // ObjectTreeView/ObjectBox, whose translatable text is regenerated on
    // every project/canvas rebuild anyway (many times per session), this
    // widget is populated exactly once, from a fixed, never-changing list
    // (objectfactory.h's paletteEntries()) - without this, its category
    // headers' translated text would stay stale forever after a runtime
    // GUI-language change instead of just until the next natural rebuild.
    // Called from MainWindow::retranslateUi().
    void retranslate();

protected:
    QMimeData *mimeData(const QList<QListWidgetItem *> &items) const override;
    Qt::DropActions supportedDropActions() const override { return Qt::CopyAction; }

private:
    void populate();
};
