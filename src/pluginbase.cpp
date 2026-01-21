// Copyright (c) 2022-2025 Manuel Schneider

#include "pluginbase.h"
#include "terminal.h"
#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QLabel>
#include <QSettings>
#include <QSignalBlocker>
#include <QVBoxLayout>
#include <QWidget>
#include <albert/icon.h>
#include <albert/indexitem.h>
#include <albert/logging.h>
#include <albert/messagebox.h>
#include <albert/widgetsutil.h>
ALBERT_LOGGING_CATEGORY("apps")
using namespace Qt::StringLiterals;
using namespace albert;
using namespace std;

static const auto ck_terminal = "terminal";
static const auto ck_use_non_localized_name = "use_non_localized_name";
static const auto ck_split_camel_case = "split_camel_case";
static const auto ck_use_acronyms = "use_acronyms";

const map<QString, QStringList> PluginBase::exec_args  // command > ExecArg
{
    {u"alacritty"_s, {u"-e"_s}},
    // {"asbru-cm", {}},
    {u"blackbox"_s, {u"--"_s}},
    {u"blackbox-terminal"_s, {u"--"_s}},
    // {"byobu", {}},
    // {"com.github.amezin.ddterm", {}},
    {u"contour"_s, {u"--"_s}},
    {u"cool-retro-term"_s, {u"-e"_s}},
    // {"cosmic-term", {}},
    {u"deepin-terminal"_s, {u"-e"_s}},
    // {"deepin-terminal-gtk", {u"-e"_s}},  // archived
    // {"domterm", {}},
    // {"electerm", {}},
    // {"fish", {}},
    {u"foot"_s, {}},  // yes empty
    {u"footclient"_s, {}},  // yes empty
    // {"gmrun", {}},
    {u"gnome-terminal"_s, {u"--"_s}},
    {u"ghostty"_s, {u"-e"_s}},
    // {"guake", {}},
    // {"hyper", {}},
    {u"io.elementary.terminal"_s, {u"-x"_s}},
    {u"kgx"_s, {u"-e"_s}},
    {u"kitty"_s, {u"--"_s}},
    {u"konsole"_s, {u"-e"_s}},
    {u"lxterminal"_s, {u"-e"_s}},
    {u"mate-terminal"_s, {u"-x"_s}},
    // {"mlterm", {}},
    // {"pangoterm", {}},
    // {"pods", {}},
    {u"ptyxis"_s, {u"--"_s}},
    // {"qtdomterm", {}},
    {u"qterminal"_s, {u"-e"_s}},
    {u"roxterm"_s, {u"-x"_s}},
    {u"sakura"_s, {u"-e"_s}},
    {u"st"_s, {u"-e"_s}},
    // {"tabby.AppImage", {}},
    {u"terminator"_s, {u"-u"_s, u"-x"_s}},  // https://github.com/gnome-terminator/terminator/issues/939
    {u"terminology"_s, {u"-e"_s}},
    // {"terminus", {}},
    // {"termit", {}},
    {u"termite"_s, {u"-e"_s}},
    // {"termius", {}},
    // {"tilda", {}},
    {u"tilix"_s, {u"-e"_s}},
    // {"txiterm", {}},
    {u"urxvt"_s, {u"-e"_s}},
    {u"urxvt-tabbed"_s, {u"-e"_s}},
    {u"urxvtc"_s, {u"-e"_s}},
    {u"uxterm"_s, {u"-e"_s}},
    // {"warp-terminal", {}},
    // {"waveterm", {}},
    {u"wezterm"_s, {u"-e"_s}},
    {u"x-terminal-emulator"_s, {u"-e"_s}},
    // {"x3270a", {}},
    {u"xfce4-terminal"_s, {u"-x"_s}},
    {u"xterm"_s, {u"-e"_s}},
    // {"yakuake", {}},
    // {"zutty", {}},
};

QString PluginBase::defaultTrigger() const { return u"apps "_s; }

void PluginBase::updateIndexItems()  { indexer.run(); }

void PluginBase::commonInitialize(const QSettings &s)
{
    use_non_localized_name_ = s.value(ck_use_non_localized_name, false).value<bool>();
    split_camel_case_       = s.value(ck_split_camel_case, false).value<bool>();
    use_acronyms_           = s.value(ck_use_acronyms, false).value<bool>();
}

void PluginBase::setUserTerminalFromConfig()
{
    if (terminals.empty())
    {
        WARN << "No terminals available.";
        terminal = nullptr;
    }
    else if (auto s = settings(); !s->contains(ck_terminal))  // unconfigured
    {
        terminal = *terminals.begin();  // guaranteed to exist since not empty
        WARN << u"No terminal configured. Using %1."_s
                    .arg(terminal->name());
    }
    else  // user configured
    {
        auto term_id = s->value(ck_terminal).toString();
        auto term_it = ranges::find_if(terminals, [&](const auto *t){ return t->id() == term_id; });
        if (term_it != terminals.end())
            terminal = *term_it;
        else
        {
            terminal = *terminals.begin();  // guaranteed to exist since not empty
            WARN << u"Configured terminal '%1' does not exist. Using %2."_s
                        .arg(term_id, terminal->id());
        }
    }
}

