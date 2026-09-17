#include "mainwindow.h"
#include "canvaswidget.h"
#include "widgetpalette.h"
#include "objecttreeview.h"
#include "propertyinspector.h"
#include "objectfactory.h"
#include "objecttreeutil.h"
#include "muibloader.h"
#include "muibsaver.h"
#include "muicodegen.h"
#include "muibqtextras.h"
#include "aboutboxdialog.h"
#include "projectpropertiesdialog.h"
#include "menueditordialog.h"

#include <QDockWidget>
#include <QIcon>
#include <QMenuBar>
#include <QMenu>
#include <QAction>
#include <QToolBar>
#include <QComboBox>
#include <QLabel>
#include <QFileDialog>
#include <QMessageBox>
#include <QInputDialog>
#include <QShortcut>
#include <QCloseEvent>
#include <QFileInfo>
#include <QFile>
#include <QDir>
#include <QLineEdit>
#include <QVariant>
#include <QSettings>
#include <QTranslator>
#include <QActionGroup>
#include <QStyleFactory>
#include <QStyle>
#include <QApplication>
#include <QLocale>
#include <QDebug>

// Bumped only if the dock/toolbar layout itself changes shape in a
// future version (new dock added/removed etc.) - passed to both
// saveState()/restoreState() so a stale saved layout from an older
// build is cleanly ignored (Qt returns false from restoreState() on a
// version mismatch) instead of restoreState() trying to apply dock
// geometry for docks that may no longer exist.
static constexpr int kWindowStateVersion = 1;

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    // Redundant with app.setWindowIcon() in main.cpp (which already covers
    // this window as its default) - kept explicit here too, purely for
    // clarity/consistency with AmigaED's own MainWindow, which does the
    // same (see its initializeGUI()).
    setWindowIcon(QIcon(QStringLiteral(":/images/muibuilderqt.png")));

    m_project = std::make_unique<MuibProject>();

    // GUI Language + Theme: load both persisted defaults and apply them
    // BEFORE any widget is built, so every tr() call and the very first
    // QApplication palette/style already reflect them from the first
    // frame - mirrors AmigaED's own readSettings()-before-everything-else
    // constructor ordering. persist=false for both: this is loading an
    // EXISTING setting, not creating a new one (see applyGuiLanguage()'s
    // own comment on why that distinction matters) - a plain startup
    // must never rewrite what it just read back out again.
    {
        QSettings settings;
        applyGuiLanguage(settings.value(QStringLiteral("MISC/DefaultGUILanguage"), QStringLiteral("en")).toString(), false);
        m_defaultStyle = settings.value(QStringLiteral("MISC/DefaultStyle")).toString();
    }
    applyApplicationStyle();

    // Before setupMenusAndToolbar(), which builds the "Recent Projects"
    // submenu straight from m_recentProjects.
    loadRecentProjectsSettings();

    setupUi();
    setupMenusAndToolbar();

    setCurrentWindow(nullptr);
    m_tree->setProject(m_project.get());
    refreshWindowSwitcher();
    updateWindowTitle();

    // Sensible defaults FIRST (also what a brand-new install without any
    // saved QSettings yet will use)...
    resize(1400, 850);
    // ...then override with whatever the user last had, if anything -
    // position, size, AND the whole dock/splitter arrangement (which
    // docks are where, how wide, tabbed or floating).
    loadWindowSettings();
}

void MainWindow::setupUi()
{
    m_canvas = new CanvasWidget(this);
    setCentralWidget(m_canvas);

    // Every QDockWidget/QToolBar below gets a fixed, never-renamed
    // objectName() - QMainWindow::saveState()/restoreState() (see
    // loadWindowStateSettings()/writeWindowStateSettings() further down)
    // key each dock/toolbar's persisted geometry off its objectName(), and
    // without one Qt falls back to an empty string, silently collapsing
    // all unnamed docks onto the same key AND printing a
    // "QMainWindow::saveState(): 'objectName' not set for QDockWidget..."
    // warning on every save (reported by the Chefentwickler from an
    // actual Windows/Qt Creator build log - purely cosmetic there since
    // this project's own restoreState() call already discards any saved
    // layout on a version mismatch, see kWindowStateVersion above, but a
    // real, easy, zero-risk fix rather than something to just live with).
    m_paletteDock = new QDockWidget(tr("Palette"), this);
    m_paletteDock->setObjectName(QStringLiteral("paletteDock"));
    m_palette = new WidgetPalette(m_paletteDock);
    m_paletteDock->setWidget(m_palette);
    addDockWidget(Qt::LeftDockWidgetArea, m_paletteDock);

    m_treeDock = new QDockWidget(tr("Project Tree"), this);
    m_treeDock->setObjectName(QStringLiteral("treeDock"));
    m_tree = new ObjectTreeView(m_treeDock);
    m_treeDock->setWidget(m_tree);
    addDockWidget(Qt::LeftDockWidgetArea, m_treeDock);
    tabifyDockWidget(m_paletteDock, m_treeDock);
    m_paletteDock->raise();

    m_inspectorDock = new QDockWidget(tr("Properties"), this);
    m_inspectorDock->setObjectName(QStringLiteral("inspectorDock"));
    m_inspector = new PropertyInspector(m_inspectorDock);
    m_inspectorDock->setWidget(m_inspector);
    addDockWidget(Qt::RightDockWidgetArea, m_inspectorDock);

    // The Properties dock (property inspector) was reported unreadably
    // narrow by default - QMainWindow computes each dock's initial width
    // from its content's sizeHint, and a QFormLayout full of QLineEdit/
    // QSpinBox rows has a fairly small natural width. Two fixes, both
    // needed together: a wider MINIMUM (so it can never collapse back to
    // unreadable, even after the user drags it smaller) and a wider
    // INITIAL size via resizeDocks() (so it starts out readable without
    // the user having to drag it at all first).
    //
    // The actual resizing-by-dragging the user asked for already works
    // out of the box - QMainWindow's dock/central-widget borders are
    // draggable like a splitter by default, for every dock (Palette/
    // Project Tree on the left just as much as Properties on the right -
    // this same properties panel is covered by the same mechanism).
    // What was missing was making that border LOOK and FEEL like a real,
    // easy-to-grab splitter handle instead of Qt's default 1px hairline -
    // the stylesheet below widens it and highlights it on hover.
    m_paletteDock->setMinimumWidth(200);
    m_inspectorDock->setMinimumWidth(340);
    setStyleSheet(styleSheet() + QStringLiteral(
        "QMainWindow::separator { background: #b0b0b0; width: 6px; height: 6px; } "
        "QMainWindow::separator:hover { background: #5a9bef; }"));
    resizeDocks({ m_paletteDock, m_inspectorDock }, { 220, 380 }, Qt::Horizontal);

    connect(m_canvas, &CanvasWidget::selectionChanged, this, &MainWindow::onCanvasSelectionChanged);
    connect(m_canvas, &CanvasWidget::objectAdded, this, &MainWindow::onCanvasObjectAdded);
    connect(m_canvas, &CanvasWidget::objectReordered, this, &MainWindow::onCanvasObjectReordered);
    connect(m_tree, &ObjectTreeView::objectSelected, this, &MainWindow::onTreeObjectSelected);
    connect(m_tree, &ObjectTreeView::windowActivated, this, &MainWindow::onTreeWindowActivated);
    connect(m_inspector, &PropertyInspector::fieldChanged, this, &MainWindow::onInspectorFieldChanged);

    auto *deleteShortcut = new QShortcut(QKeySequence::Delete, this);
    connect(deleteShortcut, &QShortcut::activated, this, &MainWindow::onDeleteSelected);
}

