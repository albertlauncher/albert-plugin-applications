// Copyright (c) 2022-2025 Manuel Schneider

#include "application.h"
#include "plugin.h"
#include <QFileInfo>
#include <albert/desktopentryparser.h>
#include <albert/icon.h>
#include <albert/logging.h>
#include <albert/systemutil.h>
#include <ranges>
using namespace Qt::StringLiterals;
using namespace albert::detail;
using namespace albert;
using namespace std;

extern Plugin* plugin;

Application::Application(const QString &id, const QString &path, ParseOptions po)
{
    id_ = id;
    path_ = path;

    DesktopEntryParser p(path);
    auto root_section = u"Desktop Entry"_s;

    // Post a warning on unsupported terminals
    try {
        if (ranges::any_of(p.getString(root_section, u"Categories"_s).split(u';', Qt::SkipEmptyParts),
                           [&](const auto &cat){ return cat == u"TerminalEmulator"_s; }))
            is_terminal_ = true;
    } catch (const out_of_range &) { }

    // Type - string, REQUIRED to be Application
    if (p.getString(root_section, u"Type"_s) != u"Application"_s)
        throw runtime_error("Desktop entries of type other than 'Application' are not handled yet.");

    // NoDisplay - boolean, must not be true
    try {
        if (p.getBoolean(root_section, u"NoDisplay"_s))
            throw runtime_error("Desktop entry excluded by 'NoDisplay'.");
    } catch (const out_of_range &) { }

    if (!po.ignore_show_in_keys)
    {
        const auto desktops(qEnvironmentVariable("XDG_CURRENT_DESKTOP").split(u':', Qt::SkipEmptyParts));

        // NotShowIn - string(s), if exists must not be in XDG_CURRENT_DESKTOP
        try {
            if (ranges::any_of(p.getString(root_section, u"NotShowIn"_s).split(u';', Qt::SkipEmptyParts),
                               [&](const auto &de){ return desktops.contains(de); }))
                throw runtime_error("Desktop entry excluded by 'NotShowIn'.");
        } catch (const out_of_range &) { }

        // OnlyShowIn - string(s), if exists has to be in XDG_CURRENT_DESKTOP
        try {
            if (!ranges::any_of(p.getString(root_section, u"OnlyShowIn"_s).split(u';', Qt::SkipEmptyParts),
                                [&](const auto &de){ return desktops.contains(de); }))
                throw runtime_error("Desktop entry excluded by 'OnlyShowIn'.");
        } catch (const out_of_range &) { }
    }

    // Localized name - localestring, may equal name if no localizations available
    // No need to catch despite optional, since falls back to name, which is required
    names_ << p.getLocaleString(root_section, u"Name"_s);

    // Non localized name - string, REQUIRED
    if (po.use_non_localized_name)
        names_ << p.getString(root_section, u"Name"_s);

    // Exec - string, REQUIRED despite not strictly by standard
    try
    {
        exec_ = splitExecWithEscapedSpaces(p.getString(root_section, u"Exec"_s)).value();
        if (exec_.isEmpty())
            throw runtime_error("Empty Exec value.");
    }
    catch (const bad_optional_access&)
    {
        throw runtime_error("Malformed Exec value.");
    }

    if (po.use_exec)
    {
        static QStringList excludes = {
            u"/"_s,
            u"bash "_s,
            u"dbus-send "_s,
            u"env "_s,
            u"flatpak "_s,
            u"java "_s,
            u"perl "_s,
            u"python "_s,
            u"ruby "_s,
            u"sh "_s
        };

        if (ranges::none_of(excludes, [this](const QString &str){ return exec_.startsWith(str); }))
            names_ << exec_.at(0);
    }

    // Comment - localestring
    try {
        description_ = p.getLocaleString(root_section, u"Comment"_s);
    } catch (const out_of_range &) { }

    // Keywords - localestring(s)
    try {
        auto keywords = p.getLocaleString(root_section, u"Keywords"_s).split(u';', Qt::SkipEmptyParts);
        if (description_.isEmpty())
            description_ = keywords.join(u", "_s);
        if (po.use_keywords)
            names_ << keywords;
    } catch (const out_of_range &) { }

    // Icon - iconstring (xdg icon naming spec)
    try {
        icon_ = p.getLocaleString(root_section, u"Icon"_s);
    } catch (const out_of_range &) { }

    // Path - string
    try {
        working_dir_ = p.getString(root_section, u"Path"_s);
    } catch (const out_of_range &) { }

    // Terminal - boolean
    try {
        term_ = p.getBoolean(root_section, u"Terminal"_s);
    } catch (const out_of_range &) { }

    // GenericName - localestring
    if (po.use_generic_name)
        try {
            names_<< p.getLocaleString(root_section, u"GenericName"_s);
        }
        catch (const out_of_range &) { }

    // Actions - string(s)
    try {
        auto action_ids = p.getString(root_section, u"Actions"_s).split(u';', Qt::SkipEmptyParts);
        for (const QString &action_id : action_ids)
        {
            try
            {
                const auto action_section = u"Desktop Action %1"_s.arg(action_id);

                // TOdo
                auto action = Action(
                    action_id,
                    p.getLocaleString(action_section, u"Name"_s), // Name - localestring, REQUIRED
                    [this, &p, &action_section]{
                        auto exec = splitExecWithEscapedSpaces(p.getString(action_section, u"Exec"_s));
                        if (!exec)
                            throw runtime_error("Malformed Exec value.");
                        else if (exec.value().isEmpty())
                            throw runtime_error("Empty Exec value.");
                        else
                            runDetachedProcess(fieldCodesExpanded(exec.value(), QUrl()));
                    }
                );

                // Name - localestring, REQUIRED
                auto name = p.getLocaleString(action_section, u"Name"_s);

                // Exec - string, REQUIRED despite not strictly by standard
                auto exec = splitExecWithEscapedSpaces(p.getString(action_section, u"Exec"_s));
                if (!exec)
                    throw runtime_error("Malformed Exec value.");
                else if (exec.value().isEmpty())
                    throw runtime_error("Empty Exec value.");
                else
                    desktop_actions_.emplace_back(*this, action_id, name, exec.value());
            }
            catch (const out_of_range &e)
            {
                WARN << u"%1: Desktop action '%2' skipped: %3"_s
                            .arg(path, action_id, QString::fromLocal8Bit(e.what()));
            }
        }
    } catch (const out_of_range &) { }

    // // MimeType, string(s)
    // try {
    //     pe.mime_types = p.getString(root_section, u"MimeType"_s).split(';', Qt::SkipEmptyParts);
    // } catch (const out_of_range &) { }
    // pe.mime_types.removeDuplicates();

    names_.removeDuplicates();
}

