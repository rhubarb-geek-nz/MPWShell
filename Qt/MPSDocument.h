// Copyright (c) 2026 Roger Brown.
// Licensed under the MIT License.

#ifndef MPSDOCUMENT_H
#define MPSDOCUMENT_H

#include <QObject>
#include <QList>

class MPSDocumentManager;
class QTextDocument;
class MPSMainWindow;

class MPSDocument : public QObject {
    Q_OBJECT
public:
    MPSDocumentManager *documentManager;
    MPSDocument(const QString &content, MPSDocumentManager *parent);
    QTextDocument *textDocument;
    MPSMainWindow *mainWindow;
    bool isDirty();
    int state, clientId, runspaceId, textCursor;
    void queueMessage(int,QString &);
    void queueMessage(int);
    void receiveMessage(int,QString &);
    MPSMainWindow *newMainWindow();
    QString filePath, fileName;
    bool save(QString &err);
    bool saveAs(const QString &, QString &err);
protected:
    ~MPSDocument();
private slots:
    void contentsChanged();
private:
    bool hasChanged;
};

#endif
