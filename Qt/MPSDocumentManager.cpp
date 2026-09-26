// Copyright (c) 2026 Roger Brown.
// Licensed under the MIT License.

#include "MPSDocumentManager.h"
#include "MPSApplication.h"
#include "MPSToolServer.h"
#include "MPSMainWindow.h"
#include <QFont>
#include <QFontDatabase>
#include <QFontInfo>
#include <QFile>
#include <QFileInfo>
#include <QMessageBox>

MPSDocumentManager::MPSDocumentManager(MPSApplication *parent) : QObject(parent)
{
    application = parent;
    fixedFont = findFixedFont();
}

QFont MPSDocumentManager::findFixedFont()
{
	QFont font = QFontDatabase::systemFont(QFontDatabase::FixedFont);
	QFontInfo fontInfo(font);

	if (fontInfo.fixedPitch())
	{
		return font;
	}

	QFont monoFont("Monospace");
	QFontInfo monoFontInfo(monoFont);

	if (monoFontInfo.fixedPitch())
	{
		return monoFont;
	}

	QFont hintFont("");
    
	hintFont.setStyleHint(QFont::TypeWriter);
    hintFont.setFixedPitch(true);

	return hintFont;
}

void MPSDocumentManager::addDocument(MPSDocument *doc)
{
	documentList.append(doc);
}

void MPSDocumentManager::removeDocument(MPSDocument *doc)
{
	documentList.removeOne(doc);

	if (documentList.isEmpty())
	{
		application->toolServer->terminate();
	}
}

MPSDocument *MPSDocumentManager::newDocument(const QString &content)
{
    MPSDocument *document=new MPSDocument(content, this);
	addDocument(document);
	return document;
}

void MPSDocumentManager::openFileError(QFile &file, MPSMainWindow *window)
{
	QString errorMessage=file.errorString();
	QMessageBox::critical(window,tr("Open file failed"),errorMessage);
}

MPSDocument *MPSDocumentManager::openDocument(const QString &openFile, MPSMainWindow *window)
{
	MPSDocument *document=nullptr;
	QFileInfo fileInfo(openFile);
	QString filePath=fileInfo.canonicalFilePath();

	if (filePath.isEmpty())
	{
		QFile file(openFile);

		if (file.open(QIODevice::ReadOnly))
		{
			file.close();
		}
		else
		{
			openFileError(file, window);
		}
	}
	else
	{
		QByteArray ba=filePath.toUtf8();

		for (const auto &item : documentList)
		{
			if (!item->filePath.isEmpty())
			{
				if (filePath == item->filePath)
				{
					document=item;
					break;
				}
			}
		}

		if (document==nullptr)
		{
			QFile file(filePath);

			if (file.open(QIODevice::ReadOnly))
			{
				QByteArray bytes=file.readAll();
				file.close();
				QString content=QString::fromUtf8(bytes);
				document=new MPSDocument(content,this);
				document->filePath=filePath;
				document->fileName=fileInfo.fileName();
				addDocument(document);
			}
			else
			{
				openFileError(file, window);
			}
		}
	}

	return document;
}
