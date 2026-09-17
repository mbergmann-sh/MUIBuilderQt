#include "projectpropertiesdialog.h"

#include <QVBoxLayout>
#include <QFormLayout>
#include <QLineEdit>
#include <QDialogButtonBox>

ProjectPropertiesDialog::ProjectPropertiesDialog(MuibProject *project, QWidget *parent)
    : QDialog(parent), m_project(project)
{
    setWindowTitle(tr("Application Properties"));
    resize(420, 0);

    auto *form = new QFormLayout();

    m_baseEdit = new QLineEdit(project->base, this);
    form->addRow(tr("Base Name (.h/.c):"), m_baseEdit);

    m_authorEdit = new QLineEdit(project->author, this);
    form->addRow(tr("Author:"), m_authorEdit);

    m_titleEdit = new QLineEdit(project->title, this);
    form->addRow(tr("Title:"), m_titleEdit);

    m_versionEdit = new QLineEdit(project->version, this);
    m_versionEdit->setPlaceholderText(tr("e.g. 1.0"));
    form->addRow(tr("Version:"), m_versionEdit);

    m_copyrightEdit = new QLineEdit(project->copyright, this);
    m_copyrightEdit->setPlaceholderText(tr("e.g. © 2026 Micha B."));
    form->addRow(tr("Copyright:"), m_copyrightEdit);

    m_descriptionEdit = new QLineEdit(project->description, this);
    form->addRow(tr("Description:"), m_descriptionEdit);

    m_helpfileEdit = new QLineEdit(project->helpfile, this);
    m_helpfileEdit->setPlaceholderText(tr("e.g. %1.guide (empty = no help file/text in the generated code)").arg(project->base.isEmpty() ? tr("MyProgram") : project->base));
    form->addRow(tr("Help File (.guide):"), m_helpfileEdit);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto *outer = new QVBoxLayout(this);
    outer->addLayout(form);
    outer->addWidget(buttons);
}

void ProjectPropertiesDialog::apply()
{
    m_project->base = m_baseEdit->text();
    m_project->author = m_authorEdit->text();
    m_project->title = m_titleEdit->text();
    m_project->version = m_versionEdit->text();
    m_project->copyright = m_copyrightEdit->text();
    m_project->description = m_descriptionEdit->text();
    m_project->helpfile = m_helpfileEdit->text();
}
