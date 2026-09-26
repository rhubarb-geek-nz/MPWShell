// Copyright (c) 2026 Roger Brown.
// Licensed under the MIT License.

#ifndef MPSPOWERSHELLPROCESS_H
#define MPSPOWERSHELLPROCESS_H

#include <QProcess>

class MPSPowerShellProcess : public QProcess {
    Q_OBJECT
public:
    MPSPowerShellProcess(QObject *parent = nullptr);
private:
};

#endif
