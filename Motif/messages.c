// Copyright (c) 2026 Roger Brown.
// Licensed under the MIT License.

#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <Xm/XmAll.h>
#include <mbcs.h>

#include "mpwshell.h"

void MPWToolServerOnRead(struct MPWToolServer* server, struct MPWToolServerMessage* message)
{
	struct MPWShellApp *app=server->userData;

	if (message->packetType == 0x46 && message->dataLen)
	{
		write(1,message->data,message->dataLen);
	}

	switch (server->state)
	{
		case 0:
			break;
		case 1:
			{
				int initialScriptLen=app->appData.initialScript ? strlen(app->appData.initialScript) : 0;
				struct MPWShellWin *win=app->winList;

				while (win)
				{
					struct MPWToolServerMessage* msg = MPWToolServerMessageNew(0x51, win->clientId, 0, initialScriptLen);
					if (initialScriptLen)
					{
						memcpy(msg->data,app->appData.initialScript,initialScriptLen);
					}
					MPWShellAppSend(app,msg);
					MPWToolServerMessageRelease(msg);
					win->state=1;
					win=win->next;
				}

				server->state=2;
			}
			break;
		case 2:
			switch (message->packetType)
			{
				case 0x42:
					{
						struct MPWShellWin *win=app->winList;

						while (win)
						{
							if (win->state == 2 && message->clientId == win->clientId && message->runspaceId == win->runspaceId)
							{
								struct MPWShellWin *next=win->next;
								win->state = 3;
								win->runspaceId=0;
								MPWShellWinObituary(win);
								win=next;
							}
							else
							{
								win=win->next;
							}
						}
					}
					break;
				case 0x40:
				case 0x43:
				case 0x44:
					{
						struct MPWShellWin *win=app->winList;

						while (win)
						{
							if (win->state == 2 && message->clientId == win->clientId && message->runspaceId == win->runspaceId)
							{
								Widget w=win->textWidget;
								XmTextPosition left,right;

								if (XmTextGetSelectionPosition(w,&left,&right))
								{
									XmTextInsertUTF8(w,right,message->data,message->dataLen);
								}
								else
								{
									right=XmTextGetInsertionPosition(w);
									XmTextInsertUTF8(w,right,message->data,message->dataLen);
								}

							}
							win=win->next;
						}
					}
					break;
				case 0x41: /* pairing request complete */
					{
						struct MPWShellWin *win=app->winList;

						while (win)
						{
							if (win->state == 1 && win->clientId == message->clientId)
							{
								win->state=2;
								win->runspaceId=message->runspaceId;
							}

							win=win->next;
						}
					}
					break;
				case 0x45: /* current command complete */
					break;
				case 0x47: /* create new document */
					{
						int initialScriptLen=app->appData.initialScript ? strlen(app->appData.initialScript) : 0;
						struct MPWShellWin *win=MPWShellWinNew(app);
						win->state=2;
						win->runspaceId=message->runspaceId;
						MPWShellWinCreate(win,NULL,message);
						MPWShellWinOpen(win);
						message=MPWToolServerMessageNew(0x55,win->clientId,win->runspaceId,initialScriptLen);
						if (initialScriptLen)
						{
							memcpy(message->data,app->appData.initialScript,initialScriptLen);
						}
						MPWShellAppSend(app,message);
						MPWToolServerMessageRelease(message);
					}
					break;
				case 0x48: /* open existing document */
					{
						const char *filePath=(const char *)message->data;
						struct MPWShellWin *win = MPWShellWinNew(app);

						if (MPWShellWinCreate(win, filePath, NULL) < 0)
						{
							MPWShellWinFree(win);
							message=MPWToolServerMessageNew(0x52,message->clientId,message->runspaceId,0);
						}
						else
						{
							int initialScriptLen=app->appData.initialScript ? strlen(app->appData.initialScript) : 0;
							win->state=2;
							win->runspaceId=message->runspaceId;
							MPWShellWinOpen(win);
							message=MPWToolServerMessageNew(0x55,win->clientId,win->runspaceId,initialScriptLen);
							if (initialScriptLen)
							{
								memcpy(message->data,app->appData.initialScript,initialScriptLen);
							}
						}
						MPWShellAppSend(app,message);
						MPWToolServerMessageRelease(message);
					}
					break;
				case 0x49:
					{
						struct MPWShellWin *win=app->winList;

						while (win)
						{
							if (win->state == 2 && message->clientId == win->clientId && message->runspaceId == win->runspaceId)
							{
								break;
							}

							win=win->next;
						}

						if (win)
						{
							struct MPWToolServerMessage *p=win->messageQueue;
							message->next = NULL;
							message->usage++;

							if (p)
							{
								while (p->next) p=p->next;
								p->next = message;
							}
							else
							{
								win->messageQueue=message;
								MPWShellWinMessageBox(win);
							}
						}
					}

					break;
			}
			break;
		default:
			break;
	}
}
