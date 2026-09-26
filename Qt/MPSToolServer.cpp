// Copyright (c) 2026 Roger Brown.
// Licensed under the MIT License.

#include "MPSToolServer.h"
#include "MPSApplication.h"
#include "MPSDocumentManager.h"
#include "MPSMainWindow.h"
#include "MPSDocument.h"
#include <QByteArray>
#include <unistd.h>

MPSToolServer::MPSToolServer(QProcess *proc, MPSApplication *parent) : QObject(parent)
{
	process = proc;
	application = parent;
	documentManager = parent->documentManager;
	serverState = 0;
	headerLen = 0;
	nextClientId = 0;
}

static int nextCharMatch(const char *data, int length, char value)
{
	int i = 0;

	while (i < length)
	{
		if (data[i] == value)
		{
			return i;
		}

		i++;
	}

	return -1;
}

static const char *signature="0c63fba6-7c2a-4b72-8de0-b3bd579dedaa";
static const int sigLen=36;

void MPSToolServer::readStdout()
{
	QByteArray data=process->readAllStandardOutput();
	int length=data.size();
	QByteArray *dataWrite=nullptr;
	const char *bytes=data.constData();

	while (length)
	{
		switch (serverState)
		{
			case 0:
				{
					int lfAt = nextCharMatch(bytes,length,0xA);

					if (lfAt < 0)
					{
						dataStdout.append(bytes,length);
						length=0;
					}
					else
					{
						if (lfAt)
						{
							dataStdout.append(bytes,lfAt);
							bytes+=lfAt;
							length-=lfAt;
						}

						int dataLen=dataStdout.size();

						if (dataLen)
						{
							if (dataLen >= sigLen)
							{
								const char *pcb = dataStdout.constData();

								if (memcmp(pcb+dataLen-sigLen,signature,sigLen))
								{
									dataStdout.append(bytes,1);
								}
								else
								{
									dataStdout.chop(sigLen);
									serverState = 2;
									for (const auto &item : documentManager->documentList)
									{
										startPairing(item);
									}
								}
							}
							else
							{
								dataStdout.append(bytes,1);
							}

							if (dataStdout.size())
							{
								if (dataWrite)
								{
									dataWrite->append(dataStdout.constData(),dataStdout.size());
								}
								else
								{
									dataWrite = new QByteArray(dataStdout);
								}

								dataStdout.clear();
							}
						}
						else
						{
							if (dataWrite)
							{
								dataWrite->append(bytes,1);
							}
							else
							{
								dataWrite = new QByteArray(bytes,1);
							}
						}

						length--;
						bytes++;
					}
				}
				break;
			case 2:
				if (headerLen < sizeof(header))
				{
					char *p=(char *)header;
					int len=sizeof(header)-headerLen;
					if (len>length) len=length;
					memcpy(p+headerLen,bytes,len);
					bytes+=len;
					length-=len;
					headerLen+=len;

					if (headerLen==sizeof(header))
					{
						dataMessage.clear();

						if (header[3])
						{
							dataMessage.reserve(header[3]+1);
						}
					}
				}
				else
				{
					if (length && (dataMessage.size() < header[3]))
					{
						int len=header[3] - dataMessage.size();
						if (len > length) len=length;
						dataMessage.append(bytes,len);
						bytes+=len;
						length-=len;
					}
				}

				if ((headerLen == sizeof(header)) && (header[3] == dataMessage.size()))
				{
					processMessage();
					headerLen=0;
					dataMessage.clear();
				}
				break;
			default:
				length=0;
				break;
		}
	}

	if (dataWrite)
	{
		length = dataWrite->size();

		if (length && write(1,dataWrite->constData(),length))
		{
		}

		delete dataWrite;
	}
}

void MPSToolServer::readStderr()
{
	QByteArray data=process->readAllStandardError();
	int len=data.size();

	if (len > 0 && write(2,data.data(),len))
	{
	}
}

void MPSToolServer::exitProcess(int exitCode, QProcess::ExitStatus)
{
	readStdout();
	readStderr();

	application->exit(exitCode);
}

void MPSToolServer::terminate()
{
	process->closeWriteChannel();
}

