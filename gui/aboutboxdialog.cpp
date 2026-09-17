#include "aboutboxdialog.h"
#include "objecttreeutil.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QPlainTextEdit>
#include <QLineEdit>
#include <QComboBox>
#include <QLabel>
#include <QPushButton>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QMessageBox>

// Walks a window's Menu/SubMenu/MenuItem tree (via `childs`, exactly as
// muicodegen.cpp's emitObject()/assignIdents() do) collecting every real,
// triggerable menu leaf (skipping BarLabel separators - see
// objecttreeutil.cpp's NewSeparator handling - and any item with no
// label, since a label is what MUIA_Menuitem_Trigger wiring needs to
// find it again in muicodegen.cpp's BuildApplication()). `pathSoFar`
// accumulates SubMenu titles for a readable combo-box breadcrumb.
//
// A "leaf" is either a real ObjType::MenuItem, OR an ObjType::SubMenu
// with an EMPTY childs list. The latter looks odd at first, but is a
// real, common case: muicodegen.cpp's emitObject() emits a childless
// SubMenu with exactly the same MenuitemObject/MUIA_Menuitem_Title/End
// shape as a true MenuItem (no MUIA_Family_Child entries at all - see
// its ObjType::SubMenu case) - it's only the PropertyInspector's always-
// shown generic "Label" row that lets a user rename any object's
// internal label, which is how real projects end up with a plain,
// non-flyout menu entry (e.g. an "About..." item) stored as a SubMenu
// node instead of a MenuItem node. Before this fix, such an entry was
// silently invisible to the AboutBox-Creator's "Verknuepfter
// Menuepunkt" combobox (skipped: SubMenu was only ever recursed into,
// never yielded) even though the object is just as capable of carrying
// a real MUIA_Menuitem_Trigger notification as an actual MenuItem -
// and muicodegen.cpp's own identOf()/ctx.objectByLabel resolution
// (used by the "Verknuepfter Menuepunkt" wiring in generateSource())
// is already fully generic by label, not by ObjType, so no codegen
// change is needed for this - only this collector was too restrictive.
static void collectMenuItems(MuibObject *node, const QString &windowLabel, QStringList pathSoFar,
                              QVector<QPair<QString, MuibObject *>> &out)
{
    if (!node)
        return;

    if (node->type == ObjType::MenuItem)
    {
        if (node->label.isEmpty())
            return;
        if (node->name.startsWith(QStringLiteral("BarLabel")))
            return;   // separator bar, not a real trigger target
        QStringList full;
        full << windowLabel;
        full << pathSoFar;
        if (!node->name.isEmpty())
            full << node->name;
        out.append(qMakePair(full.join(QStringLiteral(" > ")), node));
        return;
    }

    if (node->type == ObjType::SubMenu && node->childs.empty())
    {
        // Childless SubMenu - functionally a leaf (see comment above),
        // so it's yielded here too instead of only being a path-prefix
        // for deeper recursion.
        if (!node->label.isEmpty())
        {
            QStringList full;
            full << windowLabel;
            full << pathSoFar;
            if (!node->name.isEmpty())
                full << node->name;
            out.append(qMakePair(full.join(QStringLiteral(" > ")), node));
        }
        return;
    }

    // ObjType::Menu (root) or a SubMenu WITH children - recurse.
    if (node->type == ObjType::SubMenu && !node->name.isEmpty())
        pathSoFar << node->name;
    for (const auto &ch : node->childs)
        collectMenuItems(ch.get(), windowLabel, pathSoFar, out);
}

// Mirrors objectfactory.cpp's own (file-local) uniqueLabel() helper -
// duplicated here rather than exposed from there, since it's a two-line
// generic helper and objectfactory.h's own contract is specifically
// about default OBJECTS, not bare label strings.
static QString uniqueAboutBoxLabel(const QStringList &existingLabels)
{
    static const QString base = QStringLiteral("AboutBox");
    if (!existingLabels.contains(base))
        return base;
    int n = 2;
    while (existingLabels.contains(base + QStringLiteral("_%1").arg(n)))
        ++n;
    return base + QStringLiteral("_%1").arg(n);
}

