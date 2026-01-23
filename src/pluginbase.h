// Copyright (c) 2022-2026 Manuel Schneider

#pragma once
#include "applications.h"
#include <QFileSystemWatcher>
#include <QStringList>
#include <albert/backgroundexecutor.h>
#include <albert/extensionplugin.h>
#include <albert/indexqueryhandler.h>
#include <memory>
#include <vector>
class QFormLayout;

class PluginBase : public albert::ExtensionPlugin,
                   public albert::IndexQueryHandler,
                   public applications::Plugin
{
    Q_OBJECT

public:
    QString defaultTrigger() const override;
    void updateIndexItems() override;

    bool useNonLocalizedName() const;
    void setUseNonLocalizedName(bool);

    bool splitCamelCase() const;
    void setSplitCamelCase(bool);

    bool useAcronyms() const;
    void setUseAcronyms(bool);

protected:
    void commonInitialize(const QSettings &s);
    void addBaseConfig(QFormLayout *);
    std::vector<albert::IndexItem> buildIndexItems() const;
    static QStringList camelCaseSplit(const QString &s);

    QFileSystemWatcher fs_watcher;
    albert::BackgroundExecutor<std::vector<std::shared_ptr<applications::Application>>> indexer;
    std::vector<std::shared_ptr<applications::Application>> applications;

    bool use_non_localized_name_;
    bool split_camel_case_;
    bool use_acronyms_;

signals:
    void appsChanged();
    void useNonLocalizedNameChanged(bool);
    void splitCamelCaseChanged(bool);
    void useAcronymsChanged(bool);
};
