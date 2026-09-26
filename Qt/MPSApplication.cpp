// Copyright (c) 2026 Roger Brown.
// Licensed under the MIT License.

#include "MPSApplication.h"
#include "MPSDocumentManager.h"
#include "MPSMainWindow.h"
#include "MPSDocument.h"
#include "MPSPowerShellProcess.h"
#include "MPSToolServer.h"
#include "MPSSettings.h"

MPSApplication::MPSApplication(int &argc, char **argv) : QApplication(argc,argv)
{
	powerShellProcess = nullptr;
	toolServer = nullptr;
	documentManager = new MPSDocumentManager(this);

	setDesktopFileName("nz.geek.rhubarb.mpwshell");
	setWindowIcon(QIcon(":/mpwshell.png"));
	appTitle=tr("MPW Shell");
}

void MPSApplication::startPowerShell()
{
	MPSSettings settings(this);
	const QString szPath="PATH";

	setQuitOnLastWindowClosed(false);

	QProcessEnvironment env=QProcessEnvironment::systemEnvironment();

	QMapIterator<QString,QVariant> it(settings.toolServerEnvironment);

	while (it.hasNext())
	{
		it.next();
		QString key=it.key();
		QString value=it.value().toString();

		if (key == szPath)
		{
			QString path=env.value(szPath);

			if (value.back()==':')
			{
				value+=path;
			}
			else
			{
				if (value.at(0)==':')
				{
					path+=value;
					value=path;
				}
			}
		}

		env.insert(key,value);
	}
		
	powerShellProcess=new MPSPowerShellProcess(this);
	toolServer=new MPSToolServer(powerShellProcess, this);

	connect(powerShellProcess,&QProcess::readyReadStandardOutput,toolServer,&MPSToolServer::readStdout);
	connect(powerShellProcess,&QProcess::readyReadStandardError,toolServer,&MPSToolServer::readStderr);
	connect(powerShellProcess,QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),toolServer,&MPSToolServer::exitProcess);
	connect(powerShellProcess,&QProcess::errorOccurred,toolServer,&MPSToolServer::errorOccurred);

	powerShellProcess->setProcessEnvironment(env);

	powerShellProcess->start(settings.toolServerProgram, settings.toolServerArguments);
}

void MPSApplication::startup()
{
	startPowerShell();
}