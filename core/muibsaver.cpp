#include "muibsaver.h"
#include <QFile>
#include <QByteArray>

static const char *SAVE_VERSION_TAG = "BUILDER_SAVE_FILE1.26";

void MuibSaver::writeLine(QIODevice &f, const QString &s)
{
    f.write(s.toLatin1());
    f.write("\n", 1);
}

void MuibSaver::writeInt(QIODevice &f, int v)
{
    f.write(QByteArray::number(v));
    f.write("\n", 1);
}

void MuibSaver::writeChar(QIODevice &f, char c)
{
    f.write(&c, 1);
    f.write("\n", 1);
}

void MuibSaver::writeRawBytes(QIODevice &f, const QByteArray &bytes)
{
    f.write(bytes);
}

void MuibSaver::saveNotify(QIODevice &f, const QVector<NotifyEvent> &notify)
{
    for (const NotifyEvent &evt : notify)
    {
        writeLine(f, evt.targetLabel);
        writeInt(f, evt.targetTypeId);
        writeInt(f, evt.srcType);
        writeInt(f, evt.destType);
        writeLine(f, evt.argString);
    }
    writeLine(f, QStringLiteral("//END_ENTRIES//"));
}

void MuibSaver::saveArea(QIODevice &f, const AreaAttrs &area)
{
    writeInt(f, area.Hide ? 1 : 0);
    writeInt(f, area.Disable ? 1 : 0);
    writeInt(f, area.InputMode ? 1 : 0);
    writeInt(f, area.Phantom ? 1 : 0);
    writeInt(f, area.Weight);
    writeInt(f, static_cast<int>(area.Background));
    writeInt(f, area.Frame);
    writeChar(f, area.key);
    writeLine(f, area.TitleFrame);
}

void MuibSaver::saveStringList(QIODevice &f, const QStringList &list)
{
    for (const QString &s : list)
        writeLine(f, s);
    writeLine(f, QStringLiteral("//END_ENTRIES//"));
}

bool MuibSaver::saveFile(const MuibProject &proj, const QString &path, QString *errorOut)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
    {
        if (errorOut)
            *errorOut = QStringLiteral("Could not open file for writing: %1").arg(path);
        return false;
    }

    writeLine(file, QString::fromLatin1(SAVE_VERSION_TAG));
    writeLine(file, proj.genfile);
    writeLine(file, proj.catfile);
    writeInt(file, proj.nbWindow);
    writeInt(file, proj.nbListview);
    writeInt(file, proj.nbGroup);
    writeInt(file, proj.nbButton);
    writeInt(file, proj.nbString);
    writeInt(file, proj.nbGauge);
    writeInt(file, proj.nbCycle);
    writeInt(file, proj.nbRadio);
    writeInt(file, proj.nbCheck);
    writeInt(file, proj.nbImage);
    writeInt(file, proj.nbSlider);
    writeInt(file, proj.nbText);
    writeInt(file, proj.nbProp);
    writeInt(file, proj.nbRectangle);
    writeInt(file, proj.nbColorfield);
    writeInt(file, proj.nbPopasl);
    writeInt(file, proj.nbPopobject);
    writeInt(file, proj.nbMenu);
    writeInt(file, proj.nbSpace);
    writeInt(file, proj.nbScale);
    writeInt(file, proj.nbBarlabel);
    writeInt(file, proj.nbLabel);
    writeInt(file, proj.optCode ? 1 : 0);
    writeInt(file, proj.optEnv ? 1 : 0);
    writeInt(file, proj.optDeclarations ? 1 : 0);
    writeInt(file, proj.optLocal ? 1 : 0);
    writeInt(file, proj.optNotifications ? 1 : 0);
    writeInt(file, proj.optGenerateAll ? 1 : 0);
    writeLine(file, proj.base);
    writeLine(file, proj.author);
    writeLine(file, proj.title);
    writeLine(file, proj.version);
    writeLine(file, proj.copyright);
    writeLine(file, proj.description);
    writeLine(file, proj.helpfile);
    writeLine(file, proj.realGetString);
    writeLine(file, proj.catPrepend);
    writeInt(file, proj.help.nb_char);
    writeInt(file, proj.help.generated ? 1 : 0);
    if (proj.help.nb_char > 0)
        writeRawBytes(file, proj.help.content);
    saveStringList(file, proj.idents);
    saveStringList(file, proj.functions);
    saveStringList(file, proj.variables);
    saveNotify(file, proj.notify);
    if (proj.appMenu)
        saveObject(file, *proj.appMenu);
    else
    {
        // SaveApplication() in the original always writes application.appmenu
        // unconditionally, with no null check - because in the live app, a
        // project's appmenu is never NULL (an empty default menu strip is
        // created for every application, not just ones edited in a version
        // that happened to load/save it). A project we loaded from a
        // pre-1.17 file never had this section in its source file, so
        // `proj.appMenu` is null here; synthesize an empty Menu object so
        // the byte stream still carries exactly one menu-typeId+object at
        // this point, matching what any version>=117 reader (which is what
        // every save produces, since we always write the current version)
        // unconditionally expects to find.
        MuibObject empty(ObjType::Menu);
        saveObject(file, empty);
    }
    for (const auto &win : proj.windows)
        saveObject(file, *win);
    writeInt(file, -1);

    file.close();
    return true;
}

