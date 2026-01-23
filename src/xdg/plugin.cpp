// Copyright (c) 2022-2026 Manuel Schneider

#include "application.h"
#include "plugin.h"
#include "terminal.h"
#include "ui_configwidget.h"
#include <QComboBox>
#include <QDirIterator>
#include <QFileInfo>
#include <QLabel>
#include <QRegularExpression>
#include <QSettings>
#include <QSignalBlocker>
#include <QStandardPaths>
#include <QVBoxLayout>
#include <QWidget>
#include <albert/logging.h>
#include <albert/icon.h>
#include <albert/messagebox.h>
#include <albert/widgetsutil.h>
using namespace Qt::StringLiterals;
using namespace albert;
using namespace std;

static const auto ck_terminal = "terminal";
static const auto ck_ignore_show_in_keys = "ignore_show_in_keys";
static const auto ck_use_exec            = "use_exec";
static const auto ck_use_generic_name    = "use_generic_name";
static const auto ck_use_keywords        = "use_keywords";

const map<QString, QStringList> Plugin::exec_args  // command > ExecArg
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

static QString normalizedContainerCommand(const QStringList &Exec)
{
    QString command;

    // Todo de-env
    // e.g. env TERM=xterm-256color byobu

    // Flatpak
    if (QFileInfo(Exec.at(0)).fileName() == u"flatpak"_s)
    {
        for (const auto &arg : Exec)
            if (arg.startsWith(u"--command="_s))
                command = arg.mid(10);  // size of '--command='

        if (command.isEmpty())
            WARN << "Flatpak exec commandline w/o '--command':" << Exec.join(QChar::Space);
    }

    // Snapcraft
    else if (auto it = find_if(Exec.begin(), Exec.end(),
                               [](const auto &arg){ return arg.startsWith(u"/snap/bin/"_s); });
             it != Exec.end())
    {
        if (command = it->mid(10); command.isEmpty())  // size of '/snap/bin/'
            WARN << "Failed getting snap command: Exec:" << Exec.join(QChar::Space);
    }

    // Native command
    else
        command = QFileInfo(Exec.at(0)).fileName();

    return command;
}

static QStringList appDirectories()
{ return QStandardPaths::standardLocations(QStandardPaths::ApplicationsLocation); }

Plugin* plugin = nullptr;

Plugin::Plugin()
{
    qunsetenv("DESKTOP_AUTOSTART_ID");
    plugin = this;

    fs_watcher.addPaths(appDirectories());
    connect(&fs_watcher, &QFileSystemWatcher::directoryChanged, this, &Plugin::updateIndexItems);

    // Load settings

    const auto s = settings();
    commonInitialize(*s);
    ignore_show_in_keys_ = s->value(ck_ignore_show_in_keys, true).value<bool>();
    use_exec_            = s->value(ck_use_exec, false).value<bool>();
    use_generic_name_    = s->value(ck_use_generic_name, false).value<bool>();
    use_keywords_        = s->value(ck_use_keywords, false).value<bool>();

    // File watches

    for (const auto &path : QStandardPaths::standardLocations(QStandardPaths::ApplicationsLocation))
        for (auto dit = QDirIterator(path, QDir::Dirs|QDir::NoDotDot, QDirIterator::Subdirectories); dit.hasNext();)
            fs_watcher.addPath(QFileInfo(dit.next()).canonicalFilePath());

    connect(&fs_watcher, &QFileSystemWatcher::directoryChanged,
            this, [this](){ indexer.run(); });


    // Indexer

    indexer.parallel = [this](const bool &abort) -> vector<shared_ptr<applications::Application>>
    {
        // Get a map of unique desktop entries according to the spec

        map<QString, QString> desktop_files;  // Desktop id > path
        for (const QString &dir : appDirectories())
        {
            DEBG << "Scanning desktop entries in:" << dir;

            QDirIterator it(dir, {u"*.desktop"_s}, QDir::Files,
                            QDirIterator::Subdirectories | QDirIterator::FollowSymlinks);

            while (it.hasNext())
            {
                auto path = it.next();

                // To determine the ID of a desktop file, make its full path relative to
                // the $XDG_DATA_DIRS component in which the desktop file is installed,
                // remove the "applications/" prefix, and turn '/' into '-'. Chop off '.desktop'.
                static QRegularExpression re(u"^.*applications/"_s);
                QString id = QString(path).remove(re).replace(u'/', u'-').chopped(8);

                if (const auto &[dit, success] = desktop_files.emplace(id, path); !success)
                    DEBG << u"Desktop file '%1' at '%2' will be skipped: Shadowed by '%3'"_s
                                .arg(id, path, desktop_files[id]);
            }
        }

        Application::ParseOptions po{
            .ignore_show_in_keys = ignoreShowInKeys(),
            .use_exec = useExec(),
            .use_generic_name = useGenericName(),
            .use_keywords = useKeywords(),
            .use_non_localized_name = useNonLocalizedName()
        };

        // Index the unique desktop files
        vector<shared_ptr<applications::Application>> apps;
        for (const auto &[id, path] : desktop_files)
        {
            if (abort)
                return apps;

            try
            {
                apps.emplace_back(make_shared<Application>(id, path, po));
                DEBG << u"Valid desktop file '%1': '%2'"_s.arg(id, path);
            }
            catch (const exception &e)
            {
                DEBG << u"Skipped desktop entry '%1':"_s.arg(path) << e.what();
            }
        }

        return apps;
    };

    indexer.finish = [this]
    {
        applications = indexer.takeResult();

        INFO << u"Indexed %1 applications."_s.arg(applications.size());

        // Replace terminal apps with terminals and populate terminals
        // Filter supported terms by availability using destkop id

        terminals.clear();

        for (auto &base : applications)
            if (auto app = static_pointer_cast<::Application>(base);
                app->isTerminal())
            {
                if (auto command = normalizedContainerCommand(app->exec());
                    !command.isEmpty())
                    if (auto it = exec_args.find(command); it != exec_args.end())
                    {
                        auto term = make_shared<Terminal>(*app, it->second);
                        base = static_pointer_cast<::Application>(term);
                        terminals.emplace_back(term.get());
                    }
                    else
                        WARN << u"Terminal '%1' not supported. Please post an issue. Exec: %2"_s
                                    .arg(app->id(), app->exec().join(QChar::Space));
                else
                    WARN << u"Failed to get normalized command. Terminal '%1' not supported. Please post an issue. Exec: %2"_s
                                .arg(app->id(), app->exec().join(QChar::Space));
            }

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

        setIndexItems(buildIndexItems());

        emit appsChanged();
    };
}

