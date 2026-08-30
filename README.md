# MPWShell
Motivated Power Shell - text editor with embedded scripting engine

## Introduction
This is a clone of Notepad (what else do you call a window with a single "edit" control and a menu?) with embedded `PowerShell`.

## Why?
Think of it as using a text document as a free-form worksheet, and being able to run any fragment of `PowerShell` script in the document and have the result inserted directly into the document.

## How do I use it?
Use `Control+Return` to invoke a selected area of text or the current line as a `PowerShell` script.

## How does it work?
The project has four components
* `ToolServerPS` - this runs in a `PowerShell` process providing communications over stdin/stdout to its parent in order to manage a set of [Runspaces](https://learn.microsoft.com/en-us/powershell/scripting/developer/hosting/creating-runspaces). The parent makes requests to manage the `Runspaces` and gives them scripts to run. The output is returned to the parent.
* `MPWShellPS` - a optional set of cmdlets to run in each guest `Runspace` providing simple tools to open and create files.
* `Win32/mpwshell` - this is a clone of the classic `Notepad` that lets you execute `PowerShell` scripts inline and capture the response directly in the document
* `Motif/mpwshell` - this is an X11 editor based on [CDE](https://sourceforge.net/projects/cdesktopenv/) `dtpad` that integrates directly with PowerShell.

The mapping is one document window to one runspace within the tool server. This provides isolation for each document. Documents can be opened or created using the menus of the UI or `MPWShellPS` cmdlets within the document.

## Configuration
The program is configurable in terms of how it runs the child tool server. It can be local, remote, PowerShell Desktop or PowerShell Core. It can be a standard installation, a custom build or an app built with the PowerShell SDK.

* Windows - `HKCU:\Software\rhubarb-geek-nz\MPW Shell` values `ToolServer` and `InitialScript`.
* Motif - X11 resources `MPWShell.toolServer` and `MPWShell.initialScript`.

The `ToolServer` property is a command line for running `PowerShell`, this could include using `ssh` to run it on a remote machine. `InitialScript` is run in each runspace when created.

## Validation
The UI should be in Unicode. The pipe between the UI and the ToolServer uses UTF-8 for encoding character strings. This should work both on Windows and on Linux with a UTF-8 locale. If there are characters that don't appear correct you can verify. The two case I use to check are copyright and trademark.
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
