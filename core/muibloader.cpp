#include "muibloader.h"
#include "bytecursor.h"

#include <QFile>

// Ports SearchVersion() from load.c exactly (same version numbers,
// same string tags).
int MuibLoader::searchVersion(const QString &tag)
{
    static const struct { const char *text; int version; } table[] = {
        { "BUILDER_SAVE_FILE", 100 },
        { "BUILDER_SAVE_FILE1.01", 101 }, { "BUILDER_SAVE_FILE1.02", 102 },
        { "BUILDER_SAVE_FILE1.03", 103 }, { "BUILDER_SAVE_FILE1.04", 104 },
        { "BUILDER_SAVE_FILE1.10", 110 }, { "BUILDER_SAVE_FILE1.11", 111 },
        { "BUILDER_SAVE_FILE1.12", 112 }, { "BUILDER_SAVE_FILE1.13", 113 },
        { "BUILDER_SAVE_FILE1.14", 114 }, { "BUILDER_SAVE_FILE1.15", 115 },
        { "BUILDER_SAVE_FILE1.16", 116 }, { "BUILDER_SAVE_FILE1.17", 117 },
        { "BUILDER_SAVE_FILE1.18", 118 }, { "BUILDER_SAVE_FILE1.19", 119 },
        { "BUILDER_SAVE_FILE1.20", 120 }, { "BUILDER_SAVE_FILE1.21", 121 },
        { "BUILDER_SAVE_FILE1.22", 122 }, { "BUILDER_SAVE_FILE1.23", 123 },
        { "BUILDER_SAVE_FILE1.24", 124 }, { "BUILDER_SAVE_FILE1.25", 125 },
        { "BUILDER_SAVE_FILE1.26", 126 },
    };
    for (const auto &e : table)
        if (tag == QLatin1String(e.text))
            return e.version;
    return 0;
}

void MuibLoader::readHelp(ByteCursor &c, HelpInfo &help, bool ownerGenerated, int version)
{
    help.title = c.readLine();
    help.nb_char = c.readInt();
    if (version > 115)
        help.generated = c.readInt() != 0;
    else
        help.generated = ownerGenerated || (help.nb_char > 0);
    if (help.nb_char > 0)
        help.content = c.readRawBytes(help.nb_char);
}

void MuibLoader::readNotify(ByteCursor &c, QVector<NotifyEvent> &notify)
{
    for (;;)
    {
        QString targetLabel = c.readLine();
        if (targetLabel == QLatin1String("//END_ENTRIES//"))
            break;
        NotifyEvent evt;
        evt.targetLabel = targetLabel;
        evt.targetTypeId = c.readInt();
        evt.srcType = c.readInt();
        evt.destType = c.readInt();
        evt.argString = c.readLine();
        notify.append(evt);
    }
}

void MuibLoader::readArea(ByteCursor &c, AreaAttrs &area)
{
    area.Hide = c.readInt() != 0;
    area.Disable = c.readInt() != 0;
    area.InputMode = c.readInt() != 0;
    area.Phantom = c.readInt() != 0;
    area.Weight = c.readInt();
    area.Background = static_cast<unsigned int>(c.readInt());
    area.Frame = c.readInt();
    QString keyLine = c.readLine();
    area.key = keyLine.isEmpty() ? '\0' : keyLine.at(0).toLatin1();
    area.TitleFrame = c.readLine();
}

QStringList MuibLoader::readEntriesUntilSentinel(ByteCursor &c)
{
    QStringList result;
    for (;;)
    {
        QString line = c.readLine();
        if (line == QLatin1String("//END_ENTRIES//"))
            break;
        result.append(line);
    }
    return result;
}

QString MuibLoader::readFunctionHook(ByteCursor &c)
{
    // Simplified port of ReadFunctionHook(): the original interns the
    // string into application.Functions via PointerOnString() so that
    // identical hook names share one allocation. QString's value
    // semantics make that optimization unnecessary here.
    return c.readLine();
}

