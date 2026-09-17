// MuiBuilderQt - C code generator.
//
// IMPORTANT, READ FIRST: this is NOT a byte-for-byte port of the original
// MUIBuilder's code generator. The original's real text-emission logic
// lives in a separate helper program (Modules/C/src/GenCodeC.c) that reads
// back a token stream produced by code.c's CodeCreate() and renders it via
// a lookup table `MUIStrings[]` mapping internal MB_* IDs to real MUI
// macro/attribute name strings (e.g. "WindowObject", "MUIA_Window_Title").
// That table's *definition* is not present anywhere in the provided
// MUIBuilder v2.3 source tree (only its usages are) - it's generated or
// shipped separately and wasn't included. Faithfully reproducing the
// original tool's exact generated-code text is therefore not achievable
// from the source we have, regardless of effort.
//
// So: this is a clean, from-scratch generator that walks the same
// MuibProject/MuibObject tree (already fully load/save validated against
// all 15 real example files) and emits structurally-correct, idiomatic MUI
// C using well-known real MUI macros from the public MUI SDK. See
// CODEGEN_NOTES.md (scratch, not shipped) for the full design rationale.
//
// Scope for this first pass:
//   - one .h + one .c for the WHOLE project (all windows), not per-object
//     like the original's interactive "Generate Code" command
//   - every object marked `generated` in the file (or a Window, or a
//     notification target) gets a persistent struct-field APTR; everything
//     else is a function-local APTR assigned inline during creation
//   - notifications are emitted as best-effort DoMethod(...) calls; the
//     raw MUIA_*-ish integer values stored in the file are inserted
//     directly with a comment flagging them for hand review, since their
//     symbolic names aren't recoverable without the same missing tables
//   - all 24 object types get a real, functional MUI mapping - nothing is
//     silently dropped, but fidelity for the less common ones (Gauge,
//     Prop, DirList, PopAsl, PopObject, ColorField, Scale) is best-effort
#pragma once

#include "muibobject.h"
#include "notifytables.h"
#include <QString>
#include <QSet>
#include <QHash>
#include <QTextStream>

class MuiCodeGen
{
public:
    // Generates <baseName>.h, <baseName>.c, <baseName>_main.c and
    // <baseName>_gadgets.h (all written next to each other, using `outDir`
    // as the directory) from `proj`. Returns false and sets *errorOut on
    // failure (e.g. cannot write files).
    static bool generate(const MuibProject &proj, const QString &outDir,
                          const QString &baseName, QString *errorOut = nullptr);

    // Lower-level entry points, exposed for headless testing: build the
    // header/source text without touching the filesystem. `baseName` is
    // needed by generateSource()/generateMain()/generateGadgetStubs() so
    // their `#include "<baseName>...\""` lines actually name the files
    // generate() below writes them next to - mirrors the original tool's
    // own GenCodeC.c, which writes `#include "%s"` using
    // FilePart(HeaderFile)/FilePart(MainFile), never a fixed name.
    static QString generateHeader(const MuibProject &proj);
    static QString generateSource(const MuibProject &proj, const QString &baseName);

    // Generates <baseName>_main.c: the whole-program entry point the
    // original tool's own GenCodeC.c also emits (WriteMainFile(), see its
    // MUIMasterBase/InitApp/event-loop shape) - opens muimaster.library,
    // calls the already-generated BuildApplication(), opens every window
    // with `initopen` set in the project (falling back to just the first
    // window if none are, so a fresh project someone hasn't touched that
    // flag on yet still shows something), then runs the standard MUI
    // DoMethod(..., MUIM_Application_NewInput, ...)/Wait() event loop -
    // the single loop that fans out ANY notification-driven return ID,
    // menu item or gadget alike, not just window close. Unlike the
    // original's per-object "ObjApp" framework (CreateApp()/DisposeApp(),
    // not reproducible here - see muicodegen.h's top-of-file caveat), this
    // is written against OUR OWN generateSource()/generateHeader() shape
    // (BuildApplication(), the flat `Gui` struct).
    static QString generateMain(const MuibProject &proj, const QString &baseName);

    // Generates <baseName>_gadgets.h: one near-empty `static inline` debug
    // stub per labelled non-container object in the project (every real
    // widget - Button, String, Cycle, ... - but not Window/Group/Space/
    // Rectangle, which aren't something a user "clicks", and not the
    // Menu/SubMenu/MenuItem family, out of scope for now same as
    // elsewhere in this generator). `static inline` so a stub nobody has
    // wired up yet never trips -Wunused-function. Each stub is a starting
    // point only - actually calling one from a real MUI notification
    // needs a Hook, which is a separate, bigger feature this does not
    // attempt; these just give every gadget a ready, uniquely-named place
    // to put that logic. generateSource() always #includes this header
    // (see generate()/its own text), so it's available wherever the
    // objects themselves are built without any extra step.
    static QString generateGadgetStubs(const MuibProject &proj, const QString &baseName);

