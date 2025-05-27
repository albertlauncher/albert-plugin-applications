// Copyright (c) 2022-2025 Manuel Schneider

#include "terminal.h"
#include <QDir>
#include <QFile>
#include <QMessageBox>
#include <albert/albert.h>
#include <albert/logging.h>
#include <albert/systemutil.h>
#include <pwd.h>
#include <unistd.h>
using namespace albert;

Terminal::Terminal(const ::Application &app, const QString &apple_script):
    Application(app),
    apple_script_(apple_script)
{}

void Terminal::launch(QString script) const
{
    if (passwd *pwd = getpwuid(geteuid()); pwd == nullptr)
    {
        const char *msg = QT_TR_NOOP("Failed to run terminal with script: getpwuid(…) failed.");
        WARN << msg;
        QMessageBox::warning(nullptr, {}, tr(msg));
    }

    else if (auto s = script.simplified(); s.isEmpty())
    {
        const char* msg = QT_TR_NOOP("Failed to run terminal with script: Script is empty.");
        WARN << msg;
        QMessageBox::warning(nullptr, {}, tr(msg));
    }

    else if (QFile file(cacheLocation() / "terminal_command");
             !file.open(QIODevice::WriteOnly))
    {
        const char *msg = QT_TR_NOOP("Failed to run terminal with script: Could "
                                     "not create temporary script file.");
        WARN << msg << file.errorString();
        QMessageBox::warning(nullptr, {}, tr(msg) % QChar::Space % file.errorString());
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

        util::runDetachedProcess({QStringLiteral("/usr/bin/osascript"),
                                  QStringLiteral("-l"),
                                  QStringLiteral("AppleScript"),
                                  QStringLiteral("-e"),
                                  apple_script_.arg(command)});
    }
}