std::unique_ptr<MuibProject> MuibLoader::loadFile(const QString &path, QString *errorOut)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
    {
        if (errorOut)
            *errorOut = QStringLiteral("Could not open file: %1").arg(path);
        return nullptr;
    }
    QByteArray data = file.readAll();
    ByteCursor c(data);

    int version = searchVersion(c.readLine());
    if (version == 0)
    {
        if (errorOut)
            *errorOut = QStringLiteral("Not a MUIBuilder save file (unrecognized version tag): %1").arg(path);
        return nullptr;
    }

    auto proj = std::make_unique<MuibProject>();
    proj->loadedVersion = version;
    m_nbScaleCounter = 0;
    m_nbSpaceCounter = 0;

    if (version > 102)
    {
        proj->genfile = c.readLine();
        proj->catfile = c.readLine();
    }

    proj->nbWindow = c.readInt();
    proj->nbListview = c.readInt();
    proj->nbGroup = c.readInt();
    proj->nbButton = c.readInt();
    proj->nbString = c.readInt();
    proj->nbGauge = c.readInt();
    proj->nbCycle = c.readInt();
    proj->nbRadio = c.readInt();
    proj->nbCheck = c.readInt();
    proj->nbImage = c.readInt();
    proj->nbSlider = c.readInt();
    proj->nbText = c.readInt();
    proj->nbProp = c.readInt();

    if (version > 110)
    {
        proj->nbRectangle = c.readInt();
        proj->nbColorfield = c.readInt();
        proj->nbPopasl = c.readInt();
        proj->nbPopobject = c.readInt();
    }
    if (version > 114)
    {
        proj->nbMenu = c.readInt();
        proj->nbSpace = c.readInt();
        proj->nbScale = c.readInt();
    }
    if (version > 124)
        proj->nbBarlabel = c.readInt();
    if (version > 120)
        proj->nbLabel = c.readInt();
    if (version > 121)
    {
        proj->optCode = c.readInt() != 0;
        proj->optEnv = c.readInt() != 0;
        proj->optDeclarations = c.readInt() != 0;
        proj->optLocal = c.readInt() != 0;
        proj->optNotifications = c.readInt() != 0;
    }
    if (version > 122)
        proj->optGenerateAll = c.readInt() != 0;

    proj->base = c.readLine();
    proj->author = c.readLine();
    proj->title = c.readLine();
    proj->version = c.readLine();
    proj->copyright = c.readLine();
    proj->description = c.readLine();
    proj->helpfile = c.readLine();

    if (version > 103)
        proj->realGetString = c.readLine();

    if (version > 115)
        proj->catPrepend = c.readLine();
    else
        proj->catPrepend = QStringLiteral("MSG_");

    int helpLen = c.readInt();
    proj->help.nb_char = helpLen;
    if (version > 115)
        proj->help.generated = c.readInt() != 0;
    if (helpLen > 0)
        proj->help.content = c.readRawBytes(helpLen);

    if (version > 111)
    {
        proj->idents = readEntriesUntilSentinel(c);
        proj->functions = readEntriesUntilSentinel(c);
        proj->variables = readEntriesUntilSentinel(c);
    }

    if (version > 104)
        readNotify(c, proj->notify);

    if (version >= 117)
    {
        int menuTypeId = c.readInt();
        proj->appMenu = loadObject(c, nullptr, menuTypeId, version);
    }

    for (;;)
    {
        int typeId = c.readInt();
        if (typeId == -1)
            break;
        proj->windows.push_back(loadObject(c, nullptr, typeId, version));
    }

    return proj;
}

