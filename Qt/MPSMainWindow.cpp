// Copyright (c) 2026 Roger Brown.
// Licensed under the MIT License.

#include "MPSMainWindow.h"
#include "MPSMessageBox.h"
#include "MPSPlainTextEdit.h"
#include "MPSDocument.h"
#include "MPSDocumentManager.h"
#include "MPSApplication.h"
#include "MPSToolServer.h"
#include <QMenuBar>
#include <QFont>
#include <QDesktopServices>
#include <QUrl>
#include <QFileDialog>
#include <QTimer>
#include <QDir>

MPSMainWindow::MPSMainWindow(MPSDocument *doc, MPSMainWindow *oldWindow) : QMainWindow(nullptr)
{
	document = doc;
	document->mainWindow = this;
	alertsVisible = false;

	setAttribute(Qt::WA_DeleteOnClose);

	QFont font=document->documentManager->fixedFont;

    textEdit = new MPSPlainTextEdit(this);

	textEdit->setFont(font);
	textEdit->setDocument(document->textDocument);

    setCentralWidget(textEdit);

	if (oldWindow)
	{
		QSize size=oldWindow->size();
		resize(size);
	}
	else
	{
		QFontMetrics fm(font);
		resize(80*fm.horizontalAdvance("M"), 25*fm.lineSpacing());
	}

	documentTitleChanged();
	createMenuBar();

	if (document->state != 2)
	{
		textEdit->setReadOnly(true);
	}

	if (oldWindow)
	{
        QTextCursor oldTextCursor = oldWindow->textEdit->textCursor();
        int selectionStart=oldTextCursor.selectionStart();
        int selectionEnd=oldTextCursor.selectionEnd();
        QTextCursor newTextCursor = textEdit->textCursor();

        newTextCursor.setPosition(selectionStart);

        if (selectionEnd != selectionStart)
        {
            newTextCursor.setPosition(selectionEnd,QTextCursor::KeepAnchor);
        }

        textEdit->setTextCursor(newTextCursor);
        textEdit->ensureCursorVisible();
	}
	else
	{
		if (document->textCursor)
		{
			QTextCursor cursor=textEdit->textCursor();
			cursor.setPosition(document->textCursor);
			textEdit->setTextCursor(cursor);
			textEdit->ensureCursorVisible();
		}
	}
}

void MPSMainWindow::createMenuBar()
{
	QMenuBar *menuBar=this->menuBar();

	{
		QAction *newAction = new QAction(tr("&New"), this);
		newAction->setShortcut(QKeySequence::New);
		QAction *openAction = new QAction(tr("&Open..."), this);
		openAction->setShortcut(QKeySequence::Open);
		QAction *saveAction = new QAction(tr("&Save"), this);
		saveAction->setShortcut(QKeySequence::Save);
		QAction *saveAsAction = new QAction(tr("Save &As..."), this);
		saveAsAction->setShortcut(QKeySequence::SaveAs);
		QAction *closeAction = new QAction(tr("&Close"), this);
		closeAction->setShortcut(QKeySequence::Close);
		connect(newAction,&QAction::triggered,this,&MPSMainWindow::fileMenuNewDocument);
		connect(openAction,&QAction::triggered,this,&MPSMainWindow::fileMenuOpenDocument);
		connect(saveAction,&QAction::triggered,this,&MPSMainWindow::fileMenuSave);
		connect(saveAsAction,&QAction::triggered,this,&MPSMainWindow::fileMenuSaveAs);
		connect(closeAction,&QAction::triggered,this,&MPSMainWindow::close);

		QMenu *fileMenu = menuBar->addMenu(tr("&File"));
		fileMenu->addAction(newAction);
		fileMenu->addAction(openAction);
		fileMenu->addSeparator();
		fileMenu->addAction(saveAction);
		fileMenu->addAction(saveAsAction);
		fileMenu->addSeparator();
		fileMenu->addAction(closeAction);
	}

	{
		undoAction = new QAction(tr("&Undo"), this);
		redoAction = new QAction(tr("&Redo"), this);
		cutAction = new QAction(tr("Cu&t"), this);
		copyAction = new QAction(tr("&Copy"), this);
		pasteAction = new QAction(tr("&Paste"), this);
		deleteAction = new QAction(tr("&Delete"), this);
		selectAllAction = new QAction(tr("&SelectAll"), this);

		undoAction->setShortcut(QKeySequence::Undo);
		redoAction->setShortcut(QKeySequence::Redo);
		cutAction->setShortcut(QKeySequence::Cut);
		copyAction->setShortcut(QKeySequence::Copy);
		pasteAction->setShortcut(QKeySequence::Paste);
		deleteAction->setShortcut(QKeySequence::Delete);
		selectAllAction->setShortcut(QKeySequence::SelectAll);

		connect(undoAction,&QAction::triggered,textEdit,&MPSPlainTextEdit::undo);
		connect(redoAction,&QAction::triggered,textEdit,&MPSPlainTextEdit::redo);
		connect(cutAction,&QAction::triggered,textEdit,&MPSPlainTextEdit::cut);
		connect(copyAction,&QAction::triggered,textEdit,&MPSPlainTextEdit::copy);
		connect(pasteAction,&QAction::triggered,textEdit,&MPSPlainTextEdit::paste);
		connect(deleteAction,&QAction::triggered,this,&MPSMainWindow::removeSelectedText);
		connect(selectAllAction,&QAction::triggered,textEdit,&MPSPlainTextEdit::selectAll);

		QMenu *editMenu = menuBar->addMenu(tr("&Edit"));
		editMenu->addAction(undoAction);
		editMenu->addAction(redoAction);
		editMenu->addSeparator();
		editMenu->addAction(cutAction);
		editMenu->addAction(copyAction);
		editMenu->addAction(pasteAction);
		editMenu->addAction(deleteAction);
		editMenu->addSeparator();
		editMenu->addAction(selectAllAction);

		connect(editMenu,&QMenu::aboutToShow,this,&MPSMainWindow::aboutToShowEditMenu);
	}

	{
		QAction *helpAction = new QAction(tr("&Show Help"), this);
		helpAction->setShortcut(QKeySequence::HelpContents);
		QAction *aboutAction = new QAction(tr("&About MPW Shell..."), this);
		aboutAction->setShortcut(QKeySequence::WhatsThis);

		connect(helpAction,&QAction::triggered,this,&MPSMainWindow::helpMenuHelp);
		connect(aboutAction,&QAction::triggered,this,&MPSMainWindow::helpMenuAbout);

		QMenu *helpMenu = menuBar->addMenu(tr("&Help"));
		helpMenu->addAction(helpAction);
		helpMenu->addAction(aboutAction);
	}
}

