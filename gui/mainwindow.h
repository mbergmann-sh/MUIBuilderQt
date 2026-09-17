// MuiBuilderQt GUI - top-level window tying WidgetPalette, CanvasWidget,
// ObjectTreeView and PropertyInspector together around one in-memory
// MuibProject, plus the File menu (New/Open/Save/Generate Code) that
// drives the already-validated core/ load-save-codegen engine.
#pragma once

#include <QMainWindow>
#include "muibobject.h"
#include <memory>
#include <QStringList>
#include <QPalette>

class CanvasWidget;
class WidgetPalette;
class ObjectTreeView;
class PropertyInspector;
class QComboBox;
class QMenu;
class QAction;
class QActionGroup;
class QToolBar;
class QLabel;
class QDockWidget;
class QTranslator;

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);

    // Loads `path` right after construction (used by main.cpp when a
    // .MUIB file is given on the command line) - same code path as the
    // Open... action.
    void loadProjectFile(const QString &path);

    // GUI Language (I18n) and Theme: both public (unlike AmigaED's own
    // private applyGuiLanguage()/actionSelectTheme() split) because
    // main.cpp needs to call them directly, right after construction,
    // for the new "--GUI_Language"/"--GUI_Theme" command-line options
    // (used when MuiBuilderQt is launched as an external tool from
    // AmigaED - see main.cpp's own comment). `persist` mirrors AmigaED's
    // own distinction between a lasting default-changing choice (View
    // menu here - see the .cpp for why that differs from AmigaED's own
    // View menu, which is a session-only quick-switch there since
    // AmigaED ALSO has a separate Prefs dialog holding the real default)
    // and a THIS-RUN-ONLY override (main.cpp's command-line options -
    // AmigaED's own current language/theme should be reflected
    // immediately without silently overwriting whatever the user
    // separately chose for MuiBuilderQt's own standalone use).
    void applyGuiLanguage(const QString &langCode, bool persist = true);
    void applyTheme(const QString &styleName, bool persist = true);

    // Puts this window into "launched by AmigaED to create a new project"
    // mode (see main.cpp's own comment on the 3 --AmigaED_* command-line
    // options this comes from) - reveals "File > Finalise AmigaED
    // Project" (see onFinaliseAmigaEDProject()) and pre-fills the save
    // path so a plain Save/Ctrl+S already lands exactly where AmigaED
    // expects the finished project, without the user having to figure out
    // where to put it themselves. The project itself is already blank at
    // this point (see the constructor) - this just gives it its intended
    // name/location from the very first frame.
    void enterAmigaEDNewProjectMode(const QString &dir, const QString &name, const QString &resultFile);

protected:
    void closeEvent(QCloseEvent *event) override;

private slots:
    void onNewProject();
    void onOpenProject();
    void openRecentProject();   // loads a project from the "Recent Projects" submenu
    bool onSaveProject();
    bool onSaveProjectAs();
    void onNewWindow();
    void onDeleteCurrentWindow();
    void onGenerateCode();
    void onDeleteSelected();
    // "File > Dialog Creator > AboutBox" - opens AboutBoxDialog (creating
    // or editing m_project->aboutBox); see aboutboxdialog.h.
    void onDialogCreatorAboutBox();
    // "File > Application Properties..." - opens ProjectPropertiesDialog
    // (Base/Author/Title/Version/Copyright/Description); see
    // projectpropertiesdialog.h.
    void onProjectProperties();
    // "File > Edit Menu..." - opens MenuEditorDialog for the Canvas's
    // current window (see menueditordialog.h); replaces the previous
    // right-click-context-menu-based menu editing entirely.
    void onMenuEditor();
    // "File > Finalise AmigaED Project" - only visible/enabled while
    // enterAmigaEDNewProjectMode() has been called (see mainwindow.h's own
    // comment on m_amigaEdNewProjectMode below and AmigaED's own
    // MainWindow::launchGuiBuilderForNewProject()/
    // onGuiBuilderProcessFinished() for the launching side of this
    // handshake). Saves the project, generates its C code straight into
    // m_amigaEdProjectDir (bypassing onGenerateCode()'s own directory/
    // base-name prompts - both are already known), writes
    // m_amigaEdResultFilePath (an .ini-style file, read back by AmigaED
    // via QSettings: MainFile/ProjectName/GeneratedFiles), then closes
    // this window - AmigaED is watching for exactly that process exit.
    void onFinaliseAmigaEDProject();

    void onCanvasSelectionChanged(MuibObject *obj);
    void onCanvasObjectAdded(MuibObject *targetGroup, ObjType type);
    void onCanvasObjectReordered(MuibObject *movedObj, MuibObject *targetGroup, int newIndex);
    void onTreeObjectSelected(MuibObject *obj);
    void onTreeWindowActivated(MuibObject *win);
    void onInspectorFieldChanged(MuibObject *obj, bool structural);
    void onWindowSwitcherChanged(int index);

    // View > GUI Language / View > Theme entries clicked - see
    // applyGuiLanguage()/applyTheme() above for what these delegate to.
    void actionSetGuiLanguageEnglish();
    void actionSetGuiLanguageGerman();
    void actionSelectTheme();

