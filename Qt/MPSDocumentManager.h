// Copyright (c) 2026 Roger Brown.
// Licensed under the MIT License.

#ifndef MPSDOCUMENTMANAGER_H
#define MPSDOCUMENTMANAGER_H

#include "MPSDocument.h"
#include <QList>
#include <QFont>
#include <QFile>

class MPSApplication;

class MPSDocumentManager : public QObject {
    Q_OBJECT
public:
    MPSApplication *application;
    MPSDocumentManager(MPSApplication *parent = nullptr);
    QList<MPSDocument *> documentList;
    QFont fixedFont;
    void addDocument(MPSDocument *);
    void removeDocument(MPSDocument *);
    MPSDocument *newDocument(const QString &content);
    MPSDocument *openDocument(const QString &filePath, MPSMainWindow *refWindow);
private:
    QFont findFixedFont();
	void openFileError(QFile &file, MPSMainWindow *window);
};

#endif
