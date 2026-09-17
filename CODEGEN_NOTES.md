# Porting notes for code.c (the C code generator) - task #16

Working notes written while reading the original `code.c` (3298 lines) so
progress survives even if this session's context gets compacted. This file
is NOT part of the shipped port; it's scratch/planning only.

## Status: reading code.c, not yet writing any C++ for this part.

## Structural overview so far (lines 1-200 read)

- Globals: `genfile[512]` (output base path), `nospace[80]`, `genlabels`
  (queue of generated C identifiers), `get_string[80]`, `real_getstring[100]`,
  `nb_identificateurs` (running count of C variables that will need a
  declared APTR), `nb_var_aux`, and the six BOOL codegen options already in
  our `MuibProject` (`optCode`/`optEnv`/`optDeclarations`/`optLocal`/
  `optNotifications`/`optGenerateAll` <- code/env/declarations/local/
  notifications/generate_all).
- `IsParentFrom(objsrc, objdest)` - walks `objdest->father` chain up to
  TY_APPLI, checking whether objsrc is an ancestor. Used for notification
  target scoping (can objdest "see" objsrc as a local var vs needs global).
- `PopObjectDepth` - counts nested PopObject depth (for indentation /
  temp-var naming of nested popped-up objects presumably).
- `UpdateNodeIdent` / `CalculeNbIdent` - two-pass identifier-counting: MUI
  Builder does NOT generate a C variable for every single object by
  default; only objects that need to be *referenced later* (by a
  notification, or by name because a Cycle/Radio/String/Slider/Check has a
  paired Label, or because "generate all" option is on) get a declared
  APTR variable + identifier. `CalculeNbIdent` walks the whole tree once
  to compute `nb_identificateurs` (also has size-effects: String/Slider/
  Check with `title_exist` count as 2 idents - the widget itself AND its
  paired Label). This pre-pass is essential to replicate before actually
  emitting declarations, since the generated `.h` declares exactly this
  many APTR globals/statics.
- `GenerateListLabels` - emits (for `.cd`/catalog generation, not core to a
  first C-only pass) one line per catalog entry that's marked `ident`.

## Scope decision (mine, per unattended-operation guidance - documented so
the user can override on return)

A full, byte-for-byte faithful port of all 3298 lines (covering catalog/.cd
generation, ARexx port code, full notification-hook code generation with
every one of the original's many special-cased hook argument styles, MUI
image/pen edge cases, etc.) is not a reasonable scope for this pass. I will
port the CORE structural code generator faithfully:
  - the two-pass ident/declaration counting (CalculeNbIdent equivalent)
  - object-tree -> nested MUI_NewObject()/MUI_NewObjectA() (or the
    `WindowObject`/`GroupObject`/... macro style the original actually
    emits - TBD, need to keep reading) C code emission for every TY_*
    type actually exercised by the 15 real example files
  - the generated .h header (APTR declarations)
  - a single top-level BuildXxx()-style function per window, matching the
    original's actual function-naming/structure (TBD)
  - notifications (DoMethod/Notify wiring), since the user explicitly
    called out parameter-passing/notifications as important
  - skip: catalog/.cd/locale generation, ARexx, Ctrl/Alt-help polish,
    icon-tooltype parsing (config.icons path) - these are secondary to
    "produces compilable, structurally correct C for a GUI project".

Will revisit/expand after the core pass is validated against at least one
real example (Small.MUIB or click.MUIB - the smallest, 7-8 objects).

## TODO as I keep reading
- [x] find the main entry point (likely `GenerateCode`/`Generate` or similar)
- [x] find how MUI objects are actually emitted (macro style vs raw calls)
- [x] find notification code emission (codenotify.h mentioned - separate file!)
- [x] find header/declaration emission
- [x] find per-type body emission (Button/String/Group/Window/Menu etc.)

## MAJOR FINDING: the real architecture is a 3-stage pipeline, and one stage
## is NOT in the provided source at all.

1. `code.c`'s `CodeCreate()` (a ~1360-line function, builder.h line 1067-2428)
   walks the object tree and emits a *token stream* (TC_CREATEOBJ,
   TC_ATTRIBUT, TC_STRING, TC_FUNCTION, TC_MUIARG*, TC_VAR_AFFECT, ...) to a
   temp file (`T:MUIBuilder4.tmp`), via the low-level `Write*()` helpers
   (WriteCreateObj/WriteAttribut/WriteString/... around line 798-998). This
   is essentially a serialized AST, not C text.
