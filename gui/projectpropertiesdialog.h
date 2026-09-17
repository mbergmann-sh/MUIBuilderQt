// MuiBuilderQt GUI - "Datei > Anwendungseigenschaften..." dialog.
//
// Ports the original tool's own "Application" window (builder.c's
// NewAppli(), triggered by its "Appli"-Button/ID_APPLI - see the
// StringObject fields for application.base/author/title/version/
// copyright/description, in exactly this order/cycle-chain) - the one
// real gap this reveals: MuiBuilderQt's GUI never offered ANY way to
// set these six MuibProject-level fields, so a project created from
// scratch here (as opposed to one loaded from an old .MUIB that already
// had them) keeps them all empty forever. That silently starves the
// generated ApplicationObject's own MUIA_Application_Title/_Version/
// _Copyright/_Author/_Description attributes - and, since Aboutbox.mcc
// reads Version/Copyright straight off the owning Application object,
// it was the direct root cause of an AboutBox showing empty "Version"/
// "Copyright" lines (see the user's own bug report).
#pragma once

#include <QDialog>
#include "muibobject.h"

class QLineEdit;

class ProjectPropertiesDialog : public QDialog
{
    Q_OBJECT
public:
    // `project` is read for the initial field values and, once the user
    // accepts, directly updated in place by apply().
    explicit ProjectPropertiesDialog(MuibProject *project, QWidget *parent = nullptr);

    // Call after exec() == QDialog::Accepted - writes the dialog's
    // fields back into project->base/author/title/version/copyright/
    // description.
    void apply();

private:
    MuibProject *m_project = nullptr;

    QLineEdit *m_baseEdit = nullptr;
    QLineEdit *m_authorEdit = nullptr;
    QLineEdit *m_titleEdit = nullptr;
    QLineEdit *m_versionEdit = nullptr;
    QLineEdit *m_copyrightEdit = nullptr;
    QLineEdit *m_descriptionEdit = nullptr;
    // MUIA_Application_HelpFile's source value (project->helpfile) - the
    // real original tool's own codegen (code.c) only ever emits this
    // attribute when this field is non-empty ("strlen(application.
    // helpfile) > 0"), and it was already round-tripped faithfully by
    // MuibLoader/MuibSaver, but had no GUI to set it from - same gap as
    // the six fields above, just never noticed since it's a newer,
    // less-visited feature (see the "Hilfetexte" bugfix in README.md).
    QLineEdit *m_helpfileEdit = nullptr;
};
