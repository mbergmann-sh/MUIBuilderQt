#include "muicodegen.h"
#include "notifytables.h"

#include <QFile>
#include <QFileInfo>
#include <QTextStream>
#include <QSet>
#include <QHash>
#include <QVector>
#include <QDir>

// ---------------------------------------------------------------------
// Prelude text from the original MUIBuilder v2.3's Modules/H-Header and
// Modules/C-Header (these two files ARE fully present in the provided
// source and are trivial - a handful of #include lines - so this part
// is close to a faithful reproduction, unlike the object-tree-to-MUI-code
// emission below). One deliberate addition to the original: <stdio.h>
// and <stdlib.h> in kCHeaderPrelude, so every generated .c/_main.c file
// declares puts()/printf() (used by the generated gadget debug stubs and
// by main()'s own error paths) itself, rather than relying on picking
// them up transitively through whichever generated header happens to
// get #included first.
// ---------------------------------------------------------------------
static const char *const kHHeaderPrelude =
    "#include <exec/types.h>\n";

static const char *const kCHeaderPrelude =
    "#define MUI_OBSOLETE\n"
    "\n"
    "#include <libraries/mui.h>\n"
    "\n"
    "#include <clib/alib_protos.h>\n"
    "#include <proto/muimaster.h>\n"
    "#include <proto/exec.h>\n"
    "#include <proto/intuition.h>\n"
    "\n"
    // Not part of the original tool's own C-Header module, but needed
    // unconditionally here: NM_BARLABEL (the classic GadTools NewMenu
    // separator-bar sentinel, reused by MUI_MakeObject(MUIO_Menuitem,
    // ...) below for a menu BarLabel) lives in <libraries/gadtools.h>,
    // and relying on some OTHER header pulling it in transitively proved
    // fragile - it failed to compile under a real m68k-amigaos-gcc NDK
    // with "'MN_BARLABEL' undeclared" until included explicitly (that
    // error also uncovered a letter-transposition typo in the emitted
    // identifier itself - see emitObject()'s MenuItem/BarLabel case).
    "#include <libraries/gadtools.h>\n"
    "\n"
    "#include <stdio.h>\n"
    "#include <stdlib.h>\n"
    "\n"
    "\n"
    "#ifndef MAKE_ID\n"
    "#define MAKE_ID(a,b,c,d) ((ULONG) (a)<<24 | (ULONG) (b)<<16 | (ULONG) (c)<<8 | (ULONG) (d))\n"
    "#endif\n";

QString MuiCodeGen::sanitizeIdent(const QString &label, const QString &fallbackPrefix, int &anonCounter)
{
    QString base = label;
    if (base.isEmpty())
        base = QStringLiteral("%1%2").arg(fallbackPrefix).arg(anonCounter++);

    QString out;
    out.reserve(base.size());
    for (QChar c : base)
    {
        if (c.isLetterOrNumber() || c == QLatin1Char('_'))
            out.append(c);
        else
            out.append(QLatin1Char('_'));
    }
    if (out.isEmpty() || out.at(0).isDigit())
        out.prepend(QLatin1Char('_'));
    return out;
}

void MuiCodeGen::collectPersistentLabels(const MuibObject *obj, QSet<QString> &out)
{
    if (!obj)
        return;
    if (!obj->label.isEmpty() && (obj->generated || obj->type == ObjType::Window))
        out.insert(obj->label);

    if (obj->menu)
        collectPersistentLabels(obj->menu.get(), out);
    if (obj->root)
        collectPersistentLabels(obj->root.get(), out);
    for (const auto &ch : obj->children)
        collectPersistentLabels(ch.get(), out);
    if (obj->popObj)
        collectPersistentLabels(obj->popObj.get(), out);
    for (const auto &ch : obj->childs)
        collectPersistentLabels(ch.get(), out);
}

void MuiCodeGen::collectNotifyTargets(const MuibObject *obj, QSet<QString> &out)
{
    if (!obj)
        return;
    for (const NotifyEvent &evt : obj->notify)
        if (!evt.targetLabel.isEmpty())
            out.insert(evt.targetLabel);

    if (obj->menu)
        collectNotifyTargets(obj->menu.get(), out);
    if (obj->root)
        collectNotifyTargets(obj->root.get(), out);
    for (const auto &ch : obj->children)
        collectNotifyTargets(ch.get(), out);
    if (obj->popObj)
        collectNotifyTargets(obj->popObj.get(), out);
    for (const auto &ch : obj->childs)
        collectNotifyTargets(ch.get(), out);
}

// Assigns a C identifier to every LABELLED object in the subtree (objects
// with no label in the file get no variable at all - they're emitted as
// plain inline expressions with nothing to reference them by later,
// exactly as the original treats truly anonymous objects).
void MuiCodeGen::assignIdents(const MuibObject *obj, GenCtx &ctx)
{
    if (!obj)
        return;
    // Cycle/Radio/Register-mode-Group objects always need a unique
    // file-scope name for their static entries/titles array (see
    // collectEntriesArrays()) even when the object itself has no label
    // and gets no persistent/local variable of its own; a Window always
    // needs a unique name for its Build*Window() function, even if (in
    // principle) it somehow has no label in the file - hence the extra
    // conditions here beyond "has a label".
    bool alwaysNeedsIdent = (obj->type == ObjType::Cycle || obj->type == ObjType::Radio ||
                             (obj->type == ObjType::Group && obj->registermode) ||
                             obj->type == ObjType::Window || obj->type == ObjType::AboutBox);
    if ((!obj->label.isEmpty() || alwaysNeedsIdent) && !ctx.identFor.contains(obj))
    {
        QString ident = sanitizeIdent(obj->label, QStringLiteral("obj"), ctx.anonCounter);
        // Disambiguate label collisions (the .MUIB format doesn't
        // guarantee globally-unique labels; different objects sharing a
        // display label do turn up in real projects).
        QString unique = ident;
        int suffix = 1;
        while (ctx.usedIdents.contains(unique))
            unique = QStringLiteral("%1_%2").arg(ident).arg(suffix++);
        ctx.usedIdents.insert(unique);
        ctx.identFor.insert(obj, unique);
    }
    if (!obj->label.isEmpty() && !ctx.objectByLabel.contains(obj->label))
        ctx.objectByLabel.insert(obj->label, obj);

    if (obj->menu)
        assignIdents(obj->menu.get(), ctx);
    if (obj->root)
        assignIdents(obj->root.get(), ctx);
    for (const auto &ch : obj->children)
        assignIdents(ch.get(), ctx);
    if (obj->popObj)
        assignIdents(obj->popObj.get(), ctx);
    for (const auto &ch : obj->childs)
        assignIdents(ch.get(), ctx);
}

QString MuiCodeGen::identOf(const MuibObject *obj, GenCtx &ctx)
{
    // Only labeled objects are ever wrapped in a capturing assignment or
    // referenced by name elsewhere - an object that only has an ident for
    // collectEntriesArrays()'s sake (see assignIdents()) is NOT one of
    // those, so it must not be treated as referenceable here.
    if (!obj || obj->label.isEmpty())
        return QString();
    auto it = ctx.identFor.find(obj);
    if (it == ctx.identFor.end())
        return QString();
    const QString &ident = it.value();
    if (ctx.persistentLabels.contains(obj->label))
        return QStringLiteral("Gui.%1").arg(ident);
    return ident;
}

QString MuiCodeGen::rawIdent(const MuibObject *obj, GenCtx &ctx)
{
    if (!obj)
        return QString();
    auto it = ctx.identFor.find(obj);
    return it == ctx.identFor.end() ? QString() : it.value();
}

QString MuiCodeGen::cStringLiteral(const QString &s)
{
    QString out;
    out.reserve(s.size() + 2);
    out.append(QLatin1Char('"'));
    for (QChar c : s)
    {
        if (c == QLatin1Char('"'))
        {
            out.append(QLatin1Char('\\'));
            out.append(c);
            continue;
        }
        if (c == QLatin1Char('\n'))
        {
            out.append(QStringLiteral("\\n"));
            continue;
        }
        // Deliberately NOT escaping a backslash already present in `s`:
        // the .MUIB format's own text fields store MUI PreParse codes
        // (bold/underline/alignment/...) as their literal C-escape text -
        // e.g. the real "Click" demo project's label field is the on-disk
        // bytes '\','0','3','3','8','\','0','3','3','c',... (confirmed via
        // a hex dump of click.MUIB) - meant to be reproduced verbatim into
        // the generated C string literal so the C COMPILER's own octal-
        // escape parsing turns "\033" into a real ESC byte at compile
        // time, the standard MUIBuilder convention for typing these codes
        // straight into a content field. Escaping that backslash here (as
        // this function used to do) doubles it to "\\033", which a real C
        // compiler faithfully compiles into the LITERAL runtime text
        // "\033" instead of ESC - a user-reported bug ("sonderbare
        // Steuerzeichen ... die im laufenden Programm ... als Text
        // erscheinen"), fixed by simply not touching backslashes at all
        // and passing every other byte through verbatim (the .MUIB
        // format's text fields can also carry Latin-1 bytes - see the
        // German-umlaut ASCII-digraph handling in the earlier AmigaED
        // C/Assembler help work - and the source AmigaOS toolchains are
        // all 8-bit-clean here anyway).
        out.append(c);
    }
    out.append(QLatin1Char('"'));
    return out;
}

static void indentTo(QTextStream &out, int n)
{
    for (int i = 0; i < n; ++i)
        out << '\t';
}

// A handful of well-known real MUI frame-style constants (public MUI SDK),
// used only as a human-readable comment alongside the raw stored integer -
// the .MUIB file's own Area.Frame numbering isn't guaranteed to line up
// with MUI's MUIV_Frame_* enum, so the raw value is what's actually
// emitted; this is just a hint for whoever reviews the generated code.
static QString frameHint(int frame)
{
    switch (frame)
    {
        case 0: return QStringLiteral("MUIV_Frame_None (hinted)");
        case 1: return QStringLiteral("MUIV_Frame_Button (hinted)");
        case 2: return QStringLiteral("MUIV_Frame_ImageButton (hinted)");
        case 3: return QStringLiteral("MUIV_Frame_String (hinted)");
        case 4: return QStringLiteral("MUIV_Frame_ReadList (hinted)");
        case 5: return QStringLiteral("MUIV_Frame_InputList (hinted)");
        case 6: return QStringLiteral("MUIV_Frame_Prop (hinted)");
        case 7: return QStringLiteral("MUIV_Frame_Text (hinted)");
        case 8: return QStringLiteral("MUIV_Frame_Group (hinted)");
        default: return QStringLiteral("raw value from file, not resolved");
    }
}

void MuiCodeGen::emitArea(QTextStream &out, const MuibObject *obj, int indent)
{
    const AreaAttrs &area = obj->area;
    if (area.Hide)
    {
        indentTo(out, indent);
        out << "MUIA_ShowMe, FALSE,\n";
    }
    if (area.Disable)
    {
        indentTo(out, indent);
        out << "MUIA_Disabled, TRUE,\n";
    }
    if (area.Weight != 0)
    {
        indentTo(out, indent);
        out << "MUIA_Weight, " << area.Weight << ",\n";
    }
    if (area.Frame != 0)
    {
        indentTo(out, indent);
        out << "MUIA_Frame, " << area.Frame << ", /* " << frameHint(area.Frame) << " */\n";
    }
    if (!area.TitleFrame.isEmpty())
    {
        // Real original identifier confirmed as "MUIA_FrameTitle" (STRPTR
        // attribute, three independent sources agree: MUIBuilder's own
        // compiled-in symbol table in libraries/muibuilder.h, the
        // MB_MUIA_FrameTitle token id in muib_file.h that CodeArea()
        // writes for this exact field, and the real MUI SDK header
        // itself, include/libraries/mui.h - not "MUIA_Frame_Title"). The
        // original's CodeArea() (code.c) emits this attribute whenever
        // the field is non-empty, same condition used here. This was
        // previously missing entirely from emitArea() - a pre-existing
        // gap, not specific to Group: even for object types whose
        // Property-Inspector already showed the "Rahmentitel" row (e.g.
        // String, Button), the value was silently dropped from the
        // generated C code.
        indentTo(out, indent);
        out << "MUIA_FrameTitle, " << cStringLiteral(area.TitleFrame) << ",\n";
    }
    if (area.InputMode)
    {
        /* Area.InputMode is how MUIBuilder lets an otherwise-plain,
           Area-based object (most commonly Image, e.g. a "browse" icon
           next to a path field) act as a clickable icon button, distinct
           from the dedicated Button/Check object types (which hardcode
           their own MUIA_InputMode elsewhere and never set this generic
           flag). Confirmed against the original tool's own code.c
           (CodeArea()): `if ((InputMode) && (Area->InputMode)) {
           WriteAttribut(MB_MUIA_InputMode); WriteMUIArgAttribute(
           MB_MUIV_InputMode_RelVerify); }` - the exact attribute/value
           pair emitted here. This generic path was missing entirely
           before (an Image configured this way silently lost its
           clickability), confirmed via a real project (DVIPrint.MUIB's
           "IM_label_0" - a drawer-browse icon next to a path field, whose
           debug stub never fired because nothing made it clickable). */
        indentTo(out, indent);
        out << "MUIA_InputMode, MUIV_InputMode_RelVerify,\n";
    }
    // Real original attribute "MUIA_HelpNode" (context help, AmigaGuide-
    // style - confirmed against the real MUI SDK header,
    // include/libraries/mui.h: MUIA_HelpNode, 0x80420b85, "isg STRPTR"),
    // emitted with the object's OWN label as the node identifier -
    // verified line-by-line against code.c's CodeCreate(): every single
    // TY_* case that calls CodeArea() ALSO independently checks
    // `if (obj->Help.generated) { WriteAttribut(MB_MUIA_HelpNode);
    // WriteString(fichier, obj->label); }`, at 17 real call sites, with
    // exactly two documented exceptions among CodeArea()-using types:
    // TY_CHECK and TY_SCALE never get it (checked directly - genuinely
    // absent from the original, not an oversight in this port), so both
    // are excluded here too, faithfully matching that asymmetry rather
    // than guessing a "consistent" rule the original itself doesn't
    // follow. TY_WINDOW also gets MUIA_HelpNode, but on a completely
    // separate code path (its own WindowObject creation in
    // generateSource(), not through emitArea()/emitObject() at all -
    // handled there instead, see the "BuildXxxWindow()" helper).
    // core/muibloader.cpp's readHelp() already faithfully loads
    // obj->help.title/content/generated for essentially every object
    // type in the tree, so this data has been sitting unused since the
    // port began - a user-reported bug ("es werden keine Hilfetexte
    // generiert und angezeigt, obwohl ich welche angegeben habe.") this
    // fixes, together with the new .guide-file generator (see
    // generateGuide() below) that actually supplies the TEXT this node
    // name points to - MUIA_HelpNode alone is just a lookup key, the
    // help viewer needs a database (the .guide file) to resolve it
    // against, wired onto the Application via MUIA_Application_HelpFile
    // (see generateSource()).
    if (obj->help.generated && !obj->label.isEmpty() &&
        obj->type != ObjType::Check && obj->type != ObjType::Scale)
    {
        indentTo(out, indent);
        out << "MUIA_HelpNode, " << cStringLiteral(obj->label) << ",\n";
    }
}

