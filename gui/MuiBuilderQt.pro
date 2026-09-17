QT += core gui widgets
CONFIG += c++17
TARGET = MuiBuilderQt
TEMPLATE = app

INCLUDEPATH += ../core

HEADERS += \
    ../core/muibobject.h \
    ../core/bytecursor.h \
    ../core/muibloader.h \
    ../core/muibsaver.h \
    ../core/muicodegen.h \
    ../core/notifytables.h \
    ../core/muibqtextras.h \
    objectfactory.h \
    objecttreeutil.h \
    objectbox.h \
    canvaswidget.h \
    widgetpalette.h \
    objecttreeview.h \
    propertyinspector.h \
    aboutboxdialog.h \
    projectpropertiesdialog.h \
    menueditordialog.h \
    mainwindow.h

SOURCES += \
    main.cpp \
    ../core/muibloader.cpp \
    ../core/muibsaver.cpp \
    ../core/muicodegen.cpp \
    ../core/notifytables.cpp \
    ../core/muibqtextras.cpp \
    objectfactory.cpp \
    objecttreeutil.cpp \
    objectbox.cpp \
    canvaswidget.cpp \
    widgetpalette.cpp \
    objecttreeview.cpp \
    propertyinspector.cpp \
    aboutboxdialog.cpp \
    projectpropertiesdialog.cpp \
    menueditordialog.cpp \
    mainwindow.cpp

RESOURCES += translations.qrc images.qrc

TRANSLATIONS += translations/muibuilderqt_de.ts

# Windows .exe icon (Explorer/taskbar/Alt-Tab) - embedded into the binary at
# link time via qmake's auto-generated .rc script, same mechanism as
# AmigaED.pro's own RC_ICONS. This is separate from images.qrc/
# app.setWindowIcon() above (main.cpp) - that one sets the icon shown at
# RUNTIME (window titlebar, taskbar entry while running), this one sets the
# icon of the .exe FILE ITSELF, visible even before the app is started.
# Both point at the same artwork (images/muibuilderqt.png / .ico), just in
# the two different formats each mechanism needs.
win32: RC_ICONS = images/muibuilderqt.ico

win32 {
    # Without an explicit DESTDIR, this mkspec places the built .exe in a
    # "release" subfolder under OUT_PWD, not directly in OUT_PWD itself -
    # and that subfolder's exact name isn't something to reliably guess/
    # hardcode. Pinning DESTDIR to OUT_PWD removes that extra subfolder
    # entirely, so the .exe (and therefore windeployqt's target/the staging
    # step below) always sits at a single, predictable path. Only affects
    # the final binary's location, not where intermediate object files go.
    # (Same reasoning/pattern as AmigaED.pro's own win32 block.)
    DESTDIR = $$OUT_PWD

    # After a successful build, run windeployqt right in the .exe's own
    # output folder so it drops in the Qt DLLs (platform plugin etc.) the
    # app actually needs to run standalone there. Located via
    # QT_INSTALL_BINS - qmake's own query for "the bin/ folder of the Qt
    # kit currently being used to build" (windeployqt always ships right
    # there, next to qmake itself) - rather than assuming windeployqt is
    # already on PATH, so this works the same way whether the build is
    # triggered by Qt Creator or by running qmake6/mingw32-make from a
    # plain console, and always matches whichever Qt kit is actually
    # active for the build. Unlike AmigaED, MuiBuilderQt has no
    # third-party DLL dependency (no QScintilla or similar) - windeployqt
    # alone covers everything this app needs.
    WINDEPLOYQT_BIN = $$[QT_INSTALL_BINS]/windeployqt.exe
    WINDEPLOYQT_TARGET = $$DESTDIR/$${TARGET}.exe

    # windeployqt is run via an explicit "cmd /c ..." rather than relying
    # on whichever shell mingw32-make happens to invoke recipe lines with -
    # it doesn't always use cmd.exe: if it finds ANY sh.exe on PATH, e.g.
    # one bundled with a cross-compiler toolchain, it uses that instead,
    # which breaks path handling for a bare (non-cmd-wrapped) recipe line.
    # (Same gotcha AmigaED.pro's own win32 block documents at length - see
    # that file for the full background.)
    RETURN = $$escape_expand(\n\t)
    QMAKE_POST_LINK += cmd /c $$WINDEPLOYQT_BIN $$WINDEPLOYQT_TARGET

    # --- "MuiBuilderQt_install\install_src" staging folder ----------------
    # After windeployqt above is done, stage a clean, install-ready copy of
    # everything a distributable MuiBuilderQt needs - the .exe, every Qt
    # DLL, and every plugin subfolder windeployqt created (platforms,
    # styles, imageformats, etc.) - into <project root>\MuiBuilderQt_install
    # \install_src. That folder is meant to be handed straight to the .iss
    # installer script as its SourceDir (see
    # MuiBuilderQt_install\MuiBuilderQt.iss).
    #
    # Note this .pro file lives in gui\, not the project root (unlike
    # AmigaED.pro) - $$PWD here is gui\, so the staging folder is placed
    # one level up ($$PWD/..) to sit as a sibling of core\/gui\/tests\,
    # matching where MuiBuilderQt_install\MuiBuilderQt.iss itself lives.
    #
    # The actual copy logic (clean, recreate, robocopy) lives in
    # install_stage.bat, a plain static file shipped alongside this .pro -
    # NOT built up as qmake-generated recipe text. AmigaED.pro's own win32
    # block documents why in detail: three earlier attempts at doing this
    # directly in QMAKE_POST_LINK all hit reproducible wildcard/backslash
    # corruption caused by mingw32-make's own recipe-line shell (not
    # necessarily cmd.exe). Moving the wildcards/options into a static
    # .bat file sidesteps that - the recipe line below only ever has to
    # pass two wildcard-free paths through to "cmd /c install_stage.bat".
    #
    # Deliberately using $$replace(..., /, \\) instead of $$shell_path()
    # for the same reason as AmigaED.pro: $$shell_path() adapts to
    # whichever shell qmake believes will run the build and can rewrite
    # these into an MSYS-style form cmd.exe can't parse.
    INSTALL_SRC_DIR = $$PWD/../MuiBuilderQt_install/install_src
    INSTALL_SRC_DIR_WIN = $$replace(INSTALL_SRC_DIR, /, \\)
    DESTDIR_WIN = $$replace(DESTDIR, /, \\)
    INSTALL_STAGE_BAT_SRC = $$PWD/install_stage.bat
    INSTALL_STAGE_BAT = $$replace(INSTALL_STAGE_BAT_SRC, /, \\)

    QMAKE_POST_LINK += $$RETURN cmd /c $$INSTALL_STAGE_BAT $$DESTDIR_WIN $$INSTALL_SRC_DIR_WIN
}

DISTFILES += \
    install_stage.bat \
    ../MuiBuilderQt_install/MuiBuilderQt.iss \
    images/muibuilderqt.ico \
    images/muibuilderqt.png
