# MPWShell
Mocha Power Shell - text editor with embedded scripting engine

## Introduction
This is a simple text editor with embedded `PowerShell`.
The metaphor is using a text document as a free-form worksheet, and being able to run any fragment of script in the document and have the result inserted directly into the document.

## Features
The goals are

* Functionality
* Familiarity
* Simplicity

This project has no

* web interface
* AI
* auto-completion
* syntax highlighting
* tabbed windows
* Rust
* Go
* nagware
* telemetry

But does have

* native look and feel on the host OS
* built-in help on macOS and Windows
* AppleScript support on macOS
* versioned package installations

You are only limited by what PowerShell can do.

## How do I use it?
Use `Control+Return` ( or `⌘+⏎` on macOS ) to invoke a selected area of text or the current line as a `PowerShell` script.

## How does it work?
The project has five components
* `ToolServerPS` - this runs in a `PowerShell` process providing communications over stdin/stdout to its parent in order to manage a set of [Runspaces](https://learn.microsoft.com/en-us/powershell/scripting/developer/hosting/creating-runspaces). The parent makes requests to manage the `Runspaces` and gives them scripts to run. The output is returned to the parent.
* `MPWShellPS` - an optional set of cmdlets to run in each guest `Runspace` providing simple tools to open and create files.
* `Win32/mpwshell` - this is a clone of the classic `Notepad` that lets you execute `PowerShell` scripts inline and capture the response directly in the document
* `Motif/mpwshell` - this is an X11 editor based on [CDE](https://sourceforge.net/projects/cdesktopenv/) `dtpad` that integrates directly with PowerShell.
* `Cocoa/MPWShell` - this is a native `AppKit` app with plain-text document based UI as a front for PowerShell.

The mapping is one document window to one runspace within the tool server. This provides isolation for each document. Documents can be opened or created using the menus of the UI or `MPWShellPS` cmdlets within the document.

## Configuration
The program is configurable in terms of how it runs the child tool server. It can be local, remote, PowerShell Desktop or PowerShell Core. It can be a standard installation, a custom build or an app built with the PowerShell SDK.

* Windows - `HKCU:\Software\rhubarb-geek-nz\MPW Shell` values `ToolServer` and `InitialScript`.
* Motif - X11 resources `mpwshell.toolServer` and `mpwshell.initialScript`.
* macOS - `~/Library/Preferences/nz.geek.rhubarb.MPWShell.plist`, see [Help](Cocoa/MPWShellHelp/Resources/Base.lproj/index.html)

The `ToolServer` property is a command line for running `PowerShell`, this could include using `ssh` to run it on a remote machine. `InitialScript` is run in each runspace when created.

## Validation
The UI should be in Unicode. The pipe between the UI and the ToolServer uses UTF-8 for encoding character strings. This should work both on Windows and on Linux with a UTF-8 locale. If there are characters that don't appear correct you can verify. Two cases to check are copyright and trademark.
```
"`u{A9}"
```
should appear as a [copyright symbol](https://en.wikipedia.org/wiki/Copyright_symbol)
```
©
```
and
```
"`u{2122}"
```
should appear as a [trademark symbol](https://en.wikipedia.org/wiki/Trademark_symbol)
```
™
```
If they appear incorrect you can round trip them, by quoting and casting what was printed.
```
([int][char]"™").ToString('X')
```
this should return
```
2122
```

## Troubleshooting
The following error is caused by no DISPLAY environment for X11.

```
mpwshell: No display
```

If [PowerShell](https://github.com/PowerShell/PowerShell) is not installed, you will see

```
pwsh: No such file or directory
```

If the [rhubarb-geek-nz.ToolServer](https://www.powershellgallery.com/packages/rhubarb-geek-nz.ToolServer/0.9.2) module has not been installed in PowerShell you will see

```
Invoke-MPWShell.ToolServer: The term 'Invoke-MPWShell.ToolServer' is not recognized as a name of a cmdlet, function, script file, or executable program.
Check the spelling of the name, or if a path was included, verify that the path is correct and try again.
```

If the [rhubarb-geek-nz.MPWShell](https://www.powershellgallery.com/packages/rhubarb-geek-nz.MPWShell/0.9.2) module has not been installed in PowerShell you may see

```
Import-Module: The specified module 'rhubarb-geek-nz.MPWShell' was not loaded because no valid module file was found in any module directory.
```

If the window stays clear and does not close due to the program exiting then the PowerShell process has started correctly.

## Building
The [package.ps1](package.ps1) is the main entry point to build the program. The output should be a native package for the host OS.

## Other Build Systems
| Platform | Mechansim | Tool | Build file |
|----------|-----------|------|------------|
| Alpine Linux | apk | abuild | [APKBUILD](https://github.com/rhubarb-geek-nz/alpen-serial/blob/main/mpwshell-motif/APKBUILD) |
| Arch Linux | pacman | makepkg | [PKGBUILD](https://github.com/rhubarb-geek-nz/pacman/blob/main/mpwshell-motif/PKGBUILD) |
| Gentoo | portage | emerge | [mpwshell.ebuild](https://github.com/rhubarb-geek-nz/portage/blob/main/app-editors/mpwshell) |
| Snap | snap | snapcraft | [snapcraft.yaml](https://github.com/rhubarb-geek-nz/mpwshell-snapcraft/blob/main/snap/snapcraft.yaml) |

## Sandboxing
The program needs to run PowerShell, as such it does not meet most app store sandboxing requirements. However it can be built as a [snap](https://github.com/rhubarb-geek-nz/mpwshell-snapcraft/blob/main/snap/snapcraft.yaml) or an [MSIX](Win32/AppxManifest.xml).

## WSL
The program supports being run from within a Linux WSL environment with X11 display on the Windows desktop.

## AppleScript support
On macOS the `MPW Shell.app` application supports `do script` to allow invocation using AppleEvents. The Cocoa `ToolServer.app` is a stripped down headless application that only supports `do script`.

From `Script Editor`

```
tell application "MPW Shell"
    do script "$PSVersionTable"
end tell
```

Alternatively from `zsh`

```
% osascript -e 'tell application "ToolServer" to do script "Write-Output \"Hello World\""'
Hello World
```

## PowerShell Modules
Two PowerShell modules are used;
* [rhubarb-geek-nz.ToolServer](https://www.powershellgallery.com/packages/rhubarb-geek-nz.ToolServer/0.9.2) - provides the binary communication
* [rhubarb-geek.nz.MPWShell](https://www.powershellgallery.com/packages/rhubarb-geek-nz.MPWShell/0.9.2) - additional support cmdlets for the user

Both the Win32 MSI and macOS pkg already include these modules and are loaded directly using the PowerShell commmand line.
The Linux/FreeBSD installations and the Win32 msixbundles require the modules to be pre-installed, for example from [PSGallery](https://www.powershellgallery.com).

## Technology, toolkits and languages
This is an old-meets-new project. The UI programs use programming languages and APIs from the 20ᵗʰ century however they are intended to be good desktop citizens today. The Win32 API dates from [Windows NT](https://en.wikipedia.org/wiki/Windows_NT), Cocoa has its origins in [NeXTSTEP](https://en.wikipedia.org/wiki/NeXTSTEP) and [Motif](https://sourceforge.net/projects/motif/) is a building block of the [Common Desktop Environment](https://sourceforge.net/projects/cdesktopenv/).

|Project|OS|Toolkit|Language|Notes|
|-------|--|-------|--------|-----|
|Cocoa|macOS|AppKit|Objective-C|NeXTSTEP 1988, Mac OS X 2001|
|Motif|Linux/FreeBSD|Motif|C|OSF 1989|
|Win32|Windows|Win32|C|Windows NT 3.1 1993|

## Roadmap
Currently the UI is in English only. If there is interest then the project could be localized.
