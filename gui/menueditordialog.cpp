#include "menueditordialog.h"
#include "objecttreeutil.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGridLayout>
#include <QTreeWidget>
#include <QHeaderView>
#include <QLineEdit>
#include <QComboBox>
#include <QCheckBox>
#include <QSpinBox>
#include <QLabel>
#include <QPushButton>
#include <QDialogButtonBox>
#include <QGroupBox>
#include <QMessageBox>
#include <QRegularExpression>
#include <QRegularExpressionValidator>
#include <algorithm>

// MuibObject* stored/retrieved via a plain reinterpret_cast round-trip
// through qulonglong - same pragmatic, process-lifetime-only pattern
// objecttreeview.cpp already uses for its own tree items.
static QVariant ptrToVariant(MuibObject *p)
{
    return QVariant::fromValue<qulonglong>(reinterpret_cast<qulonglong>(p));
}
static MuibObject *variantToPtr(const QVariant &v)
{
    return reinterpret_cast<MuibObject *>(v.toULongLong());
}

// `obj`'s real hierarchy depth relative to `root` (the tree root Menu) -
// 1 for a top-level Menue (father == root), 2 for a Menuepunkt directly
// under one, 3+ for a nested Untermenuepunkt - see the class doc
// comment's rewritten "Type" section for why placement is driven by this
// instead of by obj's own ObjType. Returns 0 only for a null obj.
static int depthOf(const MuibObject *root, const MuibObject *obj)
{
    if (!obj)
        return 0;
    int depth = 1;
    const MuibObject *f = obj->father;
    while (f && f != root)
    {
        ++depth;
        f = f->father;
    }
    return depth;
}

// Same shape muicodegen.cpp's own sanitizeIdent() ends up producing
// anyway (letters/digits/underscore, not starting with a digit) - kept
// here as a small, independent, defensive re-check server-side (see
// applyIdFieldTo()), since the QLineEdit's own QRegularExpressionValidator
// (constructor) already restricts what the user can type in the first
// place. Deliberately ASCII-only, matching every other generated
// identifier and label in this project.
static bool isValidLabelId(const QString &s)
{
    static const QRegularExpression re(QStringLiteral("^[A-Za-z_][A-Za-z0-9_]*$"));
    return re.match(s).hasMatch();
}

// Shared validation for a candidate "ID (intern)" value - used by both
// applyIdFieldTo() (Aendern on an existing entry) and onAdd() (a
// brand-new entry, which has no existing MuibObject yet to run
// applyIdFieldTo() against). Non-empty, valid C-identifier shape, and not
// already used by some OTHER entry anywhere in the project. Returns true
// if `text` is acceptable; otherwise fills `errorOut` with a user-facing
// German message and returns false. Deliberately does NOT check "is this
// unchanged from the object's current label" itself - only
// applyIdFieldTo() needs that early-exit (onAdd() has no prior label to
// compare against), so that stays in the caller.
static bool validateLabelId(const QString &text, const QStringList &existingLabels, QString *errorOut)
{
    if (text.isEmpty())
    {
        *errorOut = MenuEditorDialog::tr("The ID (internal) must not be empty.");
        return false;
    }
    if (!isValidLabelId(text))
    {
        *errorOut = MenuEditorDialog::tr(
            "The ID (internal) \"%1\" is invalid - it must start with a "
            "letter or underscore and may only contain letters, "
            "digits, and underscores.").arg(text);
        return false;
    }
    if (existingLabels.contains(text))
    {
        *errorOut = MenuEditorDialog::tr(
            "The ID (internal) \"%1\" is already used elsewhere in the "
            "project. Please choose a unique ID.").arg(text);
        return false;
    }
    return true;
}

// Snapshots every labeled object's CURRENT label into `out`, keyed by
// object identity - called once, right after cloning, so labelRenames()
// has a fixed baseline to compare the final tree against. See the class
// doc comment's own "ID (intern)" section.
static void collectOriginalLabels(const MuibObject *obj, QHash<const MuibObject *, QString> &out)
{
    if (!obj)
        return;
    if (!obj->label.isEmpty())
        out.insert(obj, obj->label);
    for (const auto &c : obj->childs)
        collectOriginalLabels(c.get(), out);
}

// Walks the FINAL tree, comparing each object's current label against
// its snapshot in `originalLabelOf` - any labeled object present in both
// (i.e. not a brand-new entry added this session, which needs no
// project-wide cascade since nothing outside this dialog could possibly
// already reference a label that didn't exist before) whose label
// changed contributes one old-label -> new-label entry to `out`. See
// MenuEditorDialog::labelRenames()'s own comment for how the caller uses
// this.
static void collectLabelRenames(const MuibObject *obj,
                                 const QHash<const MuibObject *, QString> &originalLabelOf,
                                 QHash<QString, QString> &out)
{
    if (!obj)
        return;
    auto it = originalLabelOf.constFind(obj);
    if (it != originalLabelOf.constEnd() && !obj->label.isEmpty() && it.value() != obj->label)
        out.insert(it.value(), obj->label);
    for (const auto &c : obj->childs)
        collectLabelRenames(c.get(), originalLabelOf, out);
}

