// Copyright (c) 2026 Roger Brown.
// Licensed under the MIT License.

#ifndef MPSMAINWINDOW_H
#define MPSMAINWINDOW_H

#include <QMainWindow>
#include <QPlainTextEdit>
#include <QFont>

class MPSDocument;

class MPSMainWindow : public QMainWindow {
    Q_OBJECT
public:
    MPSMainWindow(MPSDocument *doc, MPSMainWindow *old);
    MPSDocument *document;
	void documentChanged();
	void documentTitleChanged();
	QStringList pendingAlerts;
	void alertClosed();
	void showAlert(QString &str);

protected:
	void closeEvent(QCloseEvent *) override;
	~MPSMainWindow();

private slots:
	void fileMenuNewDocument();
	void fileMenuOpenDocument();
	void fileMenuSave();
	void fileMenuSaveAs();
	void removeSelectedText();
	void helpMenuAbout();
	void helpMenuHelp();
	void aboutToShowEditMenu();

private:
	void createMenuBar();
	bool alertsVisible;
	void showAlert(int style,QString &str);
    QPlainTextEdit *textEdit;
	QAction *cutAction;
	QAction *copyAction;
	QAction *pasteAction;
	QAction *deleteAction;
	QAction *undoAction;
	QAction *redoAction;
	QAction *selectAllAction;
};
#endif