std::unique_ptr<MuibObject> MuibLoader::loadObject(ByteCursor &c, MuibObject *father, int objTypeId, int version)
{
    switch (objTypeId)
    {
        case static_cast<int>(ObjType::Window):     return loadWindow(c, version);
        case static_cast<int>(ObjType::Group):      return loadGroup(c, father, version);
        case static_cast<int>(ObjType::Button):      return loadButton(c, father, version);
        case static_cast<int>(ObjType::Gauge):       return loadGauge(c, father, version);
        case static_cast<int>(ObjType::Image):       return loadImage(c, father, version);
        case static_cast<int>(ObjType::ListView):    return loadListview(c, father, version);
        case static_cast<int>(ObjType::DirList):     return loadDirList(c, father, version);
        case static_cast<int>(ObjType::Check):       return loadCheck(c, father, version);
        case static_cast<int>(ObjType::Scale):       return loadScale(c, father, version);
        case static_cast<int>(ObjType::Prop):        return loadProp(c, father, version);
        case static_cast<int>(ObjType::Text):        return loadText(c, father, version);
        case static_cast<int>(ObjType::Slider):      return loadSlider(c, father, version);
        case static_cast<int>(ObjType::Cycle):       return loadCycle(c, father, version);
        case static_cast<int>(ObjType::Radio):       return loadRadio(c, father, version);
        case static_cast<int>(ObjType::String):      return loadString(c, father, version);
        case static_cast<int>(ObjType::Label):       return loadLabel(c, father, version);
        case static_cast<int>(ObjType::Space):       return loadSpace(c, father, version);
        case static_cast<int>(ObjType::Rectangle):   return loadRectangle(c, father, version);
        case static_cast<int>(ObjType::ColorField):  return loadColorField(c, father, version);
        case static_cast<int>(ObjType::PopAsl):      return loadPopAsl(c, father, version);
        case static_cast<int>(ObjType::PopObject):   return loadPopObject(c, father, version);
        case static_cast<int>(ObjType::Menu):
        case static_cast<int>(ObjType::SubMenu):
        case static_cast<int>(ObjType::MenuItem):
            return loadMenu(c, father, objTypeId, version);
        default:
            return nullptr;   // ports ErrorMessage(MSG_WrongVersion)+EXIT_PRG() - a corrupt/unsupported file
    }
}

std::unique_ptr<MuibObject> MuibLoader::loadWindow(ByteCursor &c, int version)
{
    auto win = std::make_unique<MuibObject>(ObjType::Window);
    win->label = c.readLine();
    win->generated = c.readInt() != 0;
    readHelp(c, win->help, win->generated, version);
    if (version > 104)
        readNotify(c, win->notify);
    win->title = c.readLine();
    if (version > 101)
    {
        win->appwindow = c.readInt() != 0;
        win->backdrop = c.readInt() != 0;
        win->borderless = c.readInt() != 0;
        win->closegadget = c.readInt() != 0;
        win->depthgadget = c.readInt() != 0;
        win->dragbar = c.readInt() != 0;
        win->sizegadget = c.readInt() != 0;
    }
    if (version > 104)
        win->initopen = c.readInt() != 0;
    if (version > 123)
        win->nomenu = c.readInt() != 0;
    if (version > 125)
        win->needmouse = c.readInt() != 0;
    if (version >= 114)
    {
        int menuTypeId = c.readInt();
        win->menu = loadObject(c, win.get(), menuTypeId, version);
    }

    int groupTypeId = c.readInt();
    win->root = loadObject(c, win.get(), groupTypeId, version);
    return win;
}

std::unique_ptr<MuibObject> MuibLoader::loadGroup(ByteCursor &c, MuibObject *father, int version)
{
    auto grp = std::make_unique<MuibObject>(ObjType::Group);
    grp->father = father;
    grp->label = c.readLine();
    grp->generated = c.readInt() != 0;
    readHelp(c, grp->help, grp->generated, version);
    if (version > 104)
        readNotify(c, grp->notify);
    if (version >= 113)
        readArea(c, grp->area);
    if (version < 113)
        grp->area.TitleFrame = c.readLine();
    grp->isRoot = c.readInt() != 0;
    if (version < 113)
    {
        // Obsolete pre-1.13 "title_exist" flag: originally translated to
        // Area.Frame = MUIV_Frame_Group (a MUI constant not pulled into
        // this port, since it only affects a legacy frame style, not
        // structural parsing). The byte must still be consumed to keep
        // the cursor aligned with the rest of the file.
        c.readInt();
    }
    grp->horizontal = c.readInt() != 0;
    grp->registermode = c.readInt() != 0;
    grp->sameheight = c.readInt() != 0;
    grp->samesize = c.readInt() != 0;
    grp->samewidth = c.readInt() != 0;
    grp->isVirtual = c.readInt() != 0;
    grp->rows = c.readInt() != 0;
    grp->horizspacing = c.readInt() != 0;
    grp->vertspacing = c.readInt() != 0;
    grp->columns = c.readInt() != 0;
    grp->number = c.readInt();
    if (version < 113)
        grp->area.Weight = c.readInt();
    grp->horizspace = c.readInt();
    grp->vertspace = c.readInt();
    if (version > 100)
        grp->entries = readEntriesUntilSentinel(c);

    for (;;)
    {
        int typeId = c.readInt();
        if (typeId == -1)
            break;
        grp->children.push_back(loadObject(c, grp.get(), typeId, version));
    }

    if (grp->samesize)
    {
        grp->sameheight = true;
        grp->samewidth = true;
    }
    grp->horizontal = grp->horizontal && !grp->rows && !grp->columns;
    return grp;
}