void MPSMainWindow::fileMenuNewDocument()
{
	QString str;
	MPSDocument *doc=document->documentManager->newDocument(str);
	doc->documentManager->application->toolServer->startPairing(doc);
	MPSMainWindow *window=doc->newMainWindow();
	window->show();
}

void MPSMainWindow::fileMenuOpenDocument()
{
	QString dir;

	if (document->filePath.isEmpty())
	{
		dir = QDir::currentPath();
	}
	else
	{
		QFileInfo fileInfo(document->filePath);
		dir = fileInfo.absolutePath();
	}	

	QString fileName = QFileDialog::getOpenFileName(this,tr("Open File"),dir,tr("All Files (*)"));

	if (!fileName.isEmpty())
	{
		MPSDocument *doc=document->documentManager->openDocument(fileName, this);

		if (doc && doc != document)
		{
			doc->documentManager->application->toolServer->startPairing(doc);

			MPSMainWindow *window=doc->newMainWindow();

			window->show();
		}
	}
}

void MPSMainWindow::aboutToShowEditMenu()
{
	QTextCursor cursor=textEdit->textCursor();

	bool hasSelection = cursor.selectionStart() != cursor.selectionEnd();
	bool isUpdatable=!textEdit->isReadOnly();

	cutAction->setEnabled(hasSelection && isUpdatable);
	copyAction->setEnabled(hasSelection);
	deleteAction->setEnabled(hasSelection);
	pasteAction->setEnabled(isUpdatable);
	undoAction->setEnabled(isUpdatable && document->textDocument->isUndoAvailable());
	redoAction->setEnabled(isUpdatable && document->textDocument->isRedoAvailable());
}

void MPSMainWindow::removeSelectedText()
{
	QTextCursor cursor=textEdit->textCursor();
	if (cursor.hasSelection())
	{
		cursor.removeSelectedText();
	}
}

void MPSMainWindow::helpMenuHelp()
{
	QString url="https://github.com/rhubarb-geek-nz/MPWShell";
	QDesktopServices::openUrl(QUrl(url));
}

void MPSMainWindow::showAlert(QString &str)
{
	if (alertsVisible)
	{
		pendingAlerts.append(str);
	}
	else
	{
		showAlert(2,str);
	}
}

