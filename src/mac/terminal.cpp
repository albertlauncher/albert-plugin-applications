// Copyright (c) 2022-2025 Manuel Schneider

#include "terminal.h"
#include <QDir>
#include <QFile>
#include <albert/albert.h>
#include <albert/logging.h>
#include <albert/messagebox.h>
#include <albert/systemutil.h>
#include <pwd.h>
#include <unistd.h>
using namespace albert::util;
using namespace albert;

Terminal::Terminal(const ::Application &app, const QString &apple_script):
    Application(app),
    apple_script_(apple_script)
{}

void Terminal::launch(QString script) const
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

    else if (QFile file(cacheLocation() / "terminal_command");
             !file.open(QIODevice::WriteOnly))
    {
        const char *msg = QT_TR_NOOP("Failed to run terminal with script: "
                                     "Could not create temporary script file.");
        WARN << msg << file.errorString();
        warning(tr(msg) % QChar::Space % file.errorString());
    }

    else
    {
        // Note for future self
        // QTemporaryFile does not start
        // Deleting the file introduces race condition

        file.write("clear; ");
        file.write(s.toUtf8());
        file.close();

        auto command = QStringLiteral("%1 -i %2").arg(pwd->pw_shell, file.fileName());

        try {
            util::runAppleScript(apple_script_.arg(command));
        } catch (const std::runtime_error &e) {
            WARN << e.what();
        }

    }
}
