#include "mainwindow.h"
#include <QApplication>
#include <QCommandLineParser>
#include <QCommandLineOption>
#include <QIcon>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("MuiBuilderQt"));
    app.setOrganizationName(QStringLiteral("MB-SoftWorX"));

    // Application-wide icon: set here (before any window - including
    // MainWindow itself - is constructed/shown) so that window managers on
    // Linux pick it up right from process start for the taskbar/dock/
    // alt-tab switcher, and so every top-level window (dialogs included)
    // gets it as their default, not just MainWindow. MainWindow's own
    // constructor additionally calls setWindowIcon(...) on itself further
    // down - harmless/redundant there, kept for clarity - but THIS call is
    // what makes the icon available at the earliest possible moment, which
    // is what Linux desktop environments look at. Mirrors AmigaED's own
    // main.cpp (see its comment there for the same reasoning) - the
    // Windows .exe/taskbar icon itself is a separate mechanism (RC_ICONS
    // in MuiBuilderQt.pro, embedded at link time), not this call.
    app.setWindowIcon(QIcon(QStringLiteral(":/images/muibuilderqt.png")));

    // --GUI_Language / --GUI_Theme: for launching MuiBuilderQt as an
    // external tool from AmigaED, so THAT run reflects AmigaED's own
    // currently active language/theme immediately, without silently
    // overwriting whatever MuiBuilderQt's own standalone use separately
    // saved as its default (View > GUI Language / View > Theme) - see
    // MainWindow::applyGuiLanguage()/applyTheme()'s own header comments.
    // Deliberately session-only: both are applied below with
    // persist=false, per the Chefentwickler's own explicit confirmation
    // ("Nur fuer diese Sitzung"). AmigaED itself has no QCommandLineParser
    // precedent of its own (just a naive argv[1]-opens-a-file convention,
    // preserved below as MuiBuilderQt's own positional argument) - this
    // is introduced fresh here.
    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("MuiBuilderQt - a Qt6/C++ MUI GUI builder"));
    parser.addHelpOption();
    parser.addPositionalArgument(QStringLiteral("project"),
        QStringLiteral("Optional .MUIB project file to open on startup."),
        QStringLiteral("[project.MUIB]"));

    QCommandLineOption languageOption(QStringLiteral("GUI_Language"),
        QStringLiteral("GUI language for this run only (\"en\" or \"de\") - "
                        "does not change MuiBuilderQt's own saved default."),
        QStringLiteral("language"));
    parser.addOption(languageOption);

    QCommandLineOption themeOption(QStringLiteral("GUI_Theme"),
        QStringLiteral("Theme/style for this run only - does not change "
                        "MuiBuilderQt's own saved default. Any native Qt "
                        "style name, or one of the 4 synthetic themes "
                        "(Dark, \"Workbench 1.3\", \"Workbench 3.1\", "
                        "\"Visual Studio Code Dark\")."),
        QStringLiteral("theme"));
    parser.addOption(themeOption);

    // --AmigaED_ProjectDir/--AmigaED_ProjectName/--AmigaED_ResultFile
    // (rev.160): AmigaED's own "New Project > GUI Builder Projects > MUI"
    // launches MuiBuilderQt with all 3 of these together (see AmigaED's
    // MainWindow::launchGuiBuilderForNewProject()) to create a brand new
    // project's GUI here interactively rather than from a static
    // template. All 3 together (never fewer - AmigaED always passes all
    // three, and a partial set below is treated as "not this mode at
    // all", see the isSet() checks further down) put this window into
    // MainWindow::enterAmigaEDNewProjectMode() - the resulting "File >
    // Finalise AmigaED Project" is what actually writes ResultFile and
    // hands the finished project back, see onFinaliseAmigaEDProject().
    QCommandLineOption amigaEdProjectDirOption(QStringLiteral("AmigaED_ProjectDir"),
        QStringLiteral("Internal - used when AmigaED launches MuiBuilderQt "
                        "to create a new project. Target directory for the "
                        "new project."),
        QStringLiteral("dir"));
    parser.addOption(amigaEdProjectDirOption);

    QCommandLineOption amigaEdProjectNameOption(QStringLiteral("AmigaED_ProjectName"),
        QStringLiteral("Internal - used when AmigaED launches MuiBuilderQt "
                        "to create a new project. The new project's name."),
        QStringLiteral("name"));
    parser.addOption(amigaEdProjectNameOption);

    QCommandLineOption amigaEdResultFileOption(QStringLiteral("AmigaED_ResultFile"),
        QStringLiteral("Internal - used when AmigaED launches MuiBuilderQt "
                        "to create a new project. Path of the result file "
                        "\"File > Finalise AmigaED Project\" writes once "
                        "the project is handed back to AmigaED."),
        QStringLiteral("path"));
    parser.addOption(amigaEdResultFileOption);

    parser.process(app);

    // MainWindow's constructor already loads and applies MuiBuilderQt's
    // own persisted GUI language/theme defaults (same QSettings keys
    // View > GUI Language / View > Theme write to) - so by this point the
    // window already reflects those. Only override them afterwards, and
    // only if the corresponding option was actually passed, so a plain
    // standalone start (no CLI overrides at all) behaves exactly as
    // before this feature existed.
    MainWindow window;

    if (parser.isSet(languageOption))
        window.applyGuiLanguage(parser.value(languageOption), /*persist=*/false);
    if (parser.isSet(themeOption))
        window.applyTheme(parser.value(themeOption), /*persist=*/false);

    // See the 3 options' own comments above - AmigaED always passes all
    // three together, so requiring all three here (rather than each
    // independently) is deliberate: a partial/malformed set (which
    // AmigaED itself would never produce) is simply ignored instead of
    // risking a half-configured "AmigaED mode".
    if (parser.isSet(amigaEdProjectDirOption) && parser.isSet(amigaEdProjectNameOption)
        && parser.isSet(amigaEdResultFileOption))
    {
        window.enterAmigaEDNewProjectMode(parser.value(amigaEdProjectDirOption),
                                           parser.value(amigaEdProjectNameOption),
                                           parser.value(amigaEdResultFileOption));
    }

    window.show();

    // "MuiBuilderQt <path-to-.MUIB>" opens that project right away -
    // mirrors AmigaED's own argv[1]-opens-a-file convention, now expressed
    // as QCommandLineParser's positional-argument list instead of a raw
    // argv[1] read (so it coexists cleanly with --GUI_Language/--GUI_Theme
    // in any order, e.g. "MuiBuilderQt --GUI_Language=de project.MUIB").
    const QStringList positional = parser.positionalArguments();
    if (!positional.isEmpty())
        window.loadProjectFile(positional.first());

    return app.exec();
}
