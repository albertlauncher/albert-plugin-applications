# Albert plugin: Applications

## Features

- Launch desktop applications.
- **[XDG]** Choose the terminal used for the exposed script API. On macOS the default application 
  for `*.command` files is used.
- **[XDG]** The environment variable `ALBERT_APPLICATIONS_COMMAND_PREFIX` is a semicolon-separated list of 
tokens that will be prepended to the command line used to launch applications.

## API

- Exposes `void runTerminal(const QString &script) const` allowing other plugins to run a 
  shell script in a terminal.

## Platforms

- macOS
- XDG platforms (BSD, Linux, ...)

## Technical notes

### macOS

Performs manual scan for app bundles and uses [Foundation NSBundle][foundation-nsbundle] to fetch 
application data.

### Linux/XDG

Uses the [Desktop Entry Specification][destop-entry-spec] to find applications and parse application
data. Due to the lack of a standardized terminal command execution mechanism the terminal scan is 
based on a **hardcoded heuristic**. If you want to change this read [issue #1][xte-issue] and vote
on the mentioned proposal.

#### PolicyKit (pkexec) Support

Applications that use `pkexec` to run with elevated privileges are automatically supported. The plugin
preserves necessary environment variables (`DISPLAY`, `XAUTHORITY`, `WAYLAND_DISPLAY`, `DBUS_SESSION_BUS_ADDRESS`)
when launching pkexec-based applications, ensuring authentication dialogs display properly in both X11 and
Wayland sessions.

[foundation-nsbundle]: https://developer.apple.com/documentation/foundation/bundle
[destop-entry-spec]: https://specifications.freedesktop.org/desktop-entry-spec/latest/
[xte-issue]: https://github.com/albertlauncher/albert-plugin-applications/issues/1