2. This gets handed to `muibuilder.library` (MB_* functions - MB_GetVarInfo,
   MB_GetNextCode, etc.) which stores/replays it.
3. A SEPARATE external helper *program*, `Modules/C/src/GenCodeC.c` (1071
   lines, found via grep - this IS in the source tree, under
   `Modules/C/src/`), reads the token stream back via MB_GetNextCode() and
   renders the FINAL C text, e.g. `WriteCode()` (line 256+) does
   `fprintf(file, "%s,\n", MUIStrings[name])` for TC_CREATEOBJ, etc.

The critical blocker: `MUIStrings[]` - the array mapping each MB_* numeric
ID to its real MUI macro/attribute-name STRING (e.g. "WindowObject",
"MUIA_Window_Title") - is referenced throughout GenCodeC.c but its
*definition* is nowhere in the provided source tree (grepped the whole
v2/releases/2.3 tree, only usages found, no `MUIStrings[] = {...}`
definition). This table is either generated by a build-time script not
included here, or shipped as a separate proprietary/compiled data file.
**This means byte-for-byte fidelity with the real tool's generated C output
is not achievable from the source we have, regardless of effort** - a
required data table is simply missing from what was provided.

## Scope decision (final, given the above)

Rather than trying to reverse-engineer ~1360 lines of token-stream emission
PLUS a second 1071-line interpreter PLUS a missing string table, I'm
writing a clean, DIRECT C++ generator: MuibProject/MuibObject tree -> C
text in one pass, using well-known real MUI macro names (WindowObject,
HGroup/VGroup/ColGroup, ButtonObject, StringObject, ListviewObject, etc.
from the public MUI SDK, which I do know) rather than reconstructing the
inaccessible MUIStrings table. This produces structurally-correct,
idiomatic MUI C - not a byte-identical replica of the original tool's
output. Documenting this plainly for the user rather than pretending
otherwise.

Design:
- One .h + one .c for the whole project (all windows), not per-object like
  the original's interactive "Generate Code" command - more useful as an
  AmigaED-integration deliverable.
