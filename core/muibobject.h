// MuiBuilderQt - Qt6/C++ port of MUIBuilder (originally by Eric Totel,
// 1990-2009, and the MUIBuilder Open Source Team, 2010-2011,
// GPLv3/LGPLv3, http://sourceforge.net/projects/muibuilder/).
//
// This header ports the object model from the original v2.3 C source
// (builder.h): one C struct per GUI object type (window1, group1,
// bouton1, chaine1, ...), all sharing a common prefix (id, label,
// father, generated, Help, notify, notifysource, muiobj).
//
// Design decision (stated here since this is a from-scratch port, not
// a mechanical 1:1 translation of the struct layout): rather than 24
// separate C++ classes mirroring each original struct, this port uses
// a single MuibObject class carrying an ObjType tag plus every
// type-specific field as a plain member (a "kitchen sink" object,
// exactly like the original's per-type structs laid side by side).
// This keeps the load/save logic a direct, easily-verified transliteration
// of load.c/save.c's switch(id)-based dispatch, at the cost of a wider
// class than an idiomatic polymorphic hierarchy would be. A cleaner
// subclass-per-type refactor is a reasonable follow-up once this data
// model + the file format round-trip are validated against real
// projects - not attempted in this first pass.
//
// Deliberately NOT ported (irrelevant to an offline load/save/codegen
// tool with no live GUI editor yet):
//   - muiobj (APTR to a live, running MUI/BOOPSI object)
//   - "groups"/"chain"/"muichain" runtime bookkeeping queues on window
//   - notifysource (populated at runtime by CreateChain()/LinkNotify()
//     as a reverse index of other objects' `notify` entries; never
//     read from or written to the .MUIB file directly - SaveNotify()
//     only ever walks the forward `notify` list)
//
// Object-type numbering (TY_* in the original builder.h) is preserved
// exactly, since it is baked into the .MUIB file format itself.

#pragma once

#include <QString>
#include <QByteArray>
#include <QStringList>
#include <QVector>
#include <memory>
#include <vector>

// NOTE: children/childs/windows below deliberately use std::vector, not
// QVector/QList, for owning std::unique_ptr<MuibObject> elements. Qt6's
// QList (which QVector now aliases) assumes its element type is
// copy-constructible for several of its own internals (its copy-on-write
// "detach" machinery reaches for a copy constructor even from otherwise
// read-only iteration in some cases) and fails to compile against a
// move-only type like unique_ptr. std::vector has no such assumption.

// MuiBuilderQt-only extension (see MuibObject::posMode below): the real
// original tool's window1 struct (builder.h) has NO position field at
// all, and code.c never emits MUIA_Window_LeftEdge/MUIA_Window_TopEdge
// for a generated window - confirmed by grepping the full v2.3 source
// tree (only muib_file.h's MB_MUIA_Window_LeftEdge/MB_MUIA_Window_TopEdge
// #defines exist, as leftover unused token IDs; no code path ever writes
// them). So "a window always opens centered" is not a MuiBuilderQt bug at
// all - it is the real original tool's own, permanent limitation (real
// MUI's own default open position with neither attribute set). This enum
// and the two fields below are a genuine new MuiBuilderQt-only feature,
// not a port of anything, using real MUI 5.0 SDK attributes/values
// (MUIA_Window_LeftEdge/TopEdge, "V4 isg LONG", and the real special
// values MUIV_Window_LeftEdge_Centered/_Moused and
// MUIV_Window_TopEdge_Centered/_Moused - confirmed in
// include/libraries/mui.h). See muicodegen.cpp's BuildXxxWindow() for the
// emission and core/muibqtextras.h for how this is persisted (a JSON
// sidecar, exactly like ObjType::AboutBox - the real .MUIB format has no
// room for it either).
enum class WindowPositionMode
{
    Centered = 0,   // default - emits nothing, matches the original's own always-centered behaviour byte-for-byte
    Moused   = 1,   // MUIV_Window_LeftEdge_Moused / MUIV_Window_TopEdge_Moused - opens under the mouse pointer
    Manual   = 2,   // literal posX/posY pixel coordinates
};