void MPSMainWindow::helpMenuAbout()
{
	QString aboutText=tr("Version %1, Copyright \xC2\xA9 2026 Roger Brown");
	QString str=aboutText.arg(MPWSHELL_VERSION);
	showAlert(1,str);
}

void MPSMainWindow::showAlert(int style, QString &str)
{
	if (!alertsVisible)
	{
		MPSMessageBox *aboutBox=new MPSMessageBox(this);
		aboutBox->setWindowTitle(document->documentManager->application->appTitle);
		aboutBox->setText(str);

		switch (style)
		{
			case 1:
				aboutBox->setIconPixmap(QPixmap(":/mpwshell.png"));
				break;
			case 2:
				aboutBox->setIcon(QMessageBox::Information);
				break;
		}
		aboutBox->setAttribute(Qt::WA_DeleteOnClose);
		aboutBox->setWindowModality(Qt::WindowModal);
		aboutBox->show();
		alertsVisible = true;
	}
}

void MPSMainWindow::alertClosed()
{
	alertsVisible = false;

	if (pendingAlerts.count())
	{
		QString next=pendingAlerts[0];
		pendingAlerts.removeFirst();
		showAlert(1,next);
	}
}

void MPSMainWindow::closeEvent(QCloseEvent *event)
{
	bool doClose=true;

	if (document && document->isDirty())
	{
		QMessageBox question(this);
		question.setText(tr("Content has changed"));
		question.setInformativeText(tr("Do you want to save your changes?"));
		question.setStandardButtons(QMessageBox::Yes|QMessageBox::No|QMessageBox::Cancel);
		question.setDefaultButton(QMessageBox::Cancel);

		int reply=question.exec();

		switch (reply)
		{
			case QMessageBox::Yes:
				if (document->filePath.isEmpty())
				{
					fileMenuSaveAs();

					if (document->filePath.isEmpty())
					{
						doClose = false;
					}
				}
				else
				{
					QString err;

					if (!document->save(err))
					{
						QMessageBox::critical(this,tr("File write failed"),err);

						doClose = false;
					}
				}
				break;
			case QMessageBox::No:
				break;
			case QMessageBox::Cancel:
				doClose = false;
				break;
		}
	}

	if (doClose)
	{
		MPSDocument *doc=document;
		document=nullptr;

		if (doc)
		{
			if (doc->mainWindow == this)
			{
				doc->mainWindow = nullptr;
			}

			doc->documentManager->removeDocument(doc);

			if (doc->state==2)
			{
				doc->state=3;
				doc->queueMessage(0x50);
			}

			doc->deleteLater();
		}

		event->accept();
	}
	else
	{
		event->ignore();
	}
}

MPSMainWindow::~MPSMainWindow()
{
	if (document && document->mainWindow == this)
	{
		document->mainWindow = nullptr;
	}
}

void MPSMainWindow::documentChanged()
{
    QTextCursor cursor=textEdit->textCursor();
    int startPos=cursor.selectionStart();
    int endPos=cursor.selectionEnd();
	int cur=document->textCursor;
	bool readOnly=document->state != 2;

	if (cur >= startPos && cur <= endPos)
	{
		textEdit->ensureCursorVisible();
	}

	if (readOnly!=textEdit->isReadOnly())
	{
		textEdit->setReadOnly(readOnly);
	}
}

void MPSMainWindow::fileMenuSave()
{
	if (document)
	{
		if (document->fileName.isEmpty())
		{
			fileMenuSaveAs();
		}
		else
		{
			QString err;
			if (!document->save(err))
			{
				QMessageBox::critical(this,tr("Save failed"),err);
			}
		}
	}
}

void MPSMainWindow::fileMenuSaveAs()
{
	if (document)
	{
		QString dir;

		if (document->filePath.isEmpty())
		{
			dir = QDir::currentPath();
		}
		else
		{
			QFileInfo fileInfo(document->filePath);
			dir = fileInfo.absolutePath();
		}

		QString fileName = QFileDialog::getSaveFileName(this,tr("Save file"),dir,tr("All Files (*)"));

		if (!fileName.isEmpty())
		{
			QString err;

			if (!document->saveAs(fileName,err))
			{
				QMessageBox::critical(this,tr("Save As failed"),err);
			}
		}
	}
}

void MPSMainWindow::documentTitleChanged()
{
	if (document->fileName.length())
	{
		QString compound=QString("%1 - %2").arg(document->documentManager->application->appTitle, document->fileName);
	    setWindowTitle(compound);
	}
	else
	{
	    setWindowTitle(document->documentManager->application->appTitle);
	}
}