- Faithfully reuse Modules/C-Header + Modules/H-Header content (these ARE
  fully present and trivial - just a few #include lines) as the prelude of
  the generated .c/.h.
- "Which objects get a persistent (struct-field) identifier vs. a
  function-local variable" reuses the file format's OWN `generated` flag
  per object directly (confirmed via GenerateLabels(): `if (generated)`
  emits TYPEVAR_PTR (persistent) else TYPEVAR_LOCAL_PTR (local) - this flag
  is already ported into MuibObject::generated) PLUS: force-persistent for
  every Window (needs referencing to open/attach to Application) and for
  any object whose label is targeted by a notification anywhere in the
  project (avoids replicating the original's cross-function
  parameter-passing scheme for locals referenced across function
  boundaries - see AddObjAsParameter/PositionInParametersList/
  GenerateExternalLabels in code.c, which I'm intentionally NOT porting).
- Notifications: emitted as best-effort `DoMethod(src, MUIM_Notify, ...)`
  calls using the raw stored NotifyEvent fields, with a comment flagging
  that the raw integer src/dest "type" values come straight from the file
  and may need hand-verification against real MUIA_* constants (their
  symbolic names aren't recoverable without the same missing tables).
- Types covered with real per-type MUI mappings: Window, Group (H/V/Col/
  Row/Register), Button, Text, String, ListView, Label, Cycle, Radio,
  Check, Space, Rectangle, Gauge, Slider, Prop, Image, ColorField, PopAsl,
  PopObject, Menu/SubMenu/MenuItem. All 24 types get at least a functional
  mapping (no silent drops) since the data model already carries every
  field needed.

## Status: core/muicodegen.h + core/muicodegen.cpp written.

Implemented and self-reviewed (multiple passes, several real bugs caught
and fixed before ever building - see below). Not yet build-verified: the
sandbox's Bash tool safety classifier went down mid-session right after
the first successful build+link, blocking every further `make`/test run.
Waiting it out and retrying periodically; code changes made in the
meantime were reviewed carefully by re-reading rather than by rebuilding.

Bugs caught during self-review (before any build attempt could confirm
them one way or the other):
1. Cycle/Radio/Register-titles static array names were computed two
   different ways in emitObject() (deriving from `ident`, which is ""
   for unlabeled objects, falling back to a hardcoded "cyc"/"rad"/"grp"
   prefix with NO uniqueness) vs collectEntriesArrays() (deriving from
   ctx.identFor.value(obj) with an "anon" fallback) - these could name
   the reference and the actual declared array differently, and/or
   collide across multiple unlabeled Cycle/Radio objects in the same
   file. Fixed by extending assignIdents() to always hand out a unique
   ident to every Cycle/Radio/registermode-Group object (labeled or
   not), and adding rawIdent() (vs. identOf(), which stays label-gated
   for the "does this get wrapped in a capturing assignment" decision)
   so both sites derive the exact same name from the same ctx.identFor
   entry.
2. Windows with (hypothetically) no label: BuildApplication() always
   does `Gui.<wIdent> = Build<wIdent>Window()` unconditionally, but the
   header's struct-field loop only emitted a field when the object had
   both a non-empty label AND was in persistentLabels - an unlabeled
   window would reference an undeclared struct field. Fixed by making
   Window objects always get a struct field regardless of label, and
   always get an ident via assignIdents() regardless of label (mirrors
   fix #1's approach).
3. Notification target references were being re-derived from the raw
   target label string via a second, independent sanitizeIdent() call
   in emitNotifications(), instead of looking up the actual target
   object and reading its real assigned identifier - these two could
   disagree whenever assignIdents() had to disambiguate a label
   collision (two objects sharing a display label, which does happen in
   real .MUIB files). Fixed by adding ctx.objectByLabel (label -> the
   object that owns it) and resolving through identOf(target, ctx).
4. A leftover `QTextStream discard(new QString())` leaked a QString
   (Qt's QTextStream(QString*) ctor does not take ownership). Fixed to
   a stack-local QString + QTextStream(&scratch).
5. First draft had a chunk of genuinely broken/dead code (an abandoned
   `collectNestedEntriesRec`/`collectNestedEntries` pair, one of which
   called `MuiCodeGen::generateHeader(MuibProject())` from inside a
   lambda - infinite-recursion-shaped nonsense left over from an
   editing false start) - deleted entirely, superseded by the working
   `collectEntriesArrays()`.

## Status: DONE. Build-verified, all 15 real example files pass.

Bash came back online; built tests/test_codegen.pro and ran it against
all 15 real example projects. First run: 14/15 fully passing, 1 failure
that turned out to be a false positive in the TEST's own naive bracket
balance checker (it didn't understand `"..."` string-literal context, so
the literal project text `"Amiga 1000 :)"` in MUI-Demo.MUIB tripped it).
Fixed the checker to skip bracket-counting inside string/comment context
-> all 15 pass.

While eyeballing the generated output for sanity (click.c, the smallest
project) found one more REAL bug, this time in the generator itself, not
the test: objects marked `generated` (-> persistent Gui.<ident> struct
field, declared in the header) were never actually assigned in the
source - emitObject() only wrapped the creation expression in a
capturing `(ident = ...)` for the LOCAL-variable case, silently skipping
the wrap entirely for persistent ones. Net effect: the header declared
`APTR BT_1stbutton;` etc. but Gui.BT_1stbutton was left uninitialized
forever - exactly the kind of bug the structural checks (balanced
brackets, non-empty output) can't catch, only a real eyeball read can.
Fixed in emitObject(): now wraps BOTH persistent and local objects in a
capturing assignment (identOf() already returns the right form for each
- "Gui.<ident>" or the bare local name - so the fix was just widening the
condition and only skipping the *local decl* line, not the wrap, for the
persistent case). Rebuilt, reran both test_codegen and test_loadsave
suites afterward: still ALL CHECKS PASSED for both, and manually
re-inspected click.c and MUI-Demo.c afterward to confirm every
Gui.<ident> is now actually assigned once and referenced consistently in
notifications.

Final state: core/muicodegen.h + core/muicodegen.cpp complete and
validated against all 15 real example files (structural checks + manual
eyeball read of 2 generated outputs, one small/one large with 9 windows
and heavy notification wiring). Known, intentional limitations (all
documented in the header comment and in this file above): not
byte-for-byte faithful to the original's exact generated text (impossible
- MUIStrings[] table missing from provided source); notification
argument values are raw ints from the file with a hand-review comment,
not resolved to symbolic MUIA_*/MUIM_* names; cross-window notification
targets referencing a non-persistent object in a different window's
function will produce an out-of-scope reference (documented, not fixed -
uncommon case, would require porting the original's cross-function
parameter-passing scheme); catalog/.cd/locale, ARexx, icon-tooltype
parsing all out of scope for this pass.

Task #16 (port C code generator) is complete for this pass's scope.