void MuiCodeGen::emitObject(QTextStream &out, const MuibObject *obj, int indent,
                            GenCtx &ctx, QStringList &localDecls, QStringList &notifyLines,
                            const MuibObject *rootExtrasFrom)
{
    if (!obj)
    {
        out << "NULL";
        return;
    }

    // Real original semantics (code.c's TY_CHECK case, title_exist branch,
    // verified line-by-line): when a Check has a non-empty title, the
    // checkbox itself is NOT placed directly into the parent - instead an
    // ANONYMOUS 2-column GroupObject is placed there, with the label text
    // (via the real MUI_MakeObject(MUIO_Label, ..., MUIO_Label_DoubleFrame)
    // convenience macro "Label2" - or "KeyLabel2" if a shortcut key is set
    // - confirmed against the real MUI SDK header's own Label2()/
    // KeyLabel2() macro expansions) as the FIRST child and the checkbox as
    // the SECOND child. This wrapping group itself is anonymous in the
    // original too (its own captured variable, NumLabel()+1, is never
    // referenced anywhere else in the generated code - only the checkbox's
    // own variable is used for notifications), so it needs no ident here.
    // Previously this port emitted only the bare checkbox, silently
    // dropping obj->title/obj->title_exist entirely (a user-reported bug:
    // "die generierte Checkbox hat keine Beschriftung, obwohl eine
    // angegeben ist") even though the .MUIB loader already read both
    // fields correctly (loadCheck(), core/muibloader.cpp) and the
    // Property-Inspector already let the user set obj->title - the gap
    // was purely in codegen. This special case has to run BEFORE the
    // generic ident-wrapping block just below, because the persistent
    // "Gui.<ident> = " assignment must stay attached to the INNER checkbox
    // (the object real notifications/debug-content queries target, e.g.
    // MUIA_Selected), not to this outer wrapping group - the generic path
    // below would otherwise wrap the ident around the wrong (outer)
    // expression.
    if (obj->type == ObjType::Check && obj->title_exist && !obj->title.isEmpty())
    {
        const bool hasKey = obj->area.key != '\0';
        out << "GroupObject,\n";
        indentTo(out, indent + 1);
        out << "MUIA_Group_Columns, 2,\n";
        indentTo(out, indent + 1);
        out << "Child, " << (hasKey ? "KeyLabel2(" : "Label2(") << cStringLiteral(obj->title);
        if (hasKey)
            out << ", '" << QChar(QLatin1Char(obj->area.key)) << "'";
        out << "),\n";
        indentTo(out, indent + 1);
        out << "Child, ";
        // The checkbox itself, with its own normal ident-wrap (duplicated
        // from the generic path below, since that path isn't reentrant
        // for a single nested call) - the actual checkbox creation
        // expression is unchanged from the plain (no-title) case.
        const QString innerIdent = identOf(obj, ctx);
        const bool innerPersistent = !obj->label.isEmpty() && ctx.persistentLabels.contains(obj->label);
        if (!innerIdent.isEmpty())
        {
            if (!innerPersistent)
            {
                QString declLine = QStringLiteral("APTR %1;").arg(innerIdent);
                if (!localDecls.contains(declLine))
                    localDecls << declLine;
            }
            out << "(" << innerIdent << " = ";
        }
        out << "ImageObject,\n";
        indentTo(out, indent + 2);
        out << "MUIA_Frame, MUIV_Frame_ImageButton,\n";
        indentTo(out, indent + 2);
        out << "MUIA_InputMode, MUIV_InputMode_Toggle,\n";
        indentTo(out, indent + 2);
        out << "MUIA_Image_Spec, MUII_CheckMark,\n";
        indentTo(out, indent + 2);
        out << "MUIA_Image_FreeVert, TRUE,\n";
        indentTo(out, indent + 2);
        out << "MUIA_Background, MUII_ButtonBack,\n";
        indentTo(out, indent + 2);
        out << "MUIA_Selected, " << (obj->init_state ? "TRUE" : "FALSE") << ",\n";
        emitArea(out, obj, indent + 2);
        indentTo(out, indent + 1);
        out << "End";
        if (!innerIdent.isEmpty())
            out << ")";
        out << ",\n";
        indentTo(out, indent);
        out << "End";
        return;
    }

    const QString ident = identOf(obj, ctx);
    const bool persistent = !obj->label.isEmpty() && ctx.persistentLabels.contains(obj->label);
    if (!ident.isEmpty())
    {
        // Capture this object's creation expression into a variable so
        // later same-function (or, for persistent ones, same-project)
        // references (e.g. a notification, or BuildApplication()) can use
        // it. A non-persistent object gets a function-local APTR declared
        // once at the top of the enclosing function (caller collects
        // localDecls); a persistent one already has its field declared in
        // the generated header's GUIObjects struct (see generateHeader()),
        // so here it only needs the "Gui.<ident> = " capture itself - NOT
        // another local decl, and identOf() already returned the
        // "Gui.<ident>" form for it above.
        if (!persistent)
        {
            QString declLine = QStringLiteral("APTR %1;").arg(ident);
            if (!localDecls.contains(declLine))
                localDecls << declLine;
        }
        out << "(" << ident << " = ";
    }

    switch (obj->type)
    {
        case ObjType::Window:
        {
            // Windows are only ever emitted via their own Build*Window()
            // function (see generateSource()) - emitObject() is never
            // called directly on one, but handle it defensively.
            out << "/* nested Window objects are not supported by MUI */ NULL";
            break;
        }
        case ObjType::Group:
        {
            // A registermode Group becomes a RegisterObject (MUIC_Register),
            // NOT a plain GroupObject with MUIA_Register_Titles bolted on:
            // MUIA_Register_Titles/MUIA_Register_Frame belong to the real,
            // separate Register.mui class, confirmed via MUIBuilder's own
            // compiled-in symbol table (include/libraries/muibuilder.h),
            // which lists them under "MUIC_Register" - a distinct entry
            // from "MUIC_Group"'s own MUIA_Group_* attributes just above it
            // in that same table. A base Group silently ignores an
            // attribute it doesn't recognize (BOOPSI just drops unknown
            // tags - no compile or runtime error), so this bug never
            // surfaced as a build failure - only as a missing tab strip,
            // with just the first page's widgets ever visible/reachable
            // (a user-reported symptom: a Register-mode window with no
            // way to switch pages, always stuck on whichever child was
            // added first, since MUIA_Group_ActivePage was never wired to
            // anything real to move it off page 0).
            const bool isRegister = obj->registermode;
            out << (isRegister ? "RegisterObject,\n" : "GroupObject,\n");
            indentTo(out, indent + 1);
            out << "MUIA_Group_Horiz, " << (obj->horizontal ? "TRUE" : "FALSE") << ",\n";
            if (obj->rows)
            {
                indentTo(out, indent + 1);
                out << "MUIA_Group_Rows, " << obj->number << ",\n";
            }
            if (obj->columns)
            {
                indentTo(out, indent + 1);
                out << "MUIA_Group_Columns, " << obj->number << ",\n";
            }
            if (obj->sameheight)
            {
                indentTo(out, indent + 1);
                out << "MUIA_Group_SameHeight, TRUE,\n";
            }
            if (obj->samewidth)
            {
                indentTo(out, indent + 1);
                out << "MUIA_Group_SameWidth, TRUE,\n";
            }
            // MuiBuilderQt-only: this window's own Min/Max resize
            // constraints, if any - see this function's own doc comment
            // in muicodegen.h for why they land here (on the root
            // CONTENT object) rather than on the WindowObject itself.
            // Guarded so an object that merely LOOKS like some window's
            // root by coincidence (impossible today - every window's
            // root is exclusive to it - but defensive regardless) never
            // picks this up; only the one real call site in the
            // window-building loop ever passes `rootExtrasFrom` at all.
            if (rootExtrasFrom && obj == rootExtrasFrom->root.get())
            {
                if (rootExtrasFrom->winMinWidth > 0)
                {
                    indentTo(out, indent + 1);
                    out << "MUIA_MinWidth, " << rootExtrasFrom->winMinWidth << ",\n";
                }
                if (rootExtrasFrom->winMinHeight > 0)
                {
                    indentTo(out, indent + 1);
                    out << "MUIA_MinHeight, " << rootExtrasFrom->winMinHeight << ",\n";
                }
                if (rootExtrasFrom->winMaxWidth > 0)
                {
                    indentTo(out, indent + 1);
                    out << "MUIA_MaxWidth, " << rootExtrasFrom->winMaxWidth << ",\n";
                }
                if (rootExtrasFrom->winMaxHeight > 0)
                {
                    indentTo(out, indent + 1);
                    out << "MUIA_MaxHeight, " << rootExtrasFrom->winMaxHeight << ",\n";
                }
            }
            if (isRegister)
            {
                indentTo(out, indent + 1);
                out << "MUIA_Group_PageMode, TRUE,\n";
                if (!obj->entries.isEmpty())
                {
                    QString titlesIdent = QStringLiteral("%1Titles").arg(rawIdent(obj, ctx));
                    indentTo(out, indent + 1);
                    out << "MUIA_Register_Titles, (IPTR)" << titlesIdent << ",\n";
                    // The static array itself is emitted just above the
                    // enclosing function by collectEntriesArrays(), called
                    // from generateSource() - see there.
                }
            }
            emitArea(out, obj, indent + 1);
            for (const auto &ch : obj->children)
            {
                indentTo(out, indent + 1);
                out << "Child, ";
                emitObject(out, ch.get(), indent + 1, ctx, localDecls, notifyLines);
                out << ",\n";
            }
            indentTo(out, indent);
            out << "End";
            break;
        }
        case ObjType::Button:
        {
            // Deliberately TextObject, NOT the newer "ButtonObject"
            // convenience macro: at least one real amiga-gcc NDK's mui.h
            // (bebbo's cross-compiler bundle - confirmed via a user build,
            // "'ButtonObject' undeclared") simply doesn't define it, which
            // is a hard compile error, not a warning - and because
            // ButtonObject's missing expansion also swallows the opening
            // "MUI_NewObject(" its macro is meant to contribute, the
            // matching closing paren from `End` further down ends up
            // unbalanced too, cascading into a run of unrelated-looking
            // syntax errors in the rest of the function. TextObject +
            // ButtonFrame + MUIA_InputMode/MUIA_Background below is the
            // classic, pre-convenience-macro way to build a MUI button
            // and is functionally identical to what ButtonObject itself
            // expands to - TextObject is already relied on for the Text
            // object type above and confirmed to compile fine.
            out << "TextObject,\n";
            indentTo(out, indent + 1);
            out << "ButtonFrame,\n";
            indentTo(out, indent + 1);
            out << "MUIA_Background, MUII_ButtonBack,\n";
            indentTo(out, indent + 1);
            out << "MUIA_InputMode, MUIV_InputMode_RelVerify,\n";
            indentTo(out, indent + 1);
            out << "MUIA_Text_Contents, " << cStringLiteral(obj->title) << ",\n";
            indentTo(out, indent + 1);
            out << "MUIA_Text_PreParse, \"\\33c\",\n";
            emitArea(out, obj, indent + 1);
            indentTo(out, indent);
            out << "End";
            break;
        }
        case ObjType::String:
        {
            out << "StringObject,\n";
            indentTo(out, indent + 1);
            out << "StringFrame,\n";
            indentTo(out, indent + 1);
            out << "MUIA_String_Contents, " << cStringLiteral(obj->content) << ",\n";
            if (obj->maxlen > 0)
            {
                indentTo(out, indent + 1);
                out << "MUIA_String_MaxLen, " << obj->maxlen << ",\n";
            }
            if (obj->secret)
            {
                indentTo(out, indent + 1);
                out << "MUIA_String_Secret, TRUE,\n";
            }
            emitArea(out, obj, indent + 1);
            indentTo(out, indent);
            out << "End";
            break;
        }
        case ObjType::Text:
        {
            out << "TextObject,\n";
            indentTo(out, indent + 1);
            out << "MUIA_Text_Contents, " << cStringLiteral(obj->content) << ",\n";
            if (!obj->preparse.isEmpty())
            {
                indentTo(out, indent + 1);
                out << "MUIA_Text_PreParse, " << cStringLiteral(obj->preparse) << ",\n";
            }
            emitArea(out, obj, indent + 1);
            indentTo(out, indent);
            out << "End";
            break;
        }
        case ObjType::Label:
        {
            out << "Label2(" << cStringLiteral(obj->title) << ")";
            break;
        }
        case ObjType::Space:
        {
            out << "HVSpace /* Space object - a zero-weight Rectangle in real MUI */";
            break;
        }
        case ObjType::Rectangle:
        {
            out << "RectangleObject,\n";
            if (obj->fixheight)
            {
                indentTo(out, indent + 1);
                out << "MUIA_FixHeight, " << obj->height << ",\n";
            }
            if (obj->fixwidth)
            {
                indentTo(out, indent + 1);
                out << "MUIA_FixWidth, " << obj->width << ",\n";
            }
            emitArea(out, obj, indent + 1);
            indentTo(out, indent);
            out << "End";
            break;
        }
        case ObjType::Check:
        {
            // Deliberately ImageObject with the checkmark's real attributes,
            // NOT a "CheckMarkObject" convenience macro: unlike CycleObject/
            // RadioObject/etc. just below (which really are standard,
            // long-established MUI class-wrapper macros), MUI has no
            // dedicated "Check" widget class at all - a checkbox is just a
            // specially-configured Image object. "CheckMarkObject" was
            // never a real macro in any MUI SDK (confirmed via a user
            // build, "'CheckMarkObject' undeclared" - the same kind of
            // bracket-swallowing cascade the earlier ButtonObject bug
            // caused, since the matching `End` below has no opening paren
            // left to close once the unknown macro's expansion is empty).
            // The attributes below are exactly what the real, well-known
            // CheckMark() macro (see mui.h, or MUIBuilder's own check.c
            // dialog code, which calls it directly) expands to - functionally
            // identical, but spelled out with ImageObject so it doesn't
            // depend on CheckMark() itself existing in an older/alternate
            // NDK's mui.h either.
            out << "ImageObject,\n";
            indentTo(out, indent + 1);
            out << "MUIA_Frame, MUIV_Frame_ImageButton,\n";
            indentTo(out, indent + 1);
            out << "MUIA_InputMode, MUIV_InputMode_Toggle,\n";
            indentTo(out, indent + 1);
            out << "MUIA_Image_Spec, MUII_CheckMark,\n";
            indentTo(out, indent + 1);
            out << "MUIA_Image_FreeVert, TRUE,\n";
            indentTo(out, indent + 1);
            out << "MUIA_Background, MUII_ButtonBack,\n";
            indentTo(out, indent + 1);
            out << "MUIA_Selected, " << (obj->init_state ? "TRUE" : "FALSE") << ",\n";
            emitArea(out, obj, indent + 1);
            indentTo(out, indent);
            out << "End";
            break;
        }
        case ObjType::Cycle:
        {
            QString entriesIdent = QStringLiteral("%1Entries").arg(rawIdent(obj, ctx));
            out << "CycleObject,\n";
            indentTo(out, indent + 1);
            out << "MUIA_Cycle_Entries, (IPTR)" << entriesIdent << ",\n";
            emitArea(out, obj, indent + 1);
            indentTo(out, indent);
            out << "End";
            break;
        }
        case ObjType::Radio:
        {
            QString entriesIdent = QStringLiteral("%1Entries").arg(rawIdent(obj, ctx));
            out << "RadioObject,\n";
            indentTo(out, indent + 1);
            out << "MUIA_Radio_Entries, (IPTR)" << entriesIdent << ",\n";
            emitArea(out, obj, indent + 1);
            indentTo(out, indent);
            out << "End";
            break;
        }
        case ObjType::Slider:
        {
            out << "SliderObject,\n";
            indentTo(out, indent + 1);
            out << "MUIA_Numeric_Min, " << obj->sliderMin << ",\n";
            indentTo(out, indent + 1);
            out << "MUIA_Numeric_Max, " << obj->sliderMax << ",\n";
            indentTo(out, indent + 1);
            out << "MUIA_Numeric_Value, " << obj->init << ",\n";
            if (!obj->title.isEmpty())
            {
                indentTo(out, indent + 1);
                out << "MUIA_String_Contents /* slider title, best-effort */, " << cStringLiteral(obj->title) << ",\n";
            }
            emitArea(out, obj, indent + 1);
            indentTo(out, indent);
            out << "End";
            break;
        }
        case ObjType::Gauge:
        {
            out << "GaugeObject,\n";
            indentTo(out, indent + 1);
            out << "MUIA_Gauge_Horiz, " << (obj->horizontal ? "TRUE" : "FALSE") << ",\n";
            indentTo(out, indent + 1);
            out << "MUIA_Gauge_Divide, " << obj->divide << ",\n";
            indentTo(out, indent + 1);
            out << "MUIA_Gauge_Max, " << obj->gaugeMax << ",\n";
            if (!obj->infotext.isEmpty())
            {
                indentTo(out, indent + 1);
                out << "MUIA_Gauge_InfoText, " << cStringLiteral(obj->infotext) << ",\n";
            }
            emitArea(out, obj, indent + 1);
            indentTo(out, indent);
            out << "End";
            break;
        }
        case ObjType::Prop:
        {
            out << "ScrollbarObject,\n";
            indentTo(out, indent + 1);
            out << "MUIA_Prop_Entries, " << obj->propEntries << ",\n";
            indentTo(out, indent + 1);
            out << "MUIA_Prop_Visible, " << obj->visible << ",\n";
            indentTo(out, indent + 1);
            out << "MUIA_Prop_First, " << obj->first << ",\n";
            emitArea(out, obj, indent + 1);
            indentTo(out, indent);
            out << "End";
            break;
        }
        case ObjType::ListView:
        {
            out << "ListviewObject,\n";
            indentTo(out, indent + 1);
            out << "MUIA_Listview_List, ListObject,\n";
            indentTo(out, indent + 2);
            out << "InputListFrame,\n";
            if (!obj->comparehook.isEmpty())
            {
                indentTo(out, indent + 2);
                out << "/* TODO: MUIA_List_CompareHook -> &" << obj->comparehook << "Hook (hook fn body not generated) */\n";
            }
            if (!obj->displayhook.isEmpty())
            {
                indentTo(out, indent + 2);
                out << "/* TODO: MUIA_List_DisplayHook -> &" << obj->displayhook << "Hook (hook fn body not generated) */\n";
            }
            if (!obj->constructhook.isEmpty())
            {
                indentTo(out, indent + 2);
                out << "/* TODO: MUIA_List_ConstructHook -> &" << obj->constructhook << "Hook (hook fn body not generated) */\n";
            }
            indentTo(out, indent + 1);
            out << "End,\n";
            // Real, original-MUIBuilder-tool attribute (code.c's own
            // TY_LISTVIEW case conditionally emits exactly this,
            // "MUIA_Listview_DoubleClick, TRUE", guarded on the same
            // per-object doubleclick checkbox its own ListView editor
            // dialog exposes - see PropertyInspector's "Doppelklick" row
            // for our own equivalent) - lets DoMethod(..., MUIM_Notify,
            // MUIA_Listview_DoubleClick, TRUE, ...) fire (see
            // isNotifiableGadgetType()/notifyAttrFor() above) whenever
            // the user double-clicks an entry, exactly the real MUI
            // idiom for "the user picked this one" on a Listview.
            if (obj->doubleclick)
            {
                indentTo(out, indent + 1);
                out << "MUIA_Listview_DoubleClick, TRUE,\n";
            }
            emitArea(out, obj, indent + 1);
            indentTo(out, indent);
            out << "End";
            break;
        }
        case ObjType::DirList:
        {
            /* Earlier revisions emitted a "best-effort" raw MUI_NewObject("Filelist.mcc", ...)
               instantiation here, under the assumption that directory listing is a third-party
               MCC not part of base MUI (matching this generator's own DirList/Filelist naming,
               and echoing the original MUIBuilder source tree's own "dirlist.c" naming). A real
               vbcc/NDK3.2 build proved that assumption wrong: "MUIA_Filelist_Drawer" is not a
               real MUI identifier at all, and there IS no "Filelist.mcc" - the reported error
               ("unknown identifier <MUIA_Filelist_Drawer>") was cross-checked against MUI's own
               official autodocs (Dirlist.mui, mirrored at
               github.com/amiga-mui/muidev/wiki/MUI_Dirlist), which confirm a genuine, DIFFERENT
               base-MUI class: "Dirlist" (a subclass of List.mui, present since muimaster.library
               V4 - i.e. already in AmigaOS 3.x-era MUI, no extra class/header needed beyond the
               usual <libraries/mui.h>), with the convenience macro DirlistObject and the real
               attribute MUIA_Dirlist_Directory (V4, ISG, STRPTR) for the directory path - which
               is exactly what this generator's `directory` field is meant to drive. Only the
               directory attribute is emitted here; the richer Dirlist_* attributes (patterns,
               DrawersOnly/FilesOnly, sort options, filter hook - all present in the real class,
               and already tracked in the original MUIBuilder's own dirlist.c, e.g. `accept`/
               `reject`/`sorttype`) aren't yet captured by this tool's load/save model, same as
               documented in README.md/CODEGEN_NOTES.md. */
            out << "DirlistObject,\n";
            indentTo(out, indent + 1);
            out << "MUIA_Dirlist_Directory, " << cStringLiteral(obj->directory) << ",\n";
            emitArea(out, obj, indent + 1);
            indentTo(out, indent);
            out << "End";
            break;
        }
        case ObjType::Image:
        {
            out << "ImageObject,\n";
            if (!obj->spec.isEmpty())
            {
                /* The original MUIBuilder's own code generator (code.c, TY_IMAGE
                   case) never writes a custom image spec string as-is - it always
                   prepends "5:" first (`sprintf(buffer, "5:%s", image_aux->spec)`),
                   MUI's own MUIA_Image_Spec string-prefix convention for "load this
                   as a real image/brush" rather than treating the text as one of
                   the other special forms that syntax supports. `obj->spec` is
                   loaded straight from the .MUIB file without that prefix (it's
                   added at generation time by the original tool, not stored), so
                   this generator adds it here too, unconditionally, to match. */
                indentTo(out, indent + 1);
                out << "MUIA_Image_Spec, " << cStringLiteral(QStringLiteral("5:") + obj->spec) << ",\n";
            }
            else
            {
                /* No custom spec string was stored, meaning this Image uses one of
                   MUI's built-in picker images instead (arrow/checkmark/drawer/
                   harddisk/...) - the original tool's own image.c literally assigns
                   `image_aux->type = MUII_HardDisk;` etc. (the REAL, compiled-in
                   numeric value of that constant, from whichever real mui.h the
                   original MUIBuilder binary itself was built against) whenever a
                   user picks a built-in image, and its code.c (same TY_IMAGE case)
                   writes that raw integer straight through with no symbolic-name
                   translation at all (`WriteInteger(fichier, image_aux->type)`) -
                   it does NOT go through the MB_MUII_* symbol-table indices used
                   elsewhere for pretty-printing (those are array indices into
                   muibuilder.h's own MUIStrings[] table, a different, unrelated
                   numbering). An earlier revision of this generator guessed a
                   symbolic macro name here instead ("MUII_WarpNTBadge") that a real
                   vbcc/NDK3.2 build proved isn't a real MUI identifier at all
                   ("unknown identifier <MUII_WarpNTBadge>"). Rather than guess
                   another symbolic name - risking a real-but-WRONG constant that
                   compiles fine yet shows the wrong icon, with nothing to catch it -
                   this emits `obj->imgType` (loaded verbatim from the same file,
                   i.e. already the correct real numeric value) directly, exactly as
                   the original tool's own generator does for this same case. */
                indentTo(out, indent + 1);
                out << "MUIA_Image_Spec, " << obj->imgType << ",\n";
            }
            if (obj->freehoriz)
            {
                indentTo(out, indent + 1);
                out << "MUIA_Image_FreeHoriz, TRUE,\n";
            }
            if (obj->freevert)
            {
                indentTo(out, indent + 1);
                out << "MUIA_Image_FreeVert, TRUE,\n";
            }
            emitArea(out, obj, indent + 1);
            indentTo(out, indent);
            out << "End";
            break;
        }
        case ObjType::ColorField:
        {
            out << "ColorfieldObject,\n";
            indentTo(out, indent + 1);
            out << "MUIA_Colorfield_Red, " << obj->red << ",\n";
            indentTo(out, indent + 1);
            out << "MUIA_Colorfield_Green, " << obj->green << ",\n";
            indentTo(out, indent + 1);
            out << "MUIA_Colorfield_Blue, " << obj->blue << ",\n";
            emitArea(out, obj, indent + 1);
            indentTo(out, indent);
            out << "End";
            break;
        }
        case ObjType::PopAsl:
        {
            out << "PopaslObject,\n";
            indentTo(out, indent + 1);
            out << "MUIA_Popstring_Button, PopButton(MUII_PopFile),\n";
            indentTo(out, indent + 1);
            out << "MUIA_Popasl_Type, ASL_FileRequest,\n";
            emitArea(out, obj, indent + 1);
            indentTo(out, indent);
            out << "End";
            break;
        }
        case ObjType::PopObject:
        {
            out << "PopobjectObject,\n";
            indentTo(out, indent + 1);
            out << "MUIA_Popstring_Button, PopButton(MUII_PopUp),\n";
            indentTo(out, indent + 1);
            out << "MUIA_Popobject_Object, ";
            emitObject(out, obj->popObj.get(), indent + 1, ctx, localDecls, notifyLines);
            out << ",\n";
            emitArea(out, obj, indent + 1);
            indentTo(out, indent);
            out << "End";
            break;
        }
        case ObjType::Menu:
        {
            // Real original mapping (code.c's TY_MENU case, verified against
            // v2.3 source) - the ROOT menu object (window.menu/proj.appMenu)
            // is a MenustripObject, NOT a MenuObject+MUIA_Menu_Title as this
            // port previously (incorrectly) emitted. It carries no title of
            // its own - only its MUIA_Family_Child-attached children (each a
            // SubMenu = one top-level pulldown, e.g. "Project"). MUI's own
            // Menu.mui/MenuObject/MUIA_Menu_Title is never actually used by
            // the original tool at all.
            out << "MenustripObject,\n";
            for (const auto &ch : obj->childs)
            {
                indentTo(out, indent + 1);
                out << "MUIA_Family_Child, ";
                emitObject(out, ch.get(), indent + 1, ctx, localDecls, notifyLines);
                out << ",\n";
            }
            indentTo(out, indent);
            out << "End";
            break;
        }
        case ObjType::SubMenu:
        {
            // Real original mapping (code.c's TY_SUBMENU case) - used for
            // BOTH a top-level pulldown title (directly under the root
            // Menu) and a nested cascading/flyout submenu (under another
            // SubMenu): always a MenuitemObject+MUIA_Menuitem_Title (never
            // MUI's own MenuObject), with children attached via
            // MUIA_Family_Child (not the generic Child tag). The title is
            // only emitted when non-empty, matching the original's
            // strlen(name) > 0 check. MUIA_Menu_Enabled (a distinct real
            // attribute from MUIA_Menuitem_Enabled used by leaf MenuItems -
            // both confirmed in muibuilder.h's MUIStrings[] table) is
            // emitted, FALSE, only when the entry is disabled.
            out << "MenuitemObject,\n";
            if (!obj->name.isEmpty())
            {
                indentTo(out, indent + 1);
                out << "MUIA_Menuitem_Title, " << cStringLiteral(obj->name) << ",\n";
            }
            if (!obj->menu_enable)
            {
                indentTo(out, indent + 1);
                out << "MUIA_Menu_Enabled, FALSE,\n";
            }
            for (const auto &ch : obj->childs)
            {
                indentTo(out, indent + 1);
                out << "MUIA_Family_Child, ";
                emitObject(out, ch.get(), indent + 1, ctx, localDecls, notifyLines);
                out << ",\n";
            }
            indentTo(out, indent);
            out << "End";
            break;
        }
        case ObjType::MenuItem:
        {
            // Real original mapping (code.c's TY_MENUITEM case). Special
            // case first: a name starting with the literal "BarLabel" (the
            // original's own convention, strncmp(name, "BarLabel", 8)==0)
            // is a plain separator bar, emitted as a MUI_MakeObject(...)
            // function call rather than an object-attribute-list - it has
            // no End and none of the attributes below apply to it.
            // Otherwise: leaf MenuitemObject with MUIA_Menuitem_Title (only
            // when non-empty), MUIA_Menuitem_Enabled FALSE (only when
            // disabled - the leaf-specific attribute, distinct from
            // MUIA_Menu_Enabled used by SubMenu above), MUIA_Menuitem_
            // Shortcut (only when a key is set), and Checkit/Checked/Toggle
            // - each following the original's exact nesting: Checked is
            // only emitted (TRUE) when both check_enable AND check_state
            // are set, and Toggle only emitted (TRUE) when set - the
            // original never writes an explicit FALSE for either.
            if (obj->name.startsWith(QStringLiteral("BarLabel")))
            {
                // Real original identifier is "NM_BARLABEL" (verified
                // against the original tool's own MUIStrings-style token
                // table in libraries/muibuilder.h, which literally
                // contains the string "NM_BARLABEL" at the slot code.c's
                // TY_MENUITEM case writes for this exact case) - a
                // previous revision of this port had the letters
                // transposed ("MN_BARLABEL"), which does not exist under
                // any real AmigaOS NDK and failed to compile
                // ("'MN_BARLABEL' undeclared"). NM_BARLABEL itself is the
                // long-standing classic GadTools NewMenu sentinel
                // (<libraries/gadtools.h>, "((STRPTR)~0)" as a MenuItem's
                // title/label meaning "this is a separator bar") that
                // MUI's MUI_MakeObject(MUIO_Menuitem, label, shortcut,
                // flags, data) convenience macro understands for the same
                // purpose - kCHeaderPrelude now includes
                // <libraries/gadtools.h> explicitly (below) so this
                // resolves regardless of what a given NDK's other headers
                // happen to pull in transitively.
                out << "MUI_MakeObject(MUIO_Menuitem, NM_BARLABEL, 0, 0, 0)";
                break;
            }
            out << "MenuitemObject,\n";
            if (!obj->name.isEmpty())
            {
                indentTo(out, indent + 1);
                out << "MUIA_Menuitem_Title, " << cStringLiteral(obj->name) << ",\n";
            }
            if (!obj->menu_enable)
            {
                indentTo(out, indent + 1);
                out << "MUIA_Menuitem_Enabled, FALSE,\n";
            }
            if (obj->menuKey != '\0')
            {
                indentTo(out, indent + 1);
                out << "MUIA_Menuitem_Shortcut, \"" << QChar(QLatin1Char(obj->menuKey)) << "\",\n";
            }
            if (obj->check_enable)
            {
                indentTo(out, indent + 1);
                out << "MUIA_Menuitem_Checkit, TRUE,\n";
                if (obj->check_state)
                {
                    indentTo(out, indent + 1);
                    out << "MUIA_Menuitem_Checked, TRUE,\n";
                }
            }
            if (obj->toggleMenu)
            {
                indentTo(out, indent + 1);
                out << "MUIA_Menuitem_Toggle, TRUE,\n";
            }
            indentTo(out, indent);
            out << "End";
            break;
        }
        case ObjType::AboutBox:
        {
            // Real Aboutbox.mcc mapping (Aboutbox_mcc.h/Aboutbox.c, part of
            // the real MUI 5.0 SDK the user provided - MUIC_Aboutbox =
            // "Aboutbox.mcc", AboutboxObject = MUI_NewObject(MUIC_Aboutbox).
            // Every MUIA_Aboutbox_* attribute here is [I..] (init-only,
            // never gettable/settable afterwards), so, like the reference
            // Aboutbox.c demo, each is only ever emitted once at creation
            // time - never re-applied later. Each is independently
            // optional (Aboutbox.mcc itself treats an unset one as "use
            // the default"), so only non-empty fields are emitted at all.
            out << "AboutboxObject,\n";
            if (!obj->aboutCredits.isEmpty())
            {
                indentTo(out, indent + 1);
                out << "MUIA_Aboutbox_Credits, " << cStringLiteral(obj->aboutCredits) << ",\n";
            }
            if (!obj->aboutBuild.isEmpty())
            {
                indentTo(out, indent + 1);
                out << "MUIA_Aboutbox_Build, " << cStringLiteral(obj->aboutBuild) << ",\n";
            }
            if (!obj->aboutLogoFile.isEmpty())
            {
                indentTo(out, indent + 1);
                out << "MUIA_Aboutbox_LogoFile, " << cStringLiteral(obj->aboutLogoFile) << ",\n";
                // Try the external file first, then fall back to the
                // program's own Workbench icon if that fails - same
                // fallback order (and the same MAKE_ID('E','D',...)
                // encoding of it) as the real Aboutbox.c demo; MAKE_ID
                // itself is already unconditionally defined by this
                // generator's own C prelude (kCHeaderPrelude).
                indentTo(out, indent + 1);
                out << "MUIA_Aboutbox_LogoFallbackMode, MAKE_ID('E', 'D', '\\0', '\\0'),\n";
            }
            if (!obj->aboutUrl.isEmpty())
            {
                indentTo(out, indent + 1);
                out << "MUIA_Aboutbox_URL, " << cStringLiteral(obj->aboutUrl) << ",\n";
            }
            if (!obj->aboutUrlText.isEmpty())
            {
                indentTo(out, indent + 1);
                out << "MUIA_Aboutbox_URLText, " << cStringLiteral(obj->aboutUrlText) << ",\n";
            }
            indentTo(out, indent);
            out << "End";
            break;
        }
        default:
            out << "NULL /* unsupported object type */";
            break;
    }

    if (!ident.isEmpty())
        out << ")";
}