void MainWindow::setupMenusAndToolbar()
{
    m_fileMenu = menuBar()->addMenu(tr("&File"));
    m_newProjectAct = m_fileMenu->addAction(tr("&New Project"), QKeySequence::New, this, &MainWindow::onNewProject);
    m_openProjectAct = m_fileMenu->addAction(tr("&Open..."), QKeySequence::Open, this, &MainWindow::onOpenProject);
    m_recentProjectsMenu = m_fileMenu->addMenu(tr("&Recent Projects"));
    updateRecentProjectsMenu();   // populate with whatever loadRecentProjectsSettings() already read
    m_fileMenu->addSeparator();
    m_saveProjectAct = m_fileMenu->addAction(tr("&Save"), QKeySequence::Save, this, &MainWindow::onSaveProject);
    m_saveProjectAsAct = m_fileMenu->addAction(tr("Save &As..."), QKeySequence::SaveAs, this, &MainWindow::onSaveProjectAs);
    m_fileMenu->addSeparator();
    m_newWindowAct = m_fileMenu->addAction(tr("New &Window"), QKeySequence(Qt::CTRL | Qt::Key_N), this, &MainWindow::onNewWindow);
    m_deleteWindowAct = m_fileMenu->addAction(tr("Delete Current Window"), this, &MainWindow::onDeleteCurrentWindow);
    m_menuEditorAct = m_fileMenu->addAction(tr("&Edit Menu..."), QKeySequence(Qt::CTRL | Qt::Key_M), this, &MainWindow::onMenuEditor);
    m_fileMenu->addSeparator();
    m_appPropertiesAct = m_fileMenu->addAction(tr("&Application Properties..."), this, &MainWindow::onProjectProperties);
    m_fileMenu->addSeparator();
    m_dialogCreatorMenu = m_fileMenu->addMenu(tr("Dialog Creator"));
    m_aboutBoxAct = m_dialogCreatorMenu->addAction(tr("AboutBox..."), this, &MainWindow::onDialogCreatorAboutBox);
    m_fileMenu->addSeparator();
    m_generateCodeAct = m_fileMenu->addAction(tr("&Generate Code..."), QKeySequence(Qt::CTRL | Qt::Key_G), this, &MainWindow::onGenerateCode);
    m_fileMenu->addSeparator();
    // Hidden unless launched by AmigaED for this - see mainwindow.h's own
    // comment on m_finaliseAmigaEDProjectAct/enterAmigaEDNewProjectMode().
    m_finaliseAmigaEDProjectAct = m_fileMenu->addAction(tr("Finalise AmigaED Project"), this, &MainWindow::onFinaliseAmigaEDProject);
    m_finaliseAmigaEDProjectAct->setVisible(false);
    m_fileMenu->addSeparator();
    m_quitAct = m_fileMenu->addAction(tr("&Quit"), QKeySequence::Quit, this, &QWidget::close);

    m_editMenu = menuBar()->addMenu(tr("&Edit"));
    m_deleteObjectAct = m_editMenu->addAction(tr("&Delete Object"), QKeySequence::Delete, this, &MainWindow::onDeleteSelected);

    // View menu: GUI Language (I18n) first, then Theme - both dynamically
    // built here, both persisted via QSettings and reloaded at the next
    // startup (see the constructor's own load-before-any-widget block),
    // both ported from AmigaED 4.0's own View menu at the Chefentwickler's
    // explicit request (see applyGuiLanguage()/buildThemeMenu() below for
    // the ground-truth source each was verified against).
    m_viewMenu = menuBar()->addMenu(tr("&View"));

    m_guiLanguageEnglishAct = new QAction(tr("English"), this);
    m_guiLanguageEnglishAct->setStatusTip(tr("Switch the GUI language to English"));
    m_guiLanguageEnglishAct->setCheckable(true);
    connect(m_guiLanguageEnglishAct, &QAction::triggered, this, &MainWindow::actionSetGuiLanguageEnglish);

    m_guiLanguageGermanAct = new QAction(tr("Deutsch"), this);
    m_guiLanguageGermanAct->setStatusTip(tr("Switch the GUI language to German"));
    m_guiLanguageGermanAct->setCheckable(true);
    connect(m_guiLanguageGermanAct, &QAction::triggered, this, &MainWindow::actionSetGuiLanguageGerman);

    m_guiLanguageGroup = new QActionGroup(this);
    m_guiLanguageGroup->addAction(m_guiLanguageEnglishAct);
    m_guiLanguageGroup->addAction(m_guiLanguageGermanAct);
    m_guiLanguageEnglishAct->setChecked(m_guiLanguage != QLatin1String("de"));
    m_guiLanguageGermanAct->setChecked(m_guiLanguage == QLatin1String("de"));

    m_guiLanguageMenu = m_viewMenu->addMenu(tr("GUI Language"));
    m_guiLanguageMenu->addAction(m_guiLanguageEnglishAct);
    m_guiLanguageMenu->addAction(m_guiLanguageGermanAct);

    buildThemeMenu();   // View > Theme - see its own doc comment; needs m_viewMenu, so must run after the line above

    m_windowToolbar = addToolBar(tr("Windows"));
    m_windowToolbar->setObjectName(QStringLiteral("windowSwitcherToolbar"));
    m_windowToolbarLabel = new QLabel(tr(" Window: "));
    m_windowToolbar->addWidget(m_windowToolbarLabel);
    m_windowSwitcher = new QComboBox(m_windowToolbar);
    m_windowSwitcher->setMinimumWidth(200);
    connect(m_windowSwitcher, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MainWindow::onWindowSwitcherChanged);
    m_windowToolbar->addWidget(m_windowSwitcher);
}

//
// Re-applies every tr()-wrapped string on a persistent widget after a
// runtime GUI-language change (View > GUI Language, or main.cpp's
// "--GUI_Language" command-line override) - see applyGuiLanguage(). Also
// re-runs updateRecentProjectsMenu()/updateWindowTitle(), since those
// build their OWN tr()-wrapped text (the "(no recent projects)"/"Clear
// List" entries, and the "Untitled" fallback) at a different time than
// this function and would otherwise go stale until the next unrelated
// change triggers them again.
//
void MainWindow::retranslateUi()
{
    m_fileMenu->setTitle(tr("&File"));
    m_newProjectAct->setText(tr("&New Project"));
    m_openProjectAct->setText(tr("&Open..."));
    m_recentProjectsMenu->setTitle(tr("&Recent Projects"));
    m_saveProjectAct->setText(tr("&Save"));
    m_saveProjectAsAct->setText(tr("Save &As..."));
    m_newWindowAct->setText(tr("New &Window"));
    m_deleteWindowAct->setText(tr("Delete Current Window"));
    m_menuEditorAct->setText(tr("&Edit Menu..."));
    m_appPropertiesAct->setText(tr("&Application Properties..."));
    m_dialogCreatorMenu->setTitle(tr("Dialog Creator"));
    m_aboutBoxAct->setText(tr("AboutBox..."));
    m_generateCodeAct->setText(tr("&Generate Code..."));
    m_finaliseAmigaEDProjectAct->setText(tr("Finalise AmigaED Project"));
    m_quitAct->setText(tr("&Quit"));
    updateRecentProjectsMenu();

    m_editMenu->setTitle(tr("&Edit"));
    m_deleteObjectAct->setText(tr("&Delete Object"));

    m_viewMenu->setTitle(tr("&View"));
    m_guiLanguageMenu->setTitle(tr("GUI Language"));
    m_guiLanguageEnglishAct->setText(tr("English"));
    m_guiLanguageEnglishAct->setStatusTip(tr("Switch the GUI language to English"));
    m_guiLanguageGermanAct->setText(tr("Deutsch"));
    m_guiLanguageGermanAct->setStatusTip(tr("Switch the GUI language to German"));
    if (m_themeMenu)
        m_themeMenu->setTitle(tr("Theme"));   // the theme ENTRIES themselves stay untranslated, see buildThemeMenu()

    m_windowToolbar->setWindowTitle(tr("Windows"));
    m_windowToolbarLabel->setText(tr(" Window: "));

    m_paletteDock->setWindowTitle(tr("Palette"));
    m_treeDock->setWindowTitle(tr("Project Tree"));
    m_inspectorDock->setWindowTitle(tr("Properties"));
    m_palette->retranslate();   // see its own doc comment - unlike the tree/canvas, it isn't rebuilt on its own
    m_inspector->retranslate(); // see its own doc comment - same reasoning, for whatever is currently selected
    m_canvas->retranslate();    // see its own doc comment - the "no window open" placeholder specifically

    updateWindowTitle();
}

