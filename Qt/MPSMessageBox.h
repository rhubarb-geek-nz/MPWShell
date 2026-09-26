// Copyright (c) 2026 Roger Brown.
// Licensed under the MIT License.

#ifndef MPSMESSAGEBOX_H
#define MPSMESSAGEBOX_H

#include <QMessageBox>

class MPSMainWindow;

class MPSMessageBox : public QMessageBox
{
    Q_OBJECT
public:
    MPSMessageBox(MPSMainWindow *parent = nullptr);

protected:
	~MPSMessageBox();

private:
    MPSMainWindow *mainWindow;
};
#endif
