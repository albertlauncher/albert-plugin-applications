# albert-plugin-applications

## Features

- Launch desktop applications.
- Choose the terminal used for the exposed script API.

## API

- Exposes `void runTerminal(const QString &script) const` allowing other plugins to run a 
  shell script a terminal.

## Platforms

- macOS
- UNIX

## Technical notes

Due to the lack of a standardized terminal command execution mechanism the terminal scan is based on
a hardcoded heuristic. Based on this need I am actively pushing xdg-terminal-execute proposal:

- https://gitlab.freedesktop.org/xdg/xdg-specs/-/merge_requests/87
- https://gitlab.freedesktop.org/terminal-wg/specifications/-/merge_requests/3

Register and vote if you think this makes sense.

### macOS
Performs manual scan for app bundles and uses [Foundation NSBundle][foundation-nsbundle] to fetch 
application data.

### Linux/XDG
Uses the [Desktop Entry Specification][destop-entry-spec] to find applications and parse application
data.

[foundation-nsbundle]: https://developer.apple.com/documentation/foundation/bundle
[destop-entry-spec]: https://specifications.freedesktop.org/desktop-entry-spec/latest/