void MainWindow::refreshWindowSwitcher()
{
    m_suppressWindowSwitcherSignal = true;
    m_windowSwitcher->clear();
    for (auto &win : m_project->windows)
    {
        const QString text = win->title.isEmpty() ? win->label : win->title;
        m_windowSwitcher->addItem(text, QVariant::fromValue<qulonglong>(reinterpret_cast<qulonglong>(win.get())));
    }
    MuibObject *current = m_canvas->currentWindow();
    for (int i = 0; i < m_windowSwitcher->count(); ++i)
    {
        auto *ptr = reinterpret_cast<MuibObject *>(m_windowSwitcher->itemData(i).toULongLong());
        if (ptr == current)
        {
            m_windowSwitcher->setCurrentIndex(i);
            break;
        }
    }
    m_suppressWindowSwitcherSignal = false;
}

void MainWindow::onWindowSwitcherChanged(int index)
{
    if (m_suppressWindowSwitcherSignal || index < 0)
        return;
    auto *ptr = reinterpret_cast<MuibObject *>(m_windowSwitcher->itemData(index).toULongLong());
    setCurrentWindow(ptr);
}

void MainWindow::setCurrentWindow(MuibObject *win)
{
    m_canvas->setWindow(win);
    m_inspector->showObject(win);
    m_tree->selectObject(win);
    refreshWindowSwitcher();
}

void MainWindow::refreshTreeAndKeepSelection()
{
    MuibObject *sel = m_canvas->selected();
    m_tree->setProject(m_project.get());
    m_tree->selectObject(sel);
}

void MainWindow::markDirty(bool dirty)
{
    m_dirty = dirty;
    updateWindowTitle();
}

void MainWindow::updateWindowTitle()
{
    const QString file = m_currentFilePath.isEmpty() ? tr("Untitled") : QFileInfo(m_currentFilePath).fileName();
    setWindowTitle(QStringLiteral("MuiBuilderQt - %1%2").arg(file, m_dirty ? QStringLiteral(" *") : QString()));
}

bool MainWindow::confirmDiscardUnsavedChanges()
{
    if (!m_dirty)
        return true;
    const auto res = QMessageBox::question(this, tr("Unsaved Changes"),
        tr("The current project has been modified. Save before continuing?"),
        QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);
    if (res == QMessageBox::Cancel)
        return false;
    if (res == QMessageBox::Save)
        return onSaveProject();
    return true;
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    if (confirmDiscardUnsavedChanges())
    {
        saveWindowSettings();
        event->accept();
    }
    else
    {
        event->ignore();
    }
}

void MainWindow::loadWindowSettings()
{
    QSettings settings;
    settings.beginGroup(QStringLiteral("UI"));
    const QByteArray geometry = settings.value(QStringLiteral("geometry")).toByteArray();
    const QByteArray state = settings.value(QStringLiteral("windowState")).toByteArray();
    settings.endGroup();

    // restoreGeometry()/restoreState() both return false (and leave the
    // window untouched) on empty/garbled/version-mismatched data - which
    // is exactly the "first ever launch, nothing saved yet" case, so no
    // explicit "is this the first run" check is needed: the resize(1400,
    // 850) + the dock defaults setupUi() already established simply
    // stay in effect whenever there is nothing (valid) to restore.
    if (!geometry.isEmpty())
        restoreGeometry(geometry);
    if (!state.isEmpty())
        restoreState(state, kWindowStateVersion);
}

void MainWindow::saveWindowSettings()
{
    QSettings settings;
    settings.beginGroup(QStringLiteral("UI"));
    settings.setValue(QStringLiteral("geometry"), saveGeometry());
    settings.setValue(QStringLiteral("windowState"), saveState(kWindowStateVersion));
    settings.endGroup();
}

// --- "Recent Projects" (recent projects) ----------------------------------

void MainWindow::loadRecentProjectsSettings()
{
    QSettings settings;
    settings.beginGroup(QStringLiteral("RecentProjects"));
    m_recentProjects = settings.value(QStringLiteral("List")).toStringList();
    settings.endGroup();
}

// (Re-)builds the "Recent Projects" submenu from m_recentProjects - list
// itself is stored oldest-first (so appending the newest is a plain
// append+dedupe, see addToRecentProjects()), but shown most-recent-first,
// same as AmigaED's own "Recent files"/"Recent Projects" submenus.
void MainWindow::updateRecentProjectsMenu()
{
    if (!m_recentProjectsMenu)
        return;
    m_recentProjectsMenu->clear();

    if (m_recentProjects.isEmpty())
    {
        QAction *emptyAct = m_recentProjectsMenu->addAction(tr("(no recent projects)"));
        emptyAct->setEnabled(false);
        return;
    }

    for (int i = m_recentProjects.count() - 1; i >= 0; --i)
    {
        const QString &path = m_recentProjects.at(i);
        QAction *act = m_recentProjectsMenu->addAction(QFileInfo(path).fileName());
        act->setToolTip(path);
        act->setData(path);   // full path, independent of the displayed basename
        connect(act, &QAction::triggered, this, &MainWindow::openRecentProject);
    }

    m_recentProjectsMenu->addSeparator();
    QAction *forgetAct = m_recentProjectsMenu->addAction(tr("Clear List"));
    connect(forgetAct, &QAction::triggered, this, [this]() {
        m_recentProjects.clear();
        QSettings settings;
        settings.beginGroup(QStringLiteral("RecentProjects"));
        settings.setValue(QStringLiteral("List"), m_recentProjects);
        settings.endGroup();
        updateRecentProjectsMenu();
    });
}

// Adds a freshly opened/saved project to the "Recent Projects" list - no
// duplicates (re-opening/re-saving a listed project just moves it back
// to the top), capped at kMaxRecentProjects entries (oldest dropped once
// full). Called from loadProjectFile() (covers both the Open... action
// and a .MUIB path given on the command line - main.cpp calls
// loadProjectFile() directly) and from onSaveProject() on success (covers
// both Speichern and Speichern unter..., since the latter just sets
// m_currentFilePath and delegates to the former) - same two call sites
// AmigaED's own addToRecentFiles() is hooked into.
void MainWindow::addToRecentProjects(const QString &path)
{
    if (path.isEmpty())
        return;

    m_recentProjects.removeAll(path);
    m_recentProjects.append(path);

    while (m_recentProjects.count() > kMaxRecentProjects)
        m_recentProjects.removeFirst();

    QSettings settings;
    settings.beginGroup(QStringLiteral("RecentProjects"));
    settings.setValue(QStringLiteral("List"), m_recentProjects);
    settings.endGroup();

    updateRecentProjectsMenu();
}

// Removes a (no longer existing) project from the "Recent Projects" list.
void MainWindow::removeFromRecentProjects(const QString &path)
{
    m_recentProjects.removeAll(path);

    QSettings settings;
    settings.beginGroup(QStringLiteral("RecentProjects"));
    settings.setValue(QStringLiteral("List"), m_recentProjects);
    settings.endGroup();

    updateRecentProjectsMenu();
}

// --- File menu actions ---------------------------------------------------

void MainWindow::onNewProject()
{
    if (!confirmDiscardUnsavedChanges())
        return;
    m_project = std::make_unique<MuibProject>();
    m_currentFilePath.clear();
    setCurrentWindow(nullptr);
    refreshTreeAndKeepSelection();
    markDirty(false);
}

// See the declaration in mainwindow.h for why this exists.
QString MainWindow::currentProjectDir() const
{
    if (m_currentFilePath.isEmpty())
        return QString();
    return QFileInfo(m_currentFilePath).absolutePath();
}

void MainWindow::onOpenProject()
{
    if (!confirmDiscardUnsavedChanges())
        return;
    const QString path = QFileDialog::getOpenFileName(this, tr("Open MUIB Project"), currentProjectDir(),
                                                        tr("MUIBuilder Projects (*.MUIB *.muib);;All Files (*)"));
    if (path.isEmpty())
        return;
    loadProjectFile(path);
}