enum class ObjType
{
    Appli      = 0,   // TY_APPLI - not a tree node; see MuibProject
    Window     = 1,   // TY_WINDOW
    Group      = 2,   // TY_GROUP
    Button     = 3,   // TY_KEYBUTTON
    String     = 4,   // TY_STRING
    ListView   = 5,   // TY_LISTVIEW
    Gauge      = 6,   // TY_GAUGE
    Cycle      = 7,   // TY_CYCLE
    Radio      = 8,   // TY_RADIO
    Label      = 9,   // TY_LABEL
    Space      = 10,  // TY_SPACE
    Check      = 11,  // TY_CHECK
    Scale      = 12,  // TY_SCALE
    Image      = 13,  // TY_IMAGE
    Slider     = 14,  // TY_SLIDER
    Text       = 15,  // TY_TEXT
    Prop       = 16,  // TY_PROP
    DirList    = 17,  // TY_DIRLIST
    Rectangle  = 18,  // TY_RECTANGLE
    ColorField = 19,  // TY_COLORFIELD
    PopAsl     = 20,  // TY_POPASL
    PopObject  = 21,  // TY_POPOBJECT
    Menu       = 22,  // TY_MENU
    SubMenu    = 23,  // TY_SUBMENU
    MenuItem   = 24,  // TY_MENUITEM

    // NOT a TY_* type - the original MUIBuilder file format/tool has no
    // concept of this at all. A MuiBuilderQt-only extension (the
    // "Dialog-Creator > AboutBox" feature - see gui/aboutboxdialog.h):
    // wraps the real Aboutbox.mcc custom class (part of MUI 5.0 itself,
    // "AboutBox for AmigaOS", confirmed against the real MUI 5.0 SDK
    // headers/examples the user provided - Aboutbox_mcc.h/Aboutbox.c).
    // Deliberately given a numeric value far outside the real TY_*
    // range (0-24) so it can never collide with one; MuibLoader/
    // MuibSaver never read or write this type at all - a project's
    // AboutBox is persisted separately, in a MuiBuilderQt-only JSON
    // sidecar file (see core/muibqtextras.h), never in the .MUIB file
    // itself, so opening a MuiBuilderQt project in the real original
    // tool is completely unaffected by this feature's existence.
    AboutBox   = 1000,

    Unknown    = -1
};

// Ported from "area1" in builder.h - common layout/appearance
// attributes shared by most (not all - Space and the Appli/Window/
// Group/Menu "header" objects don't have one) widget types.
struct AreaAttrs
{
    bool Hide = false;
    bool Disable = false;
    bool InputMode = false;
    bool Phantom = false;
    int Weight = 0;
    unsigned int Background = 0;
    int Frame = 0;
    char key = 0;
    QString TitleFrame;
};

// Ported from "help1" - per-object context help text (AmigaGuide-ish
// help node), plus whether it has already been emitted into a
// generated .guide file ("generated").
struct HelpInfo
{
    QString title;
    int nb_char = 0;         // length of `content` in bytes (may contain embedded newlines)
    QByteArray content;
    bool generated = false;
};

// Ported from "event1" - one MUI notification wired from this object
// to a destination object. `targetLabel`/`targetTypeId` are exactly
// what ReadNotify()/SaveNotify() read and write (the destination
// object's label and TY_* type); resolving `targetLabel` to an actual
// MuibObject* (as the original's LinkNotify() does, post-load) is
// deferred to the code-generation stage - not needed for a faithful
// load/save round-trip, since SaveNotify() only ever re-emits the
// stored label, never re-derives it from a live pointer.
struct NotifyEvent
{
    QString targetLabel;      // evt->destination->label
    int targetTypeId = 0;     // evt->id_cible
    int srcType = 0;          // evt->src_type
    int destType = 0;         // evt->dest_type
    QString argString;        // evt->argstring
};