void MPSToolServer::errorOccurred(QProcess::ProcessError error)
{
	if (error == QProcess::FailedToStart)
	{
		QString errorString = process->errorString();
		QByteArray ba = errorString.toLocal8Bit();
		ba.append("\n",1);
		int len=ba.size();
		if (len && write(2,ba.constData(),len))
		{
		}
	}
	
	application->exit(1);
}

void MPSToolServer::queueMessage(int packetType,int sourceId,int destinationId,QByteArray *data)
{
	qint32 header[4]={packetType,sourceId,destinationId,(qint32)(data ? data->size() : 0)};

	process->write((char *)header,sizeof(header));
	if (data && header[3])
	{
		process->write(*data);
	}
}

void MPSToolServer::queueMessage(int packetType,int sourceId,int destinationId,QByteArray &data)
{
	qint32 header[4]={packetType,sourceId,destinationId,(qint32)data.size()};

	process->write((char *)header,sizeof(header));

	if (header[3])
	{
		process->write(data);
	}
}

void MPSToolServer::queueMessage(int packetType,int sourceId,int destinationId,QString &data)
{
	QByteArray ba=data.toUtf8();
	queueMessage(packetType,sourceId,destinationId,ba);
}

void MPSToolServer::processMessage()
{
	switch (header[0])
	{
		case 0x41:
			for (const auto &item : documentManager->documentList)
			{
				if (item->state == 1 && item->clientId == header[2])
				{
					item->state = 2;
					item->runspaceId = header[1];
					if (item->mainWindow)
					{
						item->mainWindow->documentChanged();
					}
				}
			}
			break;
		case 0x42:
			{
				MPSDocument *document=nullptr;
				for (const auto &item : documentManager->documentList)
				{
					if (item->state == 2 && item->clientId == header[2] && item->runspaceId == header[1])
					{
						item->state=3;
						document=item;
						break;
					}
				}

				if (document)
				{
					if (document->mainWindow)
					{
						document->mainWindow->documentChanged();
						document->mainWindow->close();
					}
					else
					{
						documentManager->removeDocument(document);
						document->deleteLater();
					}
				}
			}
			break;
		case 0x40:
		case 0x43:
		case 0x44:
		case 0x49:
			{
				if (header[0]!=0x49)
				{
					dataMessage.append("\n",1);
				}

				QString str=QString::fromUtf8(dataMessage);

				for (const auto &item : documentManager->documentList)
				{
					if (item->state == 2 && item->clientId == header[2] && item->runspaceId == header[1])
					{
						item->receiveMessage(header[0],str);
					}
				}				
			}
			break;
		case 0x47:
			{
				QString str=QString::fromUtf8(dataMessage);
				MPSDocument *document=documentManager->newDocument(str);
				document->state = 2;
				document->clientId = ++(nextClientId);
				document->runspaceId = header[1];
				document->newMainWindow()->show();
				queueMessage(0x55, document->clientId, document->runspaceId, application->initialScript);
			}
			break;
		case 0x48:
			{
				QString str=QString::fromUtf8(dataMessage);
				MPSMainWindow *window = nullptr;

				for (const auto &item : documentManager->documentList)
				{
					if (item->state == 2 && item->clientId == header[2])
					{
						window = item->mainWindow;

						if (window) break;
					}
				}

				MPSDocument *document=documentManager->openDocument(str, window);

				if (document && (document->state == 0))
				{
					document->state = 2;
					document->clientId = ++(nextClientId);
					document->runspaceId = header[1];
					document->newMainWindow()->show();
					queueMessage(0x55, document->clientId, document->runspaceId, application->initialScript);
				}
				else
				{
					queueMessage(0x52, header[2], header[1], nullptr);

					if (document)
					{
						document->newMainWindow()->show();
					}
				}
			}
			break;
	}
}

void MPSToolServer::startPairing(MPSDocument *document)
{
	if (serverState == 2)
	{
		if (document->state == 0)
		{
			document->state = 1;
			document->clientId = ++nextClientId;

			queueMessage(0x51,document->clientId,document->runspaceId,application->initialScript);
		}
	}
}
