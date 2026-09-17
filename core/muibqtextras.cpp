#include "muibqtextras.h"

#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

namespace {

QString modeToString(WindowPositionMode mode)
{
    switch (mode)
    {
    case WindowPositionMode::Moused: return QStringLiteral("moused");
    case WindowPositionMode::Manual: return QStringLiteral("manual");
    case WindowPositionMode::Centered: default: return QStringLiteral("centered");
    }
}

WindowPositionMode modeFromString(const QString &s)
{
    if (s == QStringLiteral("moused"))
        return WindowPositionMode::Moused;
    if (s == QStringLiteral("manual"))
        return WindowPositionMode::Manual;
    return WindowPositionMode::Centered;
}

// --- MenuItem excludeGroup helpers --------------------------------------
// Menu items live in a recursive tree (Menu/SubMenu/MenuItem via
// `childs`), unlike Windows (a flat top-level list) - so, unlike
// posMode's own load/save loop above, these need a recursive walk. Two
// separate trees can hold MenuItem objects: the project-level appMenu
// (MuiBuilderQt's own application-menu feature) and each window's own
// `menu`; both are walked.

void resetExcludeGroups(MuibObject *node)
{
    if (!node)
        return;
    if (node->type == ObjType::MenuItem)
        node->excludeGroup = 0;
    for (auto &c : node->childs)
        resetExcludeGroups(c.get());
}

MuibObject *findMenuItemByLabel(MuibObject *node, const QString &label)
{
    if (!node)
        return nullptr;
    if (node->type == ObjType::MenuItem && node->label == label)
        return node;
    for (auto &c : node->childs)
    {
        if (MuibObject *found = findMenuItemByLabel(c.get(), label))
            return found;
    }
    return nullptr;
}

void collectExcludeGroupEntries(const MuibObject *node, QJsonArray &out)
{
    if (!node)
        return;
    if (node->type == ObjType::MenuItem && node->excludeGroup != 0 && !node->label.isEmpty())
    {
        QJsonObject entry;
        entry.insert(QStringLiteral("label"), node->label);
        entry.insert(QStringLiteral("group"), node->excludeGroup);
        out.append(entry);
    }
    for (const auto &c : node->childs)
        collectExcludeGroupEntries(c.get(), out);
}

} // namespace

QString MuibQtExtras::sidecarPathFor(const QString &muibPath)
{
    return muibPath + QStringLiteral(".mbqtextras.json");
}