// Called when the user clicks an entry in the "Recent Projects" submenu.
void MainWindow::openRecentProject()
{
    auto *act = qobject_cast<QAction *>(sender());
    if (!act)
        return;

    const QString path = act->data().toString();

    if (!QFile::exists(path))
    {
        QMessageBox::warning(this, tr("MuiBuilderQt"),
            tr("The file no longer exists:\n%1\n\n"
               "It will be removed from the \"Recent Projects\" list.").arg(path));
        removeFromRecentProjects(path);
        return;
    }

    if (!confirmDiscardUnsavedChanges())
        return;

    loadProjectFile(path);
}

void MainWindow::loadProjectFile(const QString &path)
{
    QString error;
    MuibLoader loader;
    auto proj = loader.loadFile(path, &error);
    if (!proj)
    {
        QMessageBox::critical(this, tr("Load Failed"), error.isEmpty() ? tr("Unknown error.") : error);
        return;
    }
    m_project = std::move(proj);
    m_currentFilePath = path;

    // MuiBuilderQt-only sidecar (AboutBox etc. - see muibqtextras.h) -
    // never part of the real .MUIB format, so a missing/absent sidecar
    // is normal and not reported to the user; a genuinely corrupt one
    // is (project itself already loaded fine either way).
    QString extrasError;
    if (!MuibQtExtras::load(*m_project, path, &extrasError) && !extrasError.isEmpty())
        QMessageBox::warning(this, tr("MuiBuilderQt"), extrasError);

    setCurrentWindow(m_project->windows.empty() ? nullptr : m_project->windows.front().get());
    refreshTreeAndKeepSelection();
    markDirty(false);
    addToRecentProjects(path);
}

bool MainWindow::onSaveProject()
{
    if (m_currentFilePath.isEmpty())
        return onSaveProjectAs();

    QString error;
    if (!MuibSaver::saveFile(*m_project, m_currentFilePath, &error))
    {
        QMessageBox::critical(this, tr("Save Failed"), error.isEmpty() ? tr("Unknown error.") : error);
        return false;
    }

    // MuiBuilderQt-only sidecar (AboutBox etc.) - see loadProjectFile()'s
    // matching MuibQtExtras::load() call and muibqtextras.h. A failure
    // here is reported but does not undo/fail the .MUIB save itself,
    // which already succeeded above.
    QString extrasError;
    if (!MuibQtExtras::save(*m_project, m_currentFilePath, &extrasError) && !extrasError.isEmpty())
        QMessageBox::warning(this, tr("MuiBuilderQt"), extrasError);

    markDirty(false);
    addToRecentProjects(m_currentFilePath);
    return true;
}

bool MainWindow::onSaveProjectAs()
{
    // Pre-fill with the current file's full path (not just its directory)
    // when there is one, so the dialog opens right on the project's own
    // file, name included - the standard Qt "Save As" convention, and the
    // same fix as onOpenProject()/onGenerateCode() for the same root
    // cause (see currentProjectDir()'s doc comment). Empty when there is
    // no current file yet (a brand-new, never-saved project), same as
    // before.
    const QString path = QFileDialog::getSaveFileName(this, tr("Save MUIB Project As"), m_currentFilePath,
                                                        tr("MUIBuilder Projects (*.MUIB)"));
    if (path.isEmpty())
        return false;
    m_currentFilePath = path;
    return onSaveProject();
}

void MainWindow::onNewWindow()
{
    auto win = createDefaultWindow(collectAllLabels(m_project.get()));
    MuibObject *raw = win.get();
    m_project->windows.push_back(std::move(win));
    setCurrentWindow(raw);
    refreshTreeAndKeepSelection();
    markDirty();
}

void MainWindow::onDeleteCurrentWindow()
{
    MuibObject *win = m_canvas->currentWindow();
    if (!win)
        return;
    if (QMessageBox::question(this, tr("Delete Window"),
                               tr("Really delete window \"%1\" (including all objects it contains)?")
                                   .arg(win->title.isEmpty() ? win->label : win->title))
        != QMessageBox::Yes)
        return;

    removeObjectFromProject(m_project.get(), win);
    setCurrentWindow(m_project->windows.empty() ? nullptr : m_project->windows.front().get());
    refreshTreeAndKeepSelection();
    markDirty();
}

void MainWindow::onGenerateCode()
{
    // Defaults to the CURRENT project's own directory (see
    // currentProjectDir()'s doc comment) - this is the user-reported bug:
    // opening a project via "Recent Projects" loaded it correctly, but
    // generating code from it still opened this picker at some unrelated
    // last-used/OS-default folder every time, forcing a manual
    // re-navigation back to the project's own folder before every single
    // code-generation run.
    const QString outDir = QFileDialog::getExistingDirectory(this, tr("Target Directory for Generated Code"),
                                                               currentProjectDir());
    if (outDir.isEmpty())
        return;
    const QString defaultBase = m_project->base.isEmpty() ? QStringLiteral("generated") : m_project->base;
    bool ok = false;
    const QString base = QInputDialog::getText(this, tr("Generate Code"), tr("Base name (.h/.c):"),
                                                 QLineEdit::Normal, defaultBase, &ok);
    if (!ok || base.isEmpty())
        return;

    QString error;
    if (!MuiCodeGen::generate(*m_project, outDir, base, &error))
    {
        QMessageBox::critical(this, tr("Code Generation Failed"), error.isEmpty() ? tr("Unknown error.") : error);
        return;
    }
    QMessageBox::information(this, tr("Code Generated"),
                              tr("%1.h / %1.c were generated in\n%2.").arg(base, outDir));
}

// See the declaration in mainwindow.h for the overall picture.
void MainWindow::enterAmigaEDNewProjectMode(const QString &dir, const QString &name, const QString &resultFile)
{
    m_amigaEdNewProjectMode = true;
    m_amigaEdProjectDir = dir;
    m_amigaEdProjectName = name;
    m_amigaEdResultFilePath = resultFile;

    // The project itself is already a blank, unsaved MuibProject at this
    // point (see the constructor) - this just gives it its intended save
    // path from the very first frame, so a plain Save/Ctrl+S already
    // lands exactly where AmigaED expects it, and the window title shows
    // the real target name right away instead of "Untitled".
    m_currentFilePath = dir + QDir::separator() + name + QStringLiteral(".MUIB");
    updateWindowTitle();

    if (m_finaliseAmigaEDProjectAct)
        m_finaliseAmigaEDProjectAct->setVisible(true);
}