// Best-effort notification emission: the .MUIB file stores raw
// (srcType/destType/argString) fields whose symbolic MUIA_*/MUIM_* names
// aren't recoverable without the same missing tables discussed in
// muicodegen.h, so they're emitted as commented raw values for hand
// review rather than guessed at.
//
// KNOWN LIMITATION (documented rather than silently wrong): a target
// object that is function-local (non-persistent) is only actually in
// scope inside the window function that creates it. A notification
// wiring a source in one window to a non-persistent target declared in a
// DIFFERENT window's function will reference an out-of-scope local
// variable name here and fail to compile as-is - the original handles
// this by threading such targets in as extra function parameters
// (AddObjAsParameter/PositionInParametersList in code.c), which this pass
// deliberately does not port (see muicodegen.h). Cross-window
// notifications are uncommon in the 15 real example projects checked
// against load/save; same-window notifications (the overwhelming common
// case) resolve correctly since local vars share that one function's
// scope.
// Renders a TY_CONS_INT/TY_CONS_CHAR/TY_CONS_STRING/TY_CONS_BOOL action's
// runtime value from the stored evt.argString exactly like WriteNotify()'s
// own WriteInteger(atoi(...))/WriteChar(argstring[0])/WriteString(...)/
// WriteBool(argstring[0]=='1') calls (code.c) - see notifytables.h.
QString MuiCodeGen::renderNotifyConsValue(NotifyActionKind kind, const QString &argString)
{
    switch (kind)
    {
    case NotifyActionKind::ConsInt:
        return QString::number(argString.toInt());
    case NotifyActionKind::ConsChar:
    {
        QChar ch = argString.isEmpty() ? QChar(QLatin1Char(' ')) : argString.at(0);
        QString escaped(ch);
        if (ch == QLatin1Char('\'') || ch == QLatin1Char('\\'))
            escaped.prepend(QLatin1Char('\\'));
        return QStringLiteral("'%1'").arg(escaped);
    }
    case NotifyActionKind::ConsString:
        return cStringLiteral(argString);
    case NotifyActionKind::ConsBool:
        return (!argString.isEmpty() && argString.at(0) == QLatin1Char('1')) ? QStringLiteral("TRUE") : QStringLiteral("FALSE");
    default:
        return argString;
    }
}

