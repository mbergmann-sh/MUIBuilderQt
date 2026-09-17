// MuiBuilderQt-only project state that has no home in the real .MUIB file
// format at all (the original MUIBuilder tool's format/tool has no concept
// of it). Four features currently live here: the AboutBox feature
// (ObjType::AboutBox - see muibobject.h's own comment on it), each
// Window's posMode/posX/posY plus winWidth/winHeight/winMinWidth/
// winMinHeight/winMaxWidth/winMaxHeight (see their own comments right
// next to the WindowPositionMode enum in muibobject.h), and each
// check_enable MenuItem's mutual-exclude excludeGroup (see its own
// comment in muibobject.h, right below the Menu-family fields) - all
// persisted as a small JSON sidecar file next to the .MUIB, so they
// survive a save/reload cycle in MuiBuilderQt without touching the
// .MUIB format's own byte-level structure/versioning at all. A project
// with no AboutBox, every window left at every one of these defaults
// (WindowPositionMode::Centered, every size field 0), and no MenuItem in
// any exclude group has no sidecar file at all (nothing is written, and
// a missing/absent file loads as "no extras" rather than an error) - so
// an untouched project's generated code is byte-identical to before
// these features existed.
//
// Deliberately a separate small file, not folded into MuibLoader/
// MuibSaver: those two classes are a careful, faithful port of the
// original's own load.c/save.c binary format and versioning logic - this
// is a completely different, MuiBuilderQt-invented format for
// MuiBuilderQt-invented data, and keeping the two apart means the real
// port's own correctness is never put at risk by future MuiBuilderQt-only
// features like this one.
#pragma once

#include "muibobject.h"
#include <QString>

class MuibQtExtras
{
public:
    // Sidecar path for a given .MUIB path - always "<muibPath>.mbqtextras.json",
    // regardless of the .MUIB file's own extension casing.
    static QString sidecarPathFor(const QString &muibPath);

    // Reads the sidecar for `muibPath` (if any) and populates proj.aboutBox,
    // every window's posMode/posX/posY/winWidth/winHeight/winMinWidth/
    // winMinHeight/winMaxWidth/winMaxHeight, and every MenuItem's
    // excludeGroup accordingly (leaves aboutBox null / windows at every
    // default (WindowPositionMode::Centered, every size field 0) / every
    // MenuItem's excludeGroup at 0 if the sidecar is absent, empty, or
    // has no matching entry - never treated as an error, since most
    // projects simply won't have any of this). Window and MenuItem
    // entries are matched by label (an entry for a label that no longer
    // exists in proj is silently ignored - e.g. the window/item was
    // since deleted; MenuItem labels are searched across both
    // proj.appMenu and every window's own menu). Returns true unless the
    // file exists but could not be parsed as JSON at all (a corrupt
    // sidecar - proj is left unchanged in that case; *errorOut, if
    // given, is set to a short description).
    static bool load(MuibProject &proj, const QString &muibPath, QString *errorOut = nullptr);

    // Writes (or, if there is nothing left to persist - no aboutBox,
    // every window still at every default (WindowPositionMode::Centered,
    // every size field 0), and no MenuItem in an exclude group -
    // removes) the sidecar for `muibPath`. Returns false and sets
    // *errorOut only on a real I/O failure (e.g. directory not
    // writable) - removing an already-absent sidecar is not an error.
    static bool save(const MuibProject &proj, const QString &muibPath, QString *errorOut = nullptr);
};
