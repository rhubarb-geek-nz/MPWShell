// Copyright (c) 2026 Roger Brown.
// Licensed under the MIT License.

#ifndef MPSTOOLSERVER_H
#define MPSTOOLSERVER_H

#include <QProcess>
class MPSApplication;
class MPSDocumentManager;
class MPSDocument;

class MPSToolServer : public QObject {
    Q_OBJECT
public:
    MPSToolServer(QProcess *process, MPSApplication *parent = nullptr);
	MPSApplication *application;
	void terminate();
	int serverState, nextClientId;
	QByteArray dataStdin, dataStdout, dataMessage;
	qint32 header[4];
	size_t headerLen;
	void queueMessage(int,int,int,QByteArray *);
	void queueMessage(int,int,int,QByteArray &);
	void queueMessage(int,int,int,QString &);
	void startPairing(MPSDocument *);
public slots:
	void readStdout();
	void readStderr();
	void exitProcess(int,QProcess::ExitStatus);
	void errorOccurred(QProcess::ProcessError);
private:
	QProcess *process;
	MPSDocumentManager *documentManager;
	void processMessage();
};

#endif