void MuiCodeGen::emitNotifications(const MuibObject *obj, GenCtx &ctx, QStringList &notifyLines)
{
    if (!obj)
        return;

    if (!obj->notify.isEmpty())
    {
        QString srcIdent = identOf(obj, ctx);
        if (srcIdent.isEmpty())
            srcIdent = QStringLiteral("/* unnamed source object - give it a label in the .MUIB file to reference it here */");

        const QVector<NotifyEventDef> &srcEvents = notifyEventsFor(obj->type);

        for (const NotifyEvent &evt : obj->notify)
        {
            // Resolve the target label back to the ACTUAL object it names
            // (not a fresh re-sanitized guess at its identifier, which
            // could disagree with the real one after collision
            // disambiguation in assignIdents()) so the reference here is
            // exactly what's declared in the Gui struct / local scope.
            //
            // Special case: the Application itself is never a MuibObject
            // in our tree (it has no label the .MUIB format round-trips
            // through MuibProject), but the real original always stores
            // the fixed, hardcoded label "App" for it when some other
            // object's notification targets it - confirmed directly in
            // the real source: builder.c's InitCreation() does
            // "strcpy(application.label, \"App\")" once, unconditionally,
            // never user-editable - so every real .MUIB file that has a
            // MenuItem (or any object) wired to "quit the application"
            // etc. stores targetLabel=="App" for that event.
            //
            // BUG FIX (2026-09-17, found via a real m68k-amigaos-gcc build
            // of a user project with a MenuItem's Trigger wired to "App"):
            // this used to emit the literal identifier "app" here, on the
            // assumption the emitted DoMethod() always lands inside
            // BuildApplication() where a local `APTR app` exists. That is
            // NOT true in general - emitNotifications() is called for
            // every window's menu tree from inside BuildMenustrip() (see
            // generateApplication() below) and for every window itself
            // from inside its own BuildXxxWindow(), both of which run
            // and return BEFORE BuildApplication() ever creates `app` -
            // exactly the same scope problem already solved correctly a
            // few hundred lines below for the hardcoded window-close
            // notification, which uses MUIV_Notify_Application instead of
            // an `app` variable specifically because "this function
            // returns before BuildApplication()'s own `app` variable even
            // exists". Using the same scope-independent MUI macro here
            // fixes a real "'app' undeclared" compile error and needs no
            // local variable in scope at all, in any Build*() function.
            QString targetRef;
            ObjType targetType = ObjType::Unknown;
            if (evt.targetLabel.isEmpty())
            {
                targetRef = QStringLiteral("NULL /* no target label stored in file */");
            }
            else if (evt.targetLabel == QLatin1String("App"))
            {
                targetRef = QStringLiteral("MUIV_Notify_Application");
                targetType = ObjType::Appli;
            }
            else
            {
                const MuibObject *target = ctx.objectByLabel.value(evt.targetLabel, nullptr);
                QString resolved = target ? identOf(target, ctx) : QString();
                if (resolved.isEmpty())
                {
                    targetRef = QStringLiteral("NULL /* target label \"%1\" not found/not persistent - verify by hand */").arg(evt.targetLabel);
                }
                else
                {
                    targetRef = resolved;
                    targetType = target->type;
                }
            }

            // Translate srcType/destType via the real, ground-truth
            // CEVTxxx[]/CACTxxx[]+TYxxx[] tables (notifytables.h/.cpp,
            // ported directly from the real MUIBuilder v2.3 source) when
            // both the source object's event table and the target
            // object's action table actually cover the stored indices
            // and the action isn't one of the four kinds that reference
            // data our own NotifyEvent doesn't capture (TY_WINOBJ/
            // TY_FUNCTION/TY_ID/TY_VARIABLE - see notifytables.h).
            const QVector<NotifyActionDef> &destActions = notifyActionsFor(targetType);
            bool rendered = false;
            if (evt.srcType >= 0 && evt.srcType < srcEvents.size() &&
                evt.destType >= 0 && evt.destType < destActions.size())
            {
                const NotifyEventDef &srcDef = srcEvents[evt.srcType];
                const NotifyActionDef &destDef = destActions[evt.destType];
                if (destDef.kind != NotifyActionKind::Unsupported)
                {
                    QString valueText = (destDef.kind == NotifyActionKind::Nothing)
                        ? destDef.value
                        : MuiCodeGen::renderNotifyConsValue(destDef.kind, evt.argString);

                    QStringList parts;
                    parts << srcIdent << QStringLiteral("MUIM_Notify") << srcDef.attr << srcDef.value
                          << targetRef << QString::number(destDef.argCount) << destDef.method;
                    if (destDef.argCount >= 3)
                        parts << destDef.attr << valueText;
                    else if (destDef.argCount == 2)
                        parts << valueText;
                    // argCount == 1: DoMethod(..., 1, method) - nothing further.

                    notifyLines << QStringLiteral("\tDoMethod(%1);").arg(parts.join(QStringLiteral(", ")));
                    rendered = true;
                }
            }

            if (!rendered)
            {
                // Fallback for the genuinely unrecoverable cases (see
                // above) and for any srcType/destType index outside what
                // the real tables define for this pair of object types -
                // same raw, comment-flagged best-effort dump as before.
                notifyLines << QStringLiteral(
                    "\tDoMethod(%1, MUIM_Notify, %2, %3, %4, 3, MUIM_Set, %5, %6); "
                    "/* raw srcType/argString/destType from file - verify against real MUIA_*/MUIM_* names by hand */")
                    .arg(srcIdent)
                    .arg(evt.srcType)
                    .arg(evt.argString.isEmpty() ? QStringLiteral("(IPTR)NULL") : evt.argString)
                    .arg(targetRef)
                    .arg(evt.destType)
                    .arg(evt.argString.isEmpty() ? QStringLiteral("(IPTR)NULL") : evt.argString);
            }
        }
    }

    if (obj->menu)
        emitNotifications(obj->menu.get(), ctx, notifyLines);
    if (obj->root)
        emitNotifications(obj->root.get(), ctx, notifyLines);
    for (const auto &ch : obj->children)
        emitNotifications(ch.get(), ctx, notifyLines);
    if (obj->popObj)
        emitNotifications(obj->popObj.get(), ctx, notifyLines);
    for (const auto &ch : obj->childs)
        emitNotifications(ch.get(), ctx, notifyLines);
}

// See the doc comment on this method's declaration in muicodegen.h for
// the full design rationale (why explicit MUIM_Notify/MUIM_Set wiring
// instead of the undocumented MUIA_Menuitem_Exclude bitmask attribute).
void MuiCodeGen::emitMenuExcludeGroupWiring(const MuibObject *menuNode, GenCtx &ctx, QStringList &notifyLines)
{
    if (!menuNode)
        return;

    // Group this node's DIRECT MenuItem children by excludeGroup (only
    // check_enable items with a positive group participate - excludeGroup
    // == 0 means "not in any group", and a non-check item can't
    // meaningfully be "checked"/"unchecked" via MUIA_Menuitem_Checked in
    // the first place).
    QHash<int, QVector<const MuibObject *>> groups;
    for (const auto &c : menuNode->childs)
    {
        const MuibObject *child = c.get();
        if (child && child->type == ObjType::MenuItem && child->check_enable && child->excludeGroup > 0)
            groups[child->excludeGroup].append(child);
    }

    for (auto it = groups.constBegin(); it != groups.constEnd(); ++it)
    {
        const QVector<const MuibObject *> &members = it.value();
        if (members.size() < 2)
            continue;   // a "group" of one has nothing to mutually exclude
        for (const MuibObject *a : members)
        {
            const QString identA = identOf(a, ctx);
            if (identA.isEmpty())
                continue;   // unlabeled - can't be referenced from a DoMethod call
            for (const MuibObject *b : members)
            {
                if (a == b)
                    continue;
                const QString identB = identOf(b, ctx);
                if (identB.isEmpty())
                    continue;
                notifyLines << QStringLiteral(
                    "\tDoMethod(%1, MUIM_Notify, MUIA_Menuitem_Checked, TRUE, "
                    "%2, 3, MUIM_Set, MUIA_Menuitem_Checked, FALSE); "
                    "/* MuiBuilderQt \"Mutual Group\" - see muicodegen.h's emitMenuExcludeGroupWiring() comment */")
                    .arg(identA, identB);
            }
        }
    }

    // Recurse into every child (labeled or not, container or leaf) so
    // nested SubMenus' own exclude groups are handled too - a group is
    // always scoped to its own immediate parent, never across siblings at
    // different depths.
    for (const auto &c : menuNode->childs)
        emitMenuExcludeGroupWiring(c.get(), ctx, notifyLines);
}

// Walks a subtree and appends "static CONST_STRPTR ...[] = {...};"
// declaration text for every Cycle/Radio entries list and Register-mode
// group page titles list (referenced by ident from emitObject() above),
// so they're emitted just before the functions that reference them by
// name (C requires the array to be visible before use here, since it's
// referenced inline rather than forward-declared separately).
void MuiCodeGen::collectEntriesArrays(const MuibObject *obj, GenCtx &ctx, QStringList &decls)
{
    if (!obj)
        return;

    auto ident = [&](const MuibObject *o) -> QString {
        auto it = ctx.identFor.find(o);
        return it == ctx.identFor.end() ? QString() : it.value();
    };

    auto emitArray = [&](const QString &baseIdent, const QString &suffix, const QStringList &entries, bool externVisible)
    {
        if (entries.isEmpty())
            return;
        QString name = QStringLiteral("%1%2").arg(baseIdent.isEmpty() ? QStringLiteral("anon") : baseIdent, suffix);
        // Cycle/Radio's own arrays are deliberately real, externally-linked
        // symbols (no `static`) - see collectEntriesArrayExterns()'s doc
        // comment for why (a debug stub needs to read one from a DIFFERENT
        // translation unit than the one that defines it). The Register-
        // titles case has no such reader anywhere else, so it stays
        // `static` (file-local, as before) - narrower linkage whenever
        // nothing outside this file needs it.
        QString decl = QStringLiteral("%1CONST_STRPTR %2[] =\n{\n").arg(externVisible ? QString() : QStringLiteral("static "), name);
        for (const QString &e : entries)
        {
            QString escaped = e;
            escaped.replace(QLatin1Char('\\'), QStringLiteral("\\\\"));
            escaped.replace(QLatin1Char('"'), QStringLiteral("\\\""));
            decl += QStringLiteral("\t\"%1\",\n").arg(escaped);
        }
        decl += QStringLiteral("\tNULL\n};\n");
        decls << decl;
    };

    if (obj->type == ObjType::Cycle)
        emitArray(ident(obj), QStringLiteral("Entries"), obj->entries, true);
    else if (obj->type == ObjType::Radio)
        emitArray(ident(obj), QStringLiteral("Entries"), obj->entries, true);
    else if (obj->type == ObjType::Group && obj->registermode)
        emitArray(ident(obj), QStringLiteral("Titles"), obj->entries, false);

    if (obj->menu)
        collectEntriesArrays(obj->menu.get(), ctx, decls);
    if (obj->root)
        collectEntriesArrays(obj->root.get(), ctx, decls);
    for (const auto &ch : obj->children)
        collectEntriesArrays(ch.get(), ctx, decls);
    if (obj->popObj)
        collectEntriesArrays(obj->popObj.get(), ctx, decls);
    for (const auto &ch : obj->childs)
        collectEntriesArrays(ch.get(), ctx, decls);
}

void MuiCodeGen::collectEntriesArrayExterns(const MuibObject *obj, GenCtx &ctx, QStringList &decls)
{
    if (!obj)
        return;

    auto ident = [&](const MuibObject *o) -> QString {
        auto it = ctx.identFor.find(o);
        return it == ctx.identFor.end() ? QString() : it.value();
    };

    if ((obj->type == ObjType::Cycle || obj->type == ObjType::Radio) && !obj->entries.isEmpty())
    {
        QString name = QStringLiteral("%1Entries").arg(ident(obj).isEmpty() ? QStringLiteral("anon") : ident(obj));
        decls << QStringLiteral("extern CONST_STRPTR %1[];").arg(name);
    }

    if (obj->menu)
        collectEntriesArrayExterns(obj->menu.get(), ctx, decls);
    if (obj->root)
        collectEntriesArrayExterns(obj->root.get(), ctx, decls);
    for (const auto &ch : obj->children)
        collectEntriesArrayExterns(ch.get(), ctx, decls);
    if (obj->popObj)
        collectEntriesArrayExterns(obj->popObj.get(), ctx, decls);
    for (const auto &ch : obj->childs)
        collectEntriesArrayExterns(ch.get(), ctx, decls);
}

// Upper-cases and sanitizes `s` into a valid C preprocessor identifier
// fragment, for use in an #ifndef header guard (e.g. a baseName of
// "first-test" becomes "FIRST_TEST"). Mirrors sanitizeIdent()'s
// character policy but works on a plain string, not a MuibObject label.
static QString sanitizeMacroName(const QString &s)
{
    QString out;
    out.reserve(s.size());
    for (QChar c : s)
        out.append(c.isLetterOrNumber() ? c.toUpper() : QLatin1Char('_'));
    if (out.isEmpty() || out.at(0).isDigit())
        out.prepend(QLatin1Char('_'));
    return out;
}

// True for every real, clickable/settable widget type - i.e. everything
// EXCEPT pure containers/chrome (Appli, Window, Group, Space, Rectangle)
// and MOST of the Menu/SubMenu/MenuItem family. ObjType::MenuItem is the
// one deliberate exception, added at the Chefentwickler's own explicit
// request ("waere es nicht sinnvoll, wenn jeder Menueintrag und
// Submenueintrag ... eine leere, von Hand editierbare Funktion generieren
// wuerde"): per the Menu-Editor's own doc comment (menueditordialog.h),
// the real underlying model only has two menu-family KINDS regardless of
// the GUI's "Menue"/"Menuepunkt"/"Untermenuepunkt" depth labels -
// ObjType::SubMenu (container-capable - a bare menu TITLE like "File", or
// a Menuepunkt that has been auto-converted to hold Untermenuepunkt
// children, neither of which is itself clickable/triggerable) and
// ObjType::MenuItem (always a LEAF - the actual thing a user selects,
// whatever its depth). So "every real, selectable menu entry" maps
// exactly onto "every labeled ObjType::MenuItem", with ObjType::Menu (the
// whole menu strip's own root container) and ObjType::SubMenu still
// correctly excluded here, same as every other pure-container type above.
// See isNotifiableGadgetType()/notifyAttrFor() below for the matching
// MUIA_Menuitem_Trigger wiring, and collectAllGadgets() for the one
// explicit exclusion this feature still needs (the AboutBox-linked menu
// item, and any future dialog-linked one).
static bool isStubbableGadgetType(ObjType t)
{
    switch (t)
    {
    case ObjType::Appli:
    case ObjType::Window:
    case ObjType::Group:
    case ObjType::Space:
    case ObjType::Rectangle:
    case ObjType::Menu:
    case ObjType::SubMenu:
    case ObjType::Unknown:
        return false;
    default:
        return true;
    }
}

// True for a MenuItem that is really just a visual separator bar, never a
// real, selectable entry - the same "name starts with the literal
// BarLabel" convention emitObject()'s own MenuItem/BarLabel case and
// menueditordialog.cpp's own isSeparator checks already use (see e.g.
// menueditordialog.cpp:240/330/443/478). A separator has no real MUI
// trigger event at all (MUI itself never treats a NM_BARLABEL entry as
// selectable), and its `label` is only an internal bookkeeping id, never
// a real title the puts() message could meaningfully name - so unlike
// the other menu-item-stub exclusion (collectAllGadgets()'s AboutBox-
// linked-item filter), a separator gets no stub AT ALL, not even an
// unwired one, same reasoning as why this whole generator never asks
// "what happened when the user clicked the divider between two menu
// groups" for any other widget family either.
static bool isMenuSeparator(const MuibObject *obj)
{
    return obj && obj->type == ObjType::MenuItem &&
           obj->name.startsWith(QStringLiteral("BarLabel"));
}

// Walks the same subtree shapes as collectPersistentLabels()/
// collectNotifyTargets() above, collecting every LABELLED object whose
// type passes isStubbableGadgetType() (and, for a MenuItem, isn't just a
// separator - see isMenuSeparator() above) - order matches the object
// tree's natural traversal order, so the generated stubs read top-to-
// bottom the way the project does.
static void collectGadgetObjects(const MuibObject *obj, QVector<const MuibObject *> &out)
{
    if (!obj)
        return;
    if (!obj->label.isEmpty() && isStubbableGadgetType(obj->type) && !isMenuSeparator(obj))
        out.append(obj);

    if (obj->menu)
        collectGadgetObjects(obj->menu.get(), out);
    if (obj->root)
        collectGadgetObjects(obj->root.get(), out);
    for (const auto &ch : obj->children)
        collectGadgetObjects(ch.get(), out);
    if (obj->popObj)
        collectGadgetObjects(obj->popObj.get(), out);
    for (const auto &ch : obj->childs)
        collectGadgetObjects(ch.get(), out);
}