std::unique_ptr<MuibObject> MuibLoader::loadButton(ByteCursor &c, MuibObject *father, int version)
{
    auto o = std::make_unique<MuibObject>(ObjType::Button);
    o->father = father;
    o->label = c.readLine();
    o->generated = c.readInt() != 0;
    readHelp(c, o->help, o->generated, version);
    if (version > 104)
        readNotify(c, o->notify);
    o->title = c.readLine();
    if (version < 113)
    {
        QString keyLine = c.readLine();
        o->area.key = keyLine.isEmpty() ? '\0' : keyLine.at(0).toLatin1();
        o->area.Weight = c.readInt();
    }
    else
        readArea(c, o->area);
    return o;
}

std::unique_ptr<MuibObject> MuibLoader::loadGauge(ByteCursor &c, MuibObject *father, int version)
{
    auto o = std::make_unique<MuibObject>(ObjType::Gauge);
    o->father = father;
    o->label = c.readLine();
    o->generated = c.readInt() != 0;
    readHelp(c, o->help, o->generated, version);
    if (version > 104)
        readNotify(c, o->notify);
    if (version >= 113)
        readArea(c, o->area);
    o->divide = c.readInt();
    o->horizontal = c.readInt() != 0;
    o->gaugeMax = c.readInt();
    o->fixheight = c.readInt() != 0;
    o->fixwidth = c.readInt() != 0;
    o->height = c.readInt();
    o->width = c.readInt();
    if (version > 114)
        o->infotext = c.readLine();
    return o;
}

std::unique_ptr<MuibObject> MuibLoader::loadCheck(ByteCursor &c, MuibObject *father, int version)
{
    auto o = std::make_unique<MuibObject>(ObjType::Check);
    o->father = father;
    o->label = c.readLine();
    o->generated = c.readInt() != 0;
    readHelp(c, o->help, o->generated, version);
    if (version > 104)
        readNotify(c, o->notify);
    if (version >= 113)
        readArea(c, o->area);
    o->title = c.readLine();
    o->title_exist = c.readInt() != 0;
    if (version > 117)
        o->init_state = c.readInt() != 0;
    return o;
}

std::unique_ptr<MuibObject> MuibLoader::loadScale(ByteCursor &c, MuibObject *father, int version)
{
    auto o = std::make_unique<MuibObject>(ObjType::Scale);
    o->father = father;
    o->label = c.readLine();
    if (version <= 114)
        o->label = QStringLiteral("Scale_%1").arg(m_nbScaleCounter++);
    o->generated = c.readInt() != 0;
    readHelp(c, o->help, o->generated, version);
    if (version > 104)
        readNotify(c, o->notify);
    if (version >= 113)
        readArea(c, o->area);
    o->scaleHoriz = c.readInt() != 0;
    return o;
}

std::unique_ptr<MuibObject> MuibLoader::loadProp(ByteCursor &c, MuibObject *father, int version)
{
    auto o = std::make_unique<MuibObject>(ObjType::Prop);
    o->father = father;
    o->label = c.readLine();
    o->generated = c.readInt() != 0;
    readHelp(c, o->help, o->generated, version);
    if (version > 104)
        readNotify(c, o->notify);
    if (version >= 113)
        readArea(c, o->area);
    o->horizontal = c.readInt() != 0;
    o->propEntries = c.readInt();
    o->first = c.readInt();
    o->visible = c.readInt();
    o->fixheight = c.readInt() != 0;
    o->fixwidth = c.readInt() != 0;
    o->height = c.readInt();
    o->width = c.readInt();
    return o;
}

