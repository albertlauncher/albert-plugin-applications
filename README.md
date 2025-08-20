# Albert plugin: Applications

## Features

- Launch desktop applications.
- Choose the terminal used for the exposed script API.

## API

- Exposes `void runTerminal(const QString &script) const` allowing other plugins to run a 
  shell script in a terminal.

## Platforms

- macOS
- UNIX

## Technical notes

Due to the lack of a standardized terminal command execution mechanism the terminal scan is based on
a hardcoded heuristic. 

### macOS

Performs manual scan for app bundles and uses [Foundation NSBundle][foundation-nsbundle] to fetch 
application data.

### Linux/XDG

Uses the [Desktop Entry Specification][destop-entry-spec] to find applications and parse application
data.

The environment variable `ALBERT_APPLICATIONS_COMMAND_PREFIX` is a semicolon-separated list of 
tokens that will be prepended to the command line used to launch applications.

[foundation-nsbundle]: https://developer.apple.com/documentation/foundation/bundle
[destop-entry-spec]: https://specifications.freedesktop.org/desktop-entry-spec/latest/
