// Copyright (c) 2022-2025 Manuel Schneider

#pragma once
#include "pluginbase.h"

class Plugin : public PluginBase
{
    ALBERT_PLUGIN

public:

    Plugin();

    QWidget *buildConfigWidget() override;

};