    // Generates the plain-text AmigaGuide hypertext database
    // (<baseName>.guide, or FilePart(proj.helpfile) when that's set -
    // see generate()) that MUIA_Application_HelpFile/MUIA_HelpNode
    // actually need to show anything at runtime - a from-scratch port
    // of the real MUIBuilder v2.3 source's OWN, entirely separate
    // "guide.c" tool (GenerateGuide()/GuideReference()/WriteHelp()):
    // one "@NODE <label> \"<title>\"" / content / "@ENDNODE" block per
    // Window/Group/PopObject/other-help-bearing-type in the project
    // (mirroring code.c's own MUIA_HelpNode emission - see emitArea()),
    // plus a "@{ \" <title> \" link <label> }" cross-reference line for
    // every DIRECT child that itself has no help (so the AmigaGuide
    // reader/Help viewer can still navigate down into it) - exactly the
    // recursion shape GuideReference() uses in the real source. Returns
    // an empty string if the project has no help-bearing object at all
    // (nothing worth writing - see generate()'s own gating).
    static QString generateGuide(const MuibProject &proj);

private:
    struct GenCtx
    {
        QSet<QString> persistentLabels;   // labels needing a struct field
        QSet<QString> usedIdents;         // sanitized C identifiers already handed out (dedupe)
        QHash<const MuibObject *, QString> identFor; // object -> chosen C identifier
        QHash<QString, const MuibObject *> objectByLabel; // file label -> the object that owns it (first one wins)
        int anonCounter = 0;              // for objects with an empty label
    };

    static QString sanitizeIdent(const QString &label, const QString &fallbackPrefix, int &anonCounter);
    static void collectPersistentLabels(const MuibObject *obj, QSet<QString> &out);
    static void collectNotifyTargets(const MuibObject *obj, QSet<QString> &out);
    static void assignIdents(const MuibObject *obj, GenCtx &ctx);
    // Reference identifier for a LABELLED object only (persistent objects
    // get "Gui.<ident>", local ones the bare ident); returns "" for
    // unlabeled objects even if ctx has an internal ident for them (see
    // rawIdent()).
    static QString identOf(const MuibObject *obj, GenCtx &ctx);
    // The raw ident() text for any object ctx has assigned one to,
    // labeled or not - used to name Cycle/Radio/Register static entry
    // arrays, which need a unique file-scope name even for an otherwise
    // anonymous widget.
    static QString rawIdent(const MuibObject *obj, GenCtx &ctx);

    static QString cStringLiteral(const QString &s);

    // Emits the nested MUI creation expression for `obj` into `out`,
    // indented at `indent` tab stops. `localDecls` collects `APTR name;`
    // lines for non-persistent objects that need a local variable
    // (assigned inline via `(name = ...)`), to be declared at the top of
    // the enclosing function. Returns the C++ text fragment representing
    // "this object's creation expression" (caller wraps with Child/trailing
    // comma etc. as needed) - written directly to `out` for simplicity.
    //
    // `rootExtrasFrom`: MuiBuilderQt-only extension, see the Window fields'
    // own comment in muibobject.h (winMinWidth/winMinHeight/winMaxWidth/
    // winMaxHeight). Non-null ONLY at the one call site that emits a
    // Window's own `root` object (the window-building loop in
    // generateSource()) - passing that Window here (never any other
    // object) lets the Group case, if `obj` really is
    // `rootExtrasFrom->root.get()`, emit MUIA_MinWidth/MUIA_MinHeight/
    // MUIA_MaxWidth/MUIA_MaxHeight into THIS object's own tag list (the
    // real, generic Area-class attributes that constrain a window's
    // resize range - confirmed in the real MUI 5.0 SDK's own
    // include/libraries/mui.h, listed under the Area class's generic
    // attribute block, NOT under a Window-specific "MUIA_Window_" name -
    // so unlike MUIA_Window_Width/Height, which really do belong on the
    // WindowObject itself, these 4 belong on the window's CONTENT object
    // instead). Every recursive emitObject() call (children, popObj, ...)
    // leaves this at its default nullptr, so nested Groups deeper in the
    // tree never pick this up - correctly scoped to the window's own
    // immediate root only.
    static void emitObject(QTextStream &out, const MuibObject *obj, int indent,
                            GenCtx &ctx, QStringList &localDecls, QStringList &notifyLines,
                            const MuibObject *rootExtrasFrom = nullptr);

