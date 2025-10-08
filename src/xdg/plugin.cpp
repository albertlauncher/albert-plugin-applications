// Copyright (c) 2022-2025 Manuel Schneider

#include "application.h"
#include "plugin.h"
#include "terminal.h"
#include "ui_configwidget.h"
#include <QDirIterator>
#include <QFileInfo>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QWidget>
#include <albert/widgetsutil.h>
using namespace Qt::StringLiterals;
using namespace albert::util;
using namespace albert;
using namespace std;

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

    auto s = settings();

    restore_ignore_show_in_keys(s);
    connect(this, &Plugin::ignore_show_in_keys_changed,
            this, &Plugin::updateIndexItems);

    restore_use_exec(s);
    connect(this, &Plugin::use_exec_changed,
            this, &Plugin::updateIndexItems);

    restore_use_generic_name(s);
    connect(this, &Plugin::use_generic_name_changed,
            this, &Plugin::updateIndexItems);

    restore_use_keywords(s);
    connect(this, &Plugin::use_keywords_changed,
            this, &Plugin::updateIndexItems);

    restore_use_non_localized_name(s);
    connect(this, &PluginBase::use_non_localized_name_changed,
            this, &Plugin::updateIndexItems);


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
            .ignore_show_in_keys = ignore_show_in_keys(),
            .use_exec = use_exec(),
            .use_generic_name = use_generic_name(),
            .use_keywords = use_keywords(),
            .use_non_localized_name = use_non_localized_name()
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

    indexer.finish = [this](vector<shared_ptr<applications::Application>> &&result)
    {
        applications = ::move(result);

        INFO << u"Indexed %1 applications [%2 ms]"_s
                    .arg(applications.size()).arg(indexer.runtime.count());

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

        setUserTerminalFromConfig();

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

    bind(ui.checkBox_ignoreShowInKeys,
         this,
         &Plugin::ignore_show_in_keys,
         &Plugin::set_ignore_show_in_keys);

    bind(ui.checkBox_useExec,
         this,
         &Plugin::use_exec,
         &Plugin::set_use_exec);

    bind(ui.checkBox_useGenericName,
         this,
         &Plugin::use_generic_name,
         &Plugin::set_use_generic_name);

    bind(ui.checkBox_useKeywords,
         this,
         &Plugin::use_keywords,
         &Plugin::set_use_keywords);

    addBaseConfig(ui.formLayout);

    return widget;
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
