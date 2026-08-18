# MPWShell
Mighty Power Shell - text editor with embedded scripting engine

## Introduction
This is a clone of Notepad (what else do you call a window with a single "edit" control and a menu?) with embedded PowerShell.

## Why?
Think of it as using a text document as a free-form worksheet, and being able to run any fragment of PowerShell script in the document and have the result inserted directly into the document.

## Is there any help?
There is an HtmlHelp folder with more details about this project. This is compiled into the application help file accessible by (checks notes) the help menu.

## It doesn't support printing
It uses PowerShell and that can print text documents. It must be true, it says so in the help.

## It looks old
By design.
* It is a simple UI that has stood the test of time
* It installs using an MSI
* It uses the registry for configuration, HKLM for installed configuration, customise in HKCU
* It is written in C and C#
* It re-uses your existing PowerShell Desktop but can use PowerShell 7 if you configure it using the registry. Did I mention HKCU?
* It uses the wait cursor when it is busy
* No C++, OLE, COM, MDI or tabbed windows were harmed in the making of this project
* The build script is written in PowerShell, (it gets a bit chicken and egg at this point)

What is not to like?