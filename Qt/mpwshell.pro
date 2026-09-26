# Copyright (c) 2026 Roger Brown.
# Licensed under the MIT License.

QT       += core gui widgets

TARGET = mpwshell
TEMPLATE = app

# Require C++17 or later for Qt 6 compatibility
CONFIG += c++17

SOURCES += main.cpp \
           MPSMainWindow.cpp \
           MPSMessageBox.cpp \
           MPSPowerShellProcess.cpp \
           MPSToolServer.cpp \
           MPSDocumentManager.cpp \
           MPSDocument.cpp \
           MPSApplication.cpp \
           MPSSettings.cpp \
           MPSPlainTextEdit.cpp

HEADERS += MPSMainWindow.h \
           MPSMessageBox.h \
           MPSPowerShellProcess.h \
           MPSToolServer.h \
           MPSDocumentManager.h \
           MPSDocument.h \
           MPSApplication.h \
           MPSSettings.h \
           MPSPlainTextEdit.h

RESOURCES += mpwshell.qrc

DEFINES += MPWSHELL_VERSION=\\\"$$VERSION\\\"
