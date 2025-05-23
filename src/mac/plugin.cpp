// Copyright (c) 2022-2025 Manuel Schneider

#include "application.h"
#include "plugin.h"
#include "terminal.h"
#include "ui_configwidget.h"
#include <QDir>
#include <QMessageBox>
#include <QWidget>
#include <albert/logging.h>
using namespace Qt::StringLiterals;
using namespace albert;
using namespace std;

namespace {
static QStringList appDirectories()
{
    return {
        u"/Applications"_s,
        u"/System/Applications"_s,
        u"/System/Cryptexes/App/System/Applications"_s,  // Safari Home
        u"/System/Library/CoreServices/Finder.app/Contents/Applications"_s
    };
}
}

static void scanRecurse(QStringList &result, const QString &path, const bool abort = false)
{
    for (const auto &fi : QDir(path).entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot))
        if (fi.isBundle())
            result << fi.absoluteFilePath();
        else if (abort)
            break;
        else
            scanRecurse(result, fi.absoluteFilePath());
}

static auto binary_search(vector<shared_ptr<applications::Application>> &apps, const QString &id)
{
    auto lb = lower_bound(apps.begin(), apps.end(), id,
                          [](const auto &app, const auto &id){ return app->id() < id; });
    if (lb != apps.end() && (*lb)->id() == id)
        return lb;
    return apps.end();
};

Plugin::Plugin()
{
    auto s = settings();
    commonInitialize(s);

    fs_watcher.addPaths(appDirectories());
    connect(&fs_watcher, &QFileSystemWatcher::directoryChanged, this, &Plugin::updateIndexItems);

    indexer.parallel = [this](const bool &abort)
    {
        vector<shared_ptr<applications::Application>> apps;

        apps.emplace_back(make_shared<Application>(u"/System/Library/CoreServices/Finder.app"_s,
                                                   use_non_localized_name_));

        QStringList app_paths;
        for (const auto &path : appDirectories())
            scanRecurse(app_paths, path, abort);

        for (const auto &path : app_paths)
            if (abort)
                return apps;
            else
                try {
                    apps.emplace_back(make_shared<Application>(path, use_non_localized_name_));
                } catch (const runtime_error &e) {
                    WARN << e.what();
                }

        ranges::sort(apps, [](const auto &a, const auto &b){ return a->id() < b->id(); });

        return apps;
    };

    indexer.finish = [this](auto &&result)
    {
        INFO << u"Indexed %1 applications (%2 ms)."_s
                    .arg(result.size()).arg(indexer.runtime.count());
        applications = ::move(result);

        // Add terminals (replace apps by polymorphic type)

        terminals.clear();

        if (auto it = binary_search(applications, u"com.apple.Terminal"_s); it != applications.end())
        {
            auto t = make_shared<Terminal>(
                *static_cast<::Application*>(it->get()),
                uR"(tell application "Terminal" to activate
                    tell application "Terminal" to do script "exec %1")"_s
            );
            *it = static_pointer_cast<applications::Application>(t);
            terminals.emplace_back(t.get());
        }

        if (auto it = binary_search(applications, u"com.googlecode.iterm2"_s); it != applications.end())
        {
            auto t = make_shared<Terminal>(
                *static_cast<::Application*>(it->get()),
                uR"(tell application "iTerm" to create window with default profile command "%1")"_s
            );
            *it = static_pointer_cast<applications::Application>(t);
            terminals.emplace_back(t.get());
        }

        setUserTerminalFromConfig();

        setIndexItems(buildIndexItems());

        emit appsChanged();
    };
}

QWidget *Plugin::buildConfigWidget()
{
    auto *w = new QWidget;
    Ui::ConfigWidget ui;
    ui.setupUi(w);

    addBaseConfig(ui.formLayout);

    return w;
}