Plugin::~Plugin() = default;

QWidget *Plugin::buildConfigWidget()
{
    auto widget = new QWidget;
    Ui::ConfigWidget ui;
    ui.setupUi(widget);

    bindWidget(ui.checkBox_ignoreShowInKeys,
               this,
               &Plugin::ignoreShowInKeys,
               &Plugin::setIgnoreShowInKeys);

    bindWidget(ui.checkBox_useExec,
               this,
               &Plugin::useExec,
               &Plugin::setUseExec);

    bindWidget(ui.checkBox_useGenericName,
               this,
               &Plugin::useGenericName,
               &Plugin::setUseGenericName);

    bindWidget(ui.checkBox_useKeywords,
               this,
               &Plugin::useKeywords,
               &Plugin::setUseKeywords);

    addBaseConfig(ui.formLayout);

    ui.formLayout->addRow(tr("Terminal"), createTerminalFormWidget());

    return widget;
}

QWidget *Plugin::createTerminalFormWidget()
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

void Plugin::runTerminal(const QString &script) const
{
    if (terminal)
        terminal->launch(script);
    else
        warning(tr("No terminal available."));
}

void Plugin::runTerminal(QStringList commandline, const QString working_dir) const
{
    terminal->launch(commandline, working_dir);
}

QJsonObject Plugin::telemetryData() const
{
    QJsonObject t;
    for (const auto &iapp : applications)
        if (const auto &app = static_pointer_cast<::Application>(iapp); app->isTerminal())
            t.insert(app->id(), app->exec().join(QChar::Space));

    QJsonObject o;
    o.insert(u"terminals"_s, t);
    return o;
}


bool Plugin::ignoreShowInKeys() const { return ignore_show_in_keys_; }

void Plugin::setIgnoreShowInKeys(bool v)
{
    if (ignore_show_in_keys_ != v)
    {
        settings()->setValue(ck_ignore_show_in_keys, v);
        ignore_show_in_keys_ = v;
        updateIndexItems();
    }
}

bool Plugin::useExec() const { return use_exec_; }

void Plugin::setUseExec(bool v)
{
    if (use_exec_ != v)
    {
        settings()->setValue(ck_use_exec, v);
        use_exec_ = v;
        updateIndexItems();
    }
}

bool Plugin::useGenericName() const { return use_generic_name_; }

void Plugin::setUseGenericName(bool v)
{
    if (use_generic_name_ != v)
    {
        settings()->setValue(ck_use_generic_name, v);
        use_generic_name_ = v;
        updateIndexItems();
    }
}

bool Plugin::useKeywords() const { return use_keywords_; }

void Plugin::setUseKeywords(bool v)
{
    if (use_keywords_ != v)
    {
        settings()->setValue(ck_use_keywords, v);
        use_keywords_ = v;
        updateIndexItems();
    }
}