// See the declaration in mainwindow.h for the overall picture.
void MainWindow::onFinaliseAmigaEDProject()
{
    if (!m_amigaEdNewProjectMode)
        return;   // the menu entry is hidden whenever this isn't true - defensive only

    // Confirmed BEFORE anything is written to disk, deliberately - once
    // the result file exists, AmigaED (watching for this process to exit)
    // will treat that as a completed handoff, so there must be no way to
    // back out partway through after it's already been written.
    if (QMessageBox::question(this, tr("Finalise AmigaED Project"),
            tr("This saves the project and generates its C source into:\n%1\n\n"
               "then closes MuiBuilderQt and hands the finished project back to AmigaED.\n\n"
               "Continue?").arg(m_amigaEdProjectDir),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes)
        != QMessageBox::Yes)
        return;

    const QString base = m_amigaEdProjectName;

    m_currentFilePath = m_amigaEdProjectDir + QDir::separator() + base + QStringLiteral(".MUIB");
    if (!onSaveProject())
        return;   // error already shown by onSaveProject()/MuibSaver - nothing written to the result file, AmigaED is none the wiser

    QString error;
    if (!MuiCodeGen::generate(*m_project, m_amigaEdProjectDir, base, &error))
    {
        QMessageBox::critical(this, tr("Code Generation Failed"), error.isEmpty() ? tr("Unknown error.") : error);
        return;   // same as above - the .MUIB was saved, but no result file means no handoff happened
    }

    // MuiCodeGen::generate() always writes exactly these 4 files (see
    // muicodegen.h's own doc comment on generate()/generateMain()) -
    // <base>_main.c is the one with main() in it (opens muimaster.library,
    // builds+shows the GUI, runs the event loop), so that's what AmigaED's
    // own Project::mainFile needs to point at, not <base>.c itself.
    const QString mainFile = m_amigaEdProjectDir + QDir::separator() + base + QStringLiteral("_main.c");
    const QStringList generatedFiles = {
        base + QStringLiteral(".h"),
        base + QStringLiteral(".c"),
        base + QStringLiteral("_main.c"),
        base + QStringLiteral("_gadgets.h"),
    };

    // Written via QSettings/IniFormat, read back the exact same way by
    // AmigaED's MainWindow::importGuiBuilderProject() - a plain flat
    // key=value file, no [section] needed for top-level keys.
    QSettings result(m_amigaEdResultFilePath, QSettings::IniFormat);
    result.setValue(QStringLiteral("MainFile"), mainFile);
    result.setValue(QStringLiteral("ProjectName"), base);
    result.setValue(QStringLiteral("GeneratedFiles"), generatedFiles);
    result.sync();
    if (result.status() != QSettings::NoError)
    {
        QMessageBox::critical(this, tr("Finalise Failed"),
            tr("Could not write the result file AmigaED is waiting for:\n%1").arg(m_amigaEdResultFilePath));
        return;   // still not closed - the user can fix the problem (e.g. free up disk space) and try again
    }

    // onSaveProject() above already cleared m_dirty, so closeEvent()'s own
    // confirmDiscardUnsavedChanges() call is a no-op here - this closes
    // without any further prompt, exactly as the confirmation dialog
    // above already told the user it would.
    close();
}

// --- Canvas/tree/inspector wiring ----------------------------------------

void MainWindow::onCanvasSelectionChanged(MuibObject *obj)
{
    m_inspector->showObject(obj);
    m_tree->selectObject(obj);
}

void MainWindow::onCanvasObjectAdded(MuibObject *targetGroup, ObjType type)
{
    auto obj = createDefaultObject(type, collectAllLabels(m_project.get()));
    MuibObject *raw = appendChildToGroup(targetGroup, std::move(obj));
    m_canvas->rebuild();
    m_canvas->setSelected(raw);
    refreshTreeAndKeepSelection();
    markDirty();
}

void MainWindow::onCanvasObjectReordered(MuibObject *movedObj, MuibObject *targetGroup, int newIndex)
{
    // moveObjectWithinGroup() only moves the unique_ptr within/between
    // `children` vectors - it never destroys the MuibObject itself, so
    // `movedObj` stays a valid pointer straight through rebuild() (unlike
    // onDeleteSelected(), no setSelectedSilently() dance is needed here).
    if (!moveObjectWithinGroup(movedObj, targetGroup, newIndex))
        return;
    m_canvas->rebuild();
    m_canvas->setSelected(movedObj);
    refreshTreeAndKeepSelection();
    markDirty();
}

void MainWindow::onTreeObjectSelected(MuibObject *obj)
{
    // If the selected tree item belongs to a DIFFERENT window than the
    // one currently on the canvas (e.g. the user expanded another
    // window's subtree without single-clicking its own top-level item),
    // onTreeWindowActivated() below already switched the canvas first -
    // Qt delivers windowActivated() before objectSelected() since
    // ObjectTreeView emits them in that order. So by the time this runs,
    // m_canvas is already showing the right window and a plain
    // setSelected() is enough.
    m_canvas->setSelected(obj);
    m_inspector->showObject(obj);
}

void MainWindow::onTreeWindowActivated(MuibObject *win)
{
    if (win != m_canvas->currentWindow())
    {
        m_canvas->setWindow(win);
        refreshWindowSwitcher();
    }
}

void MainWindow::onMenuEditor()
{
    MuibObject *win = m_canvas->currentWindow();
    if (!win)
    {
        QMessageBox::information(this, tr("Menu Editor"), tr("Please select a window first."));
        return;
    }

    // Menu-strip edits never touch the widget-layout tree the Canvas
    // shows (a Window's `menu` and `root` are entirely separate
    // subtrees - see muibobject.h), so unlike every other structural
    // edit in this file, this handler never calls m_canvas->rebuild()/
    // setSelected() (not meaningful here, since the Canvas has no
    // concept of a Menu-family object at all) - it just rebuilds/
    // reselects the tree afterwards, leaving the Canvas's own current
    // selection untouched.
    MenuEditorDialog dlg(win, collectAllLabels(m_project.get()), m_project.get(), this);
    if (dlg.exec() != QDialog::Accepted)
        return;

    // dlg.labelRenames() must be read BEFORE takeEditedMenu() below hands
    // ownership of the edited tree away - see labelRenames()'s own doc
    // comment for why (it walks m_root, which takeEditedMenu() releases).
    // Order relative to replaceWindowMenu() doesn't matter otherwise: the
    // rename map's keys are OLD labels that no longer occur anywhere in
    // win's own (now-replaced) menu, only possibly elsewhere in the
    // project, so the two calls can't interfere with each other.
    const QHash<QString, QString> renames = dlg.labelRenames();
    replaceWindowMenu(win, dlg.takeEditedMenu());
    applyLabelRenames(m_project.get(), renames);
    m_tree->setProject(m_project.get());
    m_tree->selectObject(win);
    m_inspector->showObject(win);
    markDirty();
}

void MainWindow::onInspectorFieldChanged(MuibObject *obj, bool structural)
{
    Q_UNUSED(obj);
    if (structural)
        m_canvas->rebuild();
    else
        m_canvas->refreshHeaders();
    refreshTreeAndKeepSelection();
    refreshWindowSwitcher();
    markDirty();
}

void MainWindow::onDialogCreatorAboutBox()
{
    AboutBoxDialog dlg(m_project.get(), this);
    if (dlg.exec() != QDialog::Accepted)
        return;
    dlg.apply();
    markDirty();
}

void MainWindow::onProjectProperties()
{
    ProjectPropertiesDialog dlg(m_project.get(), this);
    if (dlg.exec() != QDialog::Accepted)
        return;
    dlg.apply();
    markDirty();
}

void MainWindow::onDeleteSelected()
{
    MuibObject *sel = m_canvas->selected();
    if (!sel || sel == m_canvas->currentWindow())
        return;   // nothing selected, or trying to delete the window itself (use "Delete Current Window" instead)

    MuibObject *parent = sel->father ? sel->father : m_canvas->currentWindow();
    if (!removeObjectFromProject(m_project.get(), sel))
        return;   // e.g. attempted to delete a root group - not supported, see objecttreeutil.cpp

    m_canvas->setSelectedSilently(parent);
    m_canvas->rebuild();
    m_inspector->showObject(parent);
    refreshTreeAndKeepSelection();
    markDirty();
}

// --- GUI Language (I18n) --------------------------------------------------
//
// View > GUI Language entry clicked: unlike AmigaED's own
// actionSetGuiLanguageEnglish()/German() (which call applyGuiLanguage()
// with persist=false, a session-only quick-switch - AmigaED has a
// SEPARATE Prefs dialog field, "Default GUI Language", that's the only
// thing meant to change what the next start uses), MuiBuilderQt has no
// such separate preferences dialog at all - this View menu IS the only
// place to choose a language, so per the Chefentwickler's own spec
// ("Gewaehlte Einstellung soll gespeichert und beim naechsten Start des
// Programs geladen werden") clicking it must persist immediately.
//
void MainWindow::actionSetGuiLanguageEnglish()
{
    applyGuiLanguage(QStringLiteral("en"), true);
}

void MainWindow::actionSetGuiLanguageGerman()
{
    applyGuiLanguage(QStringLiteral("de"), true);
}