    static void emitNotifications(const MuibObject *obj, GenCtx &ctx, QStringList &notifyLines);
    static void emitArea(QTextStream &out, const MuibObject *obj, int indent);

    // Renders a TY_CONS_INT/TY_CONS_CHAR/TY_CONS_STRING/TY_CONS_BOOL
    // notify action's runtime value from the stored NotifyEvent::argString
    // (see notifytables.h) - a member only so it can reuse cStringLiteral().
    static QString renderNotifyConsValue(NotifyActionKind kind, const QString &argString);

    // MuiBuilderQt-only: emits the "Mutual Group" (mutual-exclude) wiring
    // for the Menu-Editor Dialog's excludeGroup field (see its comment in
    // muibobject.h). Deliberately NOT implemented via the real MUI
    // attribute MUIA_Menuitem_Exclude - that attribute exists in the real
    // MUI SDK (mui.h) but its bit-per-sibling semantics are not documented
    // anywhere in the material available to this project, and this
    // project's own rule is to never emit code whose behaviour can't be
    // verified against real documentation/source. Instead, for every
    // group of 2+ sibling MenuItems (direct children of `menuNode`, i.e.
    // same immediate parent) that share the same positive excludeGroup
    // value AND have check_enable set, this emits one
    // `DoMethod(<identA>, MUIM_Notify, MUIA_Menuitem_Checked, TRUE,
    // <identB>, 3, MUIM_Set, MUIA_Menuitem_Checked, FALSE);` line into
    // `notifyLines` for every ordered pair (A, B) with A != B in the
    // group - checking one item live-unchecks every other item in its
    // group, using only real, unambiguous MUI primitives
    // (MUIA_Menuitem_Checked/MUIM_Notify/MUIM_Set). A pair is skipped if
    // either side's identOf() comes back empty (unlabeled item - can't be
    // referenced from a DoMethod call). Recurses into every child
    // (labeled or not) so nested SubMenus' own exclude groups are handled
    // too. Caller is responsible for only invoking this where the
    // resulting notifyLines will actually be emitted into a function's
    // body - see its call site's own comment for why it is currently only
    // called for window-level menus (win->menu), not proj.appMenu.
    static void emitMenuExcludeGroupWiring(const MuibObject *menuNode, GenCtx &ctx, QStringList &notifyLines);

    // Appends 0+ extra `if (myDebug) { ... }`-body lines to `out`, printing
    // `obj`'s own live content (a String's current text, a Cycle/Radio's
    // active entry text, a Check's selected state, a Slider's numeric
    // value, or a ListView's active entry) via MUI's standard get()/
    // DoMethod() attribute-query idiom - real, useful debug information,
    // not just "this gadget was touched". Only meaningful for the types
    // hasDebugContent() (muicodegen.cpp) recognizes; a no-op otherwise.
    // Needs `ctx` (via identOf()/rawIdent()) exactly like emitObject().
    static void emitDebugContentLines(QTextStream &out, const MuibObject *obj, GenCtx &ctx);

    // Walks a subtree emitting "CONST_STRPTR ...[] = {...};" text for every
    // Cycle/Radio entries list and Register-mode group page titles list
    // (referenced by ident from emitObject()), appending each declaration's
    // text to `decls`. Cycle/Radio's own arrays are deliberately NOT
    // `static` (unlike the Register-titles ones) - see
    // collectEntriesArrayExterns() below for why.
    static void collectEntriesArrays(const MuibObject *obj, GenCtx &ctx, QStringList &decls);

    // Forward-declares ("extern CONST_STRPTR ...[];", no initializer)
    // every Cycle/Radio entries array collectEntriesArrays() will define
    // with real content elsewhere. Emitted into generateHeader()'s output
    // (<baseName>.h, #included first by both <baseName>.c and
    // <baseName>_main.c) so the array is a known, externally-linked symbol
    // from the very start of every generated translation unit - needed
    // because <baseName>_gadgets.h's debug stubs (emitDebugContentLines())
    // read a Cycle/Radio's active entry out of this same array, and
    // `static inline` means each TU that #includes the gadgets header (both
    // <baseName>.c AND <baseName>_main.c) compiles its own private copy of
    // that stub body - so the array must be a real, externally-linked
    // symbol resolved at link time, not a file-local `static` one (which
    // would silently give <baseName>_main.c's copy of the stub its own,
    // separate, always-empty array instead of the real data).
    static void collectEntriesArrayExterns(const MuibObject *obj, GenCtx &ctx, QStringList &decls);
};
