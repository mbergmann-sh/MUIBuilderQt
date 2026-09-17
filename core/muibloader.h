// Ports load.c (+ the LoadWindow/LoadGroup/LoadImage/LoadListview/
// LoadDirList/LoadText/LoadCycle/LoadRadio/LoadPopAsl/LoadMenu functions
// scattered across window.c/group.c/image.c/listview.c/dirlist.c/text.c/
// cycle.c/radio.c/popasl.c/menu.c) from MUIBuilder v2.3.
//
// Faithfully reproduces every version-conditional branch found in the
// original for file format versions 100 ("BUILDER_SAVE_FILE", the
// oldest) through 126 (current, "BUILDER_SAVE_FILE1.26") - this matters
// in practice: every example .MUIB project shipped with the original
// source (BuilderSave/*.MUIB) is itself an older version (1.03 through
// 1.21), so a loader that only understood the latest format could not
// read any of them.
#pragma once

#include "muibobject.h"
#include <QString>
#include <memory>

class ByteCursor;

class MuibLoader
{
public:
    // Returns nullptr and sets *errorOut on failure (file not openable,
    // or first line isn't a recognized "BUILDER_SAVE_FILE..." version tag -
    // ports SearchVersion() returning 0 / ErrorMessage(MSG_NotBuilderSaveFile)).
    // Instance method (not static): pre-1.15 files auto-name unlabeled
    // Scale/Space objects via a running counter that is a *global* in
    // the original (nb_scale/nb_space) - scoped to one loader instance
    // here instead, so concurrent/repeated loads never share state.
    std::unique_ptr<MuibProject> loadFile(const QString &path, QString *errorOut = nullptr);

private:
    static int searchVersion(const QString &tag);   // ports SearchVersion()

    // Common per-object helpers (ports ReadHelp/ReadNotify/ReadArea).
    static void readHelp(ByteCursor &c, HelpInfo &help, bool ownerGenerated, int version);
    static void readNotify(ByteCursor &c, QVector<NotifyEvent> &notify);
    static void readArea(ByteCursor &c, AreaAttrs &area);
    static QStringList readEntriesUntilSentinel(ByteCursor &c); // ports the "//END_ENTRIES//"-terminated loops (cycle/radio/group entries, ReadApplicationList)
    static QString readFunctionHook(ByteCursor &c);  // ports ReadFunctionHook (simplified: no PointerOnString interning)

    // ports LoadObject()'s big switch, dispatching to the per-type loaders below
    std::unique_ptr<MuibObject> loadObject(ByteCursor &c, MuibObject *father, int objTypeId, int version);

    std::unique_ptr<MuibObject> loadWindow(ByteCursor &c, int version);
    std::unique_ptr<MuibObject> loadGroup(ByteCursor &c, MuibObject *father, int version);
    std::unique_ptr<MuibObject> loadImage(ByteCursor &c, MuibObject *father, int version);
    std::unique_ptr<MuibObject> loadListview(ByteCursor &c, MuibObject *father, int version);
    std::unique_ptr<MuibObject> loadDirList(ByteCursor &c, MuibObject *father, int version);
    std::unique_ptr<MuibObject> loadText(ByteCursor &c, MuibObject *father, int version);
    std::unique_ptr<MuibObject> loadCycle(ByteCursor &c, MuibObject *father, int version);
    std::unique_ptr<MuibObject> loadRadio(ByteCursor &c, MuibObject *father, int version);
    std::unique_ptr<MuibObject> loadPopAsl(ByteCursor &c, MuibObject *father, int version);
    std::unique_ptr<MuibObject> loadPopObject(ByteCursor &c, MuibObject *father, int version);
    // The original's LoadMenu() takes a "UNUSED int version" parameter:
    // menu.c never uses it directly because ReadHelp()/ReadNotify() (both
    // defined in load.c) read a file-scope *global* `int version` set once
    // by LoadFile() instead of a threaded parameter. This port has no
    // global - `version` is passed explicitly everywhere instead - so
    // loadMenu takes and forwards a real version parameter where the
    // original silently relied on the global.
    std::unique_ptr<MuibObject> loadMenu(ByteCursor &c, MuibObject *father, int typeId, int version);

    // The types whose Load* body is inlined directly in load.c's
    // LoadObject() switch rather than delegated to another file:
    // button/gauge/check/scale/prop/slider/string/label/space/rectangle/colorfield.
    std::unique_ptr<MuibObject> loadButton(ByteCursor &c, MuibObject *father, int version);
    std::unique_ptr<MuibObject> loadGauge(ByteCursor &c, MuibObject *father, int version);
    std::unique_ptr<MuibObject> loadCheck(ByteCursor &c, MuibObject *father, int version);
    std::unique_ptr<MuibObject> loadScale(ByteCursor &c, MuibObject *father, int version);
    std::unique_ptr<MuibObject> loadProp(ByteCursor &c, MuibObject *father, int version);
    std::unique_ptr<MuibObject> loadSlider(ByteCursor &c, MuibObject *father, int version);
    std::unique_ptr<MuibObject> loadString(ByteCursor &c, MuibObject *father, int version);
    std::unique_ptr<MuibObject> loadLabel(ByteCursor &c, MuibObject *father, int version);
    std::unique_ptr<MuibObject> loadSpace(ByteCursor &c, MuibObject *father, int version);
    std::unique_ptr<MuibObject> loadRectangle(ByteCursor &c, MuibObject *father, int version);
    std::unique_ptr<MuibObject> loadColorField(ByteCursor &c, MuibObject *father, int version);

    int m_nbScaleCounter = 0;
    int m_nbSpaceCounter = 0;
};