//
// Switches the running application's GUI language: (un)installs the
// QTranslator, updates the View > GUI Language menu's checked state and
// re-applies every translatable string via retranslateUi() - all without
// requiring a restart. Ported from AmigaED 4.0's own
// MainWindow::applyGuiLanguage() (mainwindow.cpp) - see this method's own
// callers for MuiBuilderQt's one deliberate difference from AmigaED
// (what `persist` means here, given MuiBuilderQt has no Prefs dialog).
//
// `persist=false` is used for two things: loading the SAVED default at
// startup (the constructor's very first block - nothing to persist, it
// was already read FROM the same setting) and main.cpp's
// "--GUI_Language" command-line override (an external caller's - i.e.
// AmigaED's own - CURRENT language for just this run, which must never
// silently overwrite whatever the user separately chose for
// MuiBuilderQt's own standalone use - see mainwindow.h's own comment).
//
void MainWindow::applyGuiLanguage(const QString &langCode, bool persist)
{
    if (m_guiTranslator)
    {
        qApp->removeTranslator(m_guiTranslator);
        delete m_guiTranslator;
        m_guiTranslator = nullptr;
    }
    if (m_qtBaseTranslator)
    {
        qApp->removeTranslator(m_qtBaseTranslator);
        delete m_qtBaseTranslator;
        m_qtBaseTranslator = nullptr;
    }

    if (langCode == QLatin1String("de"))
    {
        m_guiTranslator = new QTranslator(this);
        if (m_guiTranslator->load(QLocale(QLocale::German), QStringLiteral("muibuilderqt"), QStringLiteral("_"), QStringLiteral(":/translations")))
        {
            qApp->installTranslator(m_guiTranslator);
        }
        else
        {
            qDebug() << "Could not load muibuilderqt_de.qm - staying with English.";
            delete m_guiTranslator;
            m_guiTranslator = nullptr;
        }

        // Qt's OWN German translation - covers strings MuiBuilderQt's own
        // muibuilderqt_de.qm has no control over, most importantly
        // QMessageBox's standard button labels ("&Yes"/"&No"/"Cancel"/
        // "OK"/...) built by every QMessageBox::question()/etc. call
        // using QMessageBox::Yes|QMessageBox::No|... - never routed
        // through MuiBuilderQt's own tr() calls at all. Ground-truth
        // confirmed a real, reported AmigaED bug otherwise (see its own
        // applyGuiLanguage() comment) - ported the same fix here
        // preemptively. Loaded independently of m_guiTranslator above/its
        // success - a missing or unloadable qtbase_de.qm should never
        // take muibuilderqt_de.qm down with it.
        m_qtBaseTranslator = new QTranslator(this);
        if (m_qtBaseTranslator->load(QLocale(QLocale::German), QStringLiteral("qtbase"), QStringLiteral("_"), QStringLiteral(":/translations")))
        {
            qApp->installTranslator(m_qtBaseTranslator);
        }
        else
        {
            qDebug() << "Could not load qtbase_de.qm - Qt's own strings (e.g. QMessageBox standard buttons) stay in English.";
            delete m_qtBaseTranslator;
            m_qtBaseTranslator = nullptr;
        }
    }

    m_guiLanguage = m_guiTranslator ? QStringLiteral("de") : QStringLiteral("en");

    if (persist)
    {
        QSettings settings;
        settings.setValue(QStringLiteral("MISC/DefaultGUILanguage"), m_guiLanguage);
    }

    // The very first call happens from the constructor, before
    // setupMenusAndToolbar() has run - there is no menu or retranslateUi()
    // target yet. The installed translator alone is enough: every tr()
    // call from here on (during the initial GUI build-up) already
    // resolves against it. On every later call (menu click, command-line
    // override), update the menu's checked state and re-apply every
    // translatable string live.
    if (m_guiLanguageEnglishAct)
    {
        m_guiLanguageEnglishAct->setChecked(m_guiLanguage != QLatin1String("de"));
        m_guiLanguageGermanAct->setChecked(m_guiLanguage == QLatin1String("de"));
        retranslateUi();
    }
}

// --- Theme -----------------------------------------------------------------
//
// true if m_defaultStyle selects one of MuiBuilderQt's 4 synthetic
// themes, ported byte-for-byte (same names, same QPalette colour values)
// from AmigaED 4.0's own mainwindow.cpp - see darkApplicationPalette()/
// workbench13ApplicationPalette()/workbench31ApplicationPalette()/
// vscodeApplicationPalette() below and buildThemeMenu()/View > Theme.
//
bool MainWindow::isDarkTheme() const
{
    return m_defaultStyle == QLatin1String("Dark");
}

bool MainWindow::isWorkbench13Theme() const
{
    return m_defaultStyle == QLatin1String("Workbench 1.3");
}

bool MainWindow::isWorkbench31Theme() const
{
    return m_defaultStyle == QLatin1String("Workbench 3.1");
}

bool MainWindow::isVSCodeTheme() const
{
    return m_defaultStyle == QLatin1String("Visual Studio Code Dark");
}

//
// Builds the dark QPalette used together with the "Fusion" style for the
// "Dark" application style. Native styles mostly ignore a custom
// QPalette for their own chrome - Fusion is the only style that reliably
// honours it on every platform, which is why applyApplicationStyle()
// always forces Fusion whenever one of the 4 synthetic themes is
// selected, regardless of what m_defaultStyle used to say before. Colour
// values ported verbatim from AmigaED 4.0's own darkApplicationPalette().
//
QPalette MainWindow::darkApplicationPalette() const
{
    QPalette palette;

    palette.setColor(QPalette::Window,            QColor(0x35, 0x35, 0x35));
    palette.setColor(QPalette::WindowText,        QColor(0xd4, 0xd4, 0xd4));
    palette.setColor(QPalette::Base,              QColor(0x23, 0x23, 0x23));
    palette.setColor(QPalette::AlternateBase,     QColor(0x35, 0x35, 0x35));
    palette.setColor(QPalette::ToolTipBase,       QColor(0x35, 0x35, 0x35));
    palette.setColor(QPalette::ToolTipText,       QColor(0xd4, 0xd4, 0xd4));
    palette.setColor(QPalette::Text,              QColor(0xd4, 0xd4, 0xd4));
    palette.setColor(QPalette::Button,            QColor(0x3c, 0x3c, 0x3c));
    palette.setColor(QPalette::ButtonText,        QColor(0xd4, 0xd4, 0xd4));
    palette.setColor(QPalette::BrightText,        QColor(0xf4, 0x47, 0x47));
    palette.setColor(QPalette::Link,              QColor(0x56, 0x9c, 0xd6));
    palette.setColor(QPalette::LinkVisited,       QColor(0xb3, 0x92, 0xf0));
    palette.setColor(QPalette::Highlight,         QColor(0x26, 0x4f, 0x78));
    palette.setColor(QPalette::HighlightedText,   QColor(0xff, 0xff, 0xff));

    palette.setColor(QPalette::Disabled, QPalette::WindowText,      QColor(0x7f, 0x7f, 0x7f));
    palette.setColor(QPalette::Disabled, QPalette::Text,            QColor(0x7f, 0x7f, 0x7f));
    palette.setColor(QPalette::Disabled, QPalette::ButtonText,      QColor(0x7f, 0x7f, 0x7f));
    palette.setColor(QPalette::Disabled, QPalette::Highlight,       QColor(0x50, 0x50, 0x50));
    palette.setColor(QPalette::Disabled, QPalette::HighlightedText, QColor(0x7f, 0x7f, 0x7f));

    return palette;
}