private:
    void setupUi();
    void setupMenusAndToolbar();
    // Re-applies every tr()-wrapped string on a persistent (long-lived)
    // widget after a runtime GUI-language change - menu titles/actions,
    // the window-switcher toolbar/label, dock titles, the window title.
    // Transient strings (QMessageBox text, file-dialog captions, ...)
    // need no such pass - they call tr() fresh every time they're shown,
    // so they already pick up whichever language is active at that
    // moment. Mirrors AmigaED's own retranslateUi(), scoped down to what
    // MuiBuilderQt's much smaller, dialog-free-at-idle GUI actually has.
    void retranslateUi();
    // Builds View > Theme: one checkable, mutually exclusive entry per
    // QStyleFactory::keys() native style, then a separator, then
    // MuiBuilderQt's own 4 synthetic themes (Dark/Workbench 1.3/
    // Workbench 3.1/Visual Studio Code Dark) - byte-for-byte the same
    // set AmigaED 4.0 offers (see is/applyXxxTheme()/applyApplicationStyle()
    // below, ported from AmigaED's own mainwindow.cpp). Called once from
    // setupMenusAndToolbar(), after m_viewMenu exists.
    void buildThemeMenu();
    // Ensures the View > Theme entry matching m_defaultStyle is checked -
    // called after buildThemeMenu() and every time applyApplicationStyle()
    // runs. A no-op before buildThemeMenu() has run yet.
    void syncThemeMenuCheckedState();
    // Applies m_defaultStyle to the running QApplication (style + palette,
    // plus a small stylesheet for "Workbench 1.3"/"Visual Studio Code
    // Dark"'s menu-/status-bar chrome - see the .cpp) - called once from
    // the constructor and again, live, from applyTheme() whenever the
    // style actually changes.
    void applyApplicationStyle();
    bool isDarkTheme() const;
    bool isWorkbench13Theme() const;
    bool isWorkbench31Theme() const;
    bool isVSCodeTheme() const;
    QPalette darkApplicationPalette() const;
    QPalette workbench13ApplicationPalette() const;
    QPalette workbench31ApplicationPalette() const;
    QPalette vscodeApplicationPalette() const;
    // Window geometry + dock/splitter layout (position, size, which
    // docks are tabbed/floating/how wide) persisted via QSettings under
    // the "UI" group - separate from the per-project MISC-style settings
    // a .MUIB file itself carries, since this is about the GUI's own
    // window arrangement, not project data. Restored right after
    // setupUi() creates the docks (so restoreState() has something to
    // apply to) and saved from closeEvent().
    void loadWindowSettings();
    void saveWindowSettings();
    // "Recent Projects" submenu (File menu) - same pattern as AmigaED's
    // own "Recent files"/"Recent Projects" submenus, at the user's
    // request: a QSettings-persisted, oldest-first, deduplicated list of
    // full .MUIB paths capped at kMaxRecentProjects, rebuilt into
    // m_recentProjectsMenu (most recent first) whenever it changes.
    // loadRecentProjectsSettings() runs once, early in the constructor -
    // before setupMenusAndToolbar() builds the submenu from it - so,
    // unlike AmigaED (whose settings-loading is reused later too), no
    // "does the menu exist yet" guard is needed on the read side.
    void loadRecentProjectsSettings();
    void updateRecentProjectsMenu();
    void addToRecentProjects(const QString &path);
    void removeFromRecentProjects(const QString &path);
    void setCurrentWindow(MuibObject *win);
    void refreshWindowSwitcher();
    void refreshTreeAndKeepSelection();
    void markDirty(bool dirty = true);
    void updateWindowTitle();
    // Confirms discarding unsaved changes (New/Open/Quit) - returns true
    // if it is safe to proceed (nothing unsaved, user saved, or user
    // explicitly chose to discard).
    bool confirmDiscardUnsavedChanges();
    // The directory a file dialog should default to for the CURRENTLY
    // open project - m_currentFilePath's own directory (an unsaved/new
    // project has none, so this is "" then, and every call site below
    // falls back to Qt's own default behaviour exactly as before). Used
    // by onOpenProject()/onSaveProjectAs()/onGenerateCode() so a project
    // loaded via the "Recent Projects" submenu (whose entries can live
    // anywhere) doesn't leave those dialogs sitting at some unrelated
    // last-used/OS-default folder - see the user's own bug report: the
    // project loads fine from that list, but generating code from it
    // still opened the "Zielverzeichnis" picker somewhere else entirely,
    // forcing a manual re-navigation to the project's own folder every
    // single time.
    QString currentProjectDir() const;

    std::unique_ptr<MuibProject> m_project;
    QString m_currentFilePath;
    bool m_dirty = false;

    CanvasWidget *m_canvas = nullptr;
    WidgetPalette *m_palette = nullptr;
    ObjectTreeView *m_tree = nullptr;
    PropertyInspector *m_inspector = nullptr;
    QComboBox *m_windowSwitcher = nullptr;
    bool m_suppressWindowSwitcherSignal = false;

    QStringList m_recentProjects;   // full paths, oldest first, no duplicates
    static constexpr int kMaxRecentProjects = 10;

    // --- "Launched by AmigaED to create a new project" mode (see
    // enterAmigaEDNewProjectMode()/onFinaliseAmigaEDProject() above and
    // main.cpp's own --AmigaED_* command-line options) - all 4 only
    // meaningful while m_amigaEdNewProjectMode is true. ---
    bool m_amigaEdNewProjectMode = false;
    QString m_amigaEdProjectDir;
    QString m_amigaEdProjectName;
    QString m_amigaEdResultFilePath;

    // --- Persistent menu/toolbar/dock widgets, kept as members so
    // retranslateUi() can reach and re-text them after a runtime GUI-
    // language change (every one of these is built once, in setupUi()/
    // setupMenusAndToolbar(), and lives for the whole session - unlike
    // the transient QMessageBox/QFileDialog strings sprinkled through
    // the .cpp, which need no such member since they call tr() fresh
    // every time they're shown). ---
    QMenu *m_fileMenu = nullptr;
    QAction *m_newProjectAct = nullptr;
    QAction *m_openProjectAct = nullptr;
    QMenu *m_recentProjectsMenu = nullptr;
    QAction *m_saveProjectAct = nullptr;
    QAction *m_saveProjectAsAct = nullptr;
    QAction *m_newWindowAct = nullptr;
    QAction *m_deleteWindowAct = nullptr;
    QAction *m_menuEditorAct = nullptr;
    QAction *m_appPropertiesAct = nullptr;
    QMenu *m_dialogCreatorMenu = nullptr;
    QAction *m_aboutBoxAct = nullptr;
    QAction *m_generateCodeAct = nullptr;
    // "File > Finalise AmigaED Project" - hidden (setVisible(false)) at
    // startup and everywhere in ordinary standalone use; only shown once
    // enterAmigaEDNewProjectMode() runs. See onFinaliseAmigaEDProject().
    QAction *m_finaliseAmigaEDProjectAct = nullptr;
    QAction *m_quitAct = nullptr;
    QMenu *m_editMenu = nullptr;
    QAction *m_deleteObjectAct = nullptr;
    QToolBar *m_windowToolbar = nullptr;
    QLabel *m_windowToolbarLabel = nullptr;
    QDockWidget *m_paletteDock = nullptr;
    QDockWidget *m_treeDock = nullptr;
    QDockWidget *m_inspectorDock = nullptr;

    // --- GUI Language (I18n) - View > GUI Language, top entry ---
    QString m_guiLanguage = QStringLiteral("en");   // "en" (default/source language) or "de"
    QTranslator *m_guiTranslator = nullptr;         // installed only while German is active
    QTranslator *m_qtBaseTranslator = nullptr;      // Qt's own German translation (QMessageBox button labels etc. - see applyGuiLanguage())
    QMenu *m_viewMenu = nullptr;
    QMenu *m_guiLanguageMenu = nullptr;
    QAction *m_guiLanguageEnglishAct = nullptr;
    QAction *m_guiLanguageGermanAct = nullptr;
    QActionGroup *m_guiLanguageGroup = nullptr;   // holds English/Deutsch for mutual exclusion in the menu

    // --- Theme - View > Theme, added right after GUI Language ---
    QString m_defaultStyle;         // "" = platform default; a QStyleFactory key, or one of the 4 synthetic theme names (see isDarkTheme() etc.)
    QMenu *m_themeMenu = nullptr;
    QActionGroup *m_themeActionGroup = nullptr;
};
