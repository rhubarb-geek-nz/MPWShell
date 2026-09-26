// Copyright (c) 2026 Roger Brown.
// Licensed under the MIT License.

#include "MPSPlainTextEdit.h"
#include "MPSMainWindow.h"
#include "MPSDocument.h"
#include <QTextBlock>

MPSPlainTextEdit::MPSPlainTextEdit(MPSMainWindow *parent) : QPlainTextEdit(parent)
{
    mainWindow = parent;
}

void MPSPlainTextEdit::keyPressEvent(QKeyEvent *event)
{
    switch (event->key())
    {
        case Qt::Key_Return:
            if (event->modifiers() & Qt::ControlModifier)
            {
                doExecute();
                return;
            }
            break;
        case Qt::Key_Enter:
            doExecute();
            return;
        case Qt::Key_Escape:
            mainWindow->document->queueMessage(0x54);
            return;
    }

    QPlainTextEdit::keyPressEvent(event);
}

void MPSPlainTextEdit::doExecute()
{
    QTextCursor cursor=textCursor();
    QString ba=cursor.selectedText();
    int index=cursor.selectionEnd();

    if (0 == ba.size())
    {
        QTextBlock block=cursor.block();

        if (block.length())
        {
            ba=block.text();

            index=block.position()+block.length()-1;
        }
        else
        {
            index=block.position();
        }
    }

    if (ba.size())
    {
        mainWindow->document->queueMessage(0x50,ba);
    }

    cursor.setPosition(index);

    cursor.insertText("\n");

    index++;

    cursor.setPosition(index);
    setTextCursor(cursor);

    mainWindow->document->textCursor=index;
}
