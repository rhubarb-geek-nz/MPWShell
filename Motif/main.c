// Copyright (c) 2026 Roger Brown.
// Licensed under the MIT License.

#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/wait.h>
#include <Xm/XmAll.h>

#include "mpwshell.h"

static XtResource resources[] = {
	{
		"title",
		"MyStringClass",
		XtRString,
		sizeof(String),
		XtOffsetOf(AppData, title),
		XtRString,
		"MPW Shell"
	},
	{
		"toolServer",
		"MyStringClass",
		XtRString,
		sizeof(String),
		XtOffsetOf(AppData, toolServer),
		XtRString,
		"pwsh -NoProfile -NoLogo -NonInteractive -Command \"Invoke-MPWShell.ToolServer -Protocol 0c63fba6-7c2a-4b72-8de0-b3bd579dedaa\""
	},
	{
		"initialScript",
		"MyStringClass",
		XtRString,
		sizeof(String),
		XtOffsetOf(AppData, initialScript),
		XtRString,
		"if ($PSStyle) { $PSStyle.OutputRendering = 'PlainText' } ; Import-Module rhubarb-geek-nz.MPWShell"
	}
};

static void childFork(const char *toolServer)
{
	const char *src=toolServer;
	int len=strlen(toolServer);
	char *buf=malloc(len+1);
	char *dst=buf;
	char *argv[256];
	char **h=argv;
	int argc=0;

	while (*src)
	{
		char quote=0;

		while (*src && isspace(*src))
		{
			src++;
		}

		if (!*src) break;

		if ((*src==0x22)||(*src==0x27))
		{
			quote=*src++;
		}

		*(h++) = dst;

		argc++;

		if (argc == sizeof(argv)/sizeof(argv[0]))
		{
			errno=E2BIG;
			perror(argv[0]);
			_exit(1);
		}

		while (*src && (quote || !isspace(*src)))
		{
			if (*src == quote)
			{
				src++;
				break;
			}

			if (*src == '\\')
			{
				src++;

				switch (*src)
				{
				case 'r':
					*dst++=0xD;
					src++;
					break;
				case 'n':
					*dst++=0xA;
					src++;
					break;
				case 't':
					*dst++=0x9;
					src++;
					break;
				default:
					*dst++ = *src++;
					break;
				}
			}
			else
			{
				*dst++ = *src++;
			}
		}

		*dst++ = 0;
	}

	*h++ = NULL;

	src=argv[0];

	if (strstr(src,"/"))
	{
		execv(src,argv);
	}
	else
	{
		execvp(src,argv);
	}

	perror(src);
}

static void PipeReader(XtPointer client_data, int *source, XtInputId *id)
{
	struct MPWShellApp *app=client_data;
	int i;

	i=MPWToolServerRead(app->toolServer);

	if (i < 1)
	{
		if (app->readPipeAdded)
		{
			app->readPipeAdded = 0;
			XtRemoveInput(app->idReader);
		}

		XtAppSetExitFlag(app->appContext);
	}
}

static void PipeWriter(XtPointer client_data, int *source, XtInputId *id)
{
	struct MPWShellApp *app=client_data;
	int i;

	i=MPWToolServerWrite(app->toolServer);

	if (i < 1)
	{
		if (app->writePipeAdded)
		{
			app->writePipeAdded = 0;
			XtRemoveInput(app->idWriter);
		}
	}
}

void MPWShellAppSend(struct MPWShellApp *app,struct MPWToolServerMessage *msg)
{
	struct MPWToolServer *server=app->toolServer;

	MPWToolServerAdd(server, msg);

	if (!app->writePipeAdded)
	{
		app->idWriter = XtAppAddInput(app->appContext, server->fdWrite, (XtPointer)XtInputWriteMask, PipeWriter, app);
		app->writePipeAdded = 1;
	}
}

static struct
{
	const char *name;
	int sigNum;
	XtSignalId sigId;
} siglist[]=
{
	{"SIGCHLD",SIGCHLD},
	{"SIGQUIT",SIGQUIT},
	{"SIGPIPE",SIGPIPE},
	{"SIGHUP",SIGHUP},
	{"SIGINT",SIGINT}
};

static void MPWShellSignalHandler(int sig)
{
	int i=XtNumber(siglist);

	while (i--)
	{
		if (siglist[i].sigNum == sig)
		{
		    XtNoticeSignal(siglist[i].sigId);
			break;
		}
	}
}