std::unique_ptr<MuibObject> MuibLoader::loadText(ByteCursor &c, MuibObject *father, int version)
{
    auto o = std::make_unique<MuibObject>(ObjType::Text);
    o->father = father;
    o->label = c.readLine();
    o->generated = c.readInt() != 0;
    readHelp(c, o->help, o->generated, version);
    if (version > 104)
        readNotify(c, o->notify);
    if (version < 113)
    {
        o->area.Background = static_cast<unsigned int>(c.readInt());
        o->area.Frame = c.readInt();
    }
    if (version >= 113)
    {
        readArea(c, o->area);
        o->preparse = c.readLine();
    }
    o->textMax = c.readInt() != 0;
    o->textMin = c.readInt() != 0;
    o->content = c.readLine();
    return o;
}

std::unique_ptr<MuibObject> MuibLoader::loadSlider(ByteCursor &c, MuibObject *father, int version)
{
    auto o = std::make_unique<MuibObject>(ObjType::Slider);
    o->father = father;
    o->label = c.readLine();
    o->generated = c.readInt() != 0;
    readHelp(c, o->help, o->generated, version);
    if (version > 104)
        readNotify(c, o->notify);
    if (version >= 113)
        readArea(c, o->area);
    o->title = c.readLine();
    o->title_exist = c.readInt() != 0;
    o->sliderMax = c.readInt();
    o->sliderMin = c.readInt();
    o->quiet = c.readInt() != 0;
    o->init = c.readInt();
    if (version > 101)
        o->reverse = c.readInt() != 0;
    if (version > 117)
        o->horizontal = c.readInt() != 0;
    return o;
}

std::unique_ptr<MuibObject> MuibLoader::loadCycle(ByteCursor &c, MuibObject *father, int version)
{
    auto o = std::make_unique<MuibObject>(ObjType::Cycle);
    o->father = father;
    o->label = c.readLine();
    o->generated = c.readInt() != 0;
    readHelp(c, o->help, o->generated, version);
    if (version > 104)
        readNotify(c, o->notify);
    if (version < 113)
        o->area.Weight = c.readInt();
    if (version >= 113)
        readArea(c, o->area);
    o->entries = readEntriesUntilSentinel(c);
    return o;
}

std::unique_ptr<MuibObject> MuibLoader::loadRadio(ByteCursor &c, MuibObject *father, int version)
{
    auto o = std::make_unique<MuibObject>(ObjType::Radio);
    o->father = father;
    o->label = c.readLine();
    o->generated = c.readInt() != 0;
    readHelp(c, o->help, o->generated, version);
    if (version > 104)
        readNotify(c, o->notify);
    if (version < 113)
    {
        o->area.TitleFrame = c.readLine();
        o->area.Weight = c.readInt();
    }
    if (version >= 113)
        readArea(c, o->area);
    o->entries = readEntriesUntilSentinel(c);
    return o;
}

std::unique_ptr<MuibObject> MuibLoader::loadString(ByteCursor &c, MuibObject *father, int version)
{
    auto o = std::make_unique<MuibObject>(ObjType::String);
    o->father = father;
    o->label = c.readLine();
    o->generated = c.readInt() != 0;
    readHelp(c, o->help, o->generated, version);
    if (version > 104)
        readNotify(c, o->notify);
    if (version >= 113)
        readArea(c, o->area);
    o->title = c.readLine();
    o->content = c.readLine();
    if (version < 113)
        o->area.Weight = c.readInt();
    o->title_exist = c.readInt() != 0;
    o->accept = c.readLine();
    o->reject = c.readLine();
    o->format_ = c.readInt();
    o->integer = c.readInt() != 0;
    o->maxlen = c.readInt();
    o->secret = c.readInt() != 0;
    return o;
}

std::unique_ptr<MuibObject> MuibLoader::loadLabel(ByteCursor &c, MuibObject *father, int version)
{
    auto o = std::make_unique<MuibObject>(ObjType::Label);
    o->father = father;
    o->label = c.readLine();
    o->generated = c.readInt() != 0;
    readHelp(c, o->help, o->generated, version);
    if (version > 104)
        readNotify(c, o->notify);
    if (version >= 113)
        readArea(c, o->area);
    o->title = c.readLine();
    if (version < 113)
        o->area.Weight = c.readInt();
    return o;
}

