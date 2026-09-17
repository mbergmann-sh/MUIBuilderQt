// MuiBuilderQt GUI - "Dialog-Creator > AboutBox" dialog (see the user's
// own request quoted in muibobject.h's ObjType::AboutBox comment): lets
// the user set the Credits/Build/LogoFile/URL/URLText text+image fields
// for the project's (at most one) AboutBox, and - if the current project
// already has a menu somewhere - pick which existing MenuItem should
// open it.
//
// Wraps the real Aboutbox.mcc custom class (part of MUI 5.0 itself),
// verified against the real MUI 5.0 SDK the user provided
// (Aboutbox_mcc.h/Aboutbox.c) - see core/muicodegen.cpp's emitObject()
// ObjType::AboutBox case and BuildApplication() emission for how these
// fields turn into real generated C code.
#pragma once

#include <QDialog>
#include "muibobject.h"

class QPlainTextEdit;
class QLineEdit;
class QComboBox;

class AboutBoxDialog : public QDialog
{
    Q_OBJECT
public:
    // `project` is read (to find existing MenuItems for the "verknuepfter
    // Menuepunkt" combo, and existing labels for a freshly generated
    // unique one) and, once the user accepts, directly updated in place
    // by apply() - the dialog never owns a separate copy of
    // project->aboutBox. If project->aboutBox already exists, every
    // field is pre-filled from it (edit-in-place).
    explicit AboutBoxDialog(MuibProject *project, QWidget *parent = nullptr);

    // Call after exec() == QDialog::Accepted (the "AboutBox entfernen"
    // button also accept()s the dialog - see wasRemoveRequested()).
    // Writes the dialog's current fields into project->aboutBox
    // (creating a new one, with a freshly generated unique label, if
    // there wasn't one already), or - if the user clicked "AboutBox
    // entfernen" - clears project->aboutBox back to nullptr instead.
    void apply();

    // True if the user clicked "AboutBox entfernen" (only meaningful
    // after exec() == QDialog::Accepted) - lets the caller show a
    // different confirmation message than for a normal create/update.
    bool wasRemoveRequested() const { return m_removeRequested; }

private slots:
    void onBrowseLogo();
    void onRemove();

private:
    void populateMenuItemCombo();

    MuibProject *m_project = nullptr;
    bool m_removeRequested = false;

    QPlainTextEdit *m_creditsEdit = nullptr;
    QLineEdit *m_buildEdit = nullptr;
    QLineEdit *m_logoFileEdit = nullptr;
    QLineEdit *m_urlEdit = nullptr;
    QLineEdit *m_urlTextEdit = nullptr;
    QComboBox *m_menuItemCombo = nullptr;   // nullptr if no MenuItems exist anywhere in the project
};
