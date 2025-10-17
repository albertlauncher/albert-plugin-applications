// Copyright (c) 2022-2025 Manuel Schneider

#pragma once
#include "pluginbase.h"
#include <QStringList>
#include <albert/telemetryprovider.h>

class Plugin : public PluginBase,
               public albert::detail::TelemetryProvider
{
    ALBERT_PLUGIN

public:

    Plugin();
    ~Plugin();

    // albert::ExtensionPlugin
    QWidget *buildConfigWidget() override;

    // albert::TelemetryProvider
    QJsonObject telemetryData() const override;

    using PluginBase::runTerminal;

    void runTerminal(QStringList commandline, const QString working_dir = {}) const;

    bool ignoreShowInKeys() const;
    void setIgnoreShowInKeys(bool);

    bool useExec() const;
    void setUseExec(bool);

    bool useGenericName() const;
    void setUseGenericName(bool);

    bool useKeywords() const;
    void setUseKeywords(bool);

private:

    bool ignore_show_in_keys_;
    bool use_exec_;
    bool use_generic_name_;
    bool use_keywords_;

};