// A single GUI object node. Which of the type-specific fields below
// are meaningful is determined entirely by `type` - exactly as in the
// original, where the field layout is fixed per TY_* switch case.
class MuibObject
{
public:
    explicit MuibObject(ObjType t = ObjType::Unknown) : type(t) {}

    // --- common to every object (the shared struct prefix) ---
    ObjType type = ObjType::Unknown;
    QString label;
    bool generated = false;
    HelpInfo help;
    QVector<NotifyEvent> notify;
    MuibObject *father = nullptr;   // non-owning; nullptr for top-level windows/appli's appmenu

    // --- common layout attributes (most types; not Space/Window/Group's
    //     "root" bool aside, Menu family, or the Appli pseudo-object) ---
    AreaAttrs area;

    // --- Window (TY_WINDOW) ---
    QString title;               // also reused by Button/ListView/Slider/Check/Label/String
    std::unique_ptr<MuibObject> root;     // the window's root Group (embedded in the original, owned here)
    std::unique_ptr<MuibObject> menu;     // optional root Menu tree
    bool appwindow = false, backdrop = false, borderless = false;
    bool closegadget = false, depthgadget = false, dragbar = false, sizegadget = false;
    bool initopen = false, nomenu = false, needmouse = false;
    // MuiBuilderQt-only extension - see the WindowPositionMode enum
    // comment above. NOT read/written by MuibLoader/MuibSaver (the real
    // .MUIB format has no field for this) - persisted via
    // core/muibqtextras.h's JSON sidecar instead, keyed by this window's
    // own `label`, exactly like ObjType::AboutBox.
    WindowPositionMode posMode = WindowPositionMode::Centered;
    int posX = 0, posY = 0;
    // MuiBuilderQt-only extension, same story as posMode/posX/posY just
    // above (a user-reported gap: "das Fenster bleibt initial sehr
    // klein" - confirmed against the real source that window1/code.c
    // have no size fields at all either, so MUI's own automatic
    // content-driven sizing has always been the only thing setting a
    // window's initial/min/max size). 0 = "not set" (the natural sentinel
    // - a genuine 0-pixel dimension is never useful), matching this
    // field's own default so an untouched project stays byte-identical.
    // winWidth/winHeight are real, WINDOW-specific attributes
    // (MUIA_Window_Width/Height, confirmed in the real MUI 5.0 SDK's
    // include/libraries/mui.h) and go straight on the WindowObject like
    // posX/posY; winMinWidth/winMinHeight/winMaxWidth/winMaxHeight are
    // real but GENERIC Area-class attributes (MUIA_MinWidth/MUIA_
    // MinHeight/MUIA_MaxWidth/MUIA_MaxHeight, confirmed in the same
    // header under the Area class's own attribute block, not a
    // "MUIA_Window_"-prefixed one) - muicodegen.cpp therefore emits them
    // on the window's root CONTENT object instead, the standard MUI
    // idiom for constraining a window's resize range (see
    // MuiCodeGen::emitObject()'s own doc comment for the full
    // rationale). Persisted via core/muibqtextras.h's JSON sidecar, like
    // posMode/posX/posY.
    int winWidth = 0, winHeight = 0;
    int winMinWidth = 0, winMinHeight = 0, winMaxWidth = 0, winMaxHeight = 0;

    // --- Group (TY_GROUP) ---
    bool isRoot = false;         // group1.root - "this is a window's root group"
    bool horizontal = false;     // also used by Gauge/Slider
    bool registermode = false, sameheight = false, samesize = false, samewidth = false;
    bool isVirtual = false;      // group1.virtual ('virtual' is a C++ keyword)
    bool rows = false, columns = false;
    bool horizspacing = false, vertspacing = false;
    int number = 0, horizspace = 0, vertspace = 0;
    QStringList entries;         // register titles (Group/Cycle/Radio all reuse this name)
    std::vector<std::unique_ptr<MuibObject>> children; // group1.child