// True for the subset of isStubbableGadgetType() this generator is
// confident enough about to wire a LIVE MUI notification for - i.e. types
// whose real MUI class exposes one well-understood "the user just
// interacted with me" attribute. isStubbableGadgetType() above is
// deliberately broader (it decides which objects get a debug stub AT
// ALL, including purely static/display types like Label/Text that
// MUI has no click/change event for in the first place); the remaining
// stubbable-but-not-listed-here types (Gauge, Scale, Prop, ColorField,
// PopAsl, PopObject, Label, Text) still get a stub function, just not an
// automatic wire-up - Label/Text simply have no change event at all, and
// the rest don't (yet) have a confidently-verified single attribute the
// same way the cases below do.
//
// Image is a partial exception: a PLAIN/decorative Image has no click
// event either, same as Label/Text - but MUIBuilder lets any Area-based
// object be turned into a clickable "icon button" via the generic
// Area.InputMode flag (see emitArea()), which the original tool's own
// code.c confirms means MUIA_InputMode, MUIV_InputMode_RelVerify - the
// exact same Pressed-based click idiom a real Button uses. So this is
// wired up, but only for the specific objects that actually have that
// flag set (checked on the object itself, not a static per-type table
// like every other case here) - an ordinary decorative Image still gets
// no notification, exactly as before.
//
// DirList is unconditionally notifiable via MUIA_List_Active (LONG,
// inherited from Dirlist's real base class List.mui - confirmed via
// MUI's own official autodocs, amiga-mui/muidev's MUI_Dirlist wiki page,
// same primary source already used for the DirList creation-code fix
// above) - fires whenever the user changes the active/selected entry,
// the standard List-family "something changed" signal, and the one this
// generator's own DirList creation code (see emitObject() above) already
// embeds directly rather than wrapped in a Listview, so no wrapper-only
// attribute (like ListView's DoubleClick below) applies here.
//
// ListView is the deliberately conditional exception of this whole
// function - notifiable ONLY when the object's own `doubleclick` flag is
// set (checked on the object itself, exactly like Image/InputMode
// above), wired via MUIA_Listview_DoubleClick. This is a REAL, original-
// MUIBuilder-tool attribute (confirmed via code.c's own TY_LISTVIEW
// case, which conditionally emits MUIA_Listview_DoubleClick, TRUE when
// listview_aux->doubleclick is set - a real per-object checkbox in the
// original tool's own ListView editor dialog, "Doppelklick" in
// PropertyInspector here) - unlike a bare List/DirList's MUIA_List_Active
// (which fires on every single-click selection change), a Listview
// wrapper's DoubleClick is the deliberate, original-tool-sanctioned way
// to distinguish "the user is just browsing" from "the user picked
// this one" - so, matching the original tool's own default
// (`doubleclick = FALSE` in InitListview()), a ListView stays silent
// until the user (or an imported project) actually opts in.
//
// MenuItem is unconditionally notifiable, exactly like Button/String/etc.
// above - every labeled ObjType::MenuItem (see isStubbableGadgetType()'s
// own comment on why that's the right predicate for "a real, selectable
// menu entry") always fires MUIA_Menuitem_Trigger the moment the user
// picks it, with no per-object opt-in flag the way Image/ListView need
// (there is nothing to configure - unlike a Listview's optional
// Doppelklick property, a menu item has exactly one real trigger event).
static bool isNotifiableGadgetType(const MuibObject *obj)
{
    if (!obj)
        return false;
    switch (obj->type)
    {
    case ObjType::Button:
    case ObjType::String:
    case ObjType::Cycle:
    case ObjType::Radio:
    case ObjType::Check:
    case ObjType::Slider:
    case ObjType::DirList:
    case ObjType::MenuItem:
        return true;
    case ObjType::Image:
        return obj->area.InputMode;
    case ObjType::ListView:
        return obj->doubleclick;
    default:
        return false;
    }
}

// The real MUI attribute/value pair that fires when the user actually
// "does something" with each isNotifiableGadgetType() object - the same
// idiom MUIBuilder's own dialog code uses (e.g. check.c's
// "DoMethod(CM_title, MUIM_Notify, MUIA_Selected, TRUE, ...)") and the
// classic "MUIA_Pressed, FALSE" button-released pattern used throughout
// real MUI programs. Only meaningful when isNotifiableGadgetType(obj).
static void notifyAttrFor(const MuibObject *obj, QString &attr, QString &value)
{
    if (!obj)
        return;
    switch (obj->type)
    {
    case ObjType::Button:
    case ObjType::Image: // clickable Image (Area.InputMode) behaves like a Button
        attr = QStringLiteral("MUIA_Pressed");
        value = QStringLiteral("FALSE");
        break;
    case ObjType::String:
        attr = QStringLiteral("MUIA_String_Acknowledge");
        value = QStringLiteral("MUIV_EveryTime");
        break;
    case ObjType::Cycle:
        attr = QStringLiteral("MUIA_Cycle_Active");
        value = QStringLiteral("MUIV_EveryTime");
        break;
    case ObjType::Radio:
        attr = QStringLiteral("MUIA_Radio_Active");
        value = QStringLiteral("MUIV_EveryTime");
        break;
    case ObjType::Check:
        attr = QStringLiteral("MUIA_Selected");
        value = QStringLiteral("MUIV_EveryTime");
        break;
    case ObjType::Slider:
        attr = QStringLiteral("MUIA_Numeric_Value");
        value = QStringLiteral("MUIV_EveryTime");
        break;
    case ObjType::DirList:
        attr = QStringLiteral("MUIA_List_Active");
        value = QStringLiteral("MUIV_EveryTime");
        break;
    case ObjType::ListView: // only reached when obj->doubleclick (see isNotifiableGadgetType())
        attr = QStringLiteral("MUIA_Listview_DoubleClick");
        value = QStringLiteral("TRUE");
        break;
    case ObjType::MenuItem:
        // Real MUI attribute, ground-truth-verified during the Menu-
        // Editor's own "Aktion" combo feature (menueditordialog.cpp's
        // buildActionChoices()) via notifytables.cpp's eventsMenuItem()[0]
        // table entry, itself ported directly from the original tool's own
        // codenotifydefs.c - not guessed here.
        attr = QStringLiteral("MUIA_Menuitem_Trigger");
        value = QStringLiteral("MUIV_EveryTime");
        break;
    default:
        break;
    }
}

// Removes whichever gadget (if any) matches the AboutBox's own linked
// MenuItem label, in place - shared by collectAllGadgets() and
// collectWireableGadgets() below. At the Chefentwickler's own explicit
// request, when he asked for menu-item debug stubs ("ausgenommen AboutBox
// und spaeter zu implementierende weitere Dialogboxen"): this is the ONE
// real "opens a dialog box" concept the object model has today (see
// MuibProject::aboutBox / collectAboutBoxLabels() above), and a stub that
// just puts()'d a debug line for it would be actively misleading, since
// selecting this item's real job is to open the AboutBox, not to do
// nothing. Written as a plain, explicit check against today's single
// concrete case rather than a more general "dialog-linked object"
// mechanism, consistent with this project's practice of solving the
// concrete case at hand and documenting the rest as a known future
// extension point - the next dialog-box feature can extend this same
// filter.
static void removeAboutLinkedMenuItem(const MuibProject &proj, QVector<const MuibObject *> &gadgets)
{
    if (!proj.aboutBox || proj.aboutBox->aboutLinkedMenuItem.isEmpty())
        return;
    const QString &excluded = proj.aboutBox->aboutLinkedMenuItem;
    QVector<const MuibObject *> filtered;
    filtered.reserve(gadgets.size());
    for (const MuibObject *g : gadgets)
    {
        if (g->label != excluded)
            filtered.append(g);
    }
    gadgets = filtered;
}

// Every appMenu + window gadget in the project (minus the AboutBox's own
// linked MenuItem, if any - see removeAboutLinkedMenuItem() above), in
// one fixed, entirely deterministic traversal order. Used ONLY by
// generateGadgetStubs() to decide which objects get a debug stub - see
// collectWireableGadgets() below for the (deliberately narrower) set used
// everywhere a notify id / real DoMethod wiring / switch-case is needed.
static QVector<const MuibObject *> collectAllGadgets(const MuibProject &proj)
{
    QVector<const MuibObject *> gadgets;
    if (proj.appMenu)
        collectGadgetObjects(proj.appMenu.get(), gadgets);
    for (const auto &w : proj.windows)
        collectGadgetObjects(w.get(), gadgets);
    removeAboutLinkedMenuItem(proj, gadgets);
    return gadgets;
}

// Same as collectAllGadgets() above, but WITHOUT proj.appMenu's own
// gadgets - used everywhere a gadget needs an actual notify id (i.e. a
// real DoMethod wiring generateSource() emits and a switch/case
// generateMain() dispatches to): assignGadgetNotifyIds(), the window-
// building loop's own DoMethod wiring, and generateMain()'s dispatch loop
// all read from this, not collectAllGadgets().
//
// Reason: proj.appMenu's own menu items are a genuinely new source of
// stubbable gadgets as of the menu-item-stub feature (every OTHER
// stubbable type - Button/String/Cycle/... - can only ever live under a
// Window's own `root`, never inside a Menu/SubMenu/MenuItem tree, so this
// distinction was previously moot). generateSource()'s BuildMenustrip()
// (the function that actually builds proj.appMenu - see its own comment)
// has no notifyLines-emission step at all, unlike every BuildXWindow() -
// exactly the same, already-documented structural limitation
// emitMenuExcludeGroupWiring()'s own comment (muicodegen.h) describes for
// the "Mutual Group" feature ("only called for window-level menus
// (win->menu), not proj.appMenu"). Assigning a real notify id and a
// switch/case to an object that can never actually be wired would be
// dishonest generated code - a case that claims to be reachable via a
// real DoMethod notification but never actually is, since nothing ever
// sends that id. A stub function is still generated for such an object
// (generateGadgetStubs() deliberately keeps using collectAllGadgets()
// directly, appMenu included) - it is simply left unwired, exactly the
// same "still just a pre-named starting point - wire it up by hand"
// category this generator already uses for Gauge/Scale/Prop/ColorField/
// PopAsl/PopObject/Label/Text, just for a structural reason instead of an
// attribute-ambiguity one.
static QVector<const MuibObject *> collectWireableGadgets(const MuibProject &proj)
{
    QVector<const MuibObject *> gadgets;
    for (const auto &w : proj.windows)
        collectGadgetObjects(w.get(), gadgets);
    removeAboutLinkedMenuItem(proj, gadgets);
    return gadgets;
}

// One small positive, sequential MUIM_Application_ReturnID per
// isNotifiableGadgetType() gadget - safely below any real
// MUIV_Application_ReturnID_* sentinel (those live in a reserved,
// documented-large value range, never overlapping small positive
// integers) and, since it is derived purely from collectWireableGadgets()'s
// fixed order over the same project, identical every time it is
// recomputed - see collectWireableGadgets() above.
static QHash<const MuibObject *, int> assignGadgetNotifyIds(const QVector<const MuibObject *> &gadgets)
{
    QHash<const MuibObject *, int> ids;
    int nextId = 1;
    for (const MuibObject *g : gadgets)
    {
        if (isNotifiableGadgetType(g))
            ids.insert(g, nextId++);
    }
    return ids;
}

// True for objects whose debug stub is worth printing live content for
// (see emitDebugContentLines()) - a String's text, a Cycle/Radio's active
// entry (only meaningful with a real, non-empty entries list - an empty
// one never even gets its static array emitted by collectEntriesArrays(),
// so there'd be nothing to index into), a Check's selected state, a
// Slider's numeric value, or a ListView's active entry. Deliberately
// narrower than isNotifiableGadgetType(): every content-reporting type
// happens to also be auto-wired EXCEPT ListView (kept manually-wired only,
// same reasoning as before - its own notification attribute is still
// ambiguous), whose stub still gets the richer content line for whenever
// the user wires it up by hand themselves.
static bool hasDebugContent(const MuibObject *obj)
{
    if (!obj)
        return false;
    switch (obj->type)
    {
    case ObjType::String:
    case ObjType::Check:
    case ObjType::Slider:
    case ObjType::ListView:
    case ObjType::DirList: // MUIA_Dirlist_Path - see emitDebugContentLines()
        return true;
    case ObjType::Cycle:
    case ObjType::Radio:
        return !obj->entries.isEmpty();
    default:
        return false;
    }
}

// Every label whose object needs a persistent Gui.<ident> struct field
// specifically so its own debug stub (generateGadgetStubs(), a separate
// file/scope) and main()'s dispatcher can reach the live object later -
// a deliberate widening beyond the original .MUIB file's own per-object
// "generate a persistent variable" flag (obj->generated), which the
// debug-content feature doesn't otherwise have any say over. Must be
// unioned into ctx.persistentLabels identically in all four generate*()
// functions, same as collectNotifyTargets() above, so they keep agreeing
// on which objects are "Gui.<ident>" vs. a function-local variable.
static void collectDebugContentLabels(const MuibObject *obj, QSet<QString> &out)
{
    if (!obj)
        return;
    if (!obj->label.isEmpty() && hasDebugContent(obj))
        out.insert(obj->label);

    if (obj->menu)
        collectDebugContentLabels(obj->menu.get(), out);
    if (obj->root)
        collectDebugContentLabels(obj->root.get(), out);
    for (const auto &ch : obj->children)
        collectDebugContentLabels(ch.get(), out);
    if (obj->popObj)
        collectDebugContentLabels(obj->popObj.get(), out);
    for (const auto &ch : obj->childs)
        collectDebugContentLabels(ch.get(), out);
}

// AboutBox feature (see muibobject.h's ObjType::AboutBox / MuibProject::
// aboutBox comments): two labels need to end up in ctx.persistentLabels
// whenever a project has an AboutBox - the AboutBox object's own label
// (so it gets a "Gui.<ident>" field: BuildApplication() creates it, but
// the wiring that opens it lives in that same function, AFTER creation,
// so it must be reachable by name - a plain local var would go out of
// scope) and its linked MenuItem's label, if any (that MenuItem is
// created inside a DIFFERENT function - its own window's Build*Window() -
// so it likewise needs a "Gui.<ident>" field to be reachable from
// BuildApplication()'s wiring). Must be unioned into ctx.persistentLabels
// identically in all four generate*() functions, same as
// collectNotifyTargets()/collectDebugContentLabels() above, so they keep
// agreeing on which objects are "Gui.<ident>" vs. a function-local
// variable.
static void collectAboutBoxLabels(const MuibProject &proj, QSet<QString> &out)
{
    if (!proj.aboutBox)
        return;
    if (!proj.aboutBox->label.isEmpty())
        out.insert(proj.aboutBox->label);
    if (!proj.aboutBox->aboutLinkedMenuItem.isEmpty())
        out.insert(proj.aboutBox->aboutLinkedMenuItem);
}