void MuibSaver::saveObject(QIODevice &f, const MuibObject &obj)
{
    writeInt(f, static_cast<int>(obj.type));
    writeLine(f, obj.label);
    writeInt(f, obj.generated ? 1 : 0);
    writeLine(f, obj.help.title);
    writeInt(f, obj.help.nb_char);
    writeInt(f, obj.help.generated ? 1 : 0);
    if (obj.help.nb_char > 0)
        writeRawBytes(f, obj.help.content);
    saveNotify(f, obj.notify);

    switch (obj.type)
    {
        case ObjType::Window:     saveWindow(f, obj); break;
        case ObjType::Group:      saveGroup(f, obj); break;
        case ObjType::Button:
            writeLine(f, obj.title);
            saveArea(f, obj.area);
            break;
        case ObjType::Text:       saveText(f, obj); break;
        case ObjType::Scale:
            saveArea(f, obj.area);
            writeInt(f, obj.scaleHoriz ? 1 : 0);
            break;
        case ObjType::Slider:
            saveArea(f, obj.area);
            writeLine(f, obj.title);
            writeInt(f, obj.title_exist ? 1 : 0);
            writeInt(f, obj.sliderMax);
            writeInt(f, obj.sliderMin);
            writeInt(f, obj.quiet ? 1 : 0);
            writeInt(f, obj.init);
            writeInt(f, obj.reverse ? 1 : 0);
            writeInt(f, obj.horizontal ? 1 : 0);
            break;
        case ObjType::Gauge:
            saveArea(f, obj.area);
            writeInt(f, obj.divide);
            writeInt(f, obj.horizontal ? 1 : 0);
            writeInt(f, obj.gaugeMax);
            writeInt(f, obj.fixheight ? 1 : 0);
            writeInt(f, obj.fixwidth ? 1 : 0);
            writeInt(f, obj.height);
            writeInt(f, obj.width);
            writeLine(f, obj.infotext);
            break;
        case ObjType::Prop:
            saveArea(f, obj.area);
            writeInt(f, obj.horizontal ? 1 : 0);
            writeInt(f, obj.propEntries);
            writeInt(f, obj.first);
            writeInt(f, obj.visible);
            writeInt(f, obj.fixheight ? 1 : 0);
            writeInt(f, obj.fixwidth ? 1 : 0);
            writeInt(f, obj.height);
            writeInt(f, obj.width);
            break;
        case ObjType::ListView:   saveListview(f, obj); break;
        case ObjType::DirList:    saveDirList(f, obj); break;
        case ObjType::Cycle:      saveCycle(f, obj); break;
        case ObjType::Radio:      saveRadio(f, obj); break;
        case ObjType::String:
            saveArea(f, obj.area);
            writeLine(f, obj.title);
            writeLine(f, obj.content);
            writeInt(f, obj.title_exist ? 1 : 0);
            writeLine(f, obj.accept);
            writeLine(f, obj.reject);
            writeInt(f, obj.format_);
            writeInt(f, obj.integer ? 1 : 0);
            writeInt(f, obj.maxlen);
            writeInt(f, obj.secret ? 1 : 0);
            break;
        case ObjType::Check:
            saveArea(f, obj.area);
            writeLine(f, obj.title);
            writeInt(f, obj.title_exist ? 1 : 0);
            writeInt(f, obj.init_state ? 1 : 0);
            break;
        case ObjType::Label:
            saveArea(f, obj.area);
            writeLine(f, obj.title);
            break;
        case ObjType::Image:      saveImage(f, obj); break;
        case ObjType::Rectangle:
            saveArea(f, obj.area);
            writeInt(f, obj.rectType);
            writeInt(f, obj.fixheight ? 1 : 0);
            writeInt(f, obj.fixwidth ? 1 : 0);
            writeInt(f, obj.height);
            writeInt(f, obj.width);
            break;
        case ObjType::ColorField:
            saveArea(f, obj.area);
            writeInt(f, obj.fixheight ? 1 : 0);
            writeInt(f, obj.fixwidth ? 1 : 0);
            writeInt(f, obj.height);
            writeInt(f, obj.width);
            writeInt(f, static_cast<int>(obj.red));
            writeInt(f, static_cast<int>(obj.green));
            writeInt(f, static_cast<int>(obj.blue));
            break;
        case ObjType::PopAsl:     savePopAsl(f, obj); break;
        case ObjType::PopObject:
            saveArea(f, obj.area);
            writeLine(f, obj.openhook);
            writeLine(f, obj.closehook);
            if (obj.popObj)
                saveObject(f, *obj.popObj);
            writeInt(f, obj.popImage);
            writeInt(f, obj.follow);
            writeInt(f, obj.light);
            writeInt(f, obj.isVolatile);
            break;
        case ObjType::Menu:
        case ObjType::SubMenu:
        case ObjType::MenuItem:
            saveMenu(f, obj);
            break;
        case ObjType::Space:
            writeInt(f, obj.spaceType);
            writeInt(f, obj.spacing);
            break;
        default:
            break;
    }
}