QString Application::subtext() const { return description_; }

unique_ptr<Icon> Application::icon() const
{
    if (QFileInfo(icon_).isAbsolute())
        return Icon::image(icon_);
    else
        return Icon::theme(icon_);
}

vector<Action> Application::actions() const
{
    vector<Action> actions = ApplicationBase::actions();

    for (const auto &a : desktop_actions_)
        actions.emplace_back(u"action-%1"_s.arg(a.id_), a.name_, [&a]{ a.launch(); });

    actions.emplace_back(u"reveal-entry"_s,
                         Plugin::tr("Open desktop entry"),
                         [this] { open(path_); });

    return actions;
}

const QStringList &Application::exec() const
{
    return exec_;
}

bool Application::isTerminal() const { return is_terminal_; }

void Application::launchExec(const QStringList &exec, QUrl url, const QString &working_dir) const
{
    auto commandline = fieldCodesExpanded(exec, url);
    auto wd = working_dir.isEmpty() ? working_dir_ : working_dir;

    if (auto prefix = qEnvironmentVariable("ALBERT_APPLICATIONS_COMMAND_PREFIX")
                          .split(u';', Qt::SkipEmptyParts);
        !prefix.isEmpty())
        commandline = prefix + commandline;

    if (term_)
        plugin->runTerminal(commandline, wd);
    else
        runDetachedProcess(commandline, wd);
}

void Application::launch() const { launchExec(exec_, {}, {}); }