QString MuiCodeGen::generateHeader(const MuibProject &proj)
{
    QString text;
    QTextStream out(&text);

    GenCtx ctx;
    collectPersistentLabels(proj.appMenu.get(), ctx.persistentLabels);
    for (const auto &w : proj.windows)
        collectPersistentLabels(w.get(), ctx.persistentLabels);
    QSet<QString> notifyTargets;
    collectNotifyTargets(proj.appMenu.get(), notifyTargets);
    for (const auto &w : proj.windows)
        collectNotifyTargets(w.get(), notifyTargets);
    ctx.persistentLabels.unite(notifyTargets);
    QSet<QString> debugContentLabels;
    collectDebugContentLabels(proj.appMenu.get(), debugContentLabels);
    for (const auto &w : proj.windows)
        collectDebugContentLabels(w.get(), debugContentLabels);
    ctx.persistentLabels.unite(debugContentLabels);
    QSet<QString> aboutBoxLabels;
    collectAboutBoxLabels(proj, aboutBoxLabels);
    ctx.persistentLabels.unite(aboutBoxLabels);

    assignIdents(proj.appMenu.get(), ctx);
    for (const auto &w : proj.windows)
        assignIdents(w.get(), ctx);
    assignIdents(proj.aboutBox.get(), ctx);

    out << "#ifndef GUI_H\n#define GUI_H\n\n";
    out << kHHeaderPrelude << "\n";
    // Anchored here, once, for every generated file (both <baseName>.c and
    // <baseName>_main.c #include this header): some Amiga NDK header sets
    // used for cross-compiling this project (e.g. an older exec/types.h
    // snapshot bundled with a vbcc/NDK distribution) do not define IPTR at
    // all, even though the object-tree emission below uses it throughout
    // (Cycle/Radio entry-list casts, Register titles, notification
    // arguments) - so a minimal, ABI-compatible definition is provided
    // unconditionally to make the generated code build out of the box
    // regardless of which NDK snapshot the toolchain ships.
    out << "typedef unsigned long IPTR;\n\n";
    out << "/* Generated by MuiBuilderQt from \"" << proj.title << "\" - see the .c file's\n"
        << "   header comment for important caveats about this code generator. */\n\n";

    // See collectEntriesArrayExterns()'s doc comment: a Cycle/Radio's
    // entries array is defined (with its real content) once, in
    // <baseName>.c - this forward-declares it here, in the header every
    // generated file includes first, so it's a known, real symbol
    // wherever it's read (including from <baseName>_gadgets.h's debug
    // stubs, compiled separately into more than one of those files).
    {
        QStringList externDecls;
        collectEntriesArrayExterns(proj.appMenu.get(), ctx, externDecls);
        for (const auto &w : proj.windows)
            collectEntriesArrayExterns(w.get(), ctx, externDecls);
        if (!externDecls.isEmpty())
        {
            for (const QString &d : externDecls)
                out << d << "\n";
            out << "\n";
        }
    }

    out << "struct GUIObjects\n{\n";
    // Only persistent (struct-field) idents belong in the header. Windows
    // always get a field (BuildApplication() always assigns Gui.<wIdent>
    // unconditionally) even in the pathological case of an empty window
    // label, where it wouldn't otherwise pass the label+persistentLabels
    // check below.
    for (auto it = ctx.identFor.constBegin(); it != ctx.identFor.constEnd(); ++it)
    {
        const MuibObject *obj = it.key();
        bool isPersistentField = obj->type == ObjType::Window ||
                                  (!obj->label.isEmpty() && ctx.persistentLabels.contains(obj->label));
        if (isPersistentField)
            out << "\tAPTR " << it.value() << ";\n";
    }
    out << "};\n\n";
    out << "extern struct GUIObjects Gui;\n\n";

    for (const auto &w : proj.windows)
    {
        QString wIdent = ctx.identFor.value(w.get());
        out << "APTR Build" << wIdent << "Window(void);\n";
    }
    if (proj.appMenu)
        out << "APTR BuildMenustrip(void);\n";
    out << "APTR BuildApplication(void);\n";

    out << "\n#endif /* GUI_H */\n";
    return text;
}

// --- Help/.guide generation --------------------------------------------
//
// Ported from the real MUIBuilder v2.3 source's entirely separate
// "guide.c" tool (GenerateGuide()/GuideReference()/WriteHelp() - see
// muicodegen.h's generateGuide() doc comment for the full shape). One
// deliberate simplification versus the real source: guide.c's own gate
// for whether a Group/PopObject gets a @NODE ("Help.generated &&
// Help.nb_char > 0", no label check) is subtly different from code.c's
// own gate for whether that same object gets a MUIA_HelpNode pointing at
// one ("help.generated && label non-empty" - see emitArea() below) - a
// real inconsistency in the original that can (rarely) produce either a
// dangling MUIA_HelpNode or an unreferenced @NODE there. This port uses
// ONE gate, hasGuideNode() below, matching emitArea()'s exactly, for
// every type including Group/PopObject, so what gets a MUIA_HelpNode and
// what gets a @NODE always agree by construction.

// Same predicate as emitArea()'s own MUIA_HelpNode gate (see there) -
// kept in perfect sync deliberately, not by coincidence.
static bool hasGuideNode(const MuibObject *obj)
{
    return obj && obj->help.generated && !obj->label.isEmpty() &&
           obj->type != ObjType::Check && obj->type != ObjType::Scale;
}

static bool subtreeHasHelp(const MuibObject *obj)
{
    if (!obj)
        return false;
    if (obj->help.generated && !obj->help.content.isEmpty())
        return true;
    if (obj->menu && subtreeHasHelp(obj->menu.get()))
        return true;
    if (obj->root && subtreeHasHelp(obj->root.get()))
        return true;
    for (const auto &ch : obj->children)
        if (subtreeHasHelp(ch.get()))
            return true;
    if (obj->popObj && subtreeHasHelp(obj->popObj.get()))
        return true;
    for (const auto &ch : obj->childs)
        if (subtreeHasHelp(ch.get()))
            return true;
    return false;
}

// Whether ANY object in the project has real, generated help content -
// used to decide whether to bother emitting MUIA_Application_HelpFile/
// writing a .guide file at all when the user hasn't set a "Hilfedatei"
// explicitly (see generateSource()'s own use of this, and generate()'s
// matching gate for actually writing the file).
static bool projectHasAnyHelp(const MuibProject &proj)
{
    for (const auto &w : proj.windows)
        if (subtreeHasHelp(w.get()))
            return true;
    return false;
}

// Ports GuideReference() (guide.c) - a cross-reference "@{ " title " link
// label }" line for a Window's/Group's direct child, or for a PopObject's
// single popped object; recurses into a help-less Group's own children
// instead of emitting a reference for it (so the reader/help viewer can
// still navigate down to whatever DOES have help further inside).
static void writeGuideReference(QTextStream &out, const MuibObject *obj)
{
    if (!obj)
        return;
    if (obj->type == ObjType::Group)
    {
        if (hasGuideNode(obj))
            out << "\t\t@{\" " << obj->help.title << " \" link " << obj->label << " }\n";
        else
            for (const auto &ch : obj->children)
                writeGuideReference(out, ch.get());
    }
    else if (hasGuideNode(obj))
    {
        out << "\t\t@{\" " << obj->help.title << " \" link " << obj->label << " }\n";
    }
}

// Ports GenerateGuide() (guide.c). A Window always gets a node
// (unconditionally, titled with its own MUIA_Window_Title text, not
// help.title - matches the real source exactly); Group/PopObject only
// when hasGuideNode() is true, in which case their DIRECT children are
// walked as cross-references into the same node's body; every other
// (leaf) type gets a plain node of its own, gated the same way. Windows/
// Groups/PopObjects additionally recurse into their children afterwards
// regardless of whether they themselves had a node, so nested help
// further down the tree is never skipped.
static void writeGuideNode(QTextStream &out, const MuibObject *obj)
{
    if (!obj)
        return;

    switch (obj->type)
    {
    case ObjType::Window:
    {
        out << "@NODE " << obj->label << " \"" << obj->title << "\"\n";
        out << QString::fromLatin1(obj->help.content);
        if (obj->root)
            for (const auto &ch : obj->root->children)
                writeGuideReference(out, ch.get());
        out << "@ENDNODE\n\n";
        if (obj->root)
            for (const auto &ch : obj->root->children)
                writeGuideNode(out, ch.get());
        break;
    }
    case ObjType::Group:
    {
        if (hasGuideNode(obj))
        {
            out << "@NODE " << obj->label << " \"" << obj->help.title << "\"\n";
            out << QString::fromLatin1(obj->help.content);
            for (const auto &ch : obj->children)
                writeGuideReference(out, ch.get());
            out << "@ENDNODE\n\n";
        }
        for (const auto &ch : obj->children)
            writeGuideNode(out, ch.get());
        break;
    }
    case ObjType::PopObject:
    {
        if (hasGuideNode(obj))
        {
            out << "@NODE " << obj->label << " \"" << obj->help.title << "\"\n";
            out << QString::fromLatin1(obj->help.content);
            writeGuideReference(out, obj->popObj.get());
            out << "@ENDNODE\n\n";
            writeGuideNode(out, obj->popObj.get());
        }
        break;
    }
    default:
        if (hasGuideNode(obj))
        {
            out << "@NODE " << obj->label << " \"" << obj->help.title << "\"\n";
            out << QString::fromLatin1(obj->help.content);
            out << "@ENDNODE\n\n";
        }
        break;
    }
}

QString MuiCodeGen::generateGuide(const MuibProject &proj)
{
    QString text;
    QTextStream out(&text);
    for (const auto &w : proj.windows)
        writeGuideNode(out, w.get());
    return text;
}

QString MuiCodeGen::generateSource(const MuibProject &proj, const QString &baseName)
{
    QString text;
    QTextStream out(&text);

    GenCtx ctx;
    collectPersistentLabels(proj.appMenu.get(), ctx.persistentLabels);
    for (const auto &w : proj.windows)
        collectPersistentLabels(w.get(), ctx.persistentLabels);
    QSet<QString> notifyTargets;
    collectNotifyTargets(proj.appMenu.get(), notifyTargets);
    for (const auto &w : proj.windows)
        collectNotifyTargets(w.get(), notifyTargets);
    ctx.persistentLabels.unite(notifyTargets);
    QSet<QString> debugContentLabels;
    collectDebugContentLabels(proj.appMenu.get(), debugContentLabels);
    for (const auto &w : proj.windows)
        collectDebugContentLabels(w.get(), debugContentLabels);
    ctx.persistentLabels.unite(debugContentLabels);
    QSet<QString> aboutBoxLabels;
    collectAboutBoxLabels(proj, aboutBoxLabels);
    ctx.persistentLabels.unite(aboutBoxLabels);

    assignIdents(proj.appMenu.get(), ctx);
    for (const auto &w : proj.windows)
        assignIdents(w.get(), ctx);
    assignIdents(proj.aboutBox.get(), ctx);

    // Same fixed ids generateMain()'s event-loop switch dispatches on -
    // see assignGadgetNotifyIds() for why recomputing this independently
    // in both places still keeps them in agreement. collectWireableGadgets()
    // (not collectAllGadgets()) - see its own comment for why proj.appMenu
    // is deliberately excluded from notify-id assignment.
    const QHash<const MuibObject *, int> gadgetNotifyIds =
        assignGadgetNotifyIds(collectWireableGadgets(proj));

    out << kCHeaderPrelude << "\n";
    // AboutBox feature: Aboutbox.mcc's own convenience macro/attributes
    // (AboutboxObject, MUIA_Aboutbox_*) live in this header, part of the
    // real MUI 5.0 SDK - only pulled in when the project actually has an
    // AboutBox, so a project without one gets no new dependency at all.
    if (proj.aboutBox)
        out << "#include <mui/Aboutbox_mcc.h>\n\n";
    out << "#include \"" << baseName << ".h\"\n";
    // Every gadget's debug-click stub, automatically available right here
    // where the objects themselves get built (see generateGadgetStubs()).
    out << "#include \"" << baseName << "_gadgets.h\"\n\n";
    out << "struct GUIObjects Gui;\n\n";

    // Static entry-list arrays (Cycle/Radio content, Register titles) for
    // every window + the app menu, emitted up front so every function
    // below can reference them by name regardless of declaration order.
    QStringList entryArrayDecls;
    collectEntriesArrays(proj.appMenu.get(), ctx, entryArrayDecls);
    for (const auto &w : proj.windows)
        collectEntriesArrays(w.get(), ctx, entryArrayDecls);
    for (const QString &decl : entryArrayDecls)
        out << decl << "\n";

    if (proj.appMenu)
    {
        // proj.appMenu IS the application-level root Menu object (TY_MENU)
        // - emitObject()'s own ObjType::Menu case already emits the full,
        // correct "MenustripObject, ... End" expression for it (see the
        // fixed mapping there, verified against code.c's real TY_MENU
        // case), so this just returns that directly. Previously this
        // wrapped the whole thing in ANOTHER hardcoded outer
        // "MenustripObject, Child, <appMenu>, End" - apparently modelled
        // on a misread of builder.c's own comment about MUIBuilder's OWN
        // internal application object ("AppMenu = MenustripObject, End,"),
        // which describes MUIBuilder's own fixed GUI init, not the pattern
        // CodeCreate() emits for a user project's appmenu. The real
        // original (code.c's TY_APPLI case) creates application.appmenu
        // as ONE standalone MenustripObject via a single CodeCreate() call
        // and then just references that variable from
        // MUIA_Application_Menustrip - never a second, nested
        // MenustripObject wrapping it.
        out << "APTR BuildMenustrip(void)\n{\n";
        QStringList localDecls, notifyLines;
        // Pre-walk once so decls are known before the expression is written.
        {
            QString scratch;
            QTextStream discard(&scratch);
            emitObject(discard, proj.appMenu.get(), 1, ctx, localDecls, notifyLines);
        }
        for (const QString &d : localDecls)
            out << "\t" << d << "\n";
        out << "\treturn ";
        QStringList discard2, discard3;
        emitObject(out, proj.appMenu.get(), 1, ctx, discard2, discard3);
        out << ";\n}\n\n";
    }

    for (const auto &w : proj.windows)
    {
        const MuibObject *win = w.get();
        QString wIdent = ctx.identFor.value(win);

        QStringList localDecls;
        QStringList notifyLines;
        // Pre-walk to collect local decls without emitting into real output.
        {
            QString scratch;
            QTextStream discard(&scratch);
            if (win->menu)
                emitObject(discard, win->menu.get(), 1, ctx, localDecls, notifyLines);
            if (win->root)
                emitObject(discard, win->root.get(), 1, ctx, localDecls, notifyLines, win);
        }
        notifyLines.clear();
        emitNotifications(win, ctx, notifyLines);
        if (win->menu)
            emitMenuExcludeGroupWiring(win->menu.get(), ctx, notifyLines);

        // Every window needs this or its close gadget/menu-Close/Workbench
        // "Close" is purely cosmetic: MUIA_Window_CloseRequest only ever
        // marks the request, it never actually closes anything by itself -
        // an app-level DoMethod is what real MUI programs always add to
        // turn that into an actual quit. MUIV_Notify_Application resolves
        // "the running Application object" without needing an `app` local
        // in scope here (this function returns before BuildApplication()'s
        // own `app` variable even exists).
        notifyLines << QStringLiteral(
            "\tDoMethod(win, MUIM_Notify, MUIA_Window_CloseRequest, TRUE, "
            "MUIV_Notify_Application, 2, MUIM_Application_ReturnID, "
            "MUIV_Application_ReturnID_Quit);");

        // Wire every isNotifiableGadgetType() gadget in THIS window up to
        // its own *_Clicked() debug stub, out of the box - same
        // MUIV_Notify_Application trick as above, routed through main()'s
        // event-loop switch (see generateMain()) since MUIM_Application_
        // ReturnID can only set an id, not call a C function directly.
        {
            QVector<const MuibObject *> windowGadgets;
            collectGadgetObjects(win, windowGadgets);
            for (const MuibObject *g : windowGadgets)
            {
                auto idIt = gadgetNotifyIds.constFind(g);
                if (idIt == gadgetNotifyIds.constEnd())
                    continue;
                QString attr, value;
                notifyAttrFor(g, attr, value);
                QString srcIdent = identOf(g, ctx);
                if (srcIdent.isEmpty())
                    continue;
                notifyLines << QStringLiteral(
                    "\tDoMethod(%1, MUIM_Notify, %2, %3, "
                    "MUIV_Notify_Application, 2, MUIM_Application_ReturnID, %4);")
                    .arg(srcIdent, attr, value)
                    .arg(*idIt);
            }
        }

        out << "APTR Build" << wIdent << "Window(void)\n{\n";
        out << "\tAPTR win;\n";
        for (const QString &d : localDecls)
            out << "\t" << d << "\n";
        out << "\n\twin = WindowObject,\n";
        out << "\t\tMUIA_Window_Title, " << cStringLiteral(win->title) << ",\n";
        if (win->help.generated && !win->label.isEmpty())
        {
            // Same real MUIA_HelpNode attribute as emitArea() below emits
            // for every other widget type (see the comment there) - a
            // Window gets it too (code.c's TY_WINDOW case, right after its
            // own MUIA_Window_Title), but on this separate code path since
            // windows are built here directly, never through emitObject().
            out << "\t\tMUIA_HelpNode, " << cStringLiteral(win->label) << ",\n";
        }
        // MuiBuilderQt-only feature (see WindowPositionMode's own comment
        // in muibobject.h): the real original tool never emits
        // MUIA_Window_LeftEdge/TopEdge at all (confirmed against the full
        // v2.3 source - a window always opens at MUI's own default
        // position, which is what the "always centered" complaint this
        // feature addresses was actually about). WindowPositionMode::
        // Centered deliberately emits NOTHING here, so an untouched
        // project's generated code is byte-identical to before this
        // feature existed; Moused/Manual use real MUI 5.0 SDK
        // attributes/values (MUIA_Window_LeftEdge/TopEdge, "V4 isg LONG",
        // MUIV_Window_LeftEdge_Moused/MUIV_Window_TopEdge_Moused -
        // confirmed in include/libraries/mui.h).
        if (win->posMode == WindowPositionMode::Moused)
        {
            out << "\t\tMUIA_Window_LeftEdge, MUIV_Window_LeftEdge_Moused,\n";
            out << "\t\tMUIA_Window_TopEdge, MUIV_Window_TopEdge_Moused,\n";
        }
        else if (win->posMode == WindowPositionMode::Manual)
        {
            out << "\t\tMUIA_Window_LeftEdge, " << win->posX << ",\n";
            out << "\t\tMUIA_Window_TopEdge, " << win->posY << ",\n";
        }
        // MuiBuilderQt-only, same absent-from-the-original story as
        // posMode/posX/posY just above (window1/code.c have no size
        // fields either - a window's initial size has always come purely
        // from MUI's own automatic layout of its content). 0 = "not set",
        // so an untouched project again emits nothing here. Width/Height
        // are real, Window-specific attributes (MUIA_Window_Width/Height,
        // "V4 isg LONG" - confirmed in include/libraries/mui.h) and go
        // directly into the WindowObject's own tag list like LeftEdge/
        // TopEdge; Min/MaxWidth/Height are real but GENERIC Area-class
        // attributes (confirmed in the same header, listed under the
        // Area class's own attribute block, not a "MUIA_Window_" one) and
        // are emitted on the root CONTENT object instead, inside
        // emitObject()'s Group case - see this function's declaration
        // comment in muicodegen.h for the full rationale.
        if (win->winWidth > 0)
            out << "\t\tMUIA_Window_Width, " << win->winWidth << ",\n";
        if (win->winHeight > 0)
            out << "\t\tMUIA_Window_Height, " << win->winHeight << ",\n";
        out << "\t\tMUIA_Window_CloseGadget, " << (win->closegadget ? "TRUE" : "FALSE") << ",\n";
        out << "\t\tMUIA_Window_SizeGadget, " << (win->sizegadget ? "TRUE" : "FALSE") << ",\n";
        out << "\t\tMUIA_Window_DragBar, " << (win->dragbar ? "TRUE" : "FALSE") << ",\n";
        out << "\t\tMUIA_Window_Borderless, " << (win->borderless ? "TRUE" : "FALSE") << ",\n";
        out << "\t\tMUIA_Window_Backdrop, " << (win->backdrop ? "TRUE" : "FALSE") << ",\n";
        if (win->menu)
        {
            out << "\t\tMUIA_Window_Menustrip, ";
            QStringList d2, n2;
            emitObject(out, win->menu.get(), 2, ctx, d2, n2);
            out << ",\n";
        }
        out << "\t\tWindowContents, ";
        if (win->root)
        {
            QStringList d2, n2;
            emitObject(out, win->root.get(), 2, ctx, d2, n2, win);
        }
        else
        {
            out << "VGroup, End";
        }
        out << ",\n\tEnd;\n";

        if (!notifyLines.isEmpty())
        {
            out << "\n\tif (win)\n\t{\n";
            for (const QString &n : notifyLines)
                out << "\t" << n << "\n";
            out << "\t}\n";
        }

        out << "\n\treturn win;\n}\n\n";
    }

    if (proj.aboutBox)
        // Registers the custom class by name with the Application object -
        // a real, documented MUI convention for any program using a
        // custom class (MUIA_Application_UsedClasses, confirmed in the
        // real MUI 5.0 SDK's mui.h), matching the real Aboutbox.c demo's
        // own "static const char *const UsedClasses[] = {\"Aboutbox.mcc\",
        // NULL};" exactly. A fixed one-entry array for now since AboutBox
        // is the only optional custom class this generator can emit.
        out << "static const char *const UsedClasses[] =\n{\n\t\"Aboutbox.mcc\",\n\tNULL\n};\n\n";

    out << "APTR BuildApplication(void)\n{\n\tAPTR app;\n\n\tapp = ApplicationObject,\n";
    out << "\t\tMUIA_Application_Title, " << cStringLiteral(proj.title) << ",\n";
    out << "\t\tMUIA_Application_Version, " << cStringLiteral(proj.version) << ",\n";
    out << "\t\tMUIA_Application_Copyright, " << cStringLiteral(proj.copyright) << ",\n";
    out << "\t\tMUIA_Application_Author, " << cStringLiteral(proj.author) << ",\n";
    out << "\t\tMUIA_Application_Description, " << cStringLiteral(proj.description) << ",\n";
    out << "\t\tMUIA_Application_Base, " << cStringLiteral(proj.base) << ",\n";
    // Real original mapping (code.c's own BuildApplication()-equivalent:
    // "if (strlen(application.helpfile) > 0) ... MUIA_Application_HelpFile,
    // FilePart(application.helpfile)" - confirmed, only the filename part
    // is ever written, never a full path). One deliberate addition beyond
    // strict original behaviour: if the project has help content but the
    // user never set a "Hilfedatei" in Anwendungseigenschaften (no GUI
    // exposed this field at all until now - see projectpropertiesdialog.h),
    // default to "<baseName>.guide" instead of silently emitting nothing -
    // otherwise every MUIA_HelpNode this generator writes (emitArea()
    // below) would be a dangling reference with no file to resolve it
    // against. See generate()'s matching guide-file write and
    // generateGuide()'s own doc comment.
    if (!proj.helpfile.isEmpty())
        out << "\t\tMUIA_Application_HelpFile, " << cStringLiteral(QFileInfo(proj.helpfile).fileName()) << ",\n";
    else if (projectHasAnyHelp(proj))
        out << "\t\tMUIA_Application_HelpFile, " << cStringLiteral(baseName + QStringLiteral(".guide")) << ",\n";
    if (proj.aboutBox)
        out << "\t\tMUIA_Application_UsedClasses, UsedClasses,\n";
    if (proj.appMenu)
        // NOT "MenuStrip" (camelCase "S") - the real MUI attribute is
        // spelled "Menustrip", one word, matching MenustripObject's own
        // casing (confirmed against the original MUIBuilder's own working
        // source, e.g. builder.c: "MUIA_Application_Menustrip, AppMenu =
        // MenustripObject, End,"). The camelCase spelling is a genuine
        // "undeclared identifier" compile error, not a style nit.
        out << "\t\tMUIA_Application_Menustrip, BuildMenustrip(),\n";
    for (const auto &w : proj.windows)
    {
        QString wIdent = ctx.identFor.value(w.get());
        out << "\t\tSubWindow, Gui." << wIdent << " = Build" << wIdent << "Window(),\n";
    }
    if (proj.aboutBox)
    {
        // The AboutBox window is a SubWindow of the Application exactly
        // like a regular window (see the real Aboutbox.c demo's own
        // "SubWindow, aboutboxWin = AboutboxObject, ..., End," - the ONLY
        // difference from a normal window there is that it starts closed,
        // i.e. no MUIA_Window_Open, TRUE here, opened later on demand).
        out << "\t\tSubWindow, ";
        QStringList discardDecls, discardNotify;
        emitObject(out, proj.aboutBox.get(), 2, ctx, discardDecls, discardNotify);
        out << ",\n";
    }
    out << "\tEnd;\n";

    if (proj.aboutBox)
    {
        const QString aboutIdent = identOf(proj.aboutBox.get(), ctx);
        out << "\n\tif (app)\n\t{\n";
        // Clicking the AboutBox's own close gadget only marks the
        // request - without this it would never actually close again
        // (same MUIA_Window_CloseRequest caveat as every other window -
        // see the loop above) - but unlike a real window, closing it
        // must NOT quit the whole application, only hide the AboutBox
        // itself again (MUIV_Notify_Self + MUIM_Set), exactly like the
        // real Aboutbox.c demo's own wiring.
        out << "\t\tDoMethod(" << aboutIdent << ", MUIM_Notify, MUIA_Window_CloseRequest, TRUE, "
               "MUIV_Notify_Self, 3, MUIM_Set, MUIA_Window_Open, FALSE);\n";
        if (!proj.aboutBox->aboutLinkedMenuItem.isEmpty())
        {
            const MuibObject *menuItemObj = ctx.objectByLabel.value(proj.aboutBox->aboutLinkedMenuItem, nullptr);
            const QString menuItemIdent = menuItemObj ? identOf(menuItemObj, ctx) : QString();
            if (!menuItemIdent.isEmpty())
                // The real, documented way to notify on a menu item being
                // selected (MUIA_Menuitem_Trigger, MUIV_EveryTime) -
                // confirmed against the real, shipped MUIBuilder v3
                // source's own internal menu wiring (InternalObjects/
                // MUIBuilderMain.c), which uses this exact same attribute/
                // value pair to route its own "Save"/"Quit"/"About"
                // menu items.
                out << "\t\tDoMethod(" << menuItemIdent << ", MUIM_Notify, MUIA_Menuitem_Trigger, MUIV_EveryTime, "
                    << aboutIdent << ", 3, MUIM_Set, MUIA_Window_Open, TRUE);\n";
        }
        out << "\t}\n";
    }

    out << "\n\treturn app;\n}\n";

    return text;
}


