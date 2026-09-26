// Copyright (c) 2026 Roger Brown.
// Licensed under the MIT License.

#ifndef MPSAPPLICATION_H
#define MPSAPPLICATION_H

#include <QApplication>

class MPSDocumentManager;
class MPSToolServer;
class QProcess;

class MPSApplication : public QApplication {
    Q_OBJECT
public:
    MPSApplication(int &argc, char **argv);
	MPSDocumentManager *documentManager;
    MPSToolServer *toolServer;
    QProcess *powerShellProcess;
    QString initialScript,appTitle;
    void startup();
private:
    void startPowerShell();
};

#endif