bool MuibQtExtras::load(MuibProject &proj, const QString &muibPath, QString *errorOut)
{
    const QString sidecarPath = sidecarPathFor(muibPath);
    QFile f(sidecarPath);
    if (!f.exists())
        return true;   // no sidecar - perfectly normal, not an error
    if (!f.open(QIODevice::ReadOnly))
    {
        if (errorOut)
            *errorOut = QStringLiteral("Konnte %1 nicht lesen.").arg(sidecarPath);
        return false;
    }
    const QByteArray data = f.readAll();
    f.close();

    QJsonParseError parseErr;
    const QJsonDocument doc = QJsonDocument::fromJson(data, &parseErr);
    if (doc.isNull() || !doc.isObject())
    {
        if (errorOut)
            *errorOut = QStringLiteral("%1 ist keine gueltige JSON-Datei (%2).")
                            .arg(sidecarPath, parseErr.errorString());
        return false;
    }

    const QJsonObject root = doc.object();

    const QJsonValue aboutVal = root.value(QStringLiteral("aboutBox"));
    if (aboutVal.isObject())
    {
        const QJsonObject about = aboutVal.toObject();
        auto box = std::make_unique<MuibObject>(ObjType::AboutBox);
        box->label = about.value(QStringLiteral("label")).toString();
        box->aboutCredits = about.value(QStringLiteral("credits")).toString();
        box->aboutBuild = about.value(QStringLiteral("build")).toString();
        box->aboutLogoFile = about.value(QStringLiteral("logoFile")).toString();
        box->aboutUrl = about.value(QStringLiteral("url")).toString();
        box->aboutUrlText = about.value(QStringLiteral("urlText")).toString();
        box->aboutLinkedMenuItem = about.value(QStringLiteral("linkedMenuItem")).toString();
        proj.aboutBox = std::move(box);
    }
    else
    {
        proj.aboutBox.reset();   // sidecar exists but has no aboutBox entry (e.g. removed)
    }

    // Every window starts at every default (Centered, every size field
    // 0) - only entries actually present in the sidecar override that,
    // matched by label. Still the same "windowPositions" JSON array key
    // as before winWidth/winHeight/winMin*/winMax* existed (a sidecar
    // written by an older MuiBuilderQt build simply has no "width"/
    // "height"/... keys in its entries, which read back as 0 - the
    // correct "not set" default - via QJsonObject::value() on a missing
    // key, so old sidecars keep loading correctly unchanged).
    for (auto &w : proj.windows)
    {
        w->posMode = WindowPositionMode::Centered;
        w->posX = 0;
        w->posY = 0;
        w->winWidth = 0;
        w->winHeight = 0;
        w->winMinWidth = 0;
        w->winMinHeight = 0;
        w->winMaxWidth = 0;
        w->winMaxHeight = 0;
    }
    const QJsonValue winsVal = root.value(QStringLiteral("windowPositions"));
    if (winsVal.isArray())
    {
        for (const QJsonValue &entryVal : winsVal.toArray())
        {
            if (!entryVal.isObject())
                continue;
            const QJsonObject entry = entryVal.toObject();
            const QString label = entry.value(QStringLiteral("label")).toString();
            if (label.isEmpty())
                continue;
            for (auto &w : proj.windows)
            {
                if (w->type != ObjType::Window || w->label != label)
                    continue;
                w->posMode = modeFromString(entry.value(QStringLiteral("mode")).toString());
                w->posX = entry.value(QStringLiteral("x")).toInt();
                w->posY = entry.value(QStringLiteral("y")).toInt();
                w->winWidth = entry.value(QStringLiteral("width")).toInt();
                w->winHeight = entry.value(QStringLiteral("height")).toInt();
                w->winMinWidth = entry.value(QStringLiteral("minWidth")).toInt();
                w->winMinHeight = entry.value(QStringLiteral("minHeight")).toInt();
                w->winMaxWidth = entry.value(QStringLiteral("maxWidth")).toInt();
                w->winMaxHeight = entry.value(QStringLiteral("maxHeight")).toInt();
                break;
            }
        }
    }

    // Same "reset to default, then apply only what's actually present"
    // pattern as posMode above, just walking a recursive tree instead of
    // a flat list - see the helpers' own comments.
    if (proj.appMenu)
        resetExcludeGroups(proj.appMenu.get());
    for (auto &w : proj.windows)
        if (w->menu)
            resetExcludeGroups(w->menu.get());

    const QJsonValue exclVal = root.value(QStringLiteral("menuExcludeGroups"));
    if (exclVal.isArray())
    {
        for (const QJsonValue &entryVal : exclVal.toArray())
        {
            if (!entryVal.isObject())
                continue;
            const QJsonObject entry = entryVal.toObject();
            const QString label = entry.value(QStringLiteral("label")).toString();
            const int group = entry.value(QStringLiteral("group")).toInt();
            if (label.isEmpty() || group <= 0)
                continue;
            MuibObject *found = proj.appMenu ? findMenuItemByLabel(proj.appMenu.get(), label) : nullptr;
            for (auto it = proj.windows.begin(); !found && it != proj.windows.end(); ++it)
            {
                if ((*it)->menu)
                    found = findMenuItemByLabel((*it)->menu.get(), label);
            }
            if (found)
                found->excludeGroup = group;
        }
    }
    return true;
}