// Appends 0+ extra statement lines to `out` (already inside the stub's
// "if (myDebug) { ... }" block, at two-tab indent - see the puts() call
// site in generateGadgetStubs() below), printing `obj`'s own live content
// via MUI's standard get()/DoMethod() attribute-query idiom. A no-op for
// any type hasDebugContent() doesn't recognize. `ident` is `obj`'s own
// rawIdent() - the caller already has it (needed for the function name
// too), so it's passed in rather than recomputed here.
//
// Real MUI facts this depends on (all verified against the public MUI SDK/
// wiki during implementation, not guessed - see the session's DirList/Image
// mistakes for why that discipline matters here too):
//   - get(obj, attr, &storage) is the standard attribute-read macro.
//   - MUIA_String_Contents (STRPTR), MUIA_Cycle_Active / MUIA_Radio_Active
//     (LONG index into the class's own entries array - exactly the same
//     CONST_STRPTR array collectEntriesArrays() emits and this function's
//     Cycle/Radio case indexes into by name), MUIA_Selected (BOOL),
//     MUIA_Numeric_Value (LONG) are all standard, well-established
//     attributes on their respective classes.
//   - MUIA_Listview_List gets a Listview's underlying List sub-object;
//     DoMethod(list, MUIM_List_GetEntry, MUIV_List_GetEntry_Active, &entry)
//     fetches the currently-active entry's own STRPTR directly, without a
//     separate MUIA_List_Active query first (confirmed exact signature via
//     the amiga-mui/muidev wiki's MUI_List page).
void MuiCodeGen::emitDebugContentLines(QTextStream &out, const MuibObject *obj, GenCtx &ctx)
{
    if (!hasDebugContent(obj))
        return;

    // The object reference used in get()/DoMethod() below MUST be the
    // "Gui.<ident>"-qualified form (identOf()), not the bare rawIdent().
    // Reason: this function's caller (generateGadgetStubs()) unions every
    // labeled hasDebugContent() object into ctx.persistentLabels via
    // collectDebugContentLabels(), specifically so the object survives as
    // a "struct GUIObjects" field reachable from ANY translation unit
    // that sees "extern struct GUIObjects Gui;" (declared in <baseName>.h)
    // - which is exactly this function's own situation: its output lands
    // in a `static inline ..._Clicked(void)` stub in <baseName>_gadgets.h,
    // a completely separate function scope from wherever the object was
    // actually built (BuildApplication(), where a non-persistent object
    // would only exist as an unreachable local variable). Using
    // rawIdent() here (the bare, unqualified name) compiled fine for the
    // *object's own creation code* (same function, local variable in
    // scope) but is simply undefined anywhere else - a real vbcc/NDK3.2
    // build of DVIPrint.MUIB confirmed this: "unknown identifier
    // <CY_label_0>" / <STR_label_0> / <LV_label_1> etc., one error per
    // content-reporting gadget. identOf() returns "" for an unlabeled
    // object (never promoted to persistent, so genuinely unreachable from
    // here either way) - correctly emitting no content line at all rather
    // than a dangling identifier.
    const QString ref = identOf(obj, ctx);
    if (ref.isEmpty())
        return;

    // Cycle/Radio's entries array, by contrast, is a plain external
    // global (not a GUIObjects struct field - see collectEntriesArrays()/
    // collectEntriesArrayExterns()), named from the object's raw,
    // unprefixed ident regardless of the object's own persistence -
    // exactly the same convention emitObject() itself already uses when
    // wiring a Cycle/Radio's notification up to this same array.
    const QString entriesIdent = rawIdent(obj, ctx);

    switch (obj->type)
    {
    case ObjType::String:
        out << "\t\t{\n";
        out << "\t\t\tSTRPTR content = NULL;\n";
        out << "\t\t\tget(" << ref << ", MUIA_String_Contents, &content);\n";
        out << "\t\t\tif (content)\n";
        out << "\t\t\t\tprintf(\"Mein Inhalt ist jetzt: %s\\n\", content);\n";
        out << "\t\t}\n";
        break;

    case ObjType::Cycle:
    case ObjType::Radio:
    {
        const bool isCycle = (obj->type == ObjType::Cycle);
        out << "\t\t{\n";
        out << "\t\t\tLONG active = 0;\n";
        out << "\t\t\tget(" << ref << ", "
            << (isCycle ? QStringLiteral("MUIA_Cycle_Active") : QStringLiteral("MUIA_Radio_Active"))
            << ", &active);\n";
        out << "\t\t\tif ((active >= 0) && (active < " << obj->entries.size() << "))\n";
        out << "\t\t\t\tprintf(\"%s wurde ausgewaehlt\\n\", " << entriesIdent << "Entries[active]);\n";
        out << "\t\t}\n";
        break;
    }

    case ObjType::Check:
        out << "\t\t{\n";
        out << "\t\t\tLONG selected = FALSE;\n";
        out << "\t\t\tget(" << ref << ", MUIA_Selected, &selected);\n";
        out << "\t\t\tprintf(\"Mein Zustand ist jetzt: %s\\n\", selected ? \"ausgewaehlt\" : \"nicht ausgewaehlt\");\n";
        out << "\t\t}\n";
        break;

    case ObjType::Slider:
        out << "\t\t{\n";
        out << "\t\t\tLONG value = 0;\n";
        out << "\t\t\tget(" << ref << ", MUIA_Numeric_Value, &value);\n";
        out << "\t\t\tprintf(\"Mein Inhalt ist jetzt: %ld\\n\", value);\n";
        out << "\t\t}\n";
        break;

    case ObjType::ListView:
        out << "\t\t{\n";
        out << "\t\t\tAPTR list = NULL;\n";
        out << "\t\t\tget(" << ref << ", MUIA_Listview_List, &list);\n";
        out << "\t\t\tif (list)\n";
        out << "\t\t\t{\n";
        out << "\t\t\t\tSTRPTR entry = NULL;\n";
        out << "\t\t\t\tDoMethod(list, MUIM_List_GetEntry, MUIV_List_GetEntry_Active, &entry);\n";
        out << "\t\t\t\tif (entry)\n";
        out << "\t\t\t\t\tprintf(\"%s wurde ausgewaehlt\\n\", entry);\n";
        out << "\t\t\t}\n";
        out << "\t\t}\n";
        break;

    case ObjType::DirList:
        // MUIA_Dirlist_Path (STRPTR, get-only) - "returns a pointer to
        // the complete path specification" of the currently active
        // entry, confirmed via MUI's own official autodocs (the same
        // amiga-mui/muidev MUI_Dirlist wiki page already used for the
        // DirList creation-code fix - see emitObject() above). Directly
        // on the Dirlist object itself, no Listview-wrapper indirection
        // needed (unlike ListView's MUIA_Listview_List above) - this
        // generator's own DirList creation code embeds it unwrapped.
        out << "\t\t{\n";
        out << "\t\t\tSTRPTR path = NULL;\n";
        out << "\t\t\tget(" << ref << ", MUIA_Dirlist_Path, &path);\n";
        out << "\t\t\tif (path)\n";
        out << "\t\t\t\tprintf(\"Ausgewaehlter Pfad: %s\\n\", path);\n";
        out << "\t\t}\n";
        break;

    default:
        break;
    }
}

// The puts() message text baked into each gadget's debug stub. Every
// non-menu type uses the established "Ich bin <label> und wurde geklickt"
// wording (unchanged - see generateGadgetStubs() below), but a MenuItem
// gets its own wording at the Chefentwickler's own explicit request
// ("...eine ... Meldung nach dem Schema 'Ich bin Menuepunkt XYZ und wurde
// getriggert'"): "geklickt" is simply the wrong word for a menu entry (no
// mouse click reaches it the way a Button's does - a keyboard shortcut or
// MUI's own menu machinery can select it too), so it uses "getriggert"
// instead, deliberately matching the real MUI event name this type fires
// (MUIA_Menuitem_Trigger - see notifyAttrFor() above). ASCII-only
// ("Menuepunkt", not "Menuepunkt" with an actual u-umlaut), matching every
// other generated-code and GUI string in this project (see e.g. the
// Menu-Editor's own "Menuepunkt"/"Untermenuepunkt" combo choices).
static QString debugStubMessageFor(const MuibObject *obj)
{
    if (obj->type == ObjType::MenuItem)
        return QStringLiteral("Ich bin Menuepunkt %1 und wurde getriggert").arg(obj->label);
    return QStringLiteral("Ich bin %1 und wurde geklickt").arg(obj->label);
}