    // --- Text (TY_TEXT) ---
    QString preparse;
    bool textMax = false, textMin = false;  // texte1.max/min
    QString content;             // also ListView format string content

    // --- Scale (TY_SCALE) ---
    bool scaleHoriz = false;     // scale1.horiz (deliberately distinct from `horizontal`)

    // --- Slider (TY_SLIDER) ---
    bool title_exist = false;    // also Check/String
    int sliderMax = 0, sliderMin = 0;
    bool quiet = false;
    int init = 0;
    bool reverse = false;

    // --- Gauge (TY_GAUGE) ---
    int divide = 0;
    int gaugeMax = 0;
    bool fixheight = false, fixwidth = false; // also Image/Rectangle/ColorField
    int height = 0, width = 0;                // also Image/Rectangle/ColorField
    QString infotext;

    // --- Prop (TY_PROP) ---
    int propEntries = 0, first = 0, visible = 0;

    // --- ListView (TY_LISTVIEW) ---
    int lvType = 0;
    bool multiselect = false, doubleclick = false;
    int select = 0;
    bool adjustheight = false, adjustwidth = false, inputmode = false;
    QString comparehook, constructhook, displayhook, multitesthook, destructhook;
    QString format;

    // --- DirList (TY_DIRLIST) ---
    bool drawers = false, files = false, filter = false, multi = false;
    bool icons = false, highlow = false;
    int sorttype = 0, sortdirs = 0;
    QString directory, accept, reject, filterhook;

    // --- Cycle/Radio (TY_CYCLE/TY_RADIO) share `entries` above.

    // --- String (TY_STRING) ---
    int format_ = 0;    // chaine1.format (avoid clashing with ListView's `format`)
    bool integer = false;
    int maxlen = 0;
    bool secret = false;

    // --- Check (TY_CHECK) ---
    bool init_state = false;

    // --- Image (TY_IMAGE) ---
    int imgType = 0;
    bool freevert = false, freehoriz = false;
    QString spec;

    // --- Rectangle (TY_RECTANGLE) ---
    int rectType = 0;

    // --- ColorField (TY_COLORFIELD) ---
    unsigned int red = 0, green = 0, blue = 0;

    // --- PopAsl (TY_POPASL) ---
    QString starthook, stophook;
    int popType = 0, popImage = 0;

    // --- PopObject (TY_POPOBJECT) ---
    QString openhook, closehook;
    std::unique_ptr<MuibObject> popObj;  // the popped-up object
    int follow = 0, light = 0, isVolatile = 0;   // "Volatile" is close enough to a keyword to avoid too

    // --- Menu family (TY_MENU/TY_SUBMENU/TY_MENUITEM) ---
    bool menu_enable = false, check_enable = false, check_state = false, toggleMenu = false;
    char menuKey = 0;
    QString name;         // menu1.name (distinct from `label`, which is the internal object label)
    std::vector<std::unique_ptr<MuibObject>> childs;  // menu1.childs (Menu/SubMenu only)
    // MuiBuilderQt-only extension (only meaningful on a check_enable leaf
    // MenuItem - mirrors Check/Toggle/Checked's own scope): 0 = no
    // mutual-exclude group; a positive number groups this item together
    // with every SIBLING MenuItem under the same immediate parent sharing
    // the same number, so checking one automatically unchecks the others
    // (like a set of radio buttons inside a menu). NOT the real MUI
    // attribute MUIA_Menuitem_Exclude (V8, isg LONG - confirmed in
    // include/libraries/mui.h) - that attribute's exact bit-per-sibling
    // convention is not documented anywhere in the available real MUI SDK
    // material, and this project never invents unverified behaviour (see
    // README). Instead, muicodegen.cpp emits this as explicit, fully
    // documented MUIM_Notify/MUIM_Set wiring between the group's siblings
    // themselves (MUIA_Menuitem_Checked TRUE on one -> MUIM_Set
    // MUIA_Menuitem_Checked FALSE on every other member) - real,
    // unambiguous MUI attributes only. Like posMode/posX/posY and
    // aboutLinkedMenuItem, this has no home in the real .MUIB format at
    // all and is persisted via core/muibqtextras.h's JSON sidecar,
    // keyed by this item's own `label`.
    int excludeGroup = 0;

