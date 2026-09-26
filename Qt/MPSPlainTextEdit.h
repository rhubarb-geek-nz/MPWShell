// Copyright (c) 2026 Roger Brown.
// Licensed under the MIT License.

#ifndef MPSPLAINTEXTEDIT_H
#define MPSPLAINTEXTEDIT_H

#include <QPlainTextEdit>

class MPSMainWindow;

class MPSPlainTextEdit : public QPlainTextEdit {
    Q_OBJECT
public:
    MPSPlainTextEdit(MPSMainWindow *parent);
protected:
    void keyPressEvent(QKeyEvent *) override;
private:
    MPSMainWindow *mainWindow;
    void doExecute();
};

#endif