std::unique_ptr<MuibObject> MuibLoader::loadSpace(ByteCursor &c, MuibObject *father, int version)
{
    auto o = std::make_unique<MuibObject>(ObjType::Space);
    o->father = father;
    o->label = c.readLine();
    if (version <= 114)
        o->label = QStringLiteral("Space_%1").arg(m_nbSpaceCounter++);
    o->generated = c.readInt() != 0;
    readHelp(c, o->help, o->generated, version);
    if (version > 104)
        readNotify(c, o->notify);
    if (version > 118)
    {
        o->spaceType = c.readInt();
        o->spacing = c.readInt();
    }
    return o;
}

std::unique_ptr<MuibObject> MuibLoader::loadRectangle(ByteCursor &c, MuibObject *father, int version)
{
    auto o = std::make_unique<MuibObject>(ObjType::Rectangle);
    o->father = father;
    o->label = c.readLine();
    o->generated = c.readInt() != 0;
    readHelp(c, o->help, o->generated, version);
    readNotify(c, o->notify);   // unconditional in the original (harmless: Rectangle didn't exist before v111)
    if (version >= 113)
        readArea(c, o->area);
    int t = c.readInt();
    if (version < 113)
        t++;
    o->rectType = t;
    o->fixheight = c.readInt() != 0;
    o->fixwidth = c.readInt() != 0;
    o->height = c.readInt();
    o->width = c.readInt();
    if (version < 113)
        o->area.Weight = c.readInt();
    return o;
}

std::unique_ptr<MuibObject> MuibLoader::loadColorField(ByteCursor &c, MuibObject *father, int version)
{
    auto o = std::make_unique<MuibObject>(ObjType::ColorField);
    o->father = father;
    o->label = c.readLine();
    o->generated = c.readInt() != 0;
    readHelp(c, o->help, o->generated, version);
    readNotify(c, o->notify);   // unconditional in the original (harmless: ColorField didn't exist before v111)
    if (version >= 113)
        readArea(c, o->area);
    if (version < 113)
        o->area.InputMode = c.readInt() != 0;
    o->fixheight = c.readInt() != 0;
    o->fixwidth = c.readInt() != 0;
    o->height = c.readInt();
    o->width = c.readInt();
    if (version > 118)
    {
        o->red = static_cast<unsigned int>(c.readInt());
        o->green = static_cast<unsigned int>(c.readInt());
        o->blue = static_cast<unsigned int>(c.readInt());
    }
    return o;
}

std::unique_ptr<MuibObject> MuibLoader::loadImage(ByteCursor &c, MuibObject *father, int version)
{
    auto o = std::make_unique<MuibObject>(ObjType::Image);
    o->father = father;
    o->label = c.readLine();
    o->generated = c.readInt() != 0;
    readHelp(c, o->help, o->generated, version);
    if (version > 104)
        readNotify(c, o->notify);
    if (version >= 113)
        readArea(c, o->area);
    o->imgType = c.readInt();
    if (version < 113)
        o->area.InputMode = c.readInt() != 0;
    o->freevert = c.readInt() != 0;
    o->freehoriz = c.readInt() != 0;
    o->fixheight = c.readInt() != 0;
    o->fixwidth = c.readInt() != 0;
    o->height = c.readInt();
    o->width = c.readInt();
    if (version < 113)
        c.readInt();   // was: Area.Frame = ReadInt() * MUIV_Frame_ImageButton - legacy frame style, not ported
    if (version > 122)
        o->spec = c.readLine();
    return o;
}

std::unique_ptr<MuibObject> MuibLoader::loadListview(ByteCursor &c, MuibObject *father, int version)
{
    auto o = std::make_unique<MuibObject>(ObjType::ListView);
    o->father = father;
    o->label = c.readLine();
    o->generated = c.readInt() != 0;
    readHelp(c, o->help, o->generated, version);
    if (version > 104)
        readNotify(c, o->notify);
    if (version < 113)
        o->area.Weight = c.readInt();
    if (version >= 113)
        readArea(c, o->area);
    o->lvType = c.readInt();
    // pre-1.13 lvType==1 => Area.Frame = MUIV_Frame_ReadList - legacy frame style, not ported (no extra byte consumed)
    if (version > 101)
    {
        o->multiselect = c.readInt() != 0;
        o->doubleclick = c.readInt() != 0;
    }
    if (version >= 113)
    {
        o->select = c.readInt();
        o->adjustheight = c.readInt() != 0;
        o->adjustwidth = c.readInt() != 0;
        o->inputmode = c.readInt() != 0;
        o->comparehook = readFunctionHook(c);
        o->constructhook = readFunctionHook(c);
        o->displayhook = readFunctionHook(c);
        o->multitesthook = readFunctionHook(c);
        if (version >= 120)
            o->destructhook = readFunctionHook(c);
        o->format = c.readLine();
        o->title = c.readLine();
        o->content = c.readLine();
    }
    return o;
}

