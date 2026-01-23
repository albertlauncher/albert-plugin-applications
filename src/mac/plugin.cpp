// Copyright (c) 2022-2026 Manuel Schneider

#include "application.h"
#include "plugin.h"
#include "ui_configwidget.h"
#include <QDir>
#include <QFile>
#include <QSettings>
#include <QWidget>
#include <albert/app.h>
#include <albert/logging.h>
#include <albert/messagebox.h>
#include <albert/systemutil.h>
#include <pwd.h>
#include <unistd.h>
using namespace Qt::StringLiterals;
using namespace albert;
using namespace std;

namespace {
static QStringList appDirectories()
{
    return {
        QDir::home().filePath(u"Applications"_s),
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

Plugin::Plugin()
{
    commonInitialize(*settings());

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

        for (const auto &path : as_const(app_paths))
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

    indexer.finish = [this]
    {
        applications = indexer.takeResult();
        INFO << u"Indexed %1 applications."_s.arg(applications.size());
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

void Plugin::runTerminal(const QString &script) const
{
    DEBG << "Launching terminal with script:" << script;

    if (passwd *pwd = getpwuid(geteuid()); pwd == nullptr)
    {
        const char *msg = QT_TR_NOOP("Failed to run terminal with script: getpwuid(…) failed.");
        WARN << msg;
        warning(tr(msg));
    }

    else if (auto s = script.simplified(); s.isEmpty())
    {
        const char* msg = QT_TR_NOOP("Failed to run terminal with script: Script is empty.");
        WARN << msg;
        warning(tr(msg));
    }

    else if (QFile file(App::cacheLocation() / "terminal.command");
             !file.open(QIODevice::WriteOnly))
    {
        const char *msg = QT_TR_NOOP("Failed to run terminal with script: "
                                     "Could not create temporary script file.");
        WARN << msg << file.errorString();
        warning(tr(msg) % QChar::Space % file.errorString());
    }

    else
    {
        // Note for future self: QTemporaryFile introduces race condition.

        file.write("clear; ");
        file.write(s.toUtf8());
        file.close();
        file.setPermissions(file.permissions() | QFileDevice::ExeOwner);

        open(file.filesystemFileName());
    }
}