static void MPWShellSignalCallback(XtPointer client_data, XtSignalId *id)
{
	int i=XtNumber(siglist);
	while (i--)
	{
		if (*id == siglist[i].sigId)
		{
			break;
		}
	}
}

int main(int argc, char** argv)
{
	struct MPWShellApp app;
	struct MPWToolServer client;
	int pipeStdin[2],pipeStdout[2];
	pid_t childPid;
	struct sigaction sa;
	int i=XtNumber(siglist);
	int s,rc=0;

	XtSetLanguageProc(NULL, NULL, NULL);

	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = MPWShellSignalHandler;
	sa.sa_flags |= SA_RESTART;

	XtToolkitInitialize();

	memset(&app, 0, sizeof(app));
	app.appContext = XtCreateApplicationContext();
	app.toolServer = &client;
	app.display = XtOpenDisplay(app.appContext, NULL, "MPWShell", "mpwShell", NULL, 0, &argc, argv);

	if (!app.display)
	{
		fprintf(stderr,"No display\n");
		return 1;
	}

	app.WM_DELETE_WINDOW = XInternAtom(app.display, "WM_DELETE_WINDOW", False);

	while (i--)
	{
		int num = siglist[i].sigNum;;
		sigaction(num,&sa,NULL);
		siglist[i].sigId = XtAppAddSignal(app.appContext,MPWShellSignalCallback,&app);
	}

	if (argc == 1)
	{
		struct MPWShellWin *win = MPWShellWinNew(&app);
		MPWShellWinCreate(win,NULL,NULL);
	}
	else
	{
		int j = 1;

		while (j < argc)
		{
			struct MPWShellWin *win = MPWShellWinNew(&app);

			if (MPWShellWinCreate(win,argv[j++],NULL)<0)
			{
				MPWShellWinFree(win);
			}
		}
	}

	if (!app.winList)
	{
		return 1;
	}

	XtGetApplicationResources(app.winList->shellWidget, (XtPointer)&app.appData, resources, XtNumber(resources), NULL, 0);

	MPWToolServerInit(&client, &app);

	if (pipe(pipeStdin) || pipe(pipeStdout))
	{
		perror("pipe");

		return 1;
	}

	childPid = fork();

	if (!childPid)
	{
		dup2(pipeStdin[0],0);
		dup2(pipeStdout[1],1);

		close(pipeStdin[0]);
		close(pipeStdin[1]);
		close(pipeStdout[0]);
		close(pipeStdout[1]);

		childFork(app.appData.toolServer);

		_exit(1);
	}	

	if (childPid == -1)
	{
		perror("fork");
		return 1;
	}

	close(pipeStdin[0]);
	close(pipeStdout[1]);

	client.fdRead = pipeStdout[0];
	client.fdWrite = pipeStdin[1];

	app.idReader = XtAppAddInput(app.appContext, client.fdRead, (XtPointer)XtInputReadMask, PipeReader, &app);
	app.readPipeAdded = 1;

	app.idWriter = XtAppAddInput(app.appContext, client.fdWrite, (XtPointer)XtInputWriteMask, PipeWriter, &app);
	app.writePipeAdded = 1;

	if (app.winList)
	{
		struct MPWShellWin *win=app.winList;

		while (win)
		{
			MPWShellWinOpen(win);
			win=win->next;
		}
	}
	else
	{
		XtAppSetExitFlag(app.appContext);
	}

	XtAppMainLoop(app.appContext);

	close(client.fdWrite);

	if (client.prefixLength)
	{
		write(1,client.prefix,client.prefixLength);
	}

	while (1)
	{
		char buf[256];
		int i=read(client.fdRead,buf,sizeof(buf));
		if (i > 0)
		{
			write(1,buf,i);
		}
		else
		{
			break;
		}
	}

	if (childPid == waitpid(childPid,&s,0))
	{
		if (WIFEXITED(s))
		{
			rc = WEXITSTATUS(s);
		}
		else
		{
			if (WIFSIGNALED(s))
			{
				rc=2;
			}
			else
			{
				rc=3;
			}
		}
	}
	
	return rc;
}

const char *MPWShellBaseName(const char *p,char c)
{
	const char *r=p;

	while (*p)
	{
		if (*p++ == c)
		{
			if (*p)
			{
				r = p;
			}
		}
	}

	return r;
}

