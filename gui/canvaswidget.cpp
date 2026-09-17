#include "canvaswidget.h"
#include "objectbox.h"

#include <QVBoxLayout>
#include <QLabel>

CanvasWidget::CanvasWidget(QWidget *parent)
    : QScrollArea(parent)
{
    setWidgetResizable(true);
    setFrameShape(QFrame::NoFrame);

    m_content = new QWidget(this);
    m_contentLayout = new QVBoxLayout(m_content);
    m_contentLayout->setContentsMargins(12, 12, 12, 12);
    m_contentLayout->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    setWidget(m_content);

    m_placeholder = new QLabel(tr("No window open.\n\n"
                                   "Use File / New Window to create a window, "
                                   "or open an existing .MUIB project."),
                                m_content);
    m_placeholder->setStyleSheet(QStringLiteral("color: #808080; font-style: italic;"));
    m_placeholder->setAlignment(Qt::AlignCenter);
    m_contentLayout->addWidget(m_placeholder);
    m_placeholder->setVisible(false);
}

void CanvasWidget::setWindow(MuibObject *win)
{
    m_window = win;
    m_selected = win;   // selecting the newly shown window itself is the least surprising default
    rebuild();
}

void CanvasWidget::rebuild()
{
    if (m_rootBox)
    {
        m_contentLayout->removeWidget(m_rootBox);
        m_rootBox->deleteLater();
        m_rootBox = nullptr;
    }

    if (!m_window)
    {
        m_placeholder->setVisible(true);
        m_selected = nullptr;
        emit selectionChanged(nullptr);
        return;
    }
    m_placeholder->setVisible(false);

    m_rootBox = new ObjectBox(m_window, m_content);
    connect(m_rootBox, &ObjectBox::objectClicked, this, [this](MuibObject *obj) {
        setSelected(obj);
    });
    connect(m_rootBox, &ObjectBox::objectDropped, this, [this](MuibObject *targetGroup, ObjType type) {
        emit objectAdded(targetGroup, type);
    });
    connect(m_rootBox, &ObjectBox::objectReordered, this, [this](MuibObject *movedObj, MuibObject *targetGroup, int newIndex) {
        emit objectReordered(movedObj, targetGroup, newIndex);
    });
    m_contentLayout->insertWidget(0, m_rootBox);

    // Keep the previous selection if it still exists somewhere under
    // this window (a rebuild after "add object" should not silently
    // deselect whatever the user had selected before dropping), falling
    // back to the window itself otherwise (e.g. right after the
    // selected object was just deleted).
    m_rootBox->applySelection(m_selected);
}

void CanvasWidget::setSelected(MuibObject *obj)
{
    m_selected = obj;
    if (m_rootBox)
        m_rootBox->applySelection(obj);
    emit selectionChanged(obj);
}

void CanvasWidget::refreshHeaders()
{
    if (m_rootBox)
        m_rootBox->refreshHeaderRecursive();
}

void CanvasWidget::retranslate()
{
    m_placeholder->setText(tr("No window open.\n\n"
                               "Use File / New Window to create a window, "
                               "or open an existing .MUIB project."));
}