// The object's own real, user-visible display text - a MenuItem's `name`
// (its actual MUIA_Menuitem_Title, e.g. "Quit"), or every other
// stubbable type's `title` (Button/String/Cycle/Radio/Check/Slider/
// ListView/... - see muibobject.h's own "also reused by Button/ListView/
// Slider/Check/Label/String" comment on that field). Purely for an
// explanatory comment above each stub function (see
// generateGadgetStubs() below) - `obj->label` (the identifier baked into
// the function name, e.g. "MenuItem_5") is an internal bookkeeping id
// assigned in creation order, NOT the real text the end user actually
// sees in the running program, so two entries can easily end up with
// generic, hard-to-tell-apart labels ("MenuItem_2" for a "Manual" item,
// "MenuItem_5" for a genuinely different "Quit" item). This is exactly
// the mix-up that led the Chefentwickler to once hand-edit the WRONG
// stub (menuTest2: added `running = FALSE;` to MenuItem_2_Clicked(),
// the "Manual" item's stub, while trying to fix "Quit", MenuItem_5's own
// stub) - the debug puts() message itself doesn't help either, since it
// also only ever echoes `label`, never the real title. Rather than
// inventing a whole new label-naming scheme (this project deliberately
// keeps every type's default label the same "TypeName_N" shape - see
// objectfactory.cpp's own comment on NOT replicating the original
// tool's distinct per-type label schemes, "for consistency with every
// other type here"), this adds a small, purely additive `/* "..." */`
// comment instead - zero risk to any already-generated code's structure
// or behavior, but now a human editing generated code by hand can see
// at a glance which real entry a given stub belongs to. Returns empty
// when there is nothing useful to show (no real display text at all, or
// it happens to already equal the label).
static QString debugStubDisplayTextFor(const MuibObject *obj)
{
    if (!obj)
        return QString();
    const QString &text = (obj->type == ObjType::MenuItem) ? obj->name : obj->title;
    if (text.isEmpty() || text == obj->label)
        return QString();
    return text;
}

// Makes `s` safe to place inside a single `/* ... */` C comment - guards
// against a real title containing the literal "*/" (which would close
// the comment early and corrupt the generated file) and collapses any
// embedded newline (a comment should stay on one line here).
static QString commentSafe(const QString &s)
{
    QString out = s;
    out.replace(QStringLiteral("*/"), QStringLiteral("* /"));
    out.replace(QLatin1Char('\n'), QLatin1Char(' '));
    return out;
}

QString MuiCodeGen::generateGadgetStubs(const MuibProject &proj, const QString &baseName)
{
    QString text;
    QTextStream out(&text);

    GenCtx ctx;
    collectPersistentLabels(proj.appMenu.get(), ctx.persistentLabels);
    for (const auto &w : proj.windows)
        collectPersistentLabels(w.get(), ctx.persistentLabels);
    QSet<QString> notifyTargets;
    collectNotifyTargets(proj.appMenu.get(), notifyTargets);
    for (const auto &w : proj.windows)
        collectNotifyTargets(w.get(), notifyTargets);
    ctx.persistentLabels.unite(notifyTargets);
    QSet<QString> debugContentLabels;
    collectDebugContentLabels(proj.appMenu.get(), debugContentLabels);
    for (const auto &w : proj.windows)
        collectDebugContentLabels(w.get(), debugContentLabels);
    ctx.persistentLabels.unite(debugContentLabels);
    QSet<QString> aboutBoxLabels;
    collectAboutBoxLabels(proj, aboutBoxLabels);
    ctx.persistentLabels.unite(aboutBoxLabels);

    assignIdents(proj.appMenu.get(), ctx);
    for (const auto &w : proj.windows)
        assignIdents(w.get(), ctx);
    assignIdents(proj.aboutBox.get(), ctx);

    const QVector<const MuibObject *> gadgets = collectAllGadgets(proj);

    const QString guard = sanitizeMacroName(baseName) + QStringLiteral("_GADGETS_H");

    out << "#ifndef " << guard << "\n#define " << guard << "\n\n";
    out << "/* Generated by MuiBuilderQt - one near-empty debug stub per gadget (and\n"
        << "   per selectable menu entry) in \"" << proj.title << "\". <baseName>_main.c\n"
        << "   automatically wires each stub belonging to a \"notifiable\" type\n"
        << "   (Button/String/Cycle/Radio/Check/Slider/DirList, every labeled menu\n"
        << "   entry, plus any Image configured as a clickable icon button, plus any\n"
        << "   ListView with its \"Doppelklick\" property enabled - see\n"
        << "   isNotifiableGadgetType() in muicodegen.cpp) up to its gadget's real MUI\n"
        << "   notification via an MUIM_Application_ReturnID, so those fire out of\n"
        << "   the box; everything else here (a ListView with \"Doppelklick\" off,\n"
        << "   Gauge, Scale, Prop, ColorField, PopAsl, PopObject, a plain/decorative\n"
        << "   Image, Label, Text) is still just a pre-named starting point - wire it\n"
        << "   up by hand (typically via a Hook) wherever that fits your application.\n"
        << "   `static inline` so an unwired one never trips -Wunused-function.\n"
        << "   A gadget whose content is worth reporting (String/Cycle/Radio/\n"
        << "   Check/Slider/ListView/DirList) also prints that live content\n"
        << "   alongside the click message - see emitDebugContentLines() in\n"
        << "   muicodegen.cpp. Whichever menu item is currently linked to the\n"
        << "   AboutBox (if any) deliberately gets no stub here at all - its real\n"
        << "   job is opening the AboutBox, see collectAllGadgets() in\n"
        << "   muicodegen.cpp. `running` (declared extern below) is the same global\n"
        << "   flag <baseName>_main.c's own event loop reads - set `running = FALSE;`\n"
        << "   from any stub here (e.g. the \"Quit\" menu item's own) to end the\n"
        << "   program cleanly from inside it. */\n\n";
    // <libraries/mui.h>/<clib/alib_protos.h>/<proto/muimaster.h> are already
    // active by the time this header is textually included (both baseName.c
    // and baseName_main.c pull in kCHeaderPrelude first) - but the stub
    // bodies below use get()/DoMethod()/MUIA_*/MUIM_* directly, so this
    // header declares its own real dependencies rather than silently
    // relying on include order in whoever happens to include it.
    out << "#include <exec/types.h>\n";
    out << "#include <libraries/mui.h>\n";
    out << "#include <clib/alib_protos.h>\n";
    out << "#include <proto/muimaster.h>\n";
    out << "#include <stdio.h>\n";
    out << "#include <stdlib.h>\n\n";
    out << "extern BOOL myDebug;\n";
    // `running` (generateMain()'s event-loop flag) is a global for
    // exactly this reason - so a stub here (the Quit MenuItem's, for
    // instance) can end the program itself with a plain `running =
    // FALSE;`, at the Chefentwickler's own explicit request. See
    // generateMain()'s own comment on this variable.
    out << "extern BOOL running;\n\n";

    if (gadgets.isEmpty())
    {
        out << "/* (no labelled, stubbable gadgets in this project yet) */\n\n";
    }
    else
    {
        for (const MuibObject *obj : gadgets)
        {
            const QString ident = rawIdent(obj, ctx);
            if (ident.isEmpty())
                continue;
            const QString displayText = debugStubDisplayTextFor(obj);
            if (!displayText.isEmpty())
                out << "/* \"" << commentSafe(displayText) << "\" */\n";
            out << "static inline void " << ident << "_Clicked(void)\n{\n";
            out << "\tif (myDebug)\n\t{\n";
            out << "\t\tputs(" << cStringLiteral(debugStubMessageFor(obj)) << ");\n";
            emitDebugContentLines(out, obj, ctx);
            out << "\t}\n}\n\n";
        }
    }

    out << "#endif /* " << guard << " */\n";
    return text;
}

QString MuiCodeGen::generateMain(const MuibProject &proj, const QString &baseName)
{
    QString text;
    QTextStream out(&text);

    GenCtx ctx;
    collectPersistentLabels(proj.appMenu.get(), ctx.persistentLabels);
    for (const auto &w : proj.windows)
        collectPersistentLabels(w.get(), ctx.persistentLabels);
    QSet<QString> notifyTargets;
    collectNotifyTargets(proj.appMenu.get(), notifyTargets);
    for (const auto &w : proj.windows)
        collectNotifyTargets(w.get(), notifyTargets);
    ctx.persistentLabels.unite(notifyTargets);
    QSet<QString> debugContentLabels;
    collectDebugContentLabels(proj.appMenu.get(), debugContentLabels);
    for (const auto &w : proj.windows)
        collectDebugContentLabels(w.get(), debugContentLabels);
    ctx.persistentLabels.unite(debugContentLabels);
    QSet<QString> aboutBoxLabels;
    collectAboutBoxLabels(proj, aboutBoxLabels);
    ctx.persistentLabels.unite(aboutBoxLabels);

    assignIdents(proj.appMenu.get(), ctx);
    for (const auto &w : proj.windows)
        assignIdents(w.get(), ctx);
    assignIdents(proj.aboutBox.get(), ctx);

    // Same fixed ids generateSource() wired each gadget's DoMethod
    // notification to - see assignGadgetNotifyIds(). collectWireableGadgets()
    // (not collectAllGadgets()) - see its own comment for why proj.appMenu
    // is deliberately excluded here, same as in generateSource().
    const QVector<const MuibObject *> allGadgets = collectWireableGadgets(proj);
    const QHash<const MuibObject *, int> gadgetNotifyIds = assignGadgetNotifyIds(allGadgets);

    out << kCHeaderPrelude << "\n";
    out << "#include \"" << baseName << ".h\"\n";
    out << "#include \"" << baseName << "_gadgets.h\"\n\n";
    // Global on/off switch for every *_Clicked() debug stub in
    // "<baseName>_gadgets.h" - flip to FALSE (or remove the assignment
    // and let your own startup code set it) once you don't want the
    // puts() calls anymore. Placed immediately after the include block
    // (before any other declaration) so the generated program works out
    // of the box without the reader having to hunt for it further down.
    out << "BOOL myDebug = TRUE;\n\n";
    // The event loop's own "keep running" flag, at the Chefentwickler's
    // own explicit request: a global (not, as before, a variable local to
    // main()'s own inner block) so any *_Clicked() debug stub in
    // "<baseName>_gadgets.h" - the Quit MenuItem's, for instance - can
    // set `running = FALSE;` by hand to cleanly end the program itself,
    // exactly the same idiom `myDebug` already establishes above.
    // Declared `extern` in the gadgets header (see generateGadgetStubs())
    // for the same reason. Purely a rename of scope, not of behavior -
    // the event loop below still reads/writes the very same variable it
    // always did, just no longer shadowed inside its own block.
    out << "BOOL running = TRUE;\n\n";
    out << "struct Library *MUIMasterBase;\n\n";

    out << "int main(void)\n{\n";
    out << "\tAPTR app;\n\n";
    out << "\tif ((MUIMasterBase = OpenLibrary(MUIMASTER_NAME, MUIMASTER_VMIN)) == NULL)\n";
    out << "\t{\n\t\tputs(\"Can't open \" MUIMASTER_NAME \"\\n\");\n\t\treturn 20;\n\t}\n\n";

    out << "\tapp = BuildApplication();\n";
    out << "\tif (!app)\n";
    out << "\t{\n\t\tputs(\"Can't create the application - BuildApplication() failed\\n\");\n";
    out << "\t\tCloseLibrary(MUIMasterBase);\n\t\treturn 20;\n\t}\n\n";

    bool anyInitOpen = false;
    for (const auto &w : proj.windows)
        if (w->initopen)
            anyInitOpen = true;

    if (!proj.windows.empty())
    {
        if (anyInitOpen)
        {
            for (const auto &w : proj.windows)
            {
                if (!w->initopen)
                    continue;
                const QString wIdent = ctx.identFor.value(w.get());
                out << "\tset(Gui." << wIdent << ", MUIA_Window_Open, TRUE);\n";
            }
        }
        else
        {
            // No window has "Initial geoeffnet"/initopen set in the
            // project - open the first one anyway so this generated
            // program actually shows something when run. Set initopen on
            // whichever window(s) you actually want open at startup (or
            // open others later yourself, e.g. from a menu action) and
            // this fallback goes away.
            const QString wIdent = ctx.identFor.value(proj.windows.front().get());
            out << "\t/* No window has 'initopen' set in the project - opening the first\n"
                << "\t   one so this program shows something; set a window's \"Initial\n"
                << "\t   geoeffnet\" flag in MuiBuilderQt to control this properly. */\n";
            out << "\tset(Gui." << wIdent << ", MUIA_Window_Open, TRUE);\n";
        }
        out << "\n";
    }

    out << "\t{\n\t\tULONG sigs = 0;\n\n";
    out << "\t\twhile (running)\n\t\t{\n";
    out << "\t\t\tULONG id = DoMethod(app, MUIM_Application_NewInput, &sigs);\n\n";
    out << "\t\t\tswitch (id)\n\t\t\t{\n";
    out << "\t\t\t\tcase MUIV_Application_ReturnID_Quit:\n";
    out << "\t\t\t\t\trunning = FALSE;\n";
    out << "\t\t\t\t\tbreak;\n\n";

    // One case per isNotifiableGadgetType() gadget, wired in
    // generateSource() to land here with exactly this id - dispatches
    // straight to that gadget's own debug stub in "<baseName>_gadgets.h",
    // so e.g. clicking a Cycle/String/Radio/Check/Slider/Button, or
    // picking a menu entry, actually prints out of the box instead of
    // silently doing nothing.
    for (const MuibObject *g : allGadgets)
    {
        auto idIt = gadgetNotifyIds.constFind(g);
        if (idIt == gadgetNotifyIds.constEnd())
            continue;
        QString ident = rawIdent(g, ctx);
        if (ident.isEmpty())
            continue;
        // Same real-display-text comment as generateGadgetStubs()'s own
        // stub functions (see debugStubDisplayTextFor()'s comment) - so
        // scanning this switch alone already shows which real, visible
        // entry each case belongs to, not just its internal label.
        const QString displayText = debugStubDisplayTextFor(g);
        out << "\t\t\t\tcase " << *idIt << ": /* " << ident
            << (displayText.isEmpty() ? QString() : QStringLiteral(" \"%1\"").arg(commentSafe(displayText)))
            << " */\n";
        out << "\t\t\t\t\t" << ident << "_Clicked();\n";
        out << "\t\t\t\t\tbreak;\n\n";
    }

    out << "\t\t\t\t/* Any further notification you wire up elsewhere as\n";
    out << "\t\t\t\t   DoMethod(..., MUIM_Notify, ..., MUIV_Notify_Application, 2,\n";
    out << "\t\t\t\t   MUIM_Application_ReturnID, <your id>) lands here too - add a\n";
    out << "\t\t\t\t   case per id you want this loop to react to directly (pick an id\n";
    out << "\t\t\t\t   outside the range this generator already uses above). */\n";
    out << "\t\t\t\tdefault:\n";
    out << "\t\t\t\t\tbreak;\n";
    out << "\t\t\t}\n\n";
    out << "\t\t\tif (running && sigs)\n\t\t\t{\n";
    out << "\t\t\t\tsigs = Wait(sigs | SIGBREAKF_CTRL_C);\n";
    out << "\t\t\t\tif (sigs & SIGBREAKF_CTRL_C)\n";
    out << "\t\t\t\t\trunning = FALSE;\n";
    out << "\t\t\t}\n";
    out << "\t\t}\n\t}\n\n";

    out << "\tMUI_DisposeObject(app);\n";
    out << "\tCloseLibrary(MUIMasterBase);\n";
    out << "\treturn 0;\n}\n";

    return text;
}

bool MuiCodeGen::generate(const MuibProject &proj, const QString &outDir,
                           const QString &baseName, QString *errorOut)
{
    QDir dir(outDir);
    if (!dir.exists() && !dir.mkpath(QStringLiteral(".")))
    {
        if (errorOut)
            *errorOut = QStringLiteral("Could not create output directory: %1").arg(outDir);
        return false;
    }

    const QString headerText = generateHeader(proj);
    const QString sourceText = generateSource(proj, baseName);
    const QString gadgetsText = generateGadgetStubs(proj, baseName);
    const QString mainText = generateMain(proj, baseName);

    const auto writeOne = [&](const QString &fileName, const QString &content) -> bool {
        const QString path = dir.filePath(fileName);
        QFile file(path);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        {
            if (errorOut)
                *errorOut = QStringLiteral("Could not write: %1").arg(path);
            return false;
        }
        file.write(content.toLatin1());
        file.close();
        return true;
    };

    // Gadgets header written before the .c/main.c that #include it, and
    // the .h before either - not that it matters for a filesystem write,
    // but it reads more naturally in that dependency order.
    if (!writeOne(baseName + QStringLiteral(".h"), headerText))
        return false;
    if (!writeOne(baseName + QStringLiteral("_gadgets.h"), gadgetsText))
        return false;
    if (!writeOne(baseName + QStringLiteral(".c"), sourceText))
        return false;
    if (!writeOne(baseName + QStringLiteral("_main.c"), mainText))
        return false;

    // Same gate generateSource() uses for whether it even emits
    // MUIA_Application_HelpFile at all - keep both in sync (see there).
    if (!proj.helpfile.isEmpty() || projectHasAnyHelp(proj))
    {
        const QString guideFileName = !proj.helpfile.isEmpty()
            ? QFileInfo(proj.helpfile).fileName()
            : baseName + QStringLiteral(".guide");
        if (!writeOne(guideFileName, generateGuide(proj)))
            return false;
    }

    return true;
}