//
// Evokes the classic Workbench 1.3 look - colours ported verbatim from
// AmigaED 4.0's own workbench13ApplicationPalette() (there measured
// directly from a real Workbench 1.3 screenshot, not guessed - see its
// own comment for the full rationale).
//
QPalette MainWindow::workbench13ApplicationPalette() const
{
    QPalette palette;

    const QColor blue(0x00, 0x55, 0xAA);
    const QColor orange(0xFF, 0x88, 0x00);

    palette.setColor(QPalette::Window,            blue);
    palette.setColor(QPalette::WindowText,        Qt::white);
    palette.setColor(QPalette::Base,              Qt::white);
    palette.setColor(QPalette::AlternateBase,     blue.lighter(130));
    palette.setColor(QPalette::ToolTipBase,       Qt::white);
    palette.setColor(QPalette::ToolTipText,       Qt::black);
    palette.setColor(QPalette::Text,              Qt::black);
    palette.setColor(QPalette::Button,            Qt::white);
    palette.setColor(QPalette::ButtonText,        blue);
    palette.setColor(QPalette::BrightText,        orange);
    palette.setColor(QPalette::Link,              Qt::white);
    palette.setColor(QPalette::LinkVisited,       QColor(0xDD, 0xDD, 0xDD));
    palette.setColor(QPalette::Highlight,         orange);
    palette.setColor(QPalette::HighlightedText,   Qt::black);

    palette.setColor(QPalette::Disabled, QPalette::WindowText,      QColor(0x99, 0xBB, 0xDD));
    palette.setColor(QPalette::Disabled, QPalette::Text,            QColor(0x99, 0x99, 0x99));
    palette.setColor(QPalette::Disabled, QPalette::ButtonText,      QColor(0x99, 0xBB, 0xDD));
    palette.setColor(QPalette::Disabled, QPalette::Highlight,       QColor(0xCC, 0xAA, 0x77));
    palette.setColor(QPalette::Disabled, QPalette::HighlightedText, QColor(0x66, 0x66, 0x66));

    return palette;
}

//
// Evokes the classic Workbench 3.1 look - colours ported verbatim from
// AmigaED 4.0's own workbench31ApplicationPalette() (there measured
// directly from a real Workbench 3.1 screenshot, not guessed).
//
QPalette MainWindow::workbench31ApplicationPalette() const
{
    QPalette palette;

    const QColor gray(0xAA, 0xAA, 0xAA);
    const QColor blue(0x00, 0x55, 0xAA);

    palette.setColor(QPalette::Window,            gray);
    palette.setColor(QPalette::WindowText,        Qt::black);
    palette.setColor(QPalette::Base,              Qt::white);
    palette.setColor(QPalette::AlternateBase,     gray.lighter(115));
    palette.setColor(QPalette::ToolTipBase,       Qt::white);
    palette.setColor(QPalette::ToolTipText,       Qt::black);
    palette.setColor(QPalette::Text,              Qt::black);
    palette.setColor(QPalette::Button,            Qt::white);
    palette.setColor(QPalette::ButtonText,        Qt::black);
    palette.setColor(QPalette::BrightText,        QColor(0xCC, 0x33, 0x22));
    palette.setColor(QPalette::Link,              blue);
    palette.setColor(QPalette::LinkVisited,       blue.darker(120));
    palette.setColor(QPalette::Highlight,         blue);
    palette.setColor(QPalette::HighlightedText,   Qt::white);

    palette.setColor(QPalette::Disabled, QPalette::WindowText,      QColor(0x77, 0x77, 0x77));
    palette.setColor(QPalette::Disabled, QPalette::Text,            QColor(0x77, 0x77, 0x77));
    palette.setColor(QPalette::Disabled, QPalette::ButtonText,      QColor(0x77, 0x77, 0x77));
    palette.setColor(QPalette::Disabled, QPalette::Highlight,       QColor(0x99, 0x99, 0x99));
    palette.setColor(QPalette::Disabled, QPalette::HighlightedText, QColor(0xDD, 0xDD, 0xDD));

    return palette;
}

//
// Builds the QPalette used together with "Fusion" for the "Visual Studio
// Code Dark" application style - every colour here is taken directly
// from Visual Studio Code's real "Dark+" theme definition, ported
// verbatim from AmigaED 4.0's own vscodeApplicationPalette() (see its own
// comment for exactly which VS Code UI token each QPalette role maps to).
//
QPalette MainWindow::vscodeApplicationPalette() const
{
    QPalette palette;

    palette.setColor(QPalette::Window,            QColor(0x25, 0x25, 0x26));   // sideBar.background
    palette.setColor(QPalette::WindowText,        QColor(0xcc, 0xcc, 0xcc));   // foreground
    palette.setColor(QPalette::Base,              QColor(0x1e, 0x1e, 0x1e));   // editor.background
    palette.setColor(QPalette::AlternateBase,     QColor(0x2d, 0x2d, 0x2d));   // tab.inactiveBackground / list hover
    palette.setColor(QPalette::ToolTipBase,       QColor(0x25, 0x25, 0x26));
    palette.setColor(QPalette::ToolTipText,       QColor(0xcc, 0xcc, 0xcc));
    palette.setColor(QPalette::Text,              QColor(0xd4, 0xd4, 0xd4));   // editor.foreground
    palette.setColor(QPalette::Button,            QColor(0x3c, 0x3c, 0x3c));   // input/dropdown.background
    palette.setColor(QPalette::ButtonText,        QColor(0xcc, 0xcc, 0xcc));
    palette.setColor(QPalette::BrightText,        QColor(0xf4, 0x84, 0x71));   // errorForeground-ish
    palette.setColor(QPalette::Link,              QColor(0x37, 0x94, 0xff));   // textLink.foreground
    palette.setColor(QPalette::LinkVisited,       QColor(0xb1, 0x80, 0xd7));
    palette.setColor(QPalette::Highlight,         QColor(0x09, 0x47, 0x71));   // list.activeSelectionBackground
    palette.setColor(QPalette::HighlightedText,   QColor(0xff, 0xff, 0xff));

    palette.setColor(QPalette::Disabled, QPalette::WindowText,      QColor(0x6a, 0x6a, 0x6a));
    palette.setColor(QPalette::Disabled, QPalette::Text,            QColor(0x6a, 0x6a, 0x6a));
    palette.setColor(QPalette::Disabled, QPalette::ButtonText,      QColor(0x6a, 0x6a, 0x6a));
    palette.setColor(QPalette::Disabled, QPalette::Highlight,       QColor(0x3c, 0x3c, 0x3c));
    palette.setColor(QPalette::Disabled, QPalette::HighlightedText, QColor(0x6a, 0x6a, 0x6a));

    return palette;
}

