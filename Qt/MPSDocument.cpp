// Copyright (c) 2026 Roger Brown.
// Licensed under the MIT License.

#include "MPSDocument.h"
#include "MPSDocumentManager.h"
#include "MPSApplication.h"
#include "MPSToolServer.h"
#include "MPSMainWindow.h"
#include <QTextDocument>
#include <QPlainTextDocumentLayout>
#include <QFile>
#include <QFileInfo>

MPSDocument::MPSDocument(const QString &content, MPSDocumentManager *parent) : QObject(parent)
{
    documentManager = parent;
    mainWindow=nullptr;
    QFont font = documentManager->fixedFont;

    textDocument = new QTextDocument(content, this);
    textDocument->setDocumentLayout(new QPlainTextDocumentLayout(textDocument));
	textDocument->setDefaultFont(font);
    textDocument->clearUndoRedoStacks();
    
    runspaceId = 0;
    clientId = 0;
    state = 0;
    textCursor = 0;
    hasChanged = false;

    connect(textDocument,&QTextDocument::contentsChanged,this,&MPSDocument::contentsChanged);
}

MPSDocument::~MPSDocument()
{
}

bool MPSDocument::isDirty()
{
    return hasChanged;
}

void MPSDocument::queueMessage(int packetType,QString &data)
{
    documentManager->application->toolServer->queueMessage(packetType,clientId,runspaceId,data);
}

void MPSDocument::queueMessage(int packetType)
{
    documentManager->application->toolServer->queueMessage(packetType,clientId,runspaceId,nullptr);
}

void MPSDocument::receiveMessage(int packetType,QString &str)
{
    switch (packetType)
    {
        case 0x40:
        case 0x43:
        case 0x44:
            {
                int len=str.length();
                if (len)
                {
                    QTextCursor cursor(textDocument);
                    cursor.setPosition(textCursor);
                    textCursor+=len;
                    cursor.insertText(str);
                    if (mainWindow)
                    {
                        mainWindow->documentChanged();
                    }
                }
            }
            break;
        case 0x49:
            if (mainWindow)
            {
                mainWindow->showAlert(str);
            }
            break;
    }    
}

MPSMainWindow *MPSDocument::newMainWindow()
{
    MPSMainWindow *oldWindow = mainWindow;

    if (oldWindow)
    {
        mainWindow = nullptr;
        oldWindow->document = nullptr;
    }

    MPSMainWindow *newWindow = new MPSMainWindow(this, oldWindow);

    if (oldWindow)
    {
        oldWindow->close();
    }

    return newWindow;
}

void MPSDocument::contentsChanged()
{
    hasChanged = true;
}

bool MPSDocument::save(QString &err)
{
    bool result = false;
    QFile file(filePath);

    if (file.open(QIODevice::WriteOnly))
    {
        QByteArray ba = textDocument->toPlainText().toUtf8();
        file.write(ba);
        file.close();
        hasChanged = false;
        result = true;
    }
    else
    {
        err = file.errorString();
    }

    return result;
}

bool MPSDocument::saveAs(const QString &path,QString &err)
{
    bool result = false;
    QFile file(path);

    if (file.open(QIODevice::WriteOnly))
    {
        QFileInfo fileInfo(file);

        QByteArray ba = textDocument->toPlainText().toUtf8();
        file.write(ba);

        filePath = fileInfo.canonicalFilePath();
        fileName = fileInfo.fileName();

        file.close();

        result = true;
        hasChanged=false;

        if (mainWindow)
        {
            mainWindow->documentTitleChanged();
        }
    }
    else
    {
        err = file.errorString();
    }

    return result;
}