QWidget *PluginBase::createTerminalFormWidget()
{
    auto *w = new QWidget();
    auto *cb = new QComboBox;
    auto *l = new QVBoxLayout;
    auto *lbl = new QLabel;

    auto updateTerminalsCheckBox = [this, cb]
    {
        QSignalBlocker block(cb);
        cb->clear();

        auto sorted_terminals = terminals;
        ranges::sort(sorted_terminals, [](const auto *t1, const auto *t2)
                     { return t1->name().compare(t2->name(), Qt::CaseInsensitive) < 0; });

        for (uint i = 0; i < sorted_terminals.size(); ++i)
        {
            const auto t = sorted_terminals.at(i);
            cb->addItem(Icon::qIcon(t->icon()), t->name(), t->id());
            cb->setItemData(i, t->id(), Qt::ToolTipRole);
            if (t->id() == terminal->id())  // is current
                cb->setCurrentIndex(i);
        }
    };

    connect(this, &PluginBase::appsChanged, cb, updateTerminalsCheckBox);

    updateTerminalsCheckBox();

    connect(cb, qOverload<int>(&QComboBox::currentIndexChanged), this, [this, cb](int index)
    {
        auto term_id = cb->itemData(index).toString();
        if (auto it = ranges::find_if(terminals, [&](const auto &t){ return t->id() == term_id; });
            it != terminals.end())
        {
            terminal = *it;
            settings()->setValue(ck_terminal, term_id);
            DEBG << "Terminal set to" << term_id;
        }
        else
            WARN << "Selected terminal vanished:" << term_id;
    });

    QString t = u"https://github.com/albertlauncher/albert/issues/new/choose"_s;
    t = tr(R"(Report missing terminals <a href="%1">here</a>.)").arg(t);
    t = uR"(<span style="font-size:9pt; color:#808080;">%1</span>)"_s.arg(t);
    lbl->setText(t);
    lbl->setOpenExternalLinks(true);

    l->addWidget(cb);
    l->addWidget(lbl);
    l->setContentsMargins(0,0,0,0);

    w->setLayout(l);

    return w;
}

void PluginBase::addBaseConfig(QFormLayout *l)
{
    auto *cb = new QCheckBox;
    l->addRow(tr("Use non-localized name"), cb);
    bindWidget(cb, this, &PluginBase::useNonLocalizedName, &PluginBase::setUseNonLocalizedName);

    cb = new QCheckBox;
    l->addRow(tr("Split CamelCase words (medial capital)"), cb);
    bindWidget(cb, this, &PluginBase::splitCamelCase, &PluginBase::setSplitCamelCase);

    cb = new QCheckBox;
    l->addRow(tr("Use acronyms"), cb);
    bindWidget(cb, this, &PluginBase::useAcronyms, &PluginBase::setUseAcronyms);

    l->addRow(tr("Terminal"), createTerminalFormWidget());
}

vector<IndexItem> PluginBase::buildIndexItems() const
{
    vector<IndexItem> r;

    for (const auto &iapp : applications)
    {
        auto app = static_pointer_cast<Application>(iapp);
        for (const auto &name : app->names())
        {
            r.emplace_back(app, name);

            // https://en.wikipedia.org/wiki/Combining_Diacritical_Marks
            static QRegularExpression re(uR"([\x{0300}-\x{036f}])"_s);
            auto normalized = name.normalized(QString::NormalizationForm_D).remove(re);

            auto ccs = camelCaseSplit(normalized);

            if (split_camel_case_)
                r.emplace_back(app, ccs.join(QChar::Space));

            if (use_acronyms_)
            {
                QString acronym;
                for (const auto &w : as_const(ccs))
                    if (w.size())
                        acronym.append(w[0]);

                if (acronym.size() > 1)
                    r.emplace_back(app, acronym);
            }
        }
    }

    return r;
}

QStringList PluginBase::camelCaseSplit(const QString &s)
{
    static QRegularExpression re(uR"([A-Z0-9]?[a-z]+|[A-Z0-9]+(?![a-z]))"_s);
    auto it = re.globalMatch(s);

    QStringList words;
    while (it.hasNext())
        words << it.next().captured();

    return words;
}

void PluginBase::runTerminal(const QString &script) const
{
    if (terminal)
        terminal->launch(script);
    else
        warning(tr("No terminal available."));
}

bool PluginBase::useNonLocalizedName() const { return use_non_localized_name_; }

void PluginBase::setUseNonLocalizedName(bool v)
{
    if (use_non_localized_name_ != v)
    {
        settings()->setValue(ck_use_non_localized_name, v);
        use_non_localized_name_ = v;
        updateIndexItems();
    }
}

bool PluginBase::splitCamelCase() const { return split_camel_case_; }

void PluginBase::setSplitCamelCase(bool v)
{
    if (split_camel_case_ != v)
    {
        settings()->setValue(ck_split_camel_case, v);
        split_camel_case_ = v;
        setIndexItems(buildIndexItems());
    }
}

bool PluginBase::useAcronyms() const { return use_acronyms_; }

void PluginBase::setUseAcronyms(bool v)
{
    if (use_acronyms_ != v)
    {
        settings()->setValue(ck_use_acronyms, v);
        use_acronyms_ = v;
        setIndexItems(buildIndexItems());
    }
}
