// Ports save.c (+ the SaveWindow/SaveGroup/SaveImage/SaveListview/
// SaveDirList/SaveText/SaveCycle/SaveRadio/SavePopAsl/SaveMenu functions)
// from MUIBuilder v2.3.
//
// Unlike the loader, the writer needs no version-conditional logic at
// all: SaveApplication() always emits the current format
// ("BUILDER_SAVE_FILE1.26") with every field, regardless of what
// version the in-memory project happened to be loaded from. A
// load-then-immediately-save round trip therefore *upgrades* an old
// file to the current format - exactly what the original tool does
// too (there is no "save as version X" option in MUIBuilder).
#pragma once

#include "muibobject.h"
#include <QString>

class QIODevice;

class MuibSaver
{
public:
    // Returns false and sets *errorOut on failure (file not writable).
    static bool saveFile(const MuibProject &proj, const QString &path, QString *errorOut = nullptr);

private:
    static void writeLine(QIODevice &f, const QString &s);   // fprintf(f, "%s\n", s)
    static void writeInt(QIODevice &f, int v);                // fprintf(f, "%d\n", v)
    static void writeChar(QIODevice &f, char c);               // fprintf(f, "%c\n", c)
    static void writeRawBytes(QIODevice &f, const QByteArray &bytes); // the un-terminated help-content blob

    static void saveNotify(QIODevice &f, const QVector<NotifyEvent> &notify);
    static void saveArea(QIODevice &f, const AreaAttrs &area);
    static void saveStringList(QIODevice &f, const QStringList &list); // ports SaveApplicationList + the cycle/radio/group entries loops

    // ports SaveObject()'s common prefix + switch
    static void saveObject(QIODevice &f, const MuibObject &obj);

    static void saveWindow(QIODevice &f, const MuibObject &win);
    static void saveGroup(QIODevice &f, const MuibObject &grp);
    static void saveImage(QIODevice &f, const MuibObject &img);
    static void saveListview(QIODevice &f, const MuibObject &lv);
    static void saveDirList(QIODevice &f, const MuibObject &dl);
    static void saveText(QIODevice &f, const MuibObject &txt);
    static void saveCycle(QIODevice &f, const MuibObject &cy);
    static void saveRadio(QIODevice &f, const MuibObject &ra);
    static void savePopAsl(QIODevice &f, const MuibObject &pa);
    static void saveMenu(QIODevice &f, const MuibObject &m);
};