std::unique_ptr<MuibObject> MuibLoader::loadDirList(ByteCursor &c, MuibObject *father, int version)
{
    auto o = std::make_unique<MuibObject>(ObjType::DirList);
    o->father = father;
    o->label = c.readLine();
    o->generated = c.readInt() != 0;
    readHelp(c, o->help, o->generated, version);
    if (version > 104)
        readNotify(c, o->notify);
    if (version < 113)
        o->area.Weight = c.readInt();
    else
        readArea(c, o->area);
    o->drawers = c.readInt() != 0;
    o->files = c.readInt() != 0;
    o->filter = c.readInt() != 0;
    o->multi = c.readInt() != 0;
    o->icons = c.readInt() != 0;
    o->highlow = c.readInt() != 0;
    o->sorttype = c.readInt();
    o->directory = c.readLine();
    if (version >= 113)
    {
        o->accept = c.readLine();
        o->reject = c.readLine();
        o->sortdirs = c.readInt();
        o->filterhook = readFunctionHook(c);
    }
    return o;
}

std::unique_ptr<MuibObject> MuibLoader::loadPopAsl(ByteCursor &c, MuibObject *father, int version)
{
    auto o = std::make_unique<MuibObject>(ObjType::PopAsl);
    o->father = father;
    o->label = c.readLine();
    o->generated = c.readInt() != 0;
    readHelp(c, o->help, o->generated, version);
    readNotify(c, o->notify);   // unconditional in the original (harmless: PopAsl didn't exist before v111)
    if (version >= 113)
        readArea(c, o->area);
    o->starthook = readFunctionHook(c);
    o->stophook = readFunctionHook(c);
    o->popType = c.readInt();
    o->popImage = c.readInt();
    return o;
}

std::unique_ptr<MuibObject> MuibLoader::loadPopObject(ByteCursor &c, MuibObject *father, int version)
{
    auto o = std::make_unique<MuibObject>(ObjType::PopObject);
    o->label = c.readLine();
    o->generated = c.readInt() != 0;
    o->father = father;
    readHelp(c, o->help, o->generated, version);
    readNotify(c, o->notify);   // unconditional in the original (harmless: PopObject didn't exist before v111)
    if (version >= 113)
        readArea(c, o->area);
    o->openhook = readFunctionHook(c);
    o->closehook = readFunctionHook(c);
    int childTypeId = c.readInt();
    o->popObj = loadObject(c, o.get(), childTypeId, version);
    o->popImage = c.readInt();
    o->follow = c.readInt();
    o->light = c.readInt();
    o->isVolatile = c.readInt();
    return o;
}

std::unique_ptr<MuibObject> MuibLoader::loadMenu(ByteCursor &c, MuibObject *father, int typeId, int version)
{
    ObjType ot = static_cast<ObjType>(typeId);
    auto o = std::make_unique<MuibObject>(ot);
    o->father = father;
    o->label = c.readLine();
    o->generated = c.readInt() != 0;
    readHelp(c, o->help, o->generated, version);
    readNotify(c, o->notify);   // unconditional in the original, no version guard
    o->menu_enable = c.readInt() != 0;
    o->check_enable = c.readInt() != 0;
    o->check_state = c.readInt() != 0;
    o->toggleMenu = c.readInt() != 0;
    QString keyLine = c.readLine();
    o->menuKey = keyLine.isEmpty() ? '\0' : keyLine.at(0).toLatin1();
    o->name = c.readLine();
    if (ot == ObjType::Menu || ot == ObjType::SubMenu)
    {
        for (;;)
        {
            int childType = c.readInt();
            if (childType == -1)
                break;
            o->childs.push_back(loadObject(c, o.get(), childType, version));
        }
    }
    return o;
}