bool MuibQtExtras::save(const MuibProject &proj, const QString &muibPath, QString *errorOut)
{
    const QString sidecarPath = sidecarPathFor(muibPath);

    QJsonArray winsArr;
    for (const auto &w : proj.windows)
    {
        const bool hasNonDefaultSize = w->winWidth != 0 || w->winHeight != 0 ||
            w->winMinWidth != 0 || w->winMinHeight != 0 || w->winMaxWidth != 0 || w->winMaxHeight != 0;
        if (w->type != ObjType::Window || (w->posMode == WindowPositionMode::Centered && !hasNonDefaultSize))
            continue;   // every field at its default - nothing worth persisting for this window
        QJsonObject entry;
        entry.insert(QStringLiteral("label"), w->label);
        entry.insert(QStringLiteral("mode"), modeToString(w->posMode));
        entry.insert(QStringLiteral("x"), w->posX);
        entry.insert(QStringLiteral("y"), w->posY);
        // Only ever written when non-zero (0 is the "not set" default and
        // QJsonObject::value() on a missing key already reads back as 0
        // on load - see load()'s own comment) - keeps an old-style
        // sidecar's entries (position-only, no size fields at all) exactly
        // as compact as they always were for a window that only uses
        // posMode/posX/posY.
        if (w->winWidth != 0) entry.insert(QStringLiteral("width"), w->winWidth);
        if (w->winHeight != 0) entry.insert(QStringLiteral("height"), w->winHeight);
        if (w->winMinWidth != 0) entry.insert(QStringLiteral("minWidth"), w->winMinWidth);
        if (w->winMinHeight != 0) entry.insert(QStringLiteral("minHeight"), w->winMinHeight);
        if (w->winMaxWidth != 0) entry.insert(QStringLiteral("maxWidth"), w->winMaxWidth);
        if (w->winMaxHeight != 0) entry.insert(QStringLiteral("maxHeight"), w->winMaxHeight);
        winsArr.append(entry);
    }

    QJsonArray exclArr;
    if (proj.appMenu)
        collectExcludeGroupEntries(proj.appMenu.get(), exclArr);
    for (const auto &w : proj.windows)
        if (w->menu)
            collectExcludeGroupEntries(w->menu.get(), exclArr);

    if (!proj.aboutBox && winsArr.isEmpty() && exclArr.isEmpty())
    {
        // Nothing to persist (any more) - remove a stale sidecar from an
        // AboutBox/window position/exclude-group the user has since
        // deleted or reset, if one exists. Not finding one to remove is
        // not an error.
        if (QFile::exists(sidecarPath))
            QFile::remove(sidecarPath);
        return true;
    }

    QJsonObject root;
    if (proj.aboutBox)
    {
        QJsonObject about;
        about.insert(QStringLiteral("label"), proj.aboutBox->label);
        about.insert(QStringLiteral("credits"), proj.aboutBox->aboutCredits);
        about.insert(QStringLiteral("build"), proj.aboutBox->aboutBuild);
        about.insert(QStringLiteral("logoFile"), proj.aboutBox->aboutLogoFile);
        about.insert(QStringLiteral("url"), proj.aboutBox->aboutUrl);
        about.insert(QStringLiteral("urlText"), proj.aboutBox->aboutUrlText);
        about.insert(QStringLiteral("linkedMenuItem"), proj.aboutBox->aboutLinkedMenuItem);
        root.insert(QStringLiteral("aboutBox"), about);
    }
    if (!winsArr.isEmpty())
        root.insert(QStringLiteral("windowPositions"), winsArr);
    if (!exclArr.isEmpty())
        root.insert(QStringLiteral("menuExcludeGroups"), exclArr);

    QFile f(sidecarPath);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate))
    {
        if (errorOut)
            *errorOut = QStringLiteral("Konnte %1 nicht schreiben.").arg(sidecarPath);
        return false;
    }
    f.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    f.close();
    return true;
}