    // --- Space (TY_SPACE) --- has no AreaAttrs in the original.
    int spaceType = 0, spacing = 0;

    // --- AboutBox (MuiBuilderQt-only, ObjType::AboutBox - see its own
    //     enum comment) - mirrors Aboutbox.mcc's real attributes exactly
    //     (Aboutbox_mcc.h): Credits/Build/LogoFile/URL/URLText are all
    //     plain CONST_STRPTR: STRING attributes, so a QString field each,
    //     left empty to mean "attribute not emitted" (Aboutbox.mcc treats
    //     each of these as independently optional). `aboutLinkedMenuItem`
    //     is NOT a real MUI/Aboutbox.mcc concept - it is this object's
    //     own `label` naming which MenuItem elsewhere in the SAME project
    //     (in some window's menu) should open this AboutBox when clicked
    //     (wired via the real MUIA_Menuitem_Trigger notification - see
    //     muicodegen.cpp's BuildApplication() emission); empty means
    //     "not linked to any menu item yet".
    QString aboutCredits, aboutBuild, aboutLogoFile, aboutUrl, aboutUrlText;
    QString aboutLinkedMenuItem;
};

// Ported from "appli1" + the top-of-file fields LoadFile()/SaveApplication()
// read directly (genfile/catfile/nb_*/code/env/declarations/local/
// notifications/generate_all/real_getstring/catprepend). This is the
// project-level container, not itself part of the object tree.
class MuibProject
{
public:
    QString genfile;      // path to generated .c/.h base name
    QString catfile;      // path to catalog/.cd translation file

    // Bookkeeping counters as literally stored in the file. Kept for
    // faithful round-tripping; a fresh save recomputes and could
    // replace these, but they are not currently recomputed - see
    // MuibSaver.
    int nbWindow = 0, nbListview = 0, nbGroup = 0, nbButton = 0, nbString = 0;
    int nbGauge = 0, nbCycle = 0, nbRadio = 0, nbCheck = 0, nbImage = 0;
    int nbSlider = 0, nbText = 0, nbProp = 0, nbRectangle = 0, nbColorfield = 0;
    int nbPopasl = 0, nbPopobject = 0, nbMenu = 0, nbSpace = 0, nbScale = 0;
    int nbBarlabel = 0, nbLabel = 0;

    // Code generation options (config.c / code.c globals in the original).
    bool optCode = true, optEnv = true, optDeclarations = true;
    bool optLocal = false, optNotifications = true, optGenerateAll = false;

    QString base, author, title, version, copyright, description, helpfile;
    QString realGetString;
    QString catPrepend = QStringLiteral("MSG_");
    HelpInfo help;
    QStringList idents, functions, variables;
    QVector<NotifyEvent> notify;
    std::unique_ptr<MuibObject> appMenu;   // application-level menu strip (may be null)

    std::vector<std::unique_ptr<MuibObject>> windows;

    // MuiBuilderQt-only extension (ObjType::AboutBox - see its own enum
    // comment): at most one AboutBox per project, created/edited via
    // Datei > Dialog-Creator > AboutBox. NEVER read or written by
    // MuibLoader/MuibSaver - persisted separately via core/muibqtextras.h
    // in a JSON sidecar file next to the .MUIB, so it round-trips only
    // through MuiBuilderQt itself, never through the real original tool
    // or its .MUIB format.
    std::unique_ptr<MuibObject> aboutBox;

    // File format version this project was loaded from (0 if newly
    // created / not yet loaded). Saving always writes the current
    // version (126) regardless, matching the original's SaveApplication().
    int loadedVersion = 0;
};
