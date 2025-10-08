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

private:

    ALBERT_PLUGIN_PROPERTY(bool, ignore_show_in_keys, true)
    ALBERT_PLUGIN_PROPERTY(bool, use_exec, false)
    ALBERT_PLUGIN_PROPERTY(bool, use_generic_name, false)
    ALBERT_PLUGIN_PROPERTY(bool, use_keywords, false)

};