void MuibSaver::saveWindow(QIODevice &f, const MuibObject &win)
{
    writeLine(f, win.title);
    writeInt(f, win.appwindow ? 1 : 0);
    writeInt(f, win.backdrop ? 1 : 0);
    writeInt(f, win.borderless ? 1 : 0);
    writeInt(f, win.closegadget ? 1 : 0);
    writeInt(f, win.depthgadget ? 1 : 0);
    writeInt(f, win.dragbar ? 1 : 0);
    writeInt(f, win.sizegadget ? 1 : 0);
    writeInt(f, win.initopen ? 1 : 0);
    writeInt(f, win.nomenu ? 1 : 0);
    writeInt(f, win.needmouse ? 1 : 0);
    if (win.menu)
        saveObject(f, *win.menu);
    else
    {
        // Same reasoning as the appMenu case in saveFile(): the original
        // SaveWindow() always writes win_aux->menu unconditionally (every
        // window gets a default, possibly-empty, local menu in the live
        // app). A window loaded from a pre-1.14 file never had this
        // section, leaving win.menu null here; synthesize an empty Menu
        // object rather than skipping the write, so a version>=114 reader
        // (what every save now targets) stays byte-aligned.
        MuibObject empty(ObjType::Menu);
        saveObject(f, empty);
    }
    if (win.root)
        saveObject(f, *win.root);
}

void MuibSaver::saveGroup(QIODevice &f, const MuibObject &grp)
{
    saveArea(f, grp.area);
    writeInt(f, grp.isRoot ? 1 : 0);
    writeInt(f, grp.horizontal ? 1 : 0);
    writeInt(f, grp.registermode ? 1 : 0);
    writeInt(f, grp.sameheight ? 1 : 0);
    writeInt(f, grp.samesize ? 1 : 0);
    writeInt(f, grp.samewidth ? 1 : 0);
    writeInt(f, grp.isVirtual ? 1 : 0);
    writeInt(f, grp.rows ? 1 : 0);
    writeInt(f, grp.horizspacing ? 1 : 0);
    writeInt(f, grp.vertspacing ? 1 : 0);
    writeInt(f, grp.columns ? 1 : 0);
    writeInt(f, grp.number);
    writeInt(f, grp.horizspace);
    writeInt(f, grp.vertspace);
    saveStringList(f, grp.entries);
    for (const auto &child : grp.children)
        saveObject(f, *child);
    writeInt(f, -1);
}