void Application::DesktopAction::launch() const { application.launchExec(exec_, {}, {}); }

QStringList Application::fieldCodesExpanded(const QStringList &exec, QUrl url) const
{
    // TODO proper support for %f %F %U

    // Code	Description
    // %% : '%'
    // %f : A single file name (including the path), even if multiple files are selected. The system reading the desktop entry should recognize that the program in question cannot handle multiple file arguments, and it should should probably spawn and execute multiple copies of a program for each selected file if the program is not able to handle additional file arguments. If files are not on the local file system (i.e. are on HTTP or FTP locations), the files will be copied to the local file system and %f will be expanded to point at the temporary file. Used for programs that do not understand the URL syntax.
    // %F : A list of files. Use for apps that can open several local files at once. Each file is passed as a separate argument to the executable program.
    // %u : A single URL. Local files may either be passed as file: URLs or as file path.
    // %U : A list of URLs. Each URL is passed as a separate argument to the executable program. Local files may either be passed as file: URLs or as file path.
    // %i : The Icon key of the desktop entry expanded as two arguments, first --icon and then the value of the Icon key. Should not expand to any arguments if the Icon key is empty or missing.
    // %c : The translated name of the application as listed in the appropriate Name key in the desktop entry.
    // %k : The location of the desktop file as either a URI (if for example gotten from the vfolder system) or a local filename or empty if no location is known.
    // Deprecated: %v %m %d %D %n %N

    QStringList c;
    for (const auto &t : exec)
    {
        if (t == u"%%"_s)
            c << u"%"_s;
        else if (t == u"%f"_s || t == u"%F"_s)
        {
            if (!url.isEmpty())
                c << url.toLocalFile();
        }
        else if (t == u"%u"_s || t == u"%U"_s)
        {
            if (!url.isEmpty())
                c << url.toString();
        }
        else if (t == u"%i"_s && !icon_.isNull())
            c << u"--icon"_s << icon_;
        else if (t == u"%c"_s)
            c << name();
        else if (t == u"%k"_s)
            c << path_;
        else if (t == u"%v"_s || t == u"%m"_s || t == u"%d"_s
                 || t == u"%D"_s || t == u"%n"_s || t == u"%N"_s)
            ;  // Skipping deprecated field codes
        else
            c << t;
    }
    return c;
}

optional<QStringList> Application::splitExecWithEscapedSpaces(const QString &s) noexcept
{
    QStringList tokens;
    QString token;
    auto c = s.begin();

    while (c != s.end())
    {
        if (*c == QChar::Space)  // separator
        {
            if (!token.isEmpty())
            {
                tokens << token;
                token.clear();
            }
        }

        else if (*c == u'"')  // quote
        {
            ++c;

            while (c != s.end())
            {
                if (*c == u'"')  // quote termination
                    break;

                else if (*c == u'\\')  // escape inside quotes
                {
                    ++c;
                    if(c == s.end())
                    {
                        WARN << u"Unterminated escape in %1"_s.arg(s);
                        return {};  // unterminated escape
                    }

                    else if (uR"("`$\)"_s.contains(*c))
                        token.append(*c);

                    else
                    {
                        WARN << u"Invalid escape '%1' in quoted string: %2"_s.arg(*c).arg(s);
                        return {};  // invalid escape
                    }
                }

                else
                    token.append(*c);  // regular char

                ++c;
            }

            if (c == s.end())
            {
                WARN << u"Unterminated quote in %1"_s.arg(s);
                return {};  // unterminated quote
            }
        }

        else if (*c == u'\\')  // escape outside quotes
        {
            ++c;
            if (c == s.end())
            {
                WARN << u"Unterminated escape at end of string: %1"_s.arg(s);
                return {};  // unterminated escape
            }
            
            // According to desktop entry spec, outside quotes we need to handle:
            // - \\ (backslash) becomes a single backslash
            // - \  (backslash space) becomes a literal space (not a separator)
            // For simplicity and compatibility, treat any escaped character as literal
            token.append(*c);
        }

        else
            token.append(*c);  // regular char

        ++c;

    }

    if (!token.isEmpty())
        tokens << token;

    return tokens;
}
