// Headless sanity/regression test for MuiCodeGen (task #16).
//
// This does NOT compile the generated C against a real Amiga SDK (no such
// toolchain is available here) - it's a structural sanity check: every
// real example project loads, generates non-empty .h/.c text, the text is
// balanced (parens/braces/brackets), every window gets a Build*Window()
// function, and BuildApplication() is present. The smallest project's
// output is also written to disk for manual inspection.
//
// Run with: ./test_codegen <path-to-BuilderSave-folder> <output-dir>

#include "muibobject.h"
#include "muibloader.h"
#include "muicodegen.h"
#include "muibqtextras.h"

#include <QCoreApplication>
#include <QDir>
#include <QDebug>
#include <QFile>
#include <QRegularExpression>

static int g_failures = 0;

#define CHECK(cond, msg) \
    do { \
        if (!(cond)) { qCritical() << "FAIL:" << msg; g_failures++; } \
        else { qInfo() << "OK:  " << msg; } \
    } while (0)

// Simple balance checker for (), {}, [] - catches gross structural bugs
// (a missing End/closing paren, an unterminated array literal, etc.)
// without needing a real C parser. Skips text inside "..." string
// literals (honoring \" and \\ escapes) and /* ... */ comments, since a
// bracket character appearing in project text (e.g. a title like
// "Amiga 1000 :)") or in one of this generator's own inline comments is
// not a real structural token.
static bool isBalanced(const QString &text, QString *detail)
{
    QVector<QChar> stack;
    bool inString = false;
    bool inComment = false;
    for (int i = 0; i < text.size(); ++i)
    {
        QChar c = text.at(i);
        if (inComment)
        {
            if (c == '*' && i + 1 < text.size() && text.at(i + 1) == '/')
            {
                inComment = false;
                ++i;
            }
            continue;
        }
        if (inString)
        {
            if (c == '\\' && i + 1 < text.size())
                ++i; // skip the escaped character (handles \" and \\)
            else if (c == '"')
                inString = false;
            continue;
        }
        if (c == '"')
        {
            inString = true;
            continue;
        }
        if (c == '/' && i + 1 < text.size() && text.at(i + 1) == '*')
        {
            inComment = true;
            ++i;
            continue;
        }
        if (c == '(' || c == '{' || c == '[')
            stack.push_back(c);
        else if (c == ')' || c == '}' || c == ']')
        {
            if (stack.isEmpty())
            {
                *detail = QStringLiteral("unmatched closing '%1' at offset %2").arg(c).arg(i);
                return false;
            }
            QChar open = stack.takeLast();
            QChar expected = (open == '(') ? ')' : (open == '{') ? '}' : ']';
            if (c != expected)
            {
                *detail = QStringLiteral("mismatched '%1' (expected '%2') at offset %3").arg(c).arg(expected).arg(i);
                return false;
            }
        }
    }
    if (!stack.isEmpty())
    {
        *detail = QStringLiteral("%1 unclosed opening bracket(s), first is '%2'").arg(stack.size()).arg(stack.first());
        return false;
    }
    return true;
}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    if (argc < 3)
    {
        qCritical() << "Usage:" << argv[0] << "<path-to-BuilderSave-folder> <output-dir>";
        return 2;
    }

    QDir dir(QString::fromLocal8Bit(argv[1]));
    QString outDir = QString::fromLocal8Bit(argv[2]);
    QDir().mkpath(outDir);

    QStringList files = dir.entryList(QStringList() << "*.MUIB", QDir::Files, QDir::Name);
    CHECK(!files.isEmpty(), "at least one .MUIB example file found");

    QString smallestFile;
    qint64 smallestSize = -1;
    for (const QString &fname : files)
    {
        qint64 sz = QFile(dir.filePath(fname)).size();
        if (smallestSize < 0 || sz < smallestSize)
        {
            smallestSize = sz;
            smallestFile = fname;
        }
    }

    for (const QString &fname : files)
    {
        QString path = dir.filePath(fname);
        MuibLoader loader;
        QString err;
        std::unique_ptr<MuibProject> proj = loader.loadFile(path, &err);
        CHECK(proj != nullptr, qPrintable(QStringLiteral("load succeeds: %1 (%2)").arg(fname, err)));
        if (!proj)
            continue;

        QString base = fname;
        base.replace(QStringLiteral(".MUIB"), QString());

        QString headerText = MuiCodeGen::generateHeader(*proj);
        QString sourceText = MuiCodeGen::generateSource(*proj, base);
        QString gadgetsText = MuiCodeGen::generateGadgetStubs(*proj, base);
        QString mainText = MuiCodeGen::generateMain(*proj, base);

        CHECK(sourceText.contains(QStringLiteral("#include \"%1.h\"").arg(base)),
              qPrintable(QStringLiteral("%1: source #includes its own generated header (%1.h)").arg(fname)));
        CHECK(sourceText.contains(QStringLiteral("#include \"%1_gadgets.h\"").arg(base)),
              qPrintable(QStringLiteral("%1: source #includes its own generated gadgets header (%1_gadgets.h)").arg(fname)));
        CHECK(mainText.contains(QStringLiteral("#include \"%1.h\"").arg(base)),
              qPrintable(QStringLiteral("%1: main #includes its own generated header (%1.h)").arg(fname)));

        CHECK(!headerText.isEmpty(), qPrintable(QStringLiteral("%1: header text non-empty").arg(fname)));
        CHECK(!sourceText.isEmpty(), qPrintable(QStringLiteral("%1: source text non-empty").arg(fname)));
        CHECK(!gadgetsText.isEmpty(), qPrintable(QStringLiteral("%1: gadgets-header text non-empty").arg(fname)));
        CHECK(!mainText.isEmpty(), qPrintable(QStringLiteral("%1: main text non-empty").arg(fname)));

        QString detail;
        bool hBalanced = isBalanced(headerText, &detail);
        CHECK(hBalanced, qPrintable(QStringLiteral("%1: header brackets balanced (%2)").arg(fname, hBalanced ? QStringLiteral("ok") : detail)));
        bool cBalanced = isBalanced(sourceText, &detail);
        CHECK(cBalanced, qPrintable(QStringLiteral("%1: source brackets balanced (%2)").arg(fname, cBalanced ? QStringLiteral("ok") : detail)));
        bool gBalanced = isBalanced(gadgetsText, &detail);
        CHECK(gBalanced, qPrintable(QStringLiteral("%1: gadgets-header brackets balanced (%2)").arg(fname, gBalanced ? QStringLiteral("ok") : detail)));
        bool mBalanced = isBalanced(mainText, &detail);
        CHECK(mBalanced, qPrintable(QStringLiteral("%1: main brackets balanced (%2)").arg(fname, mBalanced ? QStringLiteral("ok") : detail)));

        CHECK(sourceText.contains(QStringLiteral("BuildApplication")),
              qPrintable(QStringLiteral("%1: BuildApplication() present").arg(fname)));

        // cStringLiteral() must pass a backslash already present in a
        // .MUIB text field straight through (a doubled "\\" is the exact,
        // confirmed shape of a real bug: MUI PreParse codes like "\033c"
        // are stored in the .MUIB file as that literal escape text, meant
        // to be reproduced verbatim so the C COMPILER's own octal-escape
        // parsing turns it into a real ESC byte - doubling the backslash
        // instead made the compiled program print the literal characters
        // "\033c" on screen rather than applying the formatting code).
        CHECK(!headerText.contains(QStringLiteral("\\\\")) &&
                  !sourceText.contains(QStringLiteral("\\\\")) &&
                  !gadgetsText.contains(QStringLiteral("\\\\")) &&
                  !mainText.contains(QStringLiteral("\\\\")),
              qPrintable(QStringLiteral("%1: no doubled backslash in any generated string literal").arg(fname)));
        if (fname == QStringLiteral("click.MUIB"))
        {
            CHECK(sourceText.contains(QStringLiteral("\"\\0338\\033cClick on buttons\"")),
                  qPrintable(QStringLiteral("%1: the real \"Click\" demo's PreParse-coded label reproduces its exact original escape text").arg(fname)));
        }

        // "MUIA_Filelist_Drawer" / "Filelist.mcc" were never real MUI identifiers -
        // a real vbcc/NDK3.2 build of the "Small" example (which uses a DirList
        // gadget) failed with "unknown identifier <MUIA_Filelist_Drawer>". Cross-
        // checked against MUI's own official autodocs: the real class is "Dirlist"
        // (a base-MUI subclass of List.mui, present since muimaster.library V4),
        // convenience macro DirlistObject, real attribute MUIA_Dirlist_Directory.
        CHECK(!sourceText.contains(QStringLiteral("Filelist.mcc")) &&
                  !sourceText.contains(QStringLiteral("MUIA_Filelist_Drawer")),
              qPrintable(QStringLiteral("%1: no invented Filelist.mcc/MUIA_Filelist_Drawer identifiers").arg(fname)));
        if (fname == QStringLiteral("Small.MUIB"))
        {
            CHECK(sourceText.contains(QStringLiteral("DirlistObject,")),
                  qPrintable(QStringLiteral("%1: DirList gadget uses the real DirlistObject macro").arg(fname)));
            CHECK(sourceText.contains(QStringLiteral("MUIA_Dirlist_Directory,")),
                  qPrintable(QStringLiteral("%1: DirList gadget uses the real MUIA_Dirlist_Directory attribute").arg(fname)));
        }

        // "MUII_WarpNTBadge" was never a real MUI identifier either - a real
        // vbcc/NDK3.2 build of "DVIPrint" (which has an Image gadget with no
        // custom spec string, i.e. a built-in picker image) failed with
        // "unknown identifier <MUII_WarpNTBadge>". Cross-checked against the
        // original MUIBuilder's own code.c (TY_IMAGE case): when there's no
        // spec string it writes the raw MUIA_Image_Spec integer straight
        // through with no symbolic-name translation - so this generator now
        // does the same with its own loaded `imgType`, instead of guessing a
        // macro name. Also verifies the *other* half of that same code.c
        // case: a custom spec string always gets a "5:" prefix at generation
        // time (never stored with one in the .MUIB file).
        CHECK(!headerText.contains(QStringLiteral("MUII_WarpNTBadge")) &&
                  !sourceText.contains(QStringLiteral("MUII_WarpNTBadge")) &&
                  !gadgetsText.contains(QStringLiteral("MUII_WarpNTBadge")) &&
                  !mainText.contains(QStringLiteral("MUII_WarpNTBadge")),
              qPrintable(QStringLiteral("%1: no invented MUII_WarpNTBadge placeholder").arg(fname)));
        if (fname == QStringLiteral("DVIprint.MUIB"))
        {
            CHECK(sourceText.contains(QStringLiteral("MUIA_Image_Spec, ")) &&
                      !sourceText.contains(QStringLiteral("MUIA_Image_Spec, \"")),
                  qPrintable(QStringLiteral("%1: Image gadget with no custom spec emits a raw numeric MUIA_Image_Spec value").arg(fname)));

            // "IM_label_0" (the drawer-browse icon next to the path field) is
            // stored with Area.InputMode set - confirmed by directly loading
            // the real DVIprint.MUIB and inspecting the object, not guessed -
            // so it must come out as a clickable icon button (MUIA_InputMode,
            // MUIV_InputMode_RelVerify, same as a real Button) AND get wired
            // to its own debug stub via MUIA_Pressed, exactly like a Button.
            // Before this fix, Area.InputMode was silently dropped for every
            // object type going through the generic emitArea() path, and
            // Image was hard-excluded from notification wiring outright -
            // together that meant this specific gadget compiled and ran, but
            // was inert and never printed its debug line (the user's own
            // real-hardware report: every OTHER gadget in this project fires
            // its debug message, this one alone doesn't).
            CHECK(sourceText.contains(QStringLiteral("MUIA_InputMode, MUIV_InputMode_RelVerify,")),
                  qPrintable(QStringLiteral("%1: clickable Image (Area.InputMode) emits MUIA_InputMode/RelVerify").arg(fname)));
            CHECK(sourceText.contains(QStringLiteral("Gui.IM_label_0, MUIM_Notify, MUIA_Pressed, FALSE,")),
                  qPrintable(QStringLiteral("%1: clickable Image gadget is wired to its debug stub via MUIA_Pressed").arg(fname)));
        }

        // DirList/ListView notification wiring (user-report: "DVIPrint
        // compiliert, aber LV_label_0 [a DirList, despite the "LV" label
        // prefix - MUIBuilder's own naming convention] gibt nie eine
        // Meldung" - and separately, a self-built project confirmed a
        // ListView never fires on double-click either, with no property
        // in MuiBuilderQt's own inspector to even enable that). Root
        // cause: neither type was ever in isNotifiableGadgetType() at
        // all. Fixed via two REAL, separately-verified MUI attributes
        // (never guessed - see muicodegen.cpp's own doc comments for the
        // primary sources): DirList is unconditionally wired via
        // MUIA_List_Active (inherited from its real base class List.mui,
        // confirmed via MUI's official autodocs) since this generator's
        // own DirList is embedded unwrapped, not inside a Listview;
        // ListView is wired via MUIA_Listview_DoubleClick, but ONLY when
        // the object's own `doubleclick` flag is set (a REAL per-object
        // checkbox the original MUIBuilder tool's own ListView editor
        // dialog already exposed and .MUIB files already store - our own
        // loader/saver already round-tripped it, just nothing acted on
        // it) - confirmed via the original tool's own code.c, which
        // conditionally emits exactly this attribute. PropertyInspector
        // gained a "Doppelklick" checkbox for ListView so a project BUILT
        // in MuiBuilderQt itself (not just an imported historic one) can
        // enable this too.
        if (fname == QStringLiteral("DVIprint.MUIB"))
        {
            // LV_label_0 is a DirList (see the Small.MUIB DirList check
            // above) with Area.InputMode unrelated - this checks the
            // SEPARATE List_Active wiring path.
            CHECK(sourceText.contains(QStringLiteral("DoMethod(Gui.LV_label_0, MUIM_Notify, MUIA_List_Active, MUIV_EveryTime,")),
                  qPrintable(QStringLiteral("%1: DirList gadget is wired to its debug stub via MUIA_List_Active").arg(fname)));
            CHECK(gadgetsText.contains(QStringLiteral("get(Gui.LV_label_0, MUIA_Dirlist_Path, &path);")) &&
                      gadgetsText.contains(QStringLiteral("printf(\"Ausgewaehlter Pfad: %s\\n\", path);")),
                  qPrintable(QStringLiteral("%1: DirList gadget stub reports its live MUIA_Dirlist_Path via Gui.<ident>").arg(fname)));
        }
        if (fname == QStringLiteral("Small.MUIB"))
        {
            // Small.MUIB's own LV_label_1 has doubleclick=TRUE stored in
            // the real historic file - the exact kind of ListView that
            // was silently never wired before this fix.
            CHECK(sourceText.contains(QStringLiteral("MUIA_Listview_DoubleClick, TRUE,")),
                  qPrintable(QStringLiteral("%1: ListView with doubleclick set emits MUIA_Listview_DoubleClick").arg(fname)));
            CHECK(sourceText.contains(QStringLiteral("DoMethod(Gui.LV_label_1, MUIM_Notify, MUIA_Listview_DoubleClick, TRUE,")),
                  qPrintable(QStringLiteral("%1: ListView with doubleclick set is wired to its debug stub").arg(fname)));
        }
        if (fname == QStringLiteral("MUIB-Demo.MUIB"))
        {
            // MUIB-Demo.MUIB has a real ListView but none with
            // doubleclick set (a genuine negative control, not just an
            // absent-ListView vacuous pass) - confirms the wiring really
            // is conditional on the per-object flag, not blanket-enabled
            // for every ListView the moment this feature shipped.
            CHECK(sourceText.contains(QStringLiteral("ListviewObject,")) &&
                      !sourceText.contains(QStringLiteral("MUIA_Listview_DoubleClick")),
                  qPrintable(QStringLiteral("%1: a ListView without the flag set emits no MUIA_Listview_DoubleClick").arg(fname)));
        }

        // Debug-content reporting (user request: gadgets that carry actual
        // information - String, ListView, etc. - should print that live
        // content, not just "I was clicked"). Verified against real,
        // well-established MUI attributes: get()/DoMethod() are the
        // standard query idiom (xget deliberately avoided - never used
        // anywhere in the original tree or this codebase); MUIA_String_
        // Contents, MUIA_Cycle_Active/MUIA_Radio_Active (indexing the same
        // CONST_STRPTR entries array collectEntriesArrays() emits),
        // MUIA_Selected, MUIA_Numeric_Value are standard attributes on
        // their respective classes; MUIA_Listview_List + DoMethod(...,
        // MUIM_List_GetEntry, MUIV_List_GetEntry_Active, &entry) is the
        // documented way to read a Listview's currently-active entry
        // (confirmed exact signature via the amiga-mui/muidev wiki).
        if (fname == QStringLiteral("DVIprint.MUIB"))
        {
            // The object reference inside get()/DoMethod() here MUST be
            // "Gui.<ident>"-qualified, never the bare ident - a real
            // vbcc/NDK3.2 build caught this exact bug (the debug-content
            // stub lives in a completely separate function/translation
            // unit from wherever the object was actually built, so only
            // its persistent "Gui." struct field is reachable there;
            // "unknown identifier <STR_label_0>" etc. is what a bare,
            // unqualified reference produces).
            CHECK(gadgetsText.contains(QStringLiteral("get(Gui.STR_label_0, MUIA_String_Contents, &content);")) &&
                      gadgetsText.contains(QStringLiteral("printf(\"Mein Inhalt ist jetzt: %s\\n\", content);")),
                  qPrintable(QStringLiteral("%1: String gadget stub reports its live MUIA_String_Contents via Gui.<ident>").arg(fname)));
            CHECK(gadgetsText.contains(QStringLiteral("get(Gui.LV_label_1, MUIA_Listview_List, &list);")) &&
                      gadgetsText.contains(QStringLiteral("DoMethod(list, MUIM_List_GetEntry, MUIV_List_GetEntry_Active, &entry);")),
                  qPrintable(QStringLiteral("%1: ListView gadget stub reports its live active entry via Gui.<ident>").arg(fname)));
            CHECK(!gadgetsText.contains(QStringLiteral("get(STR_label_0,")) &&
                      !gadgetsText.contains(QStringLiteral("get(LV_label_1,")),
                  qPrintable(QStringLiteral("%1: no bare (un-Gui.-qualified) object reference in a debug-content get()").arg(fname)));
        }
        if (fname == QStringLiteral("yak.MUIB"))
        {
            // Cycle: its active entry must be looked up in the SAME
            // externally-linked CONST_STRPTR array collectEntriesArrays()
            // defines in the .c file and collectEntriesArrayExterns()
            // forward-declares in the .h - this is the specific
            // cross-translation-unit linkage this feature depends on
            // (the gadgets header's `static inline` stubs are compiled
            // separately into both <baseName>.c and <baseName>_main.c).
            // The entries-array index itself stays a bare, unprefixed
            // name (a plain external global, not a GUIObjects field) even
            // though the Cycle object being read is "Gui."-qualified.
            CHECK(gadgetsText.contains(QStringLiteral("get(Gui.CY_label_0, MUIA_Cycle_Active, &active);")) &&
                      gadgetsText.contains(QStringLiteral("CY_label_0Entries[active]")),
                  qPrintable(QStringLiteral("%1: Cycle gadget stub reports its live active entry via Gui.<ident> and the real entries array").arg(fname)));
            CHECK(headerText.contains(QStringLiteral("extern CONST_STRPTR CY_label_0Entries[];")),
                  qPrintable(QStringLiteral("%1: Cycle entries array is forward-declared extern in the header").arg(fname)));
            CHECK(sourceText.contains(QStringLiteral("CONST_STRPTR CY_label_0Entries[] =")) &&
                      !sourceText.contains(QStringLiteral("static CONST_STRPTR CY_label_0Entries[] =")),
                  qPrintable(QStringLiteral("%1: Cycle entries array is defined with real (non-static) linkage").arg(fname)));
            CHECK(gadgetsText.contains(QStringLiteral("get(Gui.CH_label_0, MUIA_Selected, &selected);")),
                  qPrintable(QStringLiteral("%1: Check gadget stub reports its live MUIA_Selected state via Gui.<ident>").arg(fname)));
            CHECK(gadgetsText.contains(QStringLiteral("get(Gui.SL_volume, MUIA_Numeric_Value, &value);")),
                  qPrintable(QStringLiteral("%1: Slider gadget stub reports its live MUIA_Numeric_Value via Gui.<ident>").arg(fname)));
        }

        // IPTR isn't defined by every NDK header set this project's users
        // cross-compile against (e.g. an older vbcc/NDK3.2 Include_H
        // snapshot) even though (IPTR) casts show up throughout the
        // emitted MUI code (Cycle/Radio entries, Register titles,
        // notification args) - so the header anchors its own definition,
        // once, ahead of any use in either generated .c file.
        CHECK(headerText.contains(QStringLiteral("typedef unsigned long IPTR;")),
              qPrintable(QStringLiteral("%1: header anchors its own IPTR typedef").arg(fname)));
        CHECK(headerText.indexOf(QStringLiteral("typedef unsigned long IPTR;")) <
                  headerText.indexOf(QStringLiteral("struct GUIObjects")),
              qPrintable(QStringLiteral("%1: IPTR typedef precedes its first use in the header").arg(fname)));

        // <stdio.h>/<stdlib.h> must be declared directly by every
        // generated .c/_main.c file (not just picked up transitively
        // through whichever generated header happens to get #included
        // first), and the gadgets header (which is what actually calls
        // puts()) needs both too.
        CHECK(sourceText.contains(QStringLiteral("#include <stdio.h>")) &&
                  sourceText.contains(QStringLiteral("#include <stdlib.h>")),
              qPrintable(QStringLiteral("%1: source includes stdio.h and stdlib.h").arg(fname)));
        CHECK(mainText.contains(QStringLiteral("#include <stdio.h>")) &&
                  mainText.contains(QStringLiteral("#include <stdlib.h>")),
              qPrintable(QStringLiteral("%1: main includes stdio.h and stdlib.h").arg(fname)));
        CHECK(gadgetsText.contains(QStringLiteral("#include <stdio.h>")) &&
                  gadgetsText.contains(QStringLiteral("#include <stdlib.h>")),
              qPrintable(QStringLiteral("%1: gadgets header includes stdio.h and stdlib.h").arg(fname)));

        // "BOOL myDebug = TRUE;" must work out of the box - defined
        // (not just declared extern) right after main()'s own include
        // block, ahead of every other declaration in that file.
        int myDebugDefPos = mainText.indexOf(QStringLiteral("BOOL myDebug = TRUE;"));
        CHECK(myDebugDefPos >= 0,
              qPrintable(QStringLiteral("%1: main defines BOOL myDebug = TRUE").arg(fname)));
        CHECK(myDebugDefPos >= 0 &&
                  myDebugDefPos < mainText.indexOf(QStringLiteral("MUIMasterBase")),
              qPrintable(QStringLiteral("%1: myDebug is defined immediately after main's includes").arg(fname)));

        int windowFnCount = sourceText.count(QStringLiteral("Window(void)\n{"));
        CHECK(windowFnCount == static_cast<int>(proj->windows.size()),
              qPrintable(QStringLiteral("%1: one Build*Window() per window (%2 found, %3 expected)")
                             .arg(fname).arg(windowFnCount).arg(proj->windows.size())));

        // A registermode Group must become a RegisterObject, never a plain
        // GroupObject with the Register-only MUIA_Register_Titles attribute
        // bolted on (that attribute belongs to the real, separate
        // MUIC_Register class - a base Group silently ignores it, which
        // doesn't fail this or any other structural check above, so it
        // needs its own explicit one).
        if (sourceText.contains(QStringLiteral("MUIA_Register_Titles")))
        {
            CHECK(sourceText.contains(QStringLiteral("RegisterObject,")),
                  qPrintable(QStringLiteral("%1: a registermode Group is built as RegisterObject, not GroupObject").arg(fname)));
        }

        // Every window's close gadget/menu-Close/Workbench-Close must
        // actually do something: MUIA_Window_CloseRequest only marks the
        // request, so every Build*Window() needs its own app-level
        // DoMethod turning that into a real MUIV_Application_ReturnID_Quit,
        // at least one per window (the synthetic per-window wiring below
        // guarantees a floor of exactly one; a project whose own stored
        // notify data ALSO wires a window's CloseRequest to something -
        // now correctly translated via notifytables.h/.cpp instead of
        // silently falling back to a raw, unmatched integer dump - adds
        // more real "MUIA_Window_CloseRequest, TRUE" occurrences on top,
        // which is a strict improvement, not a regression).
        int closeWiredCount = sourceText.count(QStringLiteral("MUIA_Window_CloseRequest, TRUE"));
        CHECK(closeWiredCount >= static_cast<int>(proj->windows.size()),
              qPrintable(QStringLiteral("%1: every window's CloseRequest is wired to quit (%2 found, at least %3 expected)")
                             .arg(fname).arg(closeWiredCount).arg(proj->windows.size())));
        // >=, not == : a project can ALSO have its own real, stored notify
        // event wiring something (a window's own CloseRequest again, a
        // MenuItem's Trigger, ...) to App/Quit on top of the one guaranteed
        // synthetic per-window wiring above - exactly the same reasoning as
        // closeWiredCount just above. Confirmed a real, previously-hidden
        // case of this in the wild (2026-09-17 fix, see emitNotifications()):
        // MUIB-Demo.MUIB's own "Turn" window has a genuine stored
        // CloseRequest->App/Quit event that used to silently render as the
        // undeclared, non-compiling identifier "app" (invisible to this
        // exact-string count, which is exactly how == stayed green for a
        // real, latent compile-breaking bug) - now correctly renders via
        // MUIV_Notify_Application like every other App-targeted reference,
        // legitimately pushing this project's count to 6 for 5 windows.
        int closeToQuitCount = sourceText.count(QStringLiteral("MUIV_Notify_Application, 2, MUIM_Application_ReturnID, MUIV_Application_ReturnID_Quit"));
        CHECK(closeToQuitCount >= static_cast<int>(proj->windows.size()),
              qPrintable(QStringLiteral("%1: every wired CloseRequest targets the app's Quit id (%2 found, at least %3 expected)")
                             .arg(fname).arg(closeToQuitCount).arg(proj->windows.size())));

        // Every isNotifiableGadgetType() gadget's DoMethod (generateSource())
        // must be wired to a numeric id that main()'s switch (generateMain())
        // actually dispatches with a matching "case <id>:" - i.e. the two
        // independently-recomputed id maps genuinely agree, not just that
        // both happen to be non-empty.
        static const QRegularExpression kNotifyIdRe(QStringLiteral("MUIM_Application_ReturnID, (\\d+)\\)"));
        // The comment after the ident may now also carry the object's own
        // real display text (see debugStubDisplayTextFor() in
        // muicodegen.cpp, e.g. "case 2: /* MenuItem_2 "Manual" */") -
        // [^\n]*? lazily absorbs that optional part before the closing " */".
        static const QRegularExpression kCaseIdRe(QStringLiteral("case (\\d+): /\\* (\\w+)[^\\n]*? \\*/\\n\\t+(\\w+)_Clicked\\(\\);"));
        QSet<int> sourceIds;
        {
            auto it = kNotifyIdRe.globalMatch(sourceText);
            while (it.hasNext())
                sourceIds.insert(it.next().captured(1).toInt());
        }
        QSet<int> mainIds;
        QHash<int, QString> mainIdToIdent;
        {
            auto it = kCaseIdRe.globalMatch(mainText);
            while (it.hasNext())
            {
                QRegularExpressionMatch m = it.next();
                int id = m.captured(1).toInt();
                mainIds.insert(id);
                mainIdToIdent.insert(id, m.captured(2));
                CHECK(m.captured(2) == m.captured(3),
                      qPrintable(QStringLiteral("%1: case %2's comment ident matches the _Clicked() it calls").arg(fname).arg(id)));
            }
        }
        CHECK(sourceIds == mainIds,
              qPrintable(QStringLiteral("%1: gadget notify ids agree between source (%2) and main (%3)")
                             .arg(fname).arg(sourceIds.size()).arg(mainIds.size())));
        // Every dispatched ident must actually have a stub in the gadgets
        // header, and every id must be unique (no two gadgets sharing one).
        CHECK(mainIds.size() == mainIdToIdent.size(),
              qPrintable(QStringLiteral("%1: every gadget notify id is unique").arg(fname)));
        for (auto idIt = mainIdToIdent.constBegin(); idIt != mainIdToIdent.constEnd(); ++idIt)
        {
            CHECK(gadgetsText.contains(QStringLiteral("%1_Clicked(void)").arg(idIt.value())),
                  qPrintable(QStringLiteral("%1: dispatched ident \"%2\" has a real stub in the gadgets header")
                                 .arg(fname, idIt.value())));
        }

        CHECK(mainText.contains(QStringLiteral("int main(void)")),
              qPrintable(QStringLiteral("%1: main() present").arg(fname)));
        CHECK(mainText.contains(QStringLiteral("MUIM_Application_NewInput")),
              qPrintable(QStringLiteral("%1: main() contains the event loop").arg(fname)));
        CHECK(mainText.contains(QStringLiteral("MUIV_Application_ReturnID_Quit")),
              qPrintable(QStringLiteral("%1: main() handles the Quit return ID").arg(fname)));
        // Every project has at least one window, so main() should always
        // open exactly one of them (either the/an initopen one, or the
        // documented "open the first window" fallback).
        int openCount = mainText.count(QStringLiteral("MUIA_Window_Open, TRUE"));
        CHECK(openCount >= 1, qPrintable(QStringLiteral("%1: main() opens at least one window (%2 found)").arg(fname).arg(openCount)));

        // Informational only (no independent expected count computed here
        // - see the eligibility rule documented on generateGadgetStubs()):
        // how many debug stubs this project's gadgets header ended up
        // with, so a scan of the qInfo log can catch an obviously-wrong
        // count (e.g. 0 for a window full of buttons) even without a
        // hard CHECK.
        int stubCount = gadgetsText.count(QStringLiteral("_Clicked(void)\n{"));
        qInfo().noquote() << QStringLiteral("     (%1: %2 gadget debug stub(s) generated)").arg(fname).arg(stubCount);

        // Write every project's output (not just the smallest) so any
        // failure can be inspected directly without a special re-run.
        QFile hf(QDir(outDir).filePath(base + QStringLiteral(".h")));
        if (hf.open(QIODevice::WriteOnly | QIODevice::Truncate))
            hf.write(headerText.toLatin1());
        QFile cf(QDir(outDir).filePath(base + QStringLiteral(".c")));
        if (cf.open(QIODevice::WriteOnly | QIODevice::Truncate))
            cf.write(sourceText.toLatin1());
        QFile gf(QDir(outDir).filePath(base + QStringLiteral("_gadgets.h")));
        if (gf.open(QIODevice::WriteOnly | QIODevice::Truncate))
            gf.write(gadgetsText.toLatin1());
        QFile mf(QDir(outDir).filePath(base + QStringLiteral("_main.c")));
        if (mf.open(QIODevice::WriteOnly | QIODevice::Truncate))
            mf.write(mainText.toLatin1());
        if (fname == smallestFile)
            qInfo().noquote() << QStringLiteral("     (wrote every project's generated .h/.c/_gadgets.h/_main.c to %1 for inspection)").arg(outDir);
    }

    // --- Synthetic Menu/SubMenu/MenuItem test -----------------------------
    // None of the 15 real BuilderSave examples actually populate a
    // non-empty menu (every one of them has an appMenu/window menu with
    // zero children - checked by hand against this run's own output), so
    // the corrected Menu-family mapping in muicodegen.cpp's emitObject()
    // (MenustripObject/MUIA_Family_Child instead of the old, wrong
    // MenuObject/generic-Child mapping - see its own doc comment for the
    // full code.c-verified mapping) has nothing real to exercise it.
    // Hand-build a small project instead: a window with a menu strip
    // containing a top-level "Project" pulldown (three leaf items,
    // including a BarLabel separator and a Shortcut key), a disabled
    // top-level "Edit" pulldown containing one nested flyout SubMenu,
    // which itself contains one Checkit/Checked/Toggle leaf item - this
    // hits every branch of the fixed mapping (top-level title, nested
    // SubMenu, MUIA_Menu_Enabled/Menuitem_Enabled, Shortcut, BarLabel,
    // Checkit/Checked/Toggle) in one project.
    {
        auto proj = std::make_unique<MuibProject>();
        proj->title = QStringLiteral("MenuTest");
        proj->base = QStringLiteral("menutest");

        auto win = std::make_unique<MuibObject>(ObjType::Window);
        win->label = QStringLiteral("MenuTestWin");
        win->title = QStringLiteral("Menu Test");
        auto root = std::make_unique<MuibObject>(ObjType::Group);
        root->isRoot = true;
        root->label = QStringLiteral("MenuTestWin_Root");
        win->root = std::move(root);

        auto menu = std::make_unique<MuibObject>(ObjType::Menu);
        menu->label = QStringLiteral("MN_root");
        menu->menu_enable = true;

        auto project_ = std::make_unique<MuibObject>(ObjType::SubMenu);
        project_->label = QStringLiteral("SM_Project");
        project_->name = QStringLiteral("Project");
        project_->menu_enable = true;

        auto open_ = std::make_unique<MuibObject>(ObjType::MenuItem);
        open_->label = QStringLiteral("MI_Open");
        open_->name = QStringLiteral("Open");
        open_->menu_enable = true;
        project_->childs.push_back(std::move(open_));

        auto sep = std::make_unique<MuibObject>(ObjType::MenuItem);
        sep->label = QStringLiteral("MI_Sep");
        sep->name = QStringLiteral("BarLabel");
        project_->childs.push_back(std::move(sep));

        auto quit_ = std::make_unique<MuibObject>(ObjType::MenuItem);
        quit_->label = QStringLiteral("MI_Quit");
        quit_->name = QStringLiteral("Quit");
        quit_->menu_enable = true;
        quit_->menuKey = 'Q';
        // Real-shape notify event, exactly as MuibLoader::readNotify()
        // would read it from an actual .MUIB file: srcType=0 is
        // MenuItem's own "MenuTriggered" event (CEVTMenuItem[0] =
        // MUIA_Menuitem_Trigger/MUIV_EveryTime), destType=1 is
        // Application's "ReturnQuit" action (CACTAppli[1] - see
        // notifytables.cpp's actionsAppli()), targetLabel "App" is the
        // real original's own hardcoded Application label (builder.c:
        // "strcpy(application.label, \"App\")") - see emitNotifications()'s
        // own special-case for it. This is the exact real-world shape
        // behind the user's own bug report ("Quit-Menüpunkt reagiert
        // nicht, da keine Menü-Abfrage in der Event-Schleife").
        NotifyEvent quitEvt;
        quitEvt.targetLabel = QStringLiteral("App");
        quitEvt.targetTypeId = 0;
        quitEvt.srcType = 0;
        quitEvt.destType = 1;
        quit_->notify.push_back(quitEvt);
        project_->childs.push_back(std::move(quit_));

        menu->childs.push_back(std::move(project_));

        auto edit_ = std::make_unique<MuibObject>(ObjType::SubMenu);
        edit_->label = QStringLiteral("SM_Edit");
        edit_->name = QStringLiteral("Edit");
        edit_->menu_enable = false;   // -> MUIA_Menu_Enabled, FALSE

        auto nested = std::make_unique<MuibObject>(ObjType::SubMenu);
        nested->label = QStringLiteral("SM_Nested");
        nested->name = QStringLiteral("Nested Flyout");
        nested->menu_enable = true;

        auto toggle_ = std::make_unique<MuibObject>(ObjType::MenuItem);
        toggle_->label = QStringLiteral("MI_Toggle");
        toggle_->name = QStringLiteral("Toggle Item");
        toggle_->menu_enable = true;
        toggle_->check_enable = true;
        toggle_->check_state = true;
        toggle_->toggleMenu = true;
        nested->childs.push_back(std::move(toggle_));

        edit_->childs.push_back(std::move(nested));
        menu->childs.push_back(std::move(edit_));

        win->menu = std::move(menu);
        proj->windows.push_back(std::move(win));

        const QString base = QStringLiteral("menutest");
        QString sourceText = MuiCodeGen::generateSource(*proj, base);

        CHECK(!sourceText.contains(QStringLiteral("MenuObject,")),
              "synthetic menu project: old, wrong MenuObject macro is gone");
        CHECK(sourceText.contains(QStringLiteral("MenustripObject,")),
              "synthetic menu project: root Menu uses the real MenustripObject macro");
        CHECK(sourceText.contains(QStringLiteral("MUIA_Window_Menustrip, (")),
              "synthetic menu project: window wires its menu via MUIA_Window_Menustrip");
        CHECK(sourceText.contains(QStringLiteral("MUIA_Family_Child, ")),
              "synthetic menu project: children attach via the real MUIA_Family_Child attribute");
        CHECK(sourceText.contains(QStringLiteral("MUIA_Menuitem_Title, \"Project\",")),
              "synthetic menu project: top-level pulldown title uses MUIA_Menuitem_Title (never MUI's own Menu_Title)");
        CHECK(sourceText.contains(QStringLiteral("MUIA_Menuitem_Title, \"Open\",")),
              "synthetic menu project: leaf MenuItem title present");
        CHECK(sourceText.contains(QStringLiteral("MUI_MakeObject(MUIO_Menuitem, NM_BARLABEL, 0, 0, 0)")),
              "synthetic menu project: a name starting with \"BarLabel\" becomes the real separator MUI_MakeObject() call (real identifier NM_BARLABEL, not the previously transposed MN_BARLABEL)");
        CHECK(!sourceText.contains(QStringLiteral("MN_BARLABEL")),
              "synthetic menu project: the old, letter-transposed (and non-compiling) MN_BARLABEL identifier is gone");
        CHECK(sourceText.contains(QStringLiteral("#include <libraries/gadtools.h>")),
              "synthetic menu project: gadtools.h is included (needed for the real NM_BARLABEL definition)");
        CHECK(sourceText.contains(QStringLiteral("MUIA_Menuitem_Title, \"Quit\",")) &&
                  sourceText.contains(QStringLiteral("MUIA_Menuitem_Shortcut, \"Q\",")),
              "synthetic menu project: leaf MenuItem shortcut key emitted");
        CHECK(sourceText.contains(QStringLiteral("MUIA_Menuitem_Title, \"Edit\",")) &&
                  sourceText.contains(QStringLiteral("MUIA_Menu_Enabled, FALSE,")),
              "synthetic menu project: disabled top-level pulldown emits MUIA_Menu_Enabled, FALSE (SubMenu's own attribute, distinct from leaf MenuItem's MUIA_Menuitem_Enabled)");
        CHECK(sourceText.contains(QStringLiteral("MUIA_Menuitem_Title, \"Nested Flyout\",")),
              "synthetic menu project: nested flyout SubMenu title present");
        CHECK(sourceText.contains(QStringLiteral("MUIA_Menuitem_Title, \"Toggle Item\",")) &&
                  sourceText.contains(QStringLiteral("MUIA_Menuitem_Checkit, TRUE,")) &&
                  sourceText.contains(QStringLiteral("MUIA_Menuitem_Checked, TRUE,")) &&
                  sourceText.contains(QStringLiteral("MUIA_Menuitem_Toggle, TRUE,")),
              "synthetic menu project: Checkit/Checked/Toggle leaf item attributes present");
        // Regression test for the 2026-09-17 "'app' undeclared" bug: a
        // MenuItem's notify wiring is emitted from inside BuildMenustrip(),
        // which runs and returns before BuildApplication() ever creates its
        // local `app` variable - the target reference must therefore be the
        // scope-independent MUIV_Notify_Application macro, never the literal
        // identifier "app" (which used to compile fine here in the test,
        // since QStringLiteral("...app, 2,...") matched it, but never
        // actually compiled as real m68k-amigaos-gcc C code - that gap is
        // exactly how the bug reached a real user project undetected).
        CHECK(sourceText.contains(QStringLiteral("MUIM_Notify, MUIA_Menuitem_Trigger, MUIV_EveryTime, MUIV_Notify_Application, 2, MUIM_Application_ReturnID, MUIV_Application_ReturnID_Quit);")),
              "synthetic menu project: the Quit MenuItem's stored notify event (targetLabel \"App\", srcType 0, destType 1) "
              "resolves to a real DoMethod(..., MUIV_Notify_Application, 2, MUIM_Application_ReturnID, MUIV_Application_ReturnID_Quit) call "
              "using the scope-independent macro (never the bare \"app\" identifier, which is undeclared outside BuildApplication())");
        CHECK(!sourceText.contains(QStringLiteral(", app, 2, MUIM_Application_ReturnID,")),
              "synthetic menu project: the undeclared bare \"app\" identifier is never emitted as a notify target outside BuildApplication()");


        QString detail;
        bool balanced = isBalanced(sourceText, &detail);
        CHECK(balanced, qPrintable(QStringLiteral("synthetic menu project: source brackets balanced (%1)").arg(balanced ? QStringLiteral("ok") : detail)));

        QFile mf(QDir(outDir).filePath(base + QStringLiteral(".c")));
        if (mf.open(QIODevice::WriteOnly | QIODevice::Truncate))
            mf.write(sourceText.toLatin1());
    }

    // --- Synthetic Check-with-label test -----------------------------------
    // Covers the user-reported gap ("die generierte Checkbox hat keine
    // Beschriftung, obwohl eine angegeben ist."): code.c's real TY_CHECK
    // case (title_exist branch) wraps the checkbox in an anonymous
    // 2-column GroupObject alongside a Label2()/KeyLabel2()-based text
    // label - this port previously emitted only the bare checkbox,
    // silently dropping obj->title/obj->title_exist even though the
    // .MUIB loader (loadCheck()) and the Property-Inspector already
    // round-tripped both fields correctly. Two objects here: one WITH a
    // label (wrapping expected) and one WITHOUT (bare checkbox still
    // expected - regression check that the common, unlabeled case is
    // unaffected).
    {
        auto proj = std::make_unique<MuibProject>();
        proj->title = QStringLiteral("CheckLabelTest");
        proj->base = QStringLiteral("checklabeltest");

        auto win = std::make_unique<MuibObject>(ObjType::Window);
        win->label = QStringLiteral("CLTestWin");
        win->title = QStringLiteral("Check Label Test");
        auto root = std::make_unique<MuibObject>(ObjType::Group);
        root->isRoot = true;
        root->label = QStringLiteral("CLTestWin_Root");

        auto labeledCheck = std::make_unique<MuibObject>(ObjType::Check);
        labeledCheck->label = QStringLiteral("CheckWithLabel");
        labeledCheck->title = QStringLiteral("Show hidden files");
        labeledCheck->title_exist = true;
        labeledCheck->init_state = true;
        root->children.push_back(std::move(labeledCheck));

        auto bareCheck = std::make_unique<MuibObject>(ObjType::Check);
        bareCheck->label = QStringLiteral("BareCheck");
        // title/title_exist left at their defaults (empty/false).
        root->children.push_back(std::move(bareCheck));

        win->root = std::move(root);
        proj->windows.push_back(std::move(win));

        const QString base = QStringLiteral("checklabeltest");
        QString sourceText = MuiCodeGen::generateSource(*proj, base);

        CHECK(sourceText.contains(QStringLiteral("MUIA_Group_Columns, 2,")) &&
                  sourceText.contains(QStringLiteral("Child, Label2(\"Show hidden files\"),")) &&
                  sourceText.contains(QStringLiteral("Child, (Gui.CheckWithLabel = ImageObject,")) &&
                  sourceText.indexOf(QStringLiteral("Label2(\"Show hidden files\")")) <
                      sourceText.indexOf(QStringLiteral("Gui.CheckWithLabel = ImageObject")),
              "synthetic Check-label project: a labeled Check is wrapped in a 2-column GroupObject with Label2(text) as the first child, checkbox second");
        CHECK(sourceText.contains(QStringLiteral("MUIA_Selected, TRUE,")),
              "synthetic Check-label project: the wrapped checkbox itself still carries its own attributes (init_state)");
        CHECK(sourceText.contains(QStringLiteral("(Gui.BareCheck = ImageObject,")) &&
                  !sourceText.contains(QStringLiteral("Label2(\"\")")),
              "synthetic Check-label project: a Check with no title still emits as a bare checkbox, no wrapper (regression check)");

        QString detail;
        bool balanced = isBalanced(sourceText, &detail);
        CHECK(balanced, qPrintable(QStringLiteral("synthetic Check-label project: source brackets balanced (%1)")
                                        .arg(balanced ? QStringLiteral("ok") : detail)));

        QFile clf(QDir(outDir).filePath(base + QStringLiteral(".c")));
        if (clf.open(QIODevice::WriteOnly | QIODevice::Truncate))
            clf.write(sourceText.toLatin1());
    }

    // --- Synthetic Group Frame/TitleFrame test ----------------------------
    // Covers the user-reported gap ("den Groups fehlt eine Eigenschaft, um
    // einen Rahmen um die gruppe zu legen und einen Rahmentext einzugeben"):
    // the original tool's own struct group1 (builder.h) embeds a real
    // "area Area;" field, and its CodeArea() call site (code.c, TY_GROUP)
    // passes Frame=TRUE and TitleFrame=TRUE - i.e. a real MUIBuilder Group
    // DOES support MUIA_Frame + MUIA_FrameTitle, contradicting the port's
    // previous objectfactory.cpp::typeHasAreaAttrs(Group)==false (GUI-only
    // gap) and muicodegen.cpp::emitArea() never emitting TitleFrame at all
    // for ANY type (a separate, broader codegen gap, real MUI attribute
    // name "MUIA_FrameTitle" confirmed against three independent sources -
    // see emitArea()'s own comment). This project sets both Frame and
    // TitleFrame on a plain (non-root, non-register) child Group to
    // exercise emitObject()'s ObjType::Group case's emitArea() call
    // directly.
    {
        auto proj = std::make_unique<MuibProject>();
        proj->title = QStringLiteral("GroupFrameTest");
        proj->base = QStringLiteral("groupframetest");

        auto win = std::make_unique<MuibObject>(ObjType::Window);
        win->label = QStringLiteral("GFTestWin");
        win->title = QStringLiteral("Group Frame Test");
        auto root = std::make_unique<MuibObject>(ObjType::Group);
        root->isRoot = true;
        root->label = QStringLiteral("GFTestWin_Root");

        auto framedGroup = std::make_unique<MuibObject>(ObjType::Group);
        framedGroup->label = QStringLiteral("FramedGroup");
        framedGroup->area.Frame = 8;   // MUIV_Frame_Group (hinted) - see frameHint()
        framedGroup->area.TitleFrame = QStringLiteral("Einstellungen");
        root->children.push_back(std::move(framedGroup));

        win->root = std::move(root);
        proj->windows.push_back(std::move(win));

        const QString base = QStringLiteral("groupframetest");
        QString sourceText = MuiCodeGen::generateSource(*proj, base);

        CHECK(sourceText.contains(QStringLiteral("MUIA_Frame, 8, /* MUIV_Frame_Group (hinted) */")),
              "synthetic Group-Frame project: a Group's own MUIA_Frame is emitted (previously unreachable via the GUI, but emitArea() itself already handled Frame)");
        CHECK(sourceText.contains(QStringLiteral("MUIA_FrameTitle, \"Einstellungen\",")),
              "synthetic Group-Frame project: a Group's Rahmentext (MUIA_FrameTitle) is emitted - previously missing from emitArea() entirely, for every object type, not just Group");

        QString detail;
        bool balanced = isBalanced(sourceText, &detail);
        CHECK(balanced, qPrintable(QStringLiteral("synthetic Group-Frame project: source brackets balanced (%1)")
                                        .arg(balanced ? QStringLiteral("ok") : detail)));

        QFile gf(QDir(outDir).filePath(base + QStringLiteral(".c")));
        if (gf.open(QIODevice::WriteOnly | QIODevice::Truncate))
            gf.write(sourceText.toLatin1());
    }

    // --- Synthetic AboutBox test -----------------------------------------
    // Exercises core/muicodegen.cpp's ObjType::AboutBox emitObject() case
    // and the BuildApplication() wiring added for the "Dialog-Creator >
    // AboutBox" feature (see muibobject.h's ObjType::AboutBox comment and
    // gui/aboutboxdialog.h): a window with one real menu item ("Info")
    // plus a project-level AboutBox linked to it, verifying the
    // Aboutbox.mcc attribute emission, the UsedClasses/
    // MUIA_Application_UsedClasses wiring, the SubWindow emission, the
    // self-closing MUIA_Window_CloseRequest notify, and the
    // MUIA_Menuitem_Trigger notify that opens it.
    {
        auto proj = std::make_unique<MuibProject>();
        proj->title = QStringLiteral("AboutBoxTest");
        proj->base = QStringLiteral("aboutboxtest");

        auto win = std::make_unique<MuibObject>(ObjType::Window);
        win->label = QStringLiteral("ABTestWin");
        win->title = QStringLiteral("AboutBox Test");
        auto root = std::make_unique<MuibObject>(ObjType::Group);
        root->isRoot = true;
        root->label = QStringLiteral("ABTestWin_Root");
        win->root = std::move(root);

        auto menu = std::make_unique<MuibObject>(ObjType::Menu);
        menu->label = QStringLiteral("MN_root2");
        menu->menu_enable = true;

        auto project_ = std::make_unique<MuibObject>(ObjType::SubMenu);
        project_->label = QStringLiteral("SM_Project2");
        project_->name = QStringLiteral("Project");
        project_->menu_enable = true;

        auto info_ = std::make_unique<MuibObject>(ObjType::MenuItem);
        info_->label = QStringLiteral("MI_Info");
        info_->name = QStringLiteral("Info");
        info_->menu_enable = true;
        project_->childs.push_back(std::move(info_));

        menu->childs.push_back(std::move(project_));
        win->menu = std::move(menu);
        proj->windows.push_back(std::move(win));

        auto about = std::make_unique<MuibObject>(ObjType::AboutBox);
        about->label = QStringLiteral("MyAboutBox");
        about->aboutCredits = QStringLiteral("Demonstrate the use of Aboutbox.mcc in MUI 5.0\nCopyright (C) 2015-2020 Thore Boeckelmann");
        about->aboutBuild = QStringLiteral("svn r1234");
        about->aboutLogoFile = QStringLiteral("PROGDIR:boing.png");
        about->aboutUrl = QStringLiteral("http://www.muidev.de");
        about->aboutUrlText = QStringLiteral("Visit the MUI for AmigaOS Development Site");
        about->aboutLinkedMenuItem = QStringLiteral("MI_Info");
        proj->aboutBox = std::move(about);

        const QString base = QStringLiteral("aboutboxtest");
        QString headerText = MuiCodeGen::generateHeader(*proj);
        QString sourceText = MuiCodeGen::generateSource(*proj, base);

        CHECK(sourceText.contains(QStringLiteral("#include <mui/Aboutbox_mcc.h>")),
              "synthetic AboutBox project: real Aboutbox.mcc header is included when an AboutBox exists");
        CHECK(sourceText.contains(QStringLiteral("AboutboxObject,")),
              "synthetic AboutBox project: AboutBox emits the real AboutboxObject macro");
        CHECK(sourceText.contains(QStringLiteral(
                  "MUIA_Aboutbox_Credits, \"Demonstrate the use of Aboutbox.mcc in MUI 5.0\\nCopyright (C) 2015-2020 Thore Boeckelmann\",")),
              "synthetic AboutBox project: Credits attribute emitted with the real text");
        CHECK(sourceText.contains(QStringLiteral("MUIA_Aboutbox_Build, \"svn r1234\",")),
              "synthetic AboutBox project: Build attribute emitted");
        CHECK(sourceText.contains(QStringLiteral("MUIA_Aboutbox_LogoFile, \"PROGDIR:boing.png\",")) &&
                  sourceText.contains(QStringLiteral("MUIA_Aboutbox_LogoFallbackMode, MAKE_ID('E', 'D', '\\0', '\\0'),")),
              "synthetic AboutBox project: LogoFile + its fallback mode attribute emitted");
        CHECK(sourceText.contains(QStringLiteral("MUIA_Aboutbox_URL, \"http://www.muidev.de\",")) &&
                  sourceText.contains(QStringLiteral("MUIA_Aboutbox_URLText, \"Visit the MUI for AmigaOS Development Site\",")),
              "synthetic AboutBox project: URL + URLText attributes emitted");
        CHECK(sourceText.contains(QStringLiteral("static const char *const UsedClasses[] =")) &&
                  sourceText.contains(QStringLiteral("\"Aboutbox.mcc\",")),
              "synthetic AboutBox project: UsedClasses array declares Aboutbox.mcc");
        CHECK(sourceText.contains(QStringLiteral("MUIA_Application_UsedClasses, UsedClasses,")),
              "synthetic AboutBox project: ApplicationObject sets MUIA_Application_UsedClasses");
        CHECK(sourceText.contains(QStringLiteral("SubWindow, ")) &&
                  sourceText.contains(QStringLiteral(" = AboutboxObject,")),
              "synthetic AboutBox project: AboutBox is wired in as a persistent (Gui.-qualified) SubWindow of the Application");
        CHECK(sourceText.contains(QStringLiteral(
                  "MUIM_Notify, MUIA_Window_CloseRequest, TRUE, MUIV_Notify_Self, 3, MUIM_Set, MUIA_Window_Open, FALSE")),
              "synthetic AboutBox project: closing the AboutBox just hides it (self-close notify)");
        CHECK(sourceText.contains(QStringLiteral("MUIM_Notify, MUIA_Menuitem_Trigger, MUIV_EveryTime,")) &&
                  sourceText.contains(QStringLiteral("MUIM_Set, MUIA_Window_Open, TRUE")),
              "synthetic AboutBox project: the linked menu item's MUIA_Menuitem_Trigger opens the AboutBox");

        CHECK(headerText.contains(QStringLiteral("struct GUIObjects")),
              "synthetic AboutBox project: header still declares GUIObjects");

        QString detail;
        bool balanced = isBalanced(sourceText, &detail);
        CHECK(balanced, qPrintable(QStringLiteral("synthetic AboutBox project: source brackets balanced (%1)")
                                        .arg(balanced ? QStringLiteral("ok") : detail)));

        QFile hf(QDir(outDir).filePath(base + QStringLiteral(".h")));
        if (hf.open(QIODevice::WriteOnly | QIODevice::Truncate))
            hf.write(headerText.toLatin1());
        QFile cf(QDir(outDir).filePath(base + QStringLiteral(".c")));
        if (cf.open(QIODevice::WriteOnly | QIODevice::Truncate))
            cf.write(sourceText.toLatin1());
    }

    // --- Synthetic AboutBox-linked-to-childless-SubMenu test --------------
    // Covers the user-reported gap ("Trotz vorhandenem menu erlaubt
    // aboutbox-creator nicht, die erzeugte box mit einem menupunkt zu
    // verbinden."): the AboutBox-Creator's own "Verknuepfter Menuepunkt"
    // combobox (gui/aboutboxdialog.cpp's collectMenuItems()) only offered
    // real ObjType::MenuItem leaves, silently skipping an ObjType::SubMenu
    // node with an empty childs list even though muicodegen.cpp's
    // ObjType::SubMenu emitObject() case (see above) emits such a node
    // with the EXACT same MenuitemObject/.../End shape as a true MenuItem
    // (confirmed against the user's own real project: their "About..."
    // entry is exactly such a renamed, childless SubMenu, label
    // "SubAbout"). That GUI-only gap is fixed in aboutboxdialog.cpp, not
    // here - this test instead confirms the CODEGEN side (identOf()/
    // ctx.objectByLabel/ctx.persistentLabels) was already fully generic
    // by label, not by ObjType, and correctly wires MUIA_Menuitem_Trigger
    // for a childless-SubMenu target exactly as it does for a real
    // MenuItem, so the GUI fix alone is sufficient.
    {
        auto proj = std::make_unique<MuibProject>();
        proj->title = QStringLiteral("AboutBoxSubMenuTest");
        proj->base = QStringLiteral("aboutboxsubmenutest");

        auto win = std::make_unique<MuibObject>(ObjType::Window);
        win->label = QStringLiteral("ABSMTestWin");
        win->title = QStringLiteral("AboutBox SubMenu Test");
        auto root = std::make_unique<MuibObject>(ObjType::Group);
        root->isRoot = true;
        root->label = QStringLiteral("ABSMTestWin_Root");
        win->root = std::move(root);

        auto menu = std::make_unique<MuibObject>(ObjType::Menu);
        menu->label = QStringLiteral("MN_root3");
        menu->menu_enable = true;

        auto project_ = std::make_unique<MuibObject>(ObjType::SubMenu);
        project_->label = QStringLiteral("SM_Project3");
        project_->name = QStringLiteral("Project");
        project_->menu_enable = true;

        // A childless SubMenu, renamed via the generic "Label" row exactly
        // like the user's own real "SubAbout" - functionally a leaf, no
        // MUIA_Family_Child children of its own.
        auto about_ = std::make_unique<MuibObject>(ObjType::SubMenu);
        about_->label = QStringLiteral("SM_About3");
        about_->name = QStringLiteral("About...");
        about_->menu_enable = true;
        project_->childs.push_back(std::move(about_));

        menu->childs.push_back(std::move(project_));
        win->menu = std::move(menu);
        proj->windows.push_back(std::move(win));

        auto about = std::make_unique<MuibObject>(ObjType::AboutBox);
        about->label = QStringLiteral("MyAboutBox2");
        about->aboutCredits = QStringLiteral("Test");
        about->aboutLinkedMenuItem = QStringLiteral("SM_About3");
        proj->aboutBox = std::move(about);

        const QString base = QStringLiteral("aboutboxsubmenutest");
        QString sourceText = MuiCodeGen::generateSource(*proj, base);

        CHECK(sourceText.contains(QStringLiteral(
                  "MUIA_Family_Child, (Gui.SM_About3 = MenuitemObject,\n\t\t\t\t\tMUIA_Menuitem_Title, \"About...\",\n\t\t\t\tEnd),")),
              "synthetic AboutBox/SubMenu project: the childless SubMenu emits as a plain leaf (MenuitemObject/Title/End immediately, no nested MUIA_Family_Child of its own)");
        CHECK(sourceText.contains(QStringLiteral("MUIM_Notify, MUIA_Menuitem_Trigger, MUIV_EveryTime,")) &&
                  sourceText.contains(QStringLiteral("MUIM_Set, MUIA_Window_Open, TRUE")),
              "synthetic AboutBox/SubMenu project: a childless SubMenu resolves as the AboutBox's linked menu item, same as a real MenuItem would");

        QString detail;
        bool balanced = isBalanced(sourceText, &detail);
        CHECK(balanced, qPrintable(QStringLiteral("synthetic AboutBox/SubMenu project: source brackets balanced (%1)")
                                        .arg(balanced ? QStringLiteral("ok") : detail)));

        QFile asf(QDir(outDir).filePath(base + QStringLiteral(".c")));
        if (asf.open(QIODevice::WriteOnly | QIODevice::Truncate))
            asf.write(sourceText.toLatin1());
    }

    // --- Synthetic "no AboutBox at all" regression -------------------------
    // Confirms the whole feature is fully opt-in: a project with no
    // AboutBox never pulls in Aboutbox.mcc or any of its wiring.
    {
        auto proj = std::make_unique<MuibProject>();
        proj->title = QStringLiteral("NoAboutBoxTest");
        proj->base = QStringLiteral("noaboutboxtest");
        auto win = std::make_unique<MuibObject>(ObjType::Window);
        win->label = QStringLiteral("NABWin");
        auto root = std::make_unique<MuibObject>(ObjType::Group);
        root->isRoot = true;
        root->label = QStringLiteral("NABWin_Root");
        win->root = std::move(root);
        proj->windows.push_back(std::move(win));

        const QString base = QStringLiteral("noaboutboxtest");
        QString sourceText = MuiCodeGen::generateSource(*proj, base);
        CHECK(!sourceText.contains(QStringLiteral("Aboutbox_mcc.h")),
              "no-AboutBox project: Aboutbox.mcc header is NOT included when there is no AboutBox");
        CHECK(!sourceText.contains(QStringLiteral("AboutboxObject,")),
              "no-AboutBox project: no AboutboxObject emitted");
        CHECK(!sourceText.contains(QStringLiteral("MUIA_Application_UsedClasses,")),
              "no-AboutBox project: no MUIA_Application_UsedClasses emitted");

        QString detail;
        bool balanced = isBalanced(sourceText, &detail);
        CHECK(balanced, qPrintable(QStringLiteral("no-AboutBox project: source brackets balanced (%1)")
                                        .arg(balanced ? QStringLiteral("ok") : detail)));
    }

    // --- Synthetic help/.guide generation regression -----------------------
    // Covers the user's own bug report ("es werden keine Hilfetexte
    // generiert und angezeigt, obwohl ich welche angegeben habe"): a
    // Window, a help-bearing Group, and a help-bearing leaf (String),
    // checking MUIA_HelpNode/MUIA_Application_HelpFile emission
    // (generateSource()) AND the actual .guide file content
    // (generateGuide()) together, since neither alone is useful without
    // the other (a HelpNode with no matching guide node is a dangling
    // reference at runtime).
    {
        auto proj = std::make_unique<MuibProject>();
        proj->title = QStringLiteral("HelpTest");
        proj->base = QStringLiteral("helptest");
        // proj->helpfile deliberately left empty - exercises the
        // "<baseName>.guide" auto-default (see generateSource()'s own
        // MUIA_Application_HelpFile gate).

        auto win = std::make_unique<MuibObject>(ObjType::Window);
        win->label = QStringLiteral("HelpWin");
        win->title = QStringLiteral("Help Window");
        win->help.generated = true;
        win->help.content = QByteArrayLiteral("Window help body.");

        auto root = std::make_unique<MuibObject>(ObjType::Group);
        root->isRoot = true;
        root->label = QStringLiteral("HelpWin_Root");

        auto group = std::make_unique<MuibObject>(ObjType::Group);
        group->label = QStringLiteral("HelpGroup");
        group->help.generated = true;
        group->help.title = QStringLiteral("GroupHelp");
        group->help.content = QByteArrayLiteral("Group help body.");

        auto str = std::make_unique<MuibObject>(ObjType::String);
        str->label = QStringLiteral("HelpString");
        str->help.generated = true;
        str->help.title = QStringLiteral("StringHelp");
        str->help.content = QByteArrayLiteral("String help body.");
        group->children.push_back(std::move(str));

        root->children.push_back(std::move(group));
        win->root = std::move(root);
        proj->windows.push_back(std::move(win));

        const QString base = QStringLiteral("helptest");
        QString sourceText = MuiCodeGen::generateSource(*proj, base);

        CHECK(sourceText.contains(QStringLiteral("MUIA_HelpNode, \"HelpWin\",")),
              "synthetic help project: Window gets its own MUIA_HelpNode");
        CHECK(sourceText.contains(QStringLiteral("MUIA_HelpNode, \"HelpGroup\",")),
              "synthetic help project: help-bearing Group gets MUIA_HelpNode");
        CHECK(sourceText.contains(QStringLiteral("MUIA_HelpNode, \"HelpString\",")),
              "synthetic help project: help-bearing leaf (String) gets MUIA_HelpNode");
        CHECK(sourceText.contains(QStringLiteral("MUIA_Application_HelpFile, \"helptest.guide\",")),
              "synthetic help project: MUIA_Application_HelpFile defaults to \"<baseName>.guide\" when proj.helpfile is empty but help content exists");

        QString guideText = MuiCodeGen::generateGuide(*proj);
        CHECK(guideText.contains(QStringLiteral("@NODE HelpWin \"Help Window\"")) &&
                  guideText.contains(QStringLiteral("Window help body.")),
              "synthetic help project: .guide has the Window's own node, titled with its real window title");
        CHECK(guideText.contains(QStringLiteral("@NODE HelpGroup \"GroupHelp\"")) &&
                  guideText.contains(QStringLiteral("Group help body.")),
              "synthetic help project: .guide has the Group's node, titled with its own help title");
        CHECK(guideText.contains(QStringLiteral("@NODE HelpString \"StringHelp\"")) &&
                  guideText.contains(QStringLiteral("String help body.")),
              "synthetic help project: .guide has the leaf String's node");
        CHECK(guideText.contains(QStringLiteral("@{\" GroupHelp \" link HelpGroup }")),
              "synthetic help project: the Window's node cross-references its help-bearing Group child");
        CHECK(guideText.contains(QStringLiteral("@{\" StringHelp \" link HelpString }")),
              "synthetic help project: the Group's node cross-references its help-bearing String child");
        CHECK(guideText.count(QStringLiteral("@ENDNODE")) == 3,
              qPrintable(QStringLiteral("synthetic help project: exactly 3 @ENDNODE (Window+Group+String), found %1")
                             .arg(guideText.count(QStringLiteral("@ENDNODE")))));
    }

    // --- Synthetic window-position regression -------------------------
    // Covers the still-open "Fenster wird immer in der Bildschirmmitte
    // platziert" complaint (item E): WindowPositionMode is a genuine new
    // MuiBuilderQt-only feature (see its own comment in muibobject.h),
    // since the real original tool never emits MUIA_Window_LeftEdge/
    // TopEdge at all (full v2.3 source grep confirmed no code path ever
    // writes them - a window always opens at MUI's own default position).
    // One project with three windows, one per mode: Centered (the
    // default) must emit NEITHER attribute at all, so an untouched
    // project's generated code stays byte-identical to before this
    // feature existed; Moused emits the real MUIV_Window_LeftEdge_Moused/
    // MUIV_Window_TopEdge_Moused special values (confirmed in the real
    // MUI 5.0 SDK's include/libraries/mui.h); Manual emits the literal
    // posX/posY pixel coordinates.
    {
        // Isolates one window's own Build...Window(void) function text
        // out of the whole-project source, so "no LeftEdge/TopEdge at
        // all" can be checked per-window rather than across the whole
        // file (where another window's own attributes would leak in).
        auto extractWindowFunc = [](const QString &text, const QString &winLabel) -> QString {
            const QString marker = QStringLiteral("APTR Build%1Window(void)").arg(winLabel);
            const int start = text.indexOf(marker);
            if (start < 0)
                return QString();
            const int nextStart = text.indexOf(QStringLiteral("APTR Build"), start + marker.size());
            return nextStart < 0 ? text.mid(start) : text.mid(start, nextStart - start);
        };
        auto makeWin = [](const QString &label, WindowPositionMode mode, int x, int y) {
            auto win = std::make_unique<MuibObject>(ObjType::Window);
            win->label = label;
            win->title = label;
            win->posMode = mode;
            win->posX = x;
            win->posY = y;
            auto root = std::make_unique<MuibObject>(ObjType::Group);
            root->isRoot = true;
            root->label = label + QStringLiteral("_Root");
            win->root = std::move(root);
            return win;
        };

        auto proj = std::make_unique<MuibProject>();
        proj->title = QStringLiteral("PosTest");
        proj->base = QStringLiteral("postest");
        proj->windows.push_back(makeWin(QStringLiteral("CenteredWin"), WindowPositionMode::Centered, 0, 0));
        proj->windows.push_back(makeWin(QStringLiteral("MousedWin"), WindowPositionMode::Moused, 0, 0));
        proj->windows.push_back(makeWin(QStringLiteral("ManualWin"), WindowPositionMode::Manual, 123, 45));

        const QString base = QStringLiteral("postest");
        const QString sourceText = MuiCodeGen::generateSource(*proj, base);

        const QString centeredFunc = extractWindowFunc(sourceText, QStringLiteral("CenteredWin"));
        const QString mousedFunc = extractWindowFunc(sourceText, QStringLiteral("MousedWin"));
        const QString manualFunc = extractWindowFunc(sourceText, QStringLiteral("ManualWin"));

        CHECK(!centeredFunc.isEmpty() && !centeredFunc.contains(QStringLiteral("MUIA_Window_LeftEdge")) &&
                  !centeredFunc.contains(QStringLiteral("MUIA_Window_TopEdge")),
              "synthetic window-position project: Centered mode emits no MUIA_Window_LeftEdge/TopEdge at all "
              "(byte-identical to before this feature existed)");
        CHECK(!mousedFunc.isEmpty() &&
                  mousedFunc.contains(QStringLiteral("MUIA_Window_LeftEdge, MUIV_Window_LeftEdge_Moused,")) &&
                  mousedFunc.contains(QStringLiteral("MUIA_Window_TopEdge, MUIV_Window_TopEdge_Moused,")),
              "synthetic window-position project: Moused mode emits the real MUIV_Window_LeftEdge_Moused/"
              "MUIV_Window_TopEdge_Moused special values");
        CHECK(!manualFunc.isEmpty() &&
                  manualFunc.contains(QStringLiteral("MUIA_Window_LeftEdge, 123,")) &&
                  manualFunc.contains(QStringLiteral("MUIA_Window_TopEdge, 45,")),
              "synthetic window-position project: Manual mode emits the literal posX/posY pixel coordinates");

        QString detail;
        bool balanced = isBalanced(sourceText, &detail);
        CHECK(balanced, qPrintable(QStringLiteral("synthetic window-position project: source brackets balanced (%1)")
                                        .arg(balanced ? QStringLiteral("ok") : detail)));
    }

    // --- Window size (winWidth/winHeight/winMin*/winMax*) codegen ------
    // See the winWidth/... comment right next to WindowPositionMode in
    // muibobject.h and MuiCodeGen::emitObject()'s own doc comment in
    // muicodegen.h for the full design (real MUIA_Window_Width/Height on
    // the WindowObject itself; real but GENERIC Area-class MUIA_MinWidth/
    // MUIA_MinHeight/MUIA_MaxWidth/MUIA_MaxHeight on the root CONTENT
    // object instead). One project with two windows: DefaultWin (every
    // field left at 0) must emit NONE of these six attributes anywhere,
    // so an untouched project's generated code stays byte-identical to
    // before this feature existed; SizedWin (every field set) must emit
    // all six, Width/Height inside its own WindowObject tag list and the
    // four Min/Max ones inside its root Group's own tag list.
    {
        auto extractWindowFunc = [](const QString &text, const QString &winLabel) -> QString {
            const QString marker = QStringLiteral("APTR Build%1Window(void)").arg(winLabel);
            const int start = text.indexOf(marker);
            if (start < 0)
                return QString();
            const int nextStart = text.indexOf(QStringLiteral("APTR Build"), start + marker.size());
            return nextStart < 0 ? text.mid(start) : text.mid(start, nextStart - start);
        };
        auto makeWin = [](const QString &label, int w, int h, int minW, int minH, int maxW, int maxH) {
            auto win = std::make_unique<MuibObject>(ObjType::Window);
            win->label = label;
            win->title = label;
            win->winWidth = w;
            win->winHeight = h;
            win->winMinWidth = minW;
            win->winMinHeight = minH;
            win->winMaxWidth = maxW;
            win->winMaxHeight = maxH;
            auto root = std::make_unique<MuibObject>(ObjType::Group);
            root->isRoot = true;
            root->label = label + QStringLiteral("_Root");
            win->root = std::move(root);
            return win;
        };

        auto proj = std::make_unique<MuibProject>();
        proj->title = QStringLiteral("SizeTest");
        proj->base = QStringLiteral("sizetest");
        proj->windows.push_back(makeWin(QStringLiteral("DefaultWin"), 0, 0, 0, 0, 0, 0));
        proj->windows.push_back(makeWin(QStringLiteral("SizedWin"), 640, 480, 200, 150, 1024, 768));

        const QString base = QStringLiteral("sizetest");
        const QString sourceText = MuiCodeGen::generateSource(*proj, base);

        const QString defaultFunc = extractWindowFunc(sourceText, QStringLiteral("DefaultWin"));
        const QString sizedFunc = extractWindowFunc(sourceText, QStringLiteral("SizedWin"));

        CHECK(!defaultFunc.isEmpty() &&
                  !defaultFunc.contains(QStringLiteral("MUIA_Window_Width")) &&
                  !defaultFunc.contains(QStringLiteral("MUIA_Window_Height")) &&
                  !defaultFunc.contains(QStringLiteral("MUIA_MinWidth")) &&
                  !defaultFunc.contains(QStringLiteral("MUIA_MinHeight")) &&
                  !defaultFunc.contains(QStringLiteral("MUIA_MaxWidth")) &&
                  !defaultFunc.contains(QStringLiteral("MUIA_MaxHeight")),
              "synthetic window-size project: every field left at 0 emits none of the six size attributes "
              "(byte-identical to before this feature existed)");
        CHECK(!sizedFunc.isEmpty() && sizedFunc.contains(QStringLiteral("MUIA_Window_Width, 640,")),
              "synthetic window-size project: MUIA_Window_Width emitted on the WindowObject itself");
        CHECK(!sizedFunc.isEmpty() && sizedFunc.contains(QStringLiteral("MUIA_Window_Height, 480,")),
              "synthetic window-size project: MUIA_Window_Height emitted on the WindowObject itself");
        CHECK(!sizedFunc.isEmpty() && sizedFunc.contains(QStringLiteral("MUIA_MinWidth, 200,")),
              "synthetic window-size project: MUIA_MinWidth emitted on the root content object");
        CHECK(!sizedFunc.isEmpty() && sizedFunc.contains(QStringLiteral("MUIA_MinHeight, 150,")),
              "synthetic window-size project: MUIA_MinHeight emitted on the root content object");
        CHECK(!sizedFunc.isEmpty() && sizedFunc.contains(QStringLiteral("MUIA_MaxWidth, 1024,")),
              "synthetic window-size project: MUIA_MaxWidth emitted on the root content object");
        CHECK(!sizedFunc.isEmpty() && sizedFunc.contains(QStringLiteral("MUIA_MaxHeight, 768,")),
              "synthetic window-size project: MUIA_MaxHeight emitted on the root content object");

        QString detailSz;
        bool balancedSz = isBalanced(sourceText, &detailSz);
        CHECK(balancedSz, qPrintable(QStringLiteral("synthetic window-size project: source brackets balanced (%1)")
                                          .arg(balancedSz ? QStringLiteral("ok") : detailSz)));
    }

    // --- MuibQtExtras sidecar round-trip for window size ----------------
    // Same save/reset/load proof as the window-position round-trip below,
    // for winWidth/winHeight/winMin*/winMax* instead.
    {
        auto proj = std::make_unique<MuibProject>();
        proj->title = QStringLiteral("SizeRoundtrip");
        proj->base = QStringLiteral("sizeroundtrip");

        auto winA = std::make_unique<MuibObject>(ObjType::Window);
        winA->label = QStringLiteral("SizeWinA");
        winA->winWidth = 320;
        winA->winHeight = 240;
        winA->winMinWidth = 100;
        winA->winMinHeight = 80;
        winA->winMaxWidth = 800;
        winA->winMaxHeight = 600;
        proj->windows.push_back(std::move(winA));

        auto winB = std::make_unique<MuibObject>(ObjType::Window);
        winB->label = QStringLiteral("SizeWinB");
        // left at the default (every field 0) deliberately
        proj->windows.push_back(std::move(winB));

        const QString fakeMuibPath = QDir(outDir).filePath(QStringLiteral("sizeroundtrip.MUIB"));
        const QString sidecarPath = MuibQtExtras::sidecarPathFor(fakeMuibPath);
        QFile::remove(sidecarPath);   // in case a previous run left one behind

        QString extrasError;
        bool saveOk = MuibQtExtras::save(*proj, fakeMuibPath, &extrasError);
        CHECK(saveOk, qPrintable(QStringLiteral("window-size round-trip: save() succeeded (%1)").arg(extrasError)));
        CHECK(QFile::exists(sidecarPath),
              "window-size round-trip: sidecar file was written (SizeWinA is non-default)");

        proj->windows[0]->winWidth = 0;
        proj->windows[0]->winHeight = 0;
        proj->windows[0]->winMinWidth = 0;
        proj->windows[0]->winMinHeight = 0;
        proj->windows[0]->winMaxWidth = 0;
        proj->windows[0]->winMaxHeight = 0;

        QString loadError;
        bool loadOk = MuibQtExtras::load(*proj, fakeMuibPath, &loadError);
        CHECK(loadOk, qPrintable(QStringLiteral("window-size round-trip: load() succeeded (%1)").arg(loadError)));
        CHECK(proj->windows[0]->winWidth == 320 && proj->windows[0]->winHeight == 240 &&
                  proj->windows[0]->winMinWidth == 100 && proj->windows[0]->winMinHeight == 80 &&
                  proj->windows[0]->winMaxWidth == 800 && proj->windows[0]->winMaxHeight == 600,
              "window-size round-trip: SizeWinA's six size fields survived save+load, matched by label");
        CHECK(proj->windows[1]->winWidth == 0 && proj->windows[1]->winMaxHeight == 0,
              "window-size round-trip: SizeWinB (never touched) stays at the default 0");

        proj->windows[0]->winWidth = 0;
        proj->windows[0]->winHeight = 0;
        proj->windows[0]->winMinWidth = 0;
        proj->windows[0]->winMinHeight = 0;
        proj->windows[0]->winMaxWidth = 0;
        proj->windows[0]->winMaxHeight = 0;
        MuibQtExtras::save(*proj, fakeMuibPath, nullptr);
        CHECK(!QFile::exists(sidecarPath),
              "window-size round-trip: sidecar is removed once every window is back at the default "
              "(nothing left to persist)");
    }

    // --- Menu-Editor Dialog: "Mutual Group" (excludeGroup) codegen -----
    // See muicodegen.h's emitMenuExcludeGroupWiring() doc comment for the
    // full design rationale (explicit MUIM_Notify/MUIM_Set sibling wiring
    // chosen deliberately over the undocumented MUIA_Menuitem_Exclude
    // bitmask attribute). One window whose menu tree exercises every rule
    // the function documents:
    //   - MI_A/MI_B: two check_enable MenuItems sharing excludeGroup==1
    //     directly under the root Menu -> must get pairwise wiring both
    //     ways.
    //   - MI_C: check_enable but excludeGroup==0 (not in any group) ->
    //     must get NO wiring at all.
    //   - MI_G: excludeGroup==1 (same number/parent as A/B) but
    //     check_enable==false -> must NOT be pulled into A/B's group.
    //   - SM_Sub/MI_D/MI_E: a NESTED SubMenu whose own two children reuse
    //     excludeGroup==1 - since grouping is scoped to the immediate
    //     parent, these must be wired to EACH OTHER only, never to the
    //     top-level MI_A/MI_B pair that happens to share the same number.
    //   - MI_F: the lone member of excludeGroup==2 under SM_Sub -> a
    //     "group" of one has nothing to exclude, so no wiring either.
    {
        auto proj = std::make_unique<MuibProject>();
        proj->title = QStringLiteral("ExclTest");
        proj->base = QStringLiteral("excltest");

        auto win = std::make_unique<MuibObject>(ObjType::Window);
        win->label = QStringLiteral("ExclTestWin");
        win->title = QStringLiteral("Excl Test");
        auto root = std::make_unique<MuibObject>(ObjType::Group);
        root->isRoot = true;
        root->label = QStringLiteral("ExclTestWin_Root");
        win->root = std::move(root);

        auto menu = std::make_unique<MuibObject>(ObjType::Menu);
        menu->label = QStringLiteral("MN_ExclRoot");
        menu->menu_enable = true;

        auto makeItem = [](const QString &label, bool checkEnable, int group) {
            auto item = std::make_unique<MuibObject>(ObjType::MenuItem);
            item->label = label;
            item->name = label;
            item->menu_enable = true;
            item->check_enable = checkEnable;
            item->excludeGroup = group;
            return item;
        };

        menu->childs.push_back(makeItem(QStringLiteral("MI_A"), true, 1));
        menu->childs.push_back(makeItem(QStringLiteral("MI_B"), true, 1));
        menu->childs.push_back(makeItem(QStringLiteral("MI_C"), true, 0));
        menu->childs.push_back(makeItem(QStringLiteral("MI_G"), false, 1));

        auto sub = std::make_unique<MuibObject>(ObjType::SubMenu);
        sub->label = QStringLiteral("SM_Sub");
        sub->name = QStringLiteral("Sub");
        sub->menu_enable = true;
        sub->childs.push_back(makeItem(QStringLiteral("MI_D"), true, 1));
        sub->childs.push_back(makeItem(QStringLiteral("MI_E"), true, 1));
        sub->childs.push_back(makeItem(QStringLiteral("MI_F"), true, 2));
        menu->childs.push_back(std::move(sub));

        win->menu = std::move(menu);
        proj->windows.push_back(std::move(win));

        const QString base = QStringLiteral("excltest");
        const QString sourceText = MuiCodeGen::generateSource(*proj, base);

        const QString wiringMarker = QStringLiteral("MuiBuilderQt \"Mutual Group\"");
        QStringList wiringLines;
        for (const QString &line : sourceText.split(QLatin1Char('\n')))
            if (line.contains(wiringMarker))
                wiringLines << line;
        const QString wiringBlock = wiringLines.join(QLatin1Char('\n'));

        CHECK(wiringLines.size() == 4,
              qPrintable(QStringLiteral("synthetic exclude-group project: exactly 4 wiring DoMethod lines emitted "
                                         "(2 for MI_A/MI_B + 2 for MI_D/MI_E), found %1")
                             .arg(wiringLines.size())));
        CHECK(wiringBlock.contains(QStringLiteral("DoMethod(MI_A, MUIM_Notify, MUIA_Menuitem_Checked, TRUE, MI_B, 3, MUIM_Set, MUIA_Menuitem_Checked, FALSE);")),
              "synthetic exclude-group project: MI_A -> MI_B wiring emitted");
        CHECK(wiringBlock.contains(QStringLiteral("DoMethod(MI_B, MUIM_Notify, MUIA_Menuitem_Checked, TRUE, MI_A, 3, MUIM_Set, MUIA_Menuitem_Checked, FALSE);")),
              "synthetic exclude-group project: MI_B -> MI_A wiring emitted (both directions)");
        CHECK(wiringBlock.contains(QStringLiteral("DoMethod(MI_D, MUIM_Notify, MUIA_Menuitem_Checked, TRUE, MI_E, 3, MUIM_Set, MUIA_Menuitem_Checked, FALSE);")),
              "synthetic exclude-group project: nested SubMenu's MI_D -> MI_E wiring emitted");
        CHECK(wiringBlock.contains(QStringLiteral("DoMethod(MI_E, MUIM_Notify, MUIA_Menuitem_Checked, TRUE, MI_D, 3, MUIM_Set, MUIA_Menuitem_Checked, FALSE);")),
              "synthetic exclude-group project: nested SubMenu's MI_E -> MI_D wiring emitted (both directions)");
        CHECK(!wiringBlock.contains(QStringLiteral("MI_C")),
              "synthetic exclude-group project: MI_C (excludeGroup 0) gets no wiring at all");
        CHECK(!wiringBlock.contains(QStringLiteral("MI_G")),
              "synthetic exclude-group project: MI_G (same group/parent as A/B but check_enable false) gets no wiring");
        CHECK(!wiringBlock.contains(QStringLiteral("MI_F")),
              "synthetic exclude-group project: MI_F (lone member of its group) gets no wiring");
        bool noCrossGroupWiring = true;
        for (const QString &line : wiringLines)
        {
            const bool hasTopLevel = line.contains(QStringLiteral("MI_A")) || line.contains(QStringLiteral("MI_B"));
            const bool hasNested = line.contains(QStringLiteral("MI_D")) || line.contains(QStringLiteral("MI_E"));
            if (hasTopLevel && hasNested)
                noCrossGroupWiring = false;
        }
        CHECK(noCrossGroupWiring,
              "synthetic exclude-group project: top-level group 1 (A/B) and nested SubMenu's group 1 (D/E) "
              "never wired to each other despite sharing the same group number (scoped to immediate parent)");

        QString detail2;
        bool balanced2 = isBalanced(sourceText, &detail2);
        CHECK(balanced2, qPrintable(QStringLiteral("synthetic exclude-group project: source brackets balanced (%1)")
                                         .arg(balanced2 ? QStringLiteral("ok") : detail2)));
    }

    // --- Menu-item debug stubs (Feature E: "jeder Menueintrag und Sub-
    // menueintrag ... eine leere, von Hand editierbare Funktion") --------
    // One window whose menu tree exercises every rule this feature
    // documents (see isStubbableGadgetType()/isNotifiableGadgetType()/
    // notifyAttrFor()/collectAllGadgets()/collectWireableGadgets()'s own
    // comments in muicodegen.cpp):
    //   - MI_Quit: an ordinary depth-1 leaf MenuItem -> gets a stub AND
    //     real MUIA_Menuitem_Trigger auto-wiring, same as any other
    //     notifiable gadget.
    //   - MI_Copy: a depth-2 leaf (nested under a SubMenu-converted
    //     Menuepunkt) -> proves depth doesn't matter, only "is it a real
    //     leaf MenuItem".
    //   - MI_Sep: a BarLabel separator -> must get NO stub at all (not
    //     even an unwired one - see isMenuSeparator()).
    //   - MI_About: linked from the project's own AboutBox -> must get NO
    //     stub, NO wiring, NO switch/case (see collectAllGadgets()'s own
    //     AboutBox-exclusion filter) - its real job is opening the
    //     AboutBox, not printing a debug line.
    //   - SM_File itself (the top-level "File" pulldown, a container
    //     SubMenu, never a leaf) -> must get no stub either, same as
    //     every other pure-container type.
    {
        auto proj = std::make_unique<MuibProject>();
        proj->title = QStringLiteral("MenuStubTest");
        proj->base = QStringLiteral("menustubtest");

        auto win = std::make_unique<MuibObject>(ObjType::Window);
        win->label = QStringLiteral("MenuStubWin");
        win->title = QStringLiteral("Menu Stub Test");
        auto root = std::make_unique<MuibObject>(ObjType::Group);
        root->isRoot = true;
        root->label = QStringLiteral("MenuStubWin_Root");
        win->root = std::move(root);

        auto menu = std::make_unique<MuibObject>(ObjType::Menu);
        menu->label = QStringLiteral("MN_StubRoot");
        menu->menu_enable = true;

        auto file_ = std::make_unique<MuibObject>(ObjType::SubMenu);
        file_->label = QStringLiteral("SM_File");
        file_->name = QStringLiteral("File");
        file_->menu_enable = true;

        auto about_ = std::make_unique<MuibObject>(ObjType::MenuItem);
        about_->label = QStringLiteral("MI_About");
        about_->name = QStringLiteral("About");
        about_->menu_enable = true;
        file_->childs.push_back(std::move(about_));

        auto sep_ = std::make_unique<MuibObject>(ObjType::MenuItem);
        sep_->label = QStringLiteral("MI_Sep");
        sep_->name = QStringLiteral("BarLabel");
        file_->childs.push_back(std::move(sep_));

        auto quit_ = std::make_unique<MuibObject>(ObjType::MenuItem);
        quit_->label = QStringLiteral("MI_Quit");
        quit_->name = QStringLiteral("Quit");
        quit_->menu_enable = true;
        file_->childs.push_back(std::move(quit_));

        menu->childs.push_back(std::move(file_));

        auto edit_ = std::make_unique<MuibObject>(ObjType::SubMenu);
        edit_->label = QStringLiteral("SM_Edit");
        edit_->name = QStringLiteral("Edit");
        edit_->menu_enable = true;

        auto copy_ = std::make_unique<MuibObject>(ObjType::MenuItem);
        copy_->label = QStringLiteral("MI_Copy");
        copy_->name = QStringLiteral("Copy");
        copy_->menu_enable = true;
        edit_->childs.push_back(std::move(copy_));

        menu->childs.push_back(std::move(edit_));

        win->menu = std::move(menu);
        proj->windows.push_back(std::move(win));

        auto about = std::make_unique<MuibObject>(ObjType::AboutBox);
        about->label = QStringLiteral("MenuStubAboutBox");
        about->aboutLinkedMenuItem = QStringLiteral("MI_About");
        proj->aboutBox = std::move(about);

        const QString base = QStringLiteral("menustubtest");
        const QString sourceText = MuiCodeGen::generateSource(*proj, base);
        const QString gadgetsText = MuiCodeGen::generateGadgetStubs(*proj, base);
        const QString mainText = MuiCodeGen::generateMain(*proj, base);

        CHECK(gadgetsText.contains(QStringLiteral("MI_Quit_Clicked(void)")),
              "menu-item stubs: ordinary leaf MenuItem (MI_Quit) gets a debug stub");
        CHECK(gadgetsText.contains(QStringLiteral("puts(\"Ich bin Menuepunkt MI_Quit und wurde getriggert\");")),
              "menu-item stubs: MI_Quit's stub uses the requested \"Ich bin Menuepunkt <label> und wurde getriggert\" wording (not \"geklickt\")");
        // debugStubDisplayTextFor()'s own comment (added after the
        // Chefentwickler once hand-edited the WRONG stub - MI_Copy's
        // real title "Copy" vs. MI_Quit's real title "Quit" are easy to
        // tell apart by label alone here, but a real project's own
        // auto-generated "MenuItem_2"/"MenuItem_5" labels are not) -
        // the real MUIA_Menuitem_Title text must appear as a comment
        // directly above each stub function.
        CHECK(gadgetsText.contains(QStringLiteral("/* \"Quit\" */\nstatic inline void MI_Quit_Clicked(void)")),
              "menu-item stubs: MI_Quit's stub is preceded by a comment showing its real title (\"Quit\")");
        CHECK(gadgetsText.contains(QStringLiteral("/* \"Copy\" */\nstatic inline void MI_Copy_Clicked(void)")),
              "menu-item stubs: MI_Copy's stub is preceded by a comment showing its real title (\"Copy\")");
        CHECK(gadgetsText.contains(QStringLiteral("MI_Copy_Clicked(void)")) &&
                  gadgetsText.contains(QStringLiteral("puts(\"Ich bin Menuepunkt MI_Copy und wurde getriggert\");")),
              "menu-item stubs: a depth-2 leaf (MI_Copy, nested under SM_Edit) gets a stub too - depth doesn't matter");
        CHECK(!gadgetsText.contains(QStringLiteral("MI_Sep_Clicked")),
              "menu-item stubs: a BarLabel separator (MI_Sep) gets no stub at all");
        CHECK(!gadgetsText.contains(QStringLiteral("MI_About_Clicked")),
              "menu-item stubs: the AboutBox-linked MenuItem (MI_About) gets no stub - its real job is opening the AboutBox");
        CHECK(!gadgetsText.contains(QStringLiteral("SM_File_Clicked")) &&
                  !gadgetsText.contains(QStringLiteral("SM_Edit_Clicked")),
              "menu-item stubs: container SubMenu titles (SM_File/SM_Edit) get no stub, same as every other pure-container type");

        QRegularExpression quitWire(QStringLiteral(
            "DoMethod\\((Gui\\.)?MI_Quit, MUIM_Notify, MUIA_Menuitem_Trigger, MUIV_EveryTime, "
            "MUIV_Notify_Application, 2, MUIM_Application_ReturnID, (\\d+)\\);"));
        QRegularExpressionMatch quitMatch = quitWire.match(sourceText);
        CHECK(quitMatch.hasMatch(),
              "menu-item stubs: MI_Quit gets a real MUIA_Menuitem_Trigger DoMethod wiring in generateSource()");
        QRegularExpression copyWire(QStringLiteral(
            "DoMethod\\((Gui\\.)?MI_Copy, MUIM_Notify, MUIA_Menuitem_Trigger, MUIV_EveryTime, "
            "MUIV_Notify_Application, 2, MUIM_Application_ReturnID, (\\d+)\\);"));
        QRegularExpressionMatch copyMatch = copyWire.match(sourceText);
        CHECK(copyMatch.hasMatch(),
              "menu-item stubs: MI_Copy (depth-2) gets the same real wiring");
        CHECK(quitMatch.hasMatch() && copyMatch.hasMatch() &&
                  quitMatch.captured(2) != copyMatch.captured(2),
              "menu-item stubs: MI_Quit and MI_Copy get distinct notify ids");

        CHECK(!sourceText.contains(QStringLiteral("MI_Sep, MUIM_Notify, MUIA_Menuitem_Trigger")),
              "menu-item stubs: the separator (MI_Sep) gets no DoMethod wiring");
        // MI_About legitimately DOES carry a MUIA_Menuitem_Trigger DoMethod
        // already - the real, separate, pre-existing wiring that opens the
        // AboutBox (see the "synthetic AboutBox test" above). What must be
        // ABSENT is specifically a SECOND, auto-wired-debug-stub-style one
        // (the MUIV_Notify_Application/MUIM_Application_ReturnID pattern
        // notifyAttrFor()'s callers emit - see the MI_Quit/MI_Copy checks
        // above), which this feature must never add on top of it.
        CHECK(!sourceText.contains(QStringLiteral(
                  "MI_About, MUIM_Notify, MUIA_Menuitem_Trigger, MUIV_EveryTime, "
                  "MUIV_Notify_Application, 2, MUIM_Application_ReturnID,")),
              "menu-item stubs: the AboutBox-linked item (MI_About) gets no SECOND, auto-wired-debug-stub "
              "DoMethod on top of its own, separate MUIA_Menuitem_Trigger->MUIA_Window_Open wiring");

        // The switch comment now also carries the real menu title (e.g.
        // "case 1: /* MI_Quit "Quit" */") - see debugStubDisplayTextFor()
        // in muicodegen.cpp - so match the ident prefix only, not the
        // whole comment text.
        if (quitMatch.hasMatch())
        {
            CHECK(mainText.contains(QStringLiteral("case %1: /* MI_Quit").arg(quitMatch.captured(2))) &&
                      mainText.contains(QStringLiteral("MI_Quit_Clicked();")),
                  "menu-item stubs: generateMain()'s switch dispatches MI_Quit's id to MI_Quit_Clicked()");
            CHECK(mainText.contains(QStringLiteral("case %1: /* MI_Quit \"Quit\" */").arg(quitMatch.captured(2))),
                  "menu-item stubs: the switch dispatch's own comment also shows MI_Quit's real title");
        }
        if (copyMatch.hasMatch())
            CHECK(mainText.contains(QStringLiteral("case %1: /* MI_Copy").arg(copyMatch.captured(2))) &&
                      mainText.contains(QStringLiteral("MI_Copy_Clicked();")),
                  "menu-item stubs: generateMain()'s switch dispatches MI_Copy's id to MI_Copy_Clicked()");
        CHECK(!mainText.contains(QStringLiteral("MI_About_Clicked();")) &&
                  !mainText.contains(QStringLiteral("MI_Sep_Clicked();")),
              "menu-item stubs: neither the AboutBox-linked item nor the separator ever appear in generateMain()'s dispatch");

        QString detailQ1, detailQ2, detailQ3;
        bool srcBalanced = isBalanced(sourceText, &detailQ1);
        CHECK(srcBalanced, qPrintable(QStringLiteral("menu-item stubs: source brackets balanced (%1)")
                                           .arg(srcBalanced ? QStringLiteral("ok") : detailQ1)));
        bool gadgetsBalanced = isBalanced(gadgetsText, &detailQ2);
        CHECK(gadgetsBalanced, qPrintable(QStringLiteral("menu-item stubs: gadgets-header brackets balanced (%1)")
                                               .arg(gadgetsBalanced ? QStringLiteral("ok") : detailQ2)));
        bool mainBalanced = isBalanced(mainText, &detailQ3);
        CHECK(mainBalanced, qPrintable(QStringLiteral("menu-item stubs: main brackets balanced (%1)")
                                            .arg(mainBalanced ? QStringLiteral("ok") : detailQ3)));
    }

    // --- Menu-item debug stubs: proj.appMenu gets a stub but is never
    // auto-wired (the pre-existing, already-documented structural gap
    // this feature inherits from the "Mutual Group" feature - see
    // muicodegen.h's emitMenuExcludeGroupWiring() comment and
    // collectWireableGadgets()'s own comment in muicodegen.cpp: BuildMenu-
    // strip(), the function that builds proj.appMenu, has no notify-
    // emission step at all, unlike every BuildXWindow()) -------------------
    {
        auto proj = std::make_unique<MuibProject>();
        proj->title = QStringLiteral("AppMenuStubTest");
        proj->base = QStringLiteral("appmenustubtest");

        auto appMenu = std::make_unique<MuibObject>(ObjType::Menu);
        appMenu->label = QStringLiteral("MN_AppRoot");
        appMenu->menu_enable = true;

        auto help_ = std::make_unique<MuibObject>(ObjType::SubMenu);
        help_->label = QStringLiteral("SM_AppHelp");
        help_->name = QStringLiteral("Help");
        help_->menu_enable = true;

        auto index_ = std::make_unique<MuibObject>(ObjType::MenuItem);
        index_->label = QStringLiteral("MI_AppIndex");
        index_->name = QStringLiteral("Index");
        index_->menu_enable = true;
        help_->childs.push_back(std::move(index_));

        appMenu->childs.push_back(std::move(help_));
        proj->appMenu = std::move(appMenu);

        const QString base = QStringLiteral("appmenustubtest");
        const QString gadgetsText = MuiCodeGen::generateGadgetStubs(*proj, base);
        const QString sourceText = MuiCodeGen::generateSource(*proj, base);
        const QString mainText = MuiCodeGen::generateMain(*proj, base);

        CHECK(gadgetsText.contains(QStringLiteral("MI_AppIndex_Clicked(void)")) &&
                  gadgetsText.contains(QStringLiteral("puts(\"Ich bin Menuepunkt MI_AppIndex und wurde getriggert\");")),
              "appMenu menu-item stubs: proj.appMenu's own MenuItem still gets a real, correctly-worded debug stub");
        CHECK(!sourceText.contains(QStringLiteral("MUIA_Menuitem_Trigger")),
              "appMenu menu-item stubs: proj.appMenu's own MenuItem gets NO auto-wired DoMethod "
              "(BuildMenustrip() has no notify-emission step - see collectWireableGadgets())");
        CHECK(!mainText.contains(QStringLiteral("MI_AppIndex_Clicked();")),
              "appMenu menu-item stubs: proj.appMenu's own MenuItem never appears in generateMain()'s "
              "switch dispatch either (no id was ever assigned to it)");

        QString detailA1, detailA2;
        bool gadgetsBalancedA = isBalanced(gadgetsText, &detailA1);
        CHECK(gadgetsBalancedA, qPrintable(QStringLiteral("appMenu menu-item stubs: gadgets-header brackets balanced (%1)")
                                                .arg(gadgetsBalancedA ? QStringLiteral("ok") : detailA1)));
        bool mainBalancedA = isBalanced(mainText, &detailA2);
        CHECK(mainBalancedA, qPrintable(QStringLiteral("appMenu menu-item stubs: main brackets balanced (%1)")
                                             .arg(mainBalancedA ? QStringLiteral("ok") : detailA2)));
    }

    // --- MuibQtExtras sidecar round-trip for window positions ----------
    // Confirms the JSON sidecar (core/muibqtextras.h) actually persists
    // posMode/posX/posY across a save+load cycle, keyed by label, exactly
    // like the pre-existing AboutBox round-trip this same mechanism
    // already carries - and that a project left entirely at the default
    // (Centered) writes NO sidecar file at all.
    {
        auto proj = std::make_unique<MuibProject>();
        proj->title = QStringLiteral("PosRoundtrip");
        proj->base = QStringLiteral("posroundtrip");

        auto winA = std::make_unique<MuibObject>(ObjType::Window);
        winA->label = QStringLiteral("WinA");
        winA->posMode = WindowPositionMode::Manual;
        winA->posX = 77;
        winA->posY = 88;
        proj->windows.push_back(std::move(winA));

        auto winB = std::make_unique<MuibObject>(ObjType::Window);
        winB->label = QStringLiteral("WinB");
        // left at the default (Centered) deliberately
        proj->windows.push_back(std::move(winB));

        const QString fakeMuibPath = QDir(outDir).filePath(QStringLiteral("posroundtrip.MUIB"));
        const QString sidecarPath = MuibQtExtras::sidecarPathFor(fakeMuibPath);
        QFile::remove(sidecarPath);   // in case a previous run left one behind

        QString extrasError;
        bool saveOk = MuibQtExtras::save(*proj, fakeMuibPath, &extrasError);
        CHECK(saveOk, qPrintable(QStringLiteral("MuibQtExtras round-trip: save() succeeded (%1)").arg(extrasError)));
        CHECK(QFile::exists(sidecarPath),
              "MuibQtExtras round-trip: sidecar file was written (WinA is non-default)");

        // Reset both windows to Centered/0/0 in memory, then reload from
        // the sidecar just written - proves load() actually restores the
        // saved state rather than the in-memory object merely having kept it.
        proj->windows[0]->posMode = WindowPositionMode::Centered;
        proj->windows[0]->posX = 0;
        proj->windows[0]->posY = 0;

        QString loadError;
        bool loadOk = MuibQtExtras::load(*proj, fakeMuibPath, &loadError);
        CHECK(loadOk, qPrintable(QStringLiteral("MuibQtExtras round-trip: load() succeeded (%1)").arg(loadError)));
        CHECK(proj->windows[0]->posMode == WindowPositionMode::Manual &&
                  proj->windows[0]->posX == 77 && proj->windows[0]->posY == 88,
              "MuibQtExtras round-trip: WinA's Manual posMode/posX/posY survived save+load, matched by label");
        CHECK(proj->windows[1]->posMode == WindowPositionMode::Centered,
              "MuibQtExtras round-trip: WinB (never touched) stays at the default Centered mode");

        // Now reset WinA back to Centered too and save again - the
        // sidecar should be removed entirely, not left behind empty.
        proj->windows[0]->posMode = WindowPositionMode::Centered;
        MuibQtExtras::save(*proj, fakeMuibPath, nullptr);
        CHECK(!QFile::exists(sidecarPath),
              "MuibQtExtras round-trip: sidecar is removed once every window is back at the default (nothing left to persist)");
    }

    // --- MuibQtExtras sidecar round-trip for MenuItem excludeGroup -----
    // Same save/reset/load proof as the window-position round-trip above,
    // but for the Menu-Editor Dialog's excludeGroup field - and,
    // specifically, using an item NESTED under a SubMenu (not just a
    // top-level MenuItem), since excludeGroup's persistence walk
    // (resetExcludeGroups()/findMenuItemByLabel()/
    // collectExcludeGroupEntries() in muibqtextras.cpp) is a recursive
    // tree walk, unlike posMode's flat window-list loop - a bug there
    // (e.g. only handling depth-1 children) would not be caught by a
    // top-level-only test.
    {
        auto proj = std::make_unique<MuibProject>();
        proj->title = QStringLiteral("ExclRoundtrip");
        proj->base = QStringLiteral("exclroundtrip");

        auto win = std::make_unique<MuibObject>(ObjType::Window);
        win->label = QStringLiteral("ExclRTWin");

        auto menu = std::make_unique<MuibObject>(ObjType::Menu);
        menu->label = QStringLiteral("MN_ExclRT");

        auto top = std::make_unique<MuibObject>(ObjType::MenuItem);
        top->label = QStringLiteral("MI_RT_Top");
        top->check_enable = true;
        top->excludeGroup = 5;
        menu->childs.push_back(std::move(top));

        auto untouched = std::make_unique<MuibObject>(ObjType::MenuItem);
        untouched->label = QStringLiteral("MI_RT_Untouched");
        untouched->check_enable = true;
        // left at the default (excludeGroup == 0) deliberately

        auto sub = std::make_unique<MuibObject>(ObjType::SubMenu);
        sub->label = QStringLiteral("SM_RT_Sub");

        auto nested = std::make_unique<MuibObject>(ObjType::MenuItem);
        nested->label = QStringLiteral("MI_RT_Nested");
        nested->check_enable = true;
        nested->excludeGroup = 7;
        sub->childs.push_back(std::move(nested));

        menu->childs.push_back(std::move(untouched));
        menu->childs.push_back(std::move(sub));

        win->menu = std::move(menu);
        proj->windows.push_back(std::move(win));

        const QString fakeMuibPath2 = QDir(outDir).filePath(QStringLiteral("exclroundtrip.MUIB"));
        const QString sidecarPath2 = MuibQtExtras::sidecarPathFor(fakeMuibPath2);
        QFile::remove(sidecarPath2);   // in case a previous run left one behind

        QString extrasError2;
        bool saveOk2 = MuibQtExtras::save(*proj, fakeMuibPath2, &extrasError2);
        CHECK(saveOk2, qPrintable(QStringLiteral("excludeGroup round-trip: save() succeeded (%1)").arg(extrasError2)));
        CHECK(QFile::exists(sidecarPath2),
              "excludeGroup round-trip: sidecar file was written (MI_RT_Top/MI_RT_Nested are non-default)");

        // Reset all three items to 0 in memory (mirrors what load() itself
        // does internally, but done here explicitly so the check below
        // proves load() actually restores state rather than the in-memory
        // objects merely having kept it).
        MuibObject *winPtr = proj->windows[0].get();
        MuibObject *topPtr = winPtr->menu->childs[0].get();
        MuibObject *untouchedPtr = winPtr->menu->childs[1].get();
        MuibObject *nestedPtr = winPtr->menu->childs[2]->childs[0].get();
        topPtr->excludeGroup = 0;
        nestedPtr->excludeGroup = 0;

        QString loadError2;
        bool loadOk2 = MuibQtExtras::load(*proj, fakeMuibPath2, &loadError2);
        CHECK(loadOk2, qPrintable(QStringLiteral("excludeGroup round-trip: load() succeeded (%1)").arg(loadError2)));
        CHECK(topPtr->excludeGroup == 5,
              "excludeGroup round-trip: top-level MI_RT_Top's excludeGroup survived save+load");
        CHECK(nestedPtr->excludeGroup == 7,
              "excludeGroup round-trip: SubMenu-nested MI_RT_Nested's excludeGroup survived save+load "
              "(proves the recursive tree walk, not just a top-level scan)");
        CHECK(untouchedPtr->excludeGroup == 0,
              "excludeGroup round-trip: MI_RT_Untouched (never touched) stays at the default 0");

        // Reset both non-default items back to 0 too and save again - the
        // sidecar should be removed entirely, not left behind empty.
        topPtr->excludeGroup = 0;
        nestedPtr->excludeGroup = 0;
        MuibQtExtras::save(*proj, fakeMuibPath2, nullptr);
        CHECK(!QFile::exists(sidecarPath2),
              "excludeGroup round-trip: sidecar is removed once every MenuItem is back at the default "
              "(nothing left to persist)");
    }

    qInfo() << "----";
    if (g_failures == 0)
        qInfo() << "ALL CHECKS PASSED";
    else
        qCritical() << g_failures << "CHECK(S) FAILED";

    return g_failures == 0 ? 0 : 1;
}