void MuibSaver::saveImage(QIODevice &f, const MuibObject &img)
{
    saveArea(f, img.area);
    writeInt(f, img.imgType);
    writeInt(f, img.freevert ? 1 : 0);
    writeInt(f, img.freehoriz ? 1 : 0);
    writeInt(f, img.fixheight ? 1 : 0);
    writeInt(f, img.fixwidth ? 1 : 0);
    writeInt(f, img.height);
    writeInt(f, img.width);
    writeLine(f, img.spec);
}

void MuibSaver::saveListview(QIODevice &f, const MuibObject &lv)
{
    saveArea(f, lv.area);
    writeInt(f, lv.lvType);
    writeInt(f, lv.multiselect ? 1 : 0);
    writeInt(f, lv.doubleclick ? 1 : 0);
    writeInt(f, lv.select);
    writeInt(f, lv.adjustheight ? 1 : 0);
    writeInt(f, lv.adjustwidth ? 1 : 0);
    writeInt(f, lv.inputmode ? 1 : 0);
    writeLine(f, lv.comparehook);
    writeLine(f, lv.constructhook);
    writeLine(f, lv.displayhook);
    writeLine(f, lv.multitesthook);
    writeLine(f, lv.destructhook);
    writeLine(f, lv.format);
    writeLine(f, lv.title);
    writeLine(f, lv.content);
}

void MuibSaver::saveDirList(QIODevice &f, const MuibObject &dl)
{
    saveArea(f, dl.area);
    writeInt(f, dl.drawers ? 1 : 0);
    writeInt(f, dl.files ? 1 : 0);
    writeInt(f, dl.filter ? 1 : 0);
    writeInt(f, dl.multi ? 1 : 0);
    writeInt(f, dl.icons ? 1 : 0);
    writeInt(f, dl.highlow ? 1 : 0);
    writeInt(f, dl.sorttype);
    writeLine(f, dl.directory);
    writeLine(f, dl.accept);
    writeLine(f, dl.reject);
    writeInt(f, dl.sortdirs);
    writeLine(f, dl.filterhook);
}

void MuibSaver::saveText(QIODevice &f, const MuibObject &txt)
{
    saveArea(f, txt.area);
    writeLine(f, txt.preparse);
    writeInt(f, txt.textMax ? 1 : 0);
    writeInt(f, txt.textMin ? 1 : 0);
    writeLine(f, txt.content);
}

void MuibSaver::saveCycle(QIODevice &f, const MuibObject &cy)
{
    saveArea(f, cy.area);
    saveStringList(f, cy.entries);
}

void MuibSaver::saveRadio(QIODevice &f, const MuibObject &ra)
{
    saveArea(f, ra.area);
    saveStringList(f, ra.entries);
}

void MuibSaver::savePopAsl(QIODevice &f, const MuibObject &pa)
{
    saveArea(f, pa.area);
    writeLine(f, pa.starthook);
    writeLine(f, pa.stophook);
    writeInt(f, pa.popType);
    writeInt(f, pa.popImage);
}

void MuibSaver::saveMenu(QIODevice &f, const MuibObject &m)
{
    writeInt(f, m.menu_enable ? 1 : 0);
    writeInt(f, m.check_enable ? 1 : 0);
    writeInt(f, m.check_state ? 1 : 0);
    writeInt(f, m.toggleMenu ? 1 : 0);
    writeChar(f, m.menuKey);
    writeLine(f, m.name);
    if (m.type == ObjType::Menu || m.type == ObjType::SubMenu)
    {
        for (const auto &child : m.childs)
            saveObject(f, *child);
        writeInt(f, -1);
    }
}
