// Copyright (c) 2022-2025 Manuel Schneider

#pragma once
#include "application.h"
#include <QCoreApplication>
#include <QString>

class Terminal : public Application
{
    Q_DECLARE_TR_FUNCTIONS(Terminal)

public:

    /// \note the apple script must contain the placeholder %1 for the command line to run
    Terminal(const ::Application &app, const QString& apple_script);

    using ::Application::launch;

    void launch(QString script) const;

private:

    const QString apple_script_;

};