MenuEditorDialog::MenuEditorDialog(MuibObject *window, QStringList existingLabels,
                                   const MuibProject *project, QWidget *parent)
    : QDialog(parent), m_window(window), m_project(project), m_existingLabels(std::move(existingLabels))
{
    Q_ASSERT(window && window->type == ObjType::Window);
    setWindowTitle(tr("Menu Editor: %1")
                       .arg(window->title.isEmpty() ? window->label : window->title));
    resize(640, 560);

    m_root = cloneMenuTree(window->menu.get());   // nullptr in, nullptr out - see the header's own comment
    collectOriginalLabels(m_root.get(), m_originalLabelOf);   // baseline for labelRenames() - see its own comment
    buildActionChoices();

    m_tree = new QTreeWidget(this);
    m_tree->setColumnCount(4);
    m_tree->setHeaderLabels({ tr("Menu Name"), tr("CommKey"), tr("Menu Type"), tr("ID (internal)") });
    m_tree->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_tree->setSelectionMode(QAbstractItemView::SingleSelection);
    connect(m_tree, &QTreeWidget::currentItemChanged, this, &MenuEditorDialog::onSelectionChanged);

    // --- field panel (Name/ID/CommKey/Type + checkboxes) --------------
    auto *fieldsBox = new QGroupBox(tr("Entry"), this);
    auto *form = new QFormLayout();

    m_nameEdit = new QLineEdit(this);
    form->addRow(tr("Name:"), m_nameEdit);

    // "ID (intern)" - the internal label/identifier (see the class doc
    // comment's own section on this), pre-filled with the entry's
    // current label (an existing entry) or a live preview of the next
    // auto-generated default (a new one - see refreshIdPreviewForNewEntry()).
    // Restricted to valid-C-identifier characters right at input time -
    // isValidLabelId() re-checks the same shape server-side, since a
    // validator only governs typed/pasted input, not every code path
    // that could set the field's text.
    m_idEdit = new QLineEdit(this);
    m_idEdit->setValidator(new QRegularExpressionValidator(
        QRegularExpression(QStringLiteral("[A-Za-z_][A-Za-z0-9_]*")), m_idEdit));
    connect(m_idEdit, &QLineEdit::textEdited, this, [this](const QString &) { m_idFieldUserEdited = true; });
    form->addRow(tr("ID (internal):"), m_idEdit);

    m_commKeyEdit = new QLineEdit(this);
    m_commKeyEdit->setMaxLength(1);
    m_commKeyEdit->setMaximumWidth(40);
    form->addRow(tr("CommKey:"), m_commKeyEdit);

    m_typeCombo = new QComboBox(this);
    m_typeCombo->addItem(tr("Menu (top level)"));   // index 0 - new top-level SubMenu, under the tree root
    m_typeCombo->addItem(tr("Menu Item"));              // index 1 - new leaf MenuItem, under a top-level Menue
    m_typeCombo->addItem(tr("Submenu Item"));         // index 2 - new leaf MenuItem, under an existing Menuepunkt
    form->addRow(tr("Type (for \"Add\"):"), m_typeCombo);
    connect(m_typeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int) { updateFieldEnablement(); refreshIdPreviewForNewEntry(); });

    auto *checkGrid = new QGridLayout();
    m_menuBarCheck = new QCheckBox(tr("Menu Bar (Separator)"), this);
    m_checkCheck = new QCheckBox(tr("Checkit (on/off menu item)"), this);
    m_toggleCheck = new QCheckBox(tr("Toggle Mode"), this);
    m_checkedCheck = new QCheckBox(tr("Checked (initial state)"), this);
    m_enabledCheck = new QCheckBox(tr("Enabled"), this);
    checkGrid->addWidget(m_menuBarCheck, 0, 0);
    checkGrid->addWidget(m_checkCheck,   0, 1);
    checkGrid->addWidget(m_toggleCheck,  1, 0);
    checkGrid->addWidget(m_checkedCheck, 1, 1);
    checkGrid->addWidget(m_enabledCheck, 2, 0);
    connect(m_menuBarCheck, &QCheckBox::toggled, this, &MenuEditorDialog::onSeparatorToggled);
    connect(m_checkCheck, &QCheckBox::toggled, this, [this](bool) { updateFieldEnablement(); });

    m_mutualGroupLabel = new QLabel(tr("Mutual Exclude Group (0 = none):"), this);
    m_mutualGroupSpin = new QSpinBox(this);
    m_mutualGroupSpin->setRange(0, 99);
    auto *mutualRow = new QHBoxLayout();
    mutualRow->addWidget(m_mutualGroupLabel);
    mutualRow->addWidget(m_mutualGroupSpin);
    mutualRow->addStretch(1);

    // See the class doc comment's own "Aktion" section - a deliberately
    // small, curated set of real, ground-truth-verified target/action
    // pairs (never a made-up one), with one dedicated sentinel entry so
    // pre-existing, unrepresented notify wiring from a loaded real
    // .MUIB file is never silently discarded just by opening this dialog.
    m_actionLabel = new QLabel(tr("Action (on click):"), this);
    m_actionCombo = new QComboBox(this);
    for (const auto &choice : m_actionChoices)
        m_actionCombo->addItem(choice.label);
    auto *actionRow2 = new QHBoxLayout();
    actionRow2->addWidget(m_actionLabel);
    actionRow2->addWidget(m_actionCombo, 1);

    auto *fieldsLayout = new QVBoxLayout(fieldsBox);
    fieldsLayout->addLayout(form);
    fieldsLayout->addLayout(checkGrid);
    fieldsLayout->addLayout(mutualRow);
    fieldsLayout->addLayout(actionRow2);

    // --- Add/Delete/Modify/Move Up/Move Down row ----------------------
    m_addButton = new QPushButton(tr("Add"), this);
    m_modifyButton = new QPushButton(tr("Modify"), this);
    m_deleteButton = new QPushButton(tr("Delete"), this);
    m_moveUpButton = new QPushButton(tr("Move Up"), this);
    m_moveDownButton = new QPushButton(tr("Move Down"), this);
    connect(m_addButton, &QPushButton::clicked, this, &MenuEditorDialog::onAdd);
    connect(m_modifyButton, &QPushButton::clicked, this, &MenuEditorDialog::onModify);
    connect(m_deleteButton, &QPushButton::clicked, this, &MenuEditorDialog::onDelete);
    connect(m_moveUpButton, &QPushButton::clicked, this, &MenuEditorDialog::onMoveUp);
    connect(m_moveDownButton, &QPushButton::clicked, this, &MenuEditorDialog::onMoveDown);

    auto *actionRow = new QHBoxLayout();
    actionRow->addWidget(m_addButton);
    actionRow->addWidget(m_deleteButton);
    actionRow->addWidget(m_modifyButton);
    actionRow->addStretch(1);
    actionRow->addWidget(m_moveUpButton);
    actionRow->addWidget(m_moveDownButton);

    auto *okCancel = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(okCancel, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(okCancel, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto *outer = new QVBoxLayout(this);
    outer->addWidget(m_tree, 1);
    outer->addWidget(fieldsBox);
    outer->addLayout(actionRow);
    outer->addWidget(okCancel);

    rebuildTree();
    clearFieldsForNewEntry();
}

// See the class doc comment's own "Aktion" section for the full
// rationale. Every real choice here uses srcType 0 - MenuItem's own
// "MenuTriggered" event (eventsMenuItem()[0] == MUIA_Menuitem_Trigger /
// MUIV_EveryTime, confirmed against the real MUIBuilder v2.3 source via
// notifytables.cpp, the same event this whole session's earlier "Quit
// menu item doesn't react" bugfix relies on) - "this item was clicked",
// the only sensible trigger for a menu action. destType indices are
// likewise read straight out of the real, ground-truth notifytables.cpp
// tables (actionsAppli()[1] == ReturnQuit; actionsWindow()[0]/[1]/[2] ==
// Open TRUE/Open FALSE/Activate TRUE) - never invented numbers.
void MenuEditorDialog::buildActionChoices()
{
    m_actionChoices.clear();
    m_actionChoices.push_back({ tr("None"), QString(), 0, -1, false });
    m_actionChoices.push_back({ tr("Quit Application"), QStringLiteral("App"), 0, 1, false });

    if (m_project)
    {
        for (const auto &w : m_project->windows)
        {
            if (!w || w->type != ObjType::Window || w->label.isEmpty())
                continue;
            if (w.get() == m_window)
                continue;   // a window can't sensibly open/close/activate itself via its own menu
            const QString title = w->title.isEmpty() ? w->label : w->title;
            m_actionChoices.push_back({ tr("Open Window: %1").arg(title), w->label, 0, 0, false });
            m_actionChoices.push_back({ tr("Close Window: %1").arg(title), w->label, 0, 1, false });
            m_actionChoices.push_back({ tr("Activate Window: %1").arg(title), w->label, 0, 2, false });
        }
    }

    // Always last, always present - see the class doc comment on why
    // this MUST exist and MUST be selected automatically whenever
    // obj->notify doesn't match one of the real choices above.
    m_actionChoices.push_back({ tr("Leave unchanged (wiring not represented by this editor)"),
                                 QString(), 0, -1, true });
}

// Writes `obj->notify` from the currently selected m_actionCombo entry -
// unless it's the "preserve existing" sentinel (see the class doc
// comment), in which case obj->notify is left completely untouched.
void MenuEditorDialog::applyActionChoiceTo(MuibObject *obj)
{
    const int idx = m_actionCombo->currentIndex();
    if (idx < 0 || idx >= static_cast<int>(m_actionChoices.size()))
        return;
    const ActionChoice &choice = m_actionChoices[static_cast<size_t>(idx)];
    if (choice.isPreserveExisting)
        return;
    obj->notify.clear();
    if (!choice.targetLabel.isEmpty())
    {
        NotifyEvent evt;
        evt.targetLabel = choice.targetLabel;
        evt.srcType = choice.srcType;
        evt.destType = choice.destType;
        obj->notify.push_back(evt);
    }
}

std::unique_ptr<MuibObject> MenuEditorDialog::takeEditedMenu()
{
    // An empty root (every top-level entry deleted) means "no menu at
    // all" - same as never having had one, so normalize it away here
    // rather than making every caller check childs.empty() as well as
    // nullptr.
    if (m_root && m_root->childs.empty())
        return nullptr;
    return std::move(m_root);
}

QHash<QString, QString> MenuEditorDialog::labelRenames() const
{
    QHash<QString, QString> renames;
    collectLabelRenames(m_root.get(), m_originalLabelOf, renames);
    return renames;
}

QTreeWidgetItem *MenuEditorDialog::addTreeItem(QTreeWidgetItem *parent, MuibObject *obj)
{
    auto *item = new QTreeWidgetItem();
    const bool isSeparator = obj->type == ObjType::MenuItem && obj->name.startsWith(QStringLiteral("BarLabel"));

    item->setText(0, isSeparator ? tr("(Separator)") : obj->name);
    item->setText(1, obj->menuKey != '\0' ? QString(QChar(QLatin1Char(obj->menuKey))) : QString());

    // Depth-based, like everything else in this dialog now (see the class
    // doc comment) - deliberately NOT obj->type, so a Menuepunkt that has
    // been silently widened into a SubMenu container (because it now has
    // an Untermenuepunkt of its own) still reads as "Menuepunkt" here,
    // matching how the user thinks about it, not the internal kind.
    QString typeText;
    if (isSeparator)
        typeText = tr("Separator");
    else
    {
        const int depth = depthOf(m_root.get(), obj);
        typeText = (depth <= 1) ? tr("Menu") : (depth == 2 ? tr("Menu Item") : tr("Submenu Item"));
    }
    item->setText(2, typeText);
    // Shown per the Chefentwickler's own follow-up request - always in
    // sync with the real label, since rebuildTree() (called after every
    // Add/Aendern - see onAdd()/onModify()) tears down and re-adds every
    // item from scratch via this same function.
    item->setText(3, obj->label);
    item->setData(0, Qt::UserRole, ptrToVariant(obj));

    if (parent)
        parent->addChild(item);
    else
        m_tree->addTopLevelItem(item);

    for (const auto &c : obj->childs)
        addTreeItem(item, c.get());
    return item;
}

void MenuEditorDialog::rebuildTree()
{
    m_tree->clear();
    if (m_root)
    {
        for (const auto &c : m_root->childs)
            addTreeItem(nullptr, c.get());
    }
    m_tree->expandAll();
    updateButtonStates();
}

static QTreeWidgetItem *findItemForRec(QTreeWidgetItem *item, MuibObject *obj)
{
    if (variantToPtr(item->data(0, Qt::UserRole)) == obj)
        return item;
    for (int i = 0; i < item->childCount(); ++i)
    {
        if (auto *found = findItemForRec(item->child(i), obj))
            return found;
    }
    return nullptr;
}

static void selectObjectInTree(QTreeWidget *tree, MuibObject *obj)
{
    if (!obj)
    {
        tree->setCurrentItem(nullptr);
        return;
    }
    for (int i = 0; i < tree->topLevelItemCount(); ++i)
    {
        if (auto *found = findItemForRec(tree->topLevelItem(i), obj))
        {
            tree->setCurrentItem(found);
            return;
        }
    }
}

MuibObject *MenuEditorDialog::selectedObject() const
{
    QTreeWidgetItem *item = m_tree->currentItem();
    return item ? variantToPtr(item->data(0, Qt::UserRole)) : nullptr;
}

void MenuEditorDialog::onSelectionChanged()
{
    MuibObject *obj = selectedObject();
    if (obj)
        loadFieldsFrom(obj);
    else
        clearFieldsForNewEntry();
    updateButtonStates();
}

void MenuEditorDialog::loadFieldsFrom(MuibObject *obj)
{
    const bool isSeparator = obj->type == ObjType::MenuItem && obj->name.startsWith(QStringLiteral("BarLabel"));

    m_nameEdit->setText(isSeparator ? QString() : obj->name);
    // Pre-filled with the CURRENT label (whatever it is - the original
    // auto-generated default, or an earlier rename from this same dialog
    // session) rather than left blank, per the Chefentwickler's own
    // request ("ein String Widget, das bereits den Default-Wert
    // enthaelt"). m_idFieldUserEdited is reset here too - it only tracks
    // edits made WHILE this object stays selected (see applyIdFieldTo()'s
    // own comment); switching the tree selection away and back always
    // reloads the real current value fresh.
    m_idEdit->setText(obj->label);
    m_idFieldUserEdited = false;
    m_commKeyEdit->setText(obj->menuKey != '\0' ? QString(QChar(QLatin1Char(obj->menuKey))) : QString());
    m_enabledCheck->setChecked(obj->menu_enable);
    m_menuBarCheck->setChecked(isSeparator);
    m_checkCheck->setChecked(obj->check_enable);
    m_checkedCheck->setChecked(obj->check_state);
    m_toggleCheck->setChecked(obj->toggleMenu);
    m_mutualGroupSpin->setValue(obj->excludeGroup);
    // Reflects this item's own DEPTH as a sensible default for the NEXT
    // "Hinzufuegen" click (see the class doc comment), but stays fully
    // editable - "Aendern" never reads this combo at all (see
    // applyFieldsTo()'s own comment), so changing it here never risks
    // silently converting the selected item's own kind.
    const int depth = depthOf(m_root.get(), obj);
    m_typeCombo->setCurrentIndex(depth <= 1 ? 0 : (depth == 2 ? 1 : 2));

    // Match this item's REAL obj->notify against the curated choices -
    // "Keine" if empty, the matching real choice if it's a single event
    // exactly equal to one of them, otherwise the "Unveraendert lassen"
    // sentinel (always the last entry) so nothing unrepresented is ever
    // silently lost - see the class doc comment's own "Aktion" section.
    int actionIdx = -1;
    if (obj->notify.isEmpty())
    {
        actionIdx = 0;   // "Keine" is always choices[0]
    }
    else if (obj->notify.size() == 1)
    {
        const NotifyEvent &evt = obj->notify.front();
        for (size_t i = 1; i + 1 < m_actionChoices.size(); ++i)   // skip "Keine" and the trailing sentinel
        {
            const ActionChoice &c = m_actionChoices[i];
            if (c.targetLabel == evt.targetLabel && c.srcType == evt.srcType &&
                c.destType == evt.destType && evt.argString.isEmpty())
            {
                actionIdx = static_cast<int>(i);
                break;
            }
        }
    }
    if (actionIdx < 0)
        actionIdx = static_cast<int>(m_actionChoices.size()) - 1;   // the sentinel
    m_actionCombo->setCurrentIndex(actionIdx);

    updateFieldEnablement();
}

void MenuEditorDialog::clearFieldsForNewEntry()
{
    m_nameEdit->clear();
    m_commKeyEdit->clear();
    m_enabledCheck->setChecked(true);
    m_menuBarCheck->setChecked(false);
    m_checkCheck->setChecked(false);
    m_checkedCheck->setChecked(false);
    m_toggleCheck->setChecked(false);
    m_mutualGroupSpin->setValue(0);
    m_typeCombo->setCurrentIndex(0);
    m_actionCombo->setCurrentIndex(0);   // "Keine" - a brand-new entry has no pre-existing wiring to preserve
    m_idFieldUserEdited = false;
    refreshIdPreviewForNewEntry();
    updateFieldEnablement();
}

void MenuEditorDialog::onSeparatorToggled(bool)
{
    updateFieldEnablement();
}

void MenuEditorDialog::updateButtonStates()
{
    MuibObject *obj = selectedObject();
    m_modifyButton->setEnabled(obj != nullptr);
    m_deleteButton->setEnabled(obj != nullptr);

    bool canMoveUp = false, canMoveDown = false;
    if (obj && obj->father)
    {
        auto &siblings = obj->father->childs;
        auto it = std::find_if(siblings.begin(), siblings.end(),
                                [obj](const std::unique_ptr<MuibObject> &c) { return c.get() == obj; });
        if (it != siblings.end())
        {
            const auto idx = std::distance(siblings.begin(), it);
            canMoveUp = idx > 0;
            canMoveDown = static_cast<size_t>(idx) + 1 < siblings.size();
        }
    }
    m_moveUpButton->setEnabled(canMoveUp);
    m_moveDownButton->setEnabled(canMoveDown);
}

QString MenuEditorDialog::makeUniqueLabel(const QString &base)
{
    if (!m_existingLabels.contains(base))
        return base;
    int n = 2;
    while (m_existingLabels.contains(base + QStringLiteral("_%1").arg(n)))
        ++n;
    return base + QStringLiteral("_%1").arg(n);
}

// Grey out whichever fields don't apply to what the Type combo (for a
// future "Hinzufuegen") or the "Menue-Bar" checkbox currently describe -
// a SubMenu never has CommKey/Checkit/Toggle/Checked/Mutual-Group (see
// muicodegen.cpp's own ObjType::SubMenu case: only Title + MUIA_Menu_
// Enabled are ever emitted for it), and a separator has none of Name/
// CommKey/Checkit/Toggle/Checked/Mutual-Group/Aktiviert either (it is a
// bare MUI_MakeObject(MUIO_Menuitem, NM_BARLABEL, ...) call with no
// attribute list at all - see muicodegen.cpp's BarLabel special case).
void MenuEditorDialog::updateFieldEnablement()
{
    const bool isContainer = m_typeCombo->currentIndex() == 0;   // "Menue" - the only combo choice that creates a SubMenu
    const bool isSeparator = !isContainer && m_menuBarCheck->isChecked();

    m_menuBarCheck->setEnabled(!isContainer);
    m_nameEdit->setEnabled(!isSeparator);
    if (isSeparator)
        m_nameEdit->clear();

    const bool leafFieldsEnabled = !isContainer && !isSeparator;
    m_commKeyEdit->setEnabled(leafFieldsEnabled);
    m_checkCheck->setEnabled(leafFieldsEnabled);

    const bool checkitFieldsEnabled = leafFieldsEnabled && m_checkCheck->isChecked();
    m_toggleCheck->setEnabled(checkitFieldsEnabled);
    m_checkedCheck->setEnabled(checkitFieldsEnabled);
    m_mutualGroupSpin->setEnabled(checkitFieldsEnabled);
    m_mutualGroupLabel->setEnabled(checkitFieldsEnabled);

    // "Aktion" only ever applies to a real, clickable leaf MenuItem - not
    // a separator (a bare MUI_MakeObject() call, never referenceable by
    // ident) and not a container (a pulldown title/flyout submenu is
    // opened, not "clicked"; see applyFieldsTo()'s own comment on why
    // its notify is deliberately left untouched instead of reusing this
    // combo for it).
    m_actionCombo->setEnabled(leafFieldsEnabled);
    m_actionLabel->setEnabled(leafFieldsEnabled);

    m_enabledCheck->setEnabled(!isSeparator);
}

// Only meaningful while an EXISTING entry is selected (selectedObject()
// != nullptr) - onAdd() reads m_idEdit's text directly itself instead of
// going through this, since a brand-new object isn't in m_existingLabels
// yet and needs slightly different "empty means auto-generate" handling
// (see onAdd()'s own comment). Validates and, on success, applies
// m_idEdit's current text as obj's new label, keeping m_existingLabels in
// sync (remove the old label, add the new one) so later uniqueness
// checks - both this dialog's own makeUniqueLabel() and, after this
// dialog closes, MainWindow's next drop/create elsewhere in the project -
// stay correct. Returns false (and shows a QMessageBox explaining why,
// applying nothing) if the text is empty, not a valid C identifier shape,
// or already used by some OTHER object; leaves obj->label untouched (and
// returns true) if the text is unchanged from obj->label already, so
// Aendern on an object whose ID field wasn't touched never has to pay the
// uniqueness-check cost or risk a false "already used by ... itself"
// rejection.
bool MenuEditorDialog::applyIdFieldTo(MuibObject *obj)
{
    const QString text = m_idEdit->text().trimmed();
    if (text == obj->label)
        return true;

    QString error;
    if (!validateLabelId(text, m_existingLabels, &error))
    {
        QMessageBox::warning(this, tr("Menu Editor"), error);
        return false;
    }

    m_existingLabels.removeAll(obj->label);
    obj->label = text;
    m_existingLabels << text;
    return true;
}

// Keeps the ID field showing a live, still-unique preview of what a
// brand-new entry's auto-generated label WOULD be, for as long as the
// user hasn't typed a custom one themselves - mirrors onAdd()'s own
// makeUniqueLabel(typeIdx == 0 ? "Menu" : "MenuItem") call so what's
// previewed here always matches what onAdd() would actually assign if
// the user adds the entry without touching this field. Called whenever
// something that could change that preview happens (the Type combo
// changing between "Menue" and a leaf kind, or a fresh clearFieldsForNewEntry()) -
// safe to call unconditionally in both situations since it no-ops
// whenever it wouldn't apply.
void MenuEditorDialog::refreshIdPreviewForNewEntry()
{
    if (selectedObject())
        return;   // an existing entry is selected - loadFieldsFrom() owns the field then, not this
    if (m_idFieldUserEdited)
        return;   // don't clobber an ID the user already typed for the entry-to-be-added
    m_idEdit->setText(makeUniqueLabel(m_typeCombo->currentIndex() == 0
                                           ? QStringLiteral("Menu")
                                           : QStringLiteral("MenuItem")));
}

bool MenuEditorDialog::applyFieldsTo(MuibObject *obj)
{
    // Deliberately never reads m_typeCombo - "Aendern" (Modify) can never
    // change an existing entry's real kind (SubMenu <-> MenuItem), only
    // its content - see the class doc comment's own "Type" section.
    if (!applyIdFieldTo(obj))
        return false;

    const bool isContainer = obj->type == ObjType::SubMenu;
    const bool isSeparator = !isContainer && m_menuBarCheck->isChecked();

    obj->menu_enable = m_enabledCheck->isChecked();
    obj->name = isSeparator ? QStringLiteral("BarLabel") : m_nameEdit->text();

    if (!isContainer)
    {
        const QString key = m_commKeyEdit->text();
        obj->menuKey = key.isEmpty() ? '\0' : key.at(0).toLatin1();
        if (isSeparator)
        {
            obj->check_enable = false;
            obj->check_state = false;
            obj->toggleMenu = false;
            obj->excludeGroup = 0;
            // A separator can never be referenced by ident (see
            // muicodegen.cpp's BarLabel special case - it's a bare
            // MUI_MakeObject() call with no attribute list, let alone a
            // captured variable), so any notify on it could never have
            // worked in the first place - clearing it here mirrors the
            // other leaf-only fields just above.
            obj->notify.clear();
        }
        else
        {
            obj->check_enable = m_checkCheck->isChecked();
            obj->check_state = m_checkCheck->isChecked() && m_checkedCheck->isChecked();
            obj->toggleMenu = m_checkCheck->isChecked() && m_toggleCheck->isChecked();
            obj->excludeGroup = m_checkCheck->isChecked() ? m_mutualGroupSpin->value() : 0;
            applyActionChoiceTo(obj);
        }
    }
    // isContainer (a real pulldown title/flyout submenu) deliberately
    // leaves obj->notify completely untouched - unlike a separator, a
    // SubMenu IS a real, valid notify source/target in the original tool
    // (see notifytables.cpp's own actionsSubMenu()/eventsFor(SubMenu)),
    // so a real loaded .MUIB file's container-level wiring, if any, must
    // never be silently discarded just because this dialog's own
    // curated "Aktion" combo only ever targets leaf MenuItems.
    return true;
}

void MenuEditorDialog::onAdd()
{
    // Which of the 3 real hierarchy levels this click creates - see the
    // class doc comment's rewritten "Type" section for the full
    // rationale (this replaces the old "container selected -> child,
    // leaf selected -> sibling" heuristic that caused the Chefentwickler's
    // bug report: new entries silently nesting inside whatever item the
    // combo happened to still be reflecting).
    const int typeIdx = m_typeCombo->currentIndex();   // 0=Menue, 1=Menuepunkt, 2=Untermenuepunkt
    const bool wantsSeparator = typeIdx != 0 && m_menuBarCheck->isChecked();

    // Resolve and validate the new entry's ID (intern) FIRST, before any
    // of the parent-resolution logic below runs - that logic can widen an
    // existing Menuepunkt into a SubMenu container as a side effect (see
    // the typeIdx==2 branch further down), which should never happen if
    // the Add is going to fail its own ID validation right afterwards
    // anyway. An empty field (the normal case - refreshIdPreviewForNewEntry()
    // keeps it pre-filled with a live preview already, but the user may
    // have cleared it deliberately, or scripted/pasted an empty value in)
    // falls back to the same makeUniqueLabel() call this used unconditionally
    // before this feature existed; a non-empty field is validated exactly
    // like applyIdFieldTo() validates one for Aendern.
    QString newLabel = m_idEdit->text().trimmed();
    if (newLabel.isEmpty())
    {
        newLabel = makeUniqueLabel(typeIdx == 0 ? QStringLiteral("Menu") : QStringLiteral("MenuItem"));
    }
    else
    {
        QString error;
        if (!validateLabelId(newLabel, m_existingLabels, &error))
        {
            QMessageBox::warning(this, tr("Menu Editor"), error);
            return;
        }
    }

    MuibObject *selected = selectedObject();
    MuibObject *parent = nullptr;

    if (typeIdx == 0)
    {
        // "Menue" - always a new top-level SubMenu directly under the
        // tree root, no matter what is selected; reorder afterwards with
        // "Nach oben"/"Nach unten" if it should land somewhere else.
        if (!m_root)
            m_root = std::make_unique<MuibObject>(ObjType::Menu);
        parent = m_root.get();
    }
    else if (typeIdx == 1)
    {
        // "Menuepunkt" - a leaf directly under the RELEVANT top-level
        // Menue, found by climbing up from the current selection to its
        // depth-1 ancestor - works the same whether the Menue itself, one
        // of its Menuepunkte, or a nested Untermenuepunkt is selected;
        // the new entry always lands as a sibling at THIS level, never as
        // a child of whatever was actually selected.
        MuibObject *cur = selected;
        while (cur && depthOf(m_root.get(), cur) > 1)
            cur = cur->father;
        if (!cur)
        {
            QMessageBox::information(this, tr("Add Menu Item"),
                tr("Please first select the menu (or one of its entries) to "
                   "which the new menu item should be assigned."));
            return;
        }
        parent = cur;
    }
    else
    {
        // "Untermenuepunkt" - a leaf one level below an EXISTING
        // Menuepunkt, found by climbing up from the current selection to
        // its depth-2 ancestor. That Menuepunkt is widened from a leaf
        // MenuItem into a container SubMenu right here if it isn't one
        // already - the one place this dialog ever changes an existing
        // entry's real kind (see the class doc comment) - always safe,
        // since a childless SubMenu already renders identically to a
        // plain MenuItem leaf (muicodegen.cpp), so nothing already
        // generated changes until the new child actually exists.
        MuibObject *cur = selected;
        while (cur && depthOf(m_root.get(), cur) > 2)
            cur = cur->father;
        if (!cur || depthOf(m_root.get(), cur) != 2)
        {
            QMessageBox::information(this, tr("Add Submenu Item"),
                tr("Please first select a menu item (not the menu itself), "
                   "under which the new submenu item should go."));
            return;
        }
        if (cur->type != ObjType::SubMenu)
        {
            cur->type = ObjType::SubMenu;
            // A container never uses these leaf-only fields (only Title +
            // MUIA_Menu_Enabled are ever emitted for a SubMenu - see
            // muicodegen.cpp) - clear them so this entry doesn't keep
            // showing stale leaf state if it's selected again later.
            cur->menuKey = '\0';
            cur->check_enable = false;
            cur->check_state = false;
            cur->toggleMenu = false;
            cur->excludeGroup = 0;
        }
        parent = cur;
    }

    auto newObj = std::make_unique<MuibObject>(typeIdx == 0 ? ObjType::SubMenu : ObjType::MenuItem);
    newObj->menu_enable = m_enabledCheck->isChecked();
    if (wantsSeparator)
    {
        newObj->name = QStringLiteral("BarLabel");
    }
    else
    {
        newObj->name = m_nameEdit->text().isEmpty()
                            ? (typeIdx == 0 ? QStringLiteral("New Menu") : QStringLiteral("New Item"))
                            : m_nameEdit->text();
        if (typeIdx != 0)
        {
            const QString key = m_commKeyEdit->text();
            newObj->menuKey = key.isEmpty() ? '\0' : key.at(0).toLatin1();
            newObj->check_enable = m_checkCheck->isChecked();
            newObj->check_state = m_checkCheck->isChecked() && m_checkedCheck->isChecked();
            newObj->toggleMenu = m_checkCheck->isChecked() && m_toggleCheck->isChecked();
            newObj->excludeGroup = m_checkCheck->isChecked() ? m_mutualGroupSpin->value() : 0;
        }
    }
    newObj->label = newLabel;

    MuibObject *rawNew = appendChildToMenu(parent, std::move(newObj));
    // Same leaf-only gating as applyFieldsTo()'s own applyActionChoiceTo()
    // call - never for a new top-level Menue (typeIdx==0) or a separator.
    if (typeIdx != 0 && !wantsSeparator)
        applyActionChoiceTo(rawNew);
    m_existingLabels << rawNew->label;
    rebuildTree();
    selectObjectInTree(m_tree, rawNew);
    // The just-added entry is now selected in the tree (selectObjectInTree()
    // above triggers onSelectionChanged() -> loadFieldsFrom(), which
    // already re-populates m_idEdit with rawNew->label and resets
    // m_idFieldUserEdited) - nothing further to do here for the ID field.
}

void MenuEditorDialog::onModify()
{
    MuibObject *obj = selectedObject();
    if (!obj)
        return;
    // applyFieldsTo() already showed a QMessageBox explaining why on a
    // validation failure (currently only the ID field can fail) and left
    // obj completely untouched - so on false, skip the rebuild/reselect
    // below too and just leave the dialog exactly as the user sees it,
    // ready to correct the field and try again.
    if (!applyFieldsTo(obj))
        return;
    rebuildTree();
    selectObjectInTree(m_tree, obj);
}

void MenuEditorDialog::onDelete()
{
    MuibObject *obj = selectedObject();
    if (!obj)
        return;
    MuibObject *parent = obj->father;
    removeMenuChild(obj);   // destroys obj and its whole subtree

    // Mirror image of onAdd()'s "Untermenuepunkt" widening: if this was
    // the last child of a Menuepunkt that got converted from a leaf
    // MenuItem into a SubMenu container just to hold it, convert it back
    // now that it's empty again - otherwise it would keep showing up
    // internally as a container (isContainer in applyFieldsTo()/
    // updateFieldEnablement()) with no visible difference in the tree
    // (addTreeItem() already shows it as "Menuepunkt" by depth, not by
    // ObjType) but with its own CommKey/Checkit/etc. fields permanently
    // greyed out in "Aendern" - confusing since nothing about it still
    // looks like a container.
    if (parent && parent != m_root.get() && parent->type == ObjType::SubMenu &&
        parent->childs.empty() && depthOf(m_root.get(), parent) == 2)
    {
        parent->type = ObjType::MenuItem;
    }

    rebuildTree();
    selectObjectInTree(m_tree, (parent && parent != m_root.get()) ? parent : nullptr);
}

void MenuEditorDialog::onMoveUp()
{
    MuibObject *obj = selectedObject();
    if (obj && moveMenuChildUpDown(obj, true))
    {
        rebuildTree();
        selectObjectInTree(m_tree, obj);
    }
}

void MenuEditorDialog::onMoveDown()
{
    MuibObject *obj = selectedObject();
    if (obj && moveMenuChildUpDown(obj, false))
    {
        rebuildTree();
        selectObjectInTree(m_tree, obj);
    }
}