AboutBoxDialog::AboutBoxDialog(MuibProject *project, QWidget *parent)
    : QDialog(parent), m_project(project)
{
    setWindowTitle(tr("AboutBox Creator"));
    resize(480, 460);

    auto *form = new QFormLayout();

    m_creditsEdit = new QPlainTextEdit(this);
    m_creditsEdit->setPlaceholderText(
        tr("e.g. \"Demonstrate the use of Aboutbox.mcc in MUI 5.0\"\nCopyright (C) 2015-2020 ..."));
    m_creditsEdit->setMinimumHeight(100);
    form->addRow(tr("Text (Credits):"), m_creditsEdit);

    m_buildEdit = new QLineEdit(this);
    m_buildEdit->setPlaceholderText(tr("e.g. \"svn r1234\" (optional)"));
    form->addRow(tr("Build:"), m_buildEdit);

    auto *logoRow = new QHBoxLayout();
    m_logoFileEdit = new QLineEdit(this);
    m_logoFileEdit->setPlaceholderText(tr("e.g. PROGDIR:boing.png (optional)"));
    auto *browseBtn = new QPushButton(tr("Browse..."), this);
    connect(browseBtn, &QPushButton::clicked, this, &AboutBoxDialog::onBrowseLogo);
    logoRow->addWidget(m_logoFileEdit, 1);
    logoRow->addWidget(browseBtn);
    form->addRow(tr("Image (Logo):"), logoRow);

    m_urlEdit = new QLineEdit(this);
    m_urlEdit->setPlaceholderText(tr("e.g. http://www.example.com (optional)"));
    form->addRow(tr("URL:"), m_urlEdit);

    m_urlTextEdit = new QLineEdit(this);
    m_urlTextEdit->setPlaceholderText(tr("Display text for the link (optional)"));
    form->addRow(tr("URL Text:"), m_urlTextEdit);

    populateMenuItemCombo();
    if (m_menuItemCombo)
    {
        form->addRow(tr("Linked Menu Item:"), m_menuItemCombo);
    }
    else
    {
        auto *hint = new QLabel(
            tr("(No menu with entries exists in the project yet - the AboutBox can be linked to\n"
               "a menu item later via \"File > Dialog Creator > AboutBox\".)"),
            this);
        hint->setStyleSheet(QStringLiteral("color: #808080; font-style: italic;"));
        form->addRow(hint);
    }

    // Pre-fill from an existing AboutBox, if the project already has one
    // (edit-in-place semantics).
    if (m_project->aboutBox)
    {
        MuibObject *box = m_project->aboutBox.get();
        m_creditsEdit->setPlainText(box->aboutCredits);
        m_buildEdit->setText(box->aboutBuild);
        m_logoFileEdit->setText(box->aboutLogoFile);
        m_urlEdit->setText(box->aboutUrl);
        m_urlTextEdit->setText(box->aboutUrlText);
        if (m_menuItemCombo)
        {
            const int idx = m_menuItemCombo->findData(box->aboutLinkedMenuItem);
            m_menuItemCombo->setCurrentIndex(idx >= 0 ? idx : 0);
        }
    }

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    // Only offered when there is actually something to remove.
    if (m_project->aboutBox)
    {
        auto *removeBtn = buttons->addButton(tr("Remove AboutBox"), QDialogButtonBox::DestructiveRole);
        connect(removeBtn, &QPushButton::clicked, this, &AboutBoxDialog::onRemove);
    }

    auto *outer = new QVBoxLayout(this);
    outer->addLayout(form);
    outer->addWidget(buttons);
}

void AboutBoxDialog::onBrowseLogo()
{
    const QString path = QFileDialog::getOpenFileName(
        this, tr("Choose Logo Image"), QString(),
        tr("Images (*.png *.iff *.ilbm *.jpg *.jpeg *.bmp);;All Files (*)"));
    if (path.isEmpty())
        return;
    // Real Aboutbox.mcc demo convention (Examples/Aboutbox.c:
    // MUIA_Aboutbox_LogoFile, "PROGDIR:boing.png") - the generated
    // Amiga program looks for the image next to its own binary at
    // runtime, so only the FILE NAME travels into the generated code;
    // actually copying/bundling the picked image into the output
    // directory is out of scope (see README.md) - the user places it
    // beside the compiled binary themselves, same as the real demo.
    m_logoFileEdit->setText(QStringLiteral("PROGDIR:") + QFileInfo(path).fileName());
}

void AboutBoxDialog::onRemove()
{
    if (QMessageBox::question(this, tr("Remove AboutBox"),
                               tr("Really remove this project's AboutBox?"))
        != QMessageBox::Yes)
        return;
    m_removeRequested = true;
    accept();
}

void AboutBoxDialog::populateMenuItemCombo()
{
    QVector<QPair<QString, MuibObject *>> items;
    for (const auto &w : m_project->windows)
    {
        if (!w->menu)
            continue;
        const QString winLabel = w->title.isEmpty() ? w->label : w->title;
        collectMenuItems(w->menu.get(), winLabel, QStringList(), items);
    }

    if (items.isEmpty())
    {
        m_menuItemCombo = nullptr;
        return;
    }

    m_menuItemCombo = new QComboBox(this);
    m_menuItemCombo->addItem(tr("(not linked)"), QString());
    for (const auto &pair : items)
        m_menuItemCombo->addItem(pair.first, pair.second->label);
}

void AboutBoxDialog::apply()
{
    if (m_removeRequested)
    {
        m_project->aboutBox.reset();
        return;
    }

    if (!m_project->aboutBox)
    {
        auto box = std::make_unique<MuibObject>(ObjType::AboutBox);
        box->label = uniqueAboutBoxLabel(collectAllLabels(m_project));
        m_project->aboutBox = std::move(box);
    }

    MuibObject *box = m_project->aboutBox.get();
    box->aboutCredits = m_creditsEdit->toPlainText();
    box->aboutBuild = m_buildEdit->text();
    box->aboutLogoFile = m_logoFileEdit->text();
    box->aboutUrl = m_urlEdit->text();
    box->aboutUrlText = m_urlTextEdit->text();
    box->aboutLinkedMenuItem = m_menuItemCombo ? m_menuItemCombo->currentData().toString() : QString();
}