//
// Applies m_defaultStyle to the running application. Called once from
// the constructor (initial startup style, before any widget exists) and
// again, live, from applyTheme() whenever the style actually changes -
// so switching to/from any of the 4 synthetic themes takes effect
// immediately, no restart needed. Ported from AmigaED 4.0's own
// applyApplicationStyle(), with the editor-tab/lexer/output-pane
// re-colouring calls at its end dropped - MuiBuilderQt has no code
// editor or compiler output pane for those to apply to.
//
void MainWindow::applyApplicationStyle()
{
    if (isDarkTheme())
    {
        QApplication::setStyle(QStyleFactory::create("Fusion"));
        QApplication::setPalette(darkApplicationPalette());
    }
    else if (isWorkbench13Theme())
    {
        QApplication::setStyle(QStyleFactory::create("Fusion"));
        QApplication::setPalette(workbench13ApplicationPalette());
    }
    else if (isWorkbench31Theme())
    {
        QApplication::setStyle(QStyleFactory::create("Fusion"));
        QApplication::setPalette(workbench31ApplicationPalette());
    }
    else if (isVSCodeTheme())
    {
        QApplication::setStyle(QStyleFactory::create("Fusion"));
        QApplication::setPalette(vscodeApplicationPalette());
    }
    else
    {
        QApplication::setStyle(m_defaultStyle);
        // Undo a previously applied dark/Workbench/VS-Code palette (if
        // any) - go back to the newly chosen style's own standard
        // palette, rather than leaving a stale one in place after
        // switching away from one of the 4 synthetic themes above.
        if (QApplication::style())
            QApplication::setPalette(QApplication::style()->standardPalette());
    }

    // QMainWindow::separator - the drag handle between the docked Project
    // Tree/Palette, the central Canvas, and the docked Properties panel.
    // Bug report (Chefentwickler, with Dark-theme screenshots): under the
    // dark synthetic themes, the separator was only recognizable by its
    // grip dots, the bar itself effectively invisible. Root cause: none
    // of the 4 synthetic QPalettes above set QPalette::Light/Midlight/
    // Dark/Mid/Shadow (only ported the roles AmigaED's own palette
    // functions set - see their own comments), so Qt auto-derives those
    // from Window/Button - a reasonable default for a light-ish base
    // colour, but for a near-black base (Dark/Visual Studio Code Dark)
    // the derived shades compress toward black too, leaving Fusion's own
    // separator rendering (which relies on exactly those roles for its
    // bevel) with almost no contrast against the surrounding chrome. This
    // MuiBuilderQt-only addition (no AmigaED equivalent to port - see the
    // grip-visibility bug report) gives every one of the 4 themes its own
    // explicit, clearly visible separator colour instead, rather than
    // guessing which ones the auto-derivation happens to fail for.
    const QString separatorCss = isWorkbench13Theme()
        ? "QMainWindow::separator { background-color: #CCCCCC; width: 6px; height: 6px; }"
        : isWorkbench31Theme()
              ? "QMainWindow::separator { background-color: #666666; width: 6px; height: 6px; }"
              : isVSCodeTheme()
                    ? "QMainWindow::separator { background-color: #6e6e6e; width: 6px; height: 6px; }"
                    : "QMainWindow::separator { background-color: #5a5a5a; width: 6px; height: 6px; }";   // Dark

    if (isWorkbench13Theme())
    {
        // Fusion's own menu bar rendering synthesizes a shading gradient
        // from a single QPalette::Window colour - against this theme's
        // saturated blue, that washes out badly enough to make the
        // (white, per the palette above) menu text unreadable in places.
        // An explicit stylesheet for just the menu bar/menus sidesteps
        // that gradient synthesis entirely - ported verbatim from
        // AmigaED 4.0's own applyApplicationStyle() (the separator rule
        // appended at the end is MuiBuilderQt's own addition, see above).
        this->setStyleSheet(
            "QMenuBar { background-color: #0055AA; color: white; }"
            "QMenuBar::item { background-color: #0055AA; color: white; }"
            "QMenuBar::item:selected, QMenuBar::item:pressed { background-color: #FF8800; color: black; }"
            "QMenu { background-color: white; color: black; border: 1px solid #0055AA; }"
            "QMenu::item:selected { background-color: #FF8800; color: black; }"
            + separatorCss
        );
    }
    else if (isVSCodeTheme())
    {
        // Same "palette for the bulk, stylesheet for a couple of
        // standout details" approach as "Workbench 1.3" above, for the
        // status bar/menu bar/toolbar/tab-bar chrome Fusion's palette
        // alone doesn't reach - ported verbatim from AmigaED 4.0's own
        // applyApplicationStyle() (its own tab-bar rule is harmless here
        // even though MuiBuilderQt has no tab bar - kept for a byte-for-
        // byte port rather than a selectively-trimmed one; the separator
        // rule appended at the end is MuiBuilderQt's own addition, see
        // above).
        this->setStyleSheet(
            "QStatusBar { background-color: #007ACC; color: #ffffff; }"
            "QStatusBar::item { border: none; }"
            "QStatusBar QLabel { color: #ffffff; }"
            "QMenuBar { background-color: #3c3c3c; color: #cccccc; }"
            "QMenuBar::item { background-color: #3c3c3c; color: #cccccc; }"
            "QMenuBar::item:selected, QMenuBar::item:pressed { background-color: #094771; color: #ffffff; }"
            "QMenu { background-color: #252526; color: #cccccc; border: 1px solid #454545; }"
            "QMenu::item:selected { background-color: #094771; color: #ffffff; }"
            "QToolBar { background-color: #333333; border: none; spacing: 2px; }"
            "QTabBar::tab { background-color: #2d2d2d; color: #969696; padding: 4px 10px; }"
            "QTabBar::tab:selected { background-color: #1e1e1e; color: #ffffff; }"
            + separatorCss
        );
    }
    else if (isDarkTheme() || isWorkbench31Theme())
    {
        // Neither of these 2 themes needed a stylesheet before this
        // separator fix (their palettes alone rendered every other
        // control acceptably) - now they get one just for the separator.
        this->setStyleSheet(separatorCss);
    }
    else
    {
        this->setStyleSheet(QString());   // clear any stale stylesheet override, e.g. after switching away from one of the 4 synthetic themes
    }

    syncThemeMenuCheckedState();
}

//
// Applies `styleName` as the new m_defaultStyle, persists it (unless
// `persist` is false - see mainwindow.h's own comment on when a caller
// passes false) and applies it immediately. Public wrapper used both by
// actionSelectTheme() (View > Theme menu clicks - always persist, same
// reasoning as applyGuiLanguage()'s own comment: no separate Prefs
// dialog here) and by main.cpp's "--GUI_Theme" command-line option
// (persist=false - a this-run-only override reflecting AmigaED's own
// current theme).
//
void MainWindow::applyTheme(const QString &styleName, bool persist)
{
    m_defaultStyle = styleName;

    if (persist)
    {
        QSettings settings;
        settings.setValue(QStringLiteral("MISC/DefaultStyle"), m_defaultStyle);
    }

    applyApplicationStyle();   // also calls syncThemeMenuCheckedState()
}

//
// Populates View > Theme with one checkable entry per style available on
// this platform (QStyleFactory::keys()), followed by a separator and the
// 4 synthetic entries (Dark, Workbench 1.3, Workbench 3.1, Visual Studio
// Code Dark) that aren't real QStyleFactory keys - byte-for-byte the same
// set AmigaED 4.0's own buildThemeMenu() offers. All of them share one
// QActionGroup (m_themeActionGroup) for mutual exclusion, and one shared
// slot (actionSelectTheme()), which reads whichever action actually
// fired via sender() rather than needing a separate slot per entry.
// Called once from setupMenusAndToolbar(), after m_viewMenu exists and
// m_defaultStyle has already been loaded (constructor) and applied
// (applyApplicationStyle()).
//
void MainWindow::buildThemeMenu()
{
    m_themeMenu = m_viewMenu->addMenu(tr("Theme"));
    m_themeActionGroup = new QActionGroup(this);
    m_themeActionGroup->setExclusive(true);

    const QStringList nativeStyles = QStyleFactory::keys();
    for (const QString &styleName : nativeStyles)
    {
        QAction *act = m_themeMenu->addAction(styleName);
        act->setCheckable(true);
        m_themeActionGroup->addAction(act);
        connect(act, &QAction::triggered, this, &MainWindow::actionSelectTheme);
    }

    m_themeMenu->addSeparator();

    // Kept untranslated, like the native style names above (Qt style keys
    // aren't translated either) - MISC/DefaultStyle is saved/restored as
    // this exact literal text, which this menu must keep matching exactly.
    const QStringList syntheticThemes = { QStringLiteral("Dark"),
                                           QStringLiteral("Workbench 1.3"),
                                           QStringLiteral("Workbench 3.1"),
                                           QStringLiteral("Visual Studio Code Dark") };
    for (const QString &name : syntheticThemes)
    {
        QAction *act = m_themeMenu->addAction(name);
        act->setCheckable(true);
        m_themeActionGroup->addAction(act);
        connect(act, &QAction::triggered, this, &MainWindow::actionSelectTheme);
    }

    syncThemeMenuCheckedState();
}

//
// View > Theme entry clicked: applies the clicked action's own text as
// the new theme, via applyTheme() (persist=true - see its own comment).
//
void MainWindow::actionSelectTheme()
{
    QAction *act = qobject_cast<QAction *>(sender());
    if (!act)
        return;
    applyTheme(act->text(), true);
}

//
// Ensures the entry matching the current m_defaultStyle is checked (and,
// via m_themeActionGroup's exclusivity, every other entry isn't) - called
// after buildThemeMenu() and every time applyApplicationStyle() runs. A
// no-op if the menu hasn't been built yet (see buildThemeMenu()'s call
// site in setupMenusAndToolbar() vs. applyApplicationStyle()'s own,
// earlier one from the constructor).
//
void MainWindow::syncThemeMenuCheckedState()
{
    if (!m_themeMenu)
        return;

    const QList<QAction *> actions = m_themeMenu->actions();
    for (QAction *act : actions)
    {
        if (act->isCheckable())
            act->setChecked(act->text() == m_defaultStyle);
    }
}
