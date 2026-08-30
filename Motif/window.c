// Copyright (c) 2026 Roger Brown.
// Licensed under the MIT License.

#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <wchar.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <Xm/XmAll.h>
#include <X11/xpm.h>
#include <mbcs.h>

#include "mpwshell.h"
#include "mpwshell.xpm"
#include "mpwshell.xbm"

static const char *xmDIALOG_CANCEL_BUTTON = "Cancel";
static const char *xmDIALOG_HELP_BUTTON = "Help";
static const char *xmDIALOG_OK_BUTTON = "Ok";

struct MPWShellWin *MPWShellWinNew(struct MPWShellApp *app)
{
	struct MPWShellWin *win=calloc(sizeof(*win),1);
	win->clientId=++(app->nextClientId);
	win->app=app;
	win->next=app->winList;
	app->winList=win;
	return win;
}

void XmTextInsertUTF8(Widget w,XmTextPosition pos,const unsigned char *str,int len)
{
	wchar_t *buf=malloc(sizeof(buf[0])*(len+1));
	wchar_t *dest=buf;
	const unsigned char *src=str;

	while (len>0)
	{
		int run=mbcsLen(src,len);

		if (run > 0)
		{
			*dest++=mbcsToChar(src,len);
			src+=run;
			len-=run;
		}
		else
		{
			break;
		}
	}

	*dest=0;

	XmTextInsertWcs(w,pos,buf);

	free(buf);
}

static void keyPressHandler(Widget w, XtPointer client_data, XEvent *event, Boolean *call_relative)
{
	if (event->type == KeyPress)
	{
		struct MPWShellWin *win = client_data;
		if (win->textWidget == w)
		{
			KeySym keysym = XLookupKeysym(&event->xkey, 0);

			if (
				((event->xkey.state&ControlMask)&&(keysym==XK_Return))
				||
				((keysym==XK_KP_Enter)||(keysym==XK_Execute))
			)
			{
				XmTextPosition left = -1, right = -1;
				wchar_t *buf = NULL;
				int dataLen = 0;

				if (XmTextGetSelectionPosition(w,&left,&right))
				{
					buf = XmTextGetSelectionWcs(w);
					dataLen = right - left;
				}
				else
				{
					Position x,y;
					right = left=XmTextGetInsertionPosition(w);
					XmTextPosToXY(w,left,&x,&y);
					left=XmTextXYToPos(w,-0x3FFF,y);
					right=XmTextXYToPos(w,0x3FFF,y);
					if (right > left)
					{
						int len = right - left;
						buf = (XtPointer)XtMalloc(sizeof(buf[0])*(len+1));

						if (buf)
						{
							if (XmCOPY_SUCCEEDED == XmTextGetSubstringWcs(w,left,len,len+1,buf))
							{
								dataLen = len;
							}
						}
					}
				}

				if (buf)
				{
					if (dataLen)
					{
						struct MPWToolServerMessage *msg = MPWToolServerMessageNewWcs(0x50,win->clientId,win->runspaceId,buf,dataLen);

						if (msg)
						{
							MPWShellAppSend(win->app,msg);
							XmTextInsert(w,right,"\n");
							XmTextSetInsertionPosition(w,right+1);
							MPWToolServerMessageRelease(msg);
						}
					}

					XtFree((XtPointer)buf);
				}
			}
			else
			{
				switch (keysym)
				{
					case XK_Escape:
					case XK_Break:
						{
							struct MPWToolServerMessage *msg=MPWToolServerMessageNew(0x54,win->clientId,win->runspaceId,0);

							if (msg)
							{
								MPWShellAppSend(win->app,msg);
								MPWToolServerMessageRelease(msg);
							}
						}
						break;
					default:
						break;
				}
			}
		}
	}
}

static void MPWShellWinCloseAnyway(struct MPWShellWin *win)
{
	switch (win->state)
	{
		case 2:
			{
				struct MPWToolServerMessage *msg=MPWToolServerMessageNew(0x50,win->clientId,win->runspaceId,4);

				if (msg)
				{
					memcpy(msg->data,"Exit",4);
					msg->data[msg->dataLen]=0;
					MPWShellAppSend(win->app,msg);
					MPWToolServerMessageRelease(msg);
				}
			}
			break;
		default:
			MPWShellWinDestroy(win);
			break;
	}
}

static void MPWShellWinClose(Widget w,XtPointer client_data,XtPointer call_data)
{
	struct MPWShellWin *win = client_data;

	if (!win->deleting)
	{
		if (win->changed)
		{
			if (!win->yesNoCancelMapped)
			{
				win->yesNoCancelMapped = 1;
				XtManageChild(win->saveYesNoCancelBox);
			}
		}
		else
		{
			MPWShellWinCloseAnyway(win);
		}
	}
}

void MPWShellWinObituary(struct MPWShellWin *win)
{
	MPWShellWinClose(NULL, win, NULL);
}

static void FileCascadingCB(Widget w,XtPointer client_data, XtPointer call_data)
{
	struct MPWShellWin *win = client_data;
	XtSetSensitive(win->fileSaveGadget,win->changed ?  True : False);
}

static void EditCascadingCB(Widget w,XtPointer client_data, XtPointer call_data)
{
	struct MPWShellWin *win = client_data;
	XmTextPosition left = -1, right = -1, last = XmTextGetLastPosition(win->textWidget);
	Boolean b=XmTextGetSelectionPosition(win->textWidget, &left, &right);

	XtSetSensitive(win->editCutGadget, b ?  True : False);
	XtSetSensitive(win->editCopyGadget, b ?  True : False);
	XtSetSensitive(win->editClearGadget, b ?  True : False);
	XtSetSensitive(win->editSelectAllGadget, last ?  True : False);
}

static void HelpCascadingCB(Widget w,XtPointer client_data, XtPointer call_data)
{
}

static void FileNewCB(Widget w,XtPointer client_data, XtPointer call_data)
{
	struct MPWShellWin *win = client_data;
	struct MPWShellApp *app=win->app;
	struct MPWShellWin *newWin=MPWShellWinNew(app);
	MPWShellWinCreate(newWin,NULL,NULL);
	MPWShellWinOpen(newWin);
	if (app->toolServer->state==2)
	{
		int initialScriptLen=app->appData.initialScript ? strlen(app->appData.initialScript) : 0;
		struct MPWToolServerMessage* msg = MPWToolServerMessageNew(0x51, newWin->clientId, 0, initialScriptLen);
		if (initialScriptLen)
		{
			memcpy(msg->data,app->appData.initialScript,initialScriptLen);
		}
		MPWShellAppSend(app,msg);
		MPWToolServerMessageRelease(msg);
		newWin->state=1;
	}
}

static int MPWShellWinSave(struct MPWShellWin *win, const char *filePath)
{
	int rc=-1;

	if (filePath)
	{
		mode_t mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;
		int fd = creat(filePath, mode);

		if (fd != -1)
		{
			char *p = XmTextGetString(win->textWidget);

			rc=0;

			if (p)
			{
				int len = strlen(p);

				if (len)
				{
					if (len != write(fd,p,len))
					{
						rc = -1;
					}
				}

				XtFree(p);
			}

			if (rc != -1)
			{
				win->changed=0;
			}

			close(fd);

			return 0;
		}
	}

	return rc;
}

static void FileDialogOK(Widget w,XtPointer client_data, XtPointer call_data)
{
	struct MPWShellWin *win = client_data;
	struct MPWShellApp *app=win->app;
	int rc=-1;
	XmFileSelectionBoxCallbackStruct *cb = call_data;

	if (cb->value)
	{
		char *filePath = XmStringUnparse(cb->value, NULL, XmCHARSET_TEXT, XmCHARSET_TEXT, NULL, 0, XmOUTPUT_ALL);

		if (filePath)
		{
			switch (win->selectionDialogMode)
			{
				case 1:
					win = MPWShellWinNew(app);

					if (MPWShellWinCreate(win,filePath,NULL)<0)
					{
						MPWShellWinFree(win);
					}
					else
					{
						rc=0;
						MPWShellWinOpen(win);

						if (app->toolServer->state == 2)
						{
							int initialScriptLen=app->appData.initialScript ? strlen(app->appData.initialScript) : 0;
							struct MPWToolServerMessage* msg = MPWToolServerMessageNew(0x51, win->clientId, 0, initialScriptLen);
							if (initialScriptLen)
							{
								memcpy(msg->data,app->appData.initialScript,initialScriptLen);
							}
							MPWShellAppSend(app,msg);
							MPWToolServerMessageRelease(msg);
							win->state=1;
						}
					}
					break;
				case 2:
					rc=MPWShellWinSave(win,filePath);

					if (rc != -1)
					{
						char buf[256];
						Arg args[2];
						int n=0;

						if (win->filePath) free(win->filePath);
						win->filePath = strdup(filePath);
						win->baseName = MPWShellBaseName(win->filePath, '/');

						snprintf(buf, sizeof(buf), "%s - %s", app->appData.title, win->baseName);

						XtSetArg(args[n], XmNtitle, buf); n++;

						XtSetValues(win->shellWidget, args, n);

						if (win->closeAfterSave)
						{
							win->closeAfterSave = 0;
							MPWShellWinCloseAnyway(win);
						}
					}
					break;
				default:
					break;
			}

			XtFree(filePath);
		}
	}

	if (rc != -1)
	{
		XtUnmanageChild(w);
	}
}

static void FileDialogCancel(Widget w,XtPointer client_data, XtPointer call_data)
{
	XtUnmanageChild(w);
}

static void FileDialogUnmap(Widget w,XtPointer client_data, XtPointer call_data)
{
	struct MPWShellWin *win = client_data;
	win->selectionDialogMode = 0;
	win->closeAfterSave = 0;
}

static void SaveYesNoCancelUnmapCB(Widget w,XtPointer client_data, XtPointer call_data)
{
	struct MPWShellWin *win = client_data;
	win->yesNoCancelMapped = 0;
}

static void FileOpenCB(Widget w,XtPointer client_data, XtPointer call_data)
{
	struct MPWShellWin *win = client_data;
	if (win->selectionDialogMode == 0)
	{
		Widget dialog = win->selectionDialog;
		Arg args[10];
		int n=0;
		XmString openLabel = XmStringCreateLocalized("Open");

		XtSetArg(args[n], XmNtitle,win->app->appData.title); n++;
		XtSetArg(args[n], XmNokLabelString, openLabel); n++;
		XtSetArg(args[n], XmNdefaultButton, XtNameToWidget(dialog, xmDIALOG_OK_BUTTON)); n++;
		XtSetValues(dialog, args, n);

		XtManageChild(dialog);
		XmStringFree(openLabel);
		win->selectionDialogMode = 1;
	}
}

static void FileSaveCB(Widget w,XtPointer client_data, XtPointer call_data)
{
	struct MPWShellWin *win = client_data;

	if (win->filePath)
	{
		MPWShellWinSave(win, win->filePath);
	}
}

static void FileSaveAsCB(Widget w,XtPointer client_data, XtPointer call_data)
{
	struct MPWShellWin *win = client_data;
	if (win->selectionDialogMode == 0)
	{
		Arg args[10];
		int n=0;
		Widget dialog=win->selectionDialog;
		XmString saveLabel = XmStringCreateLocalized("Save");

		XtSetArg(args[n], XmNtitle,win->app->appData.title); n++;
		XtSetArg(args[n], XmNokLabelString, saveLabel); n++;
		XtSetArg(args[n], XmNdefaultButton, XtNameToWidget(dialog, xmDIALOG_OK_BUTTON)); n++;
		XtSetValues(dialog, args, n);

		XtManageChild(dialog);
		XmStringFree(saveLabel);

		win->selectionDialogMode = 2;
	}
}

static void FileCloseCB(Widget w,XtPointer client_data, XtPointer call_data)
{
	MPWShellWinClose(w,client_data,call_data);
}

static void EditCutCB(Widget w,XtPointer client_data, XtPointer call_data)
{
	struct MPWShellWin *win = client_data;
	Time time=XtLastTimestampProcessed(win->app->display);
	XmTextCut(win->textWidget,time);
}

static void EditCopyCB(Widget w,XtPointer client_data, XtPointer call_data)
{
	struct MPWShellWin *win = client_data;
	Time time=XtLastTimestampProcessed(win->app->display);
	XmTextCopy(win->textWidget,time);
}

static void EditPasteCB(Widget w,XtPointer client_data, XtPointer call_data)
{
	struct MPWShellWin *win = client_data;
	XmTextPaste(win->textWidget);
}

static void EditClearCB(Widget w,XtPointer client_data, XtPointer call_data)
{
	struct MPWShellWin *win = client_data;
	XmTextRemove(win->textWidget);
}

void MPWShellWinMessageBox(struct MPWShellWin *win)
{
	if (win->messageQueue && !win->aboutMapped)
	{
		Arg args[3];
		int n=0;
		struct MPWToolServerMessage *msg=win->messageQueue;
		XmString messageText = XmStringGenerate(msg->data, "UTF-8", XmCHARSET_TEXT, NULL);
		Widget messageBox = msg->packetType ? win->infoMessageBox : win->aboutMessageBox;

		win->messageQueue = msg->next;
		win->aboutMapped = 1;

		XtSetArg(args[n], XmNmessageString, messageText); n++;

		MPWToolServerMessageRelease(msg);

		XtSetValues(messageBox,args,n);

		XmStringFree(messageText);

		XtManageChild(messageBox);
	}
}

static Boolean MessageBoxWorkProc(XtPointer closure)
{
	struct MPWShellWin *win=closure;

	XtRemoveWorkProc(win->messageWorkProc);

	MPWShellWinMessageBox(win);	

	return True;
}
static void HelpAboutCB(Widget w,XtPointer client_data, XtPointer call_data)
{
	struct MPWShellWin *win = client_data;
	const char *msgText=MPWSHELL_ABOUT;
	int msgLen=strlen(msgText);

	struct MPWToolServerMessage *msg=MPWToolServerMessageNew(0,win->clientId,0,msgLen);

	if (msg)
	{
		memcpy(msg->data,msgText,msgLen);

		if (win->messageQueue)
		{
			struct MPWToolServerMessage *p=win->messageQueue;
			while (p->next) p=p->next;
			p->next=msg;
		}
		else
		{
			win->messageQueue = msg;
		}

		MPWShellWinMessageBox(win);
	}
}

static void HelpAboutOkCB(Widget w,XtPointer client_data, XtPointer call_data)
{
}

static void HelpAboutUnmapCB(Widget w,XtPointer client_data, XtPointer call_data)
{
	struct MPWShellWin *win = client_data;

	win->aboutMapped = 0;

	if (win->messageQueue)
	{
		win->messageWorkProc = XtAppAddWorkProc(win->app->appContext,MessageBoxWorkProc,win);
	}
}

static void ValueChangedCB(Widget w,XtPointer client_data, XtPointer call_data)
{
	struct MPWShellWin *win = client_data;
	win->changed=1;
}

static void EditSelectAllCB(Widget w,XtPointer client_data, XtPointer call_data)
{
	struct MPWShellWin *win = client_data;
	Time time=XtLastTimestampProcessed(win->app->display);
	XmTextPosition left=0,right=XmTextGetLastPosition(win->textWidget);
	XmTextSetSelection(win->textWidget,left,right,time);
}

struct MenuItem
{
	char *name;
	char *labelString;
	char *acceleratorText;
	const char *accelerator;
	const char mnemonic;
	XtCallbackProc callback;
	size_t gadgetOffset;
};

static struct MenuItem fileMenuItems[]={
	{"new","New","Ctrl+N","Ctrl <Key>N",'N',FileNewCB},
	{"open","Open","Ctrl+O","Ctrl <Key>O",'O',FileOpenCB},
	{NULL},
	{"save","Save","Ctrl+S","Ctrl <Key>S",'S',FileSaveCB,offsetof(struct MPWShellWin,fileSaveGadget)},
	{"saveas","Save As...",NULL,NULL,'A',FileSaveAsCB},
	{NULL},
	{"close","Close","Ctrl+W","Ctrl <Key>W",'W',FileCloseCB}
};

static struct MenuItem editMenuItems[]={
	{"cut","Cut","Ctrl+X","Ctrl <Key>X",'t',EditCutCB,offsetof(struct MPWShellWin,editCutGadget)},
	{"copy","Copy","Ctrl+C","Ctrl <Key>C",'C',EditCopyCB,offsetof(struct MPWShellWin,editCopyGadget)},
	{"paste","Paste","Ctrl+V","Ctrl <Key>V",'V',EditPasteCB},
	{"clear","Clear",NULL,NULL,'e',EditClearCB,offsetof(struct MPWShellWin,editClearGadget)},
	{NULL},
	{"selectall","Select All","Ctrl+A","Ctrl <Key>A",'A',EditSelectAllCB,offsetof(struct MPWShellWin,editSelectAllGadget)}
};

static struct MenuItem helpMenuItems[]={
	{"about","About MPW Shell","F1","<Key>F1",'A',HelpAboutCB}
};

static void CreateMenuItems(struct MPWShellWin *win,Widget pane,struct MenuItem *item,int itemCount)
{
	Widget wList[12];
	int count=0;

	while (itemCount--)
	{
		if (item->name)
		{
			Arg args[10];
			int n=0;
			Widget gadget;
			XmString accelStr=NULL;
			XmString labelStr=XmStringCreateLocalized(item->labelString);
			XtSetArg(args[n],XmNsubMenuId,pane); n++;
			XtSetArg(args[n],XmNlabelString,labelStr); n++;
			if (item->mnemonic)
			{
				XtSetArg(args[n],XmNmnemonic,'F'); n++;
			}
			if (item->acceleratorText)
			{
				accelStr=XmStringCreateLocalized(item->acceleratorText);
				XtSetArg(args[n], XmNacceleratorText, accelStr); n++;
			}
			if (item->accelerator)
			{
				XtSetArg(args[n], XmNaccelerator, item->accelerator); n++;
			}
			gadget=XmCreatePushButtonGadget(pane,item->name,args,n);
			
			XtAddCallback(gadget, XmNactivateCallback, item->callback, win);
			wList[count++]=gadget;

			if (item->gadgetOffset)
			{
				((Widget *)(((char *)win)+item->gadgetOffset))[0]=gadget;
			}

			XmStringFree(labelStr);
			if (accelStr) XmStringFree(accelStr);
		}
		else
		{
			wList[count++] = XmCreateSeparatorGadget (pane, "ExSp", NULL, 0);
		}

		item++;
	}

	XtManageChildren(wList,count);
}

struct MenuPopup
{
	char *pullDownName;
	char *labelString;
	char mnemonic;
	char *cascadeName;
	XtCallbackProc cascadeCB;
	struct MenuItem *item;
	int itemCount;
};

static struct MenuPopup menuBarList[]=
{
	{"fileMenu","File",'F',"File",FileCascadingCB,fileMenuItems,XtNumber(fileMenuItems)},
	{"editMenu","Edit",'E',"Edit",EditCascadingCB,editMenuItems,XtNumber(editMenuItems)},
	{"helpMenu","Help",'H',"Edit",HelpCascadingCB,helpMenuItems,XtNumber(helpMenuItems)}
};

static void CreateMenuBar(struct MPWShellWin *win,struct MenuPopup *popup, int popupCount)
{
	Widget last;

	while (popupCount--)
	{
		Widget pane = XmCreatePulldownMenu(win->menubarWidget,popup->pullDownName,NULL,0);
		Arg args[5];
		int n=0;
		Widget cascade;
		XmString labelStr=XmStringCreateLocalized(popup->labelString);
		XtSetArg(args[n],XmNsubMenuId,pane); n++;
		XtSetArg(args[n],XmNlabelString,labelStr); n++;
		XtSetArg(args[n],XmNmnemonic,popup->mnemonic); n++;
		cascade=XmCreateCascadeButtonGadget(win->menubarWidget,popup->cascadeName,args,n);
		XtAddCallback(cascade, XmNcascadingCallback,popup->cascadeCB,win);
		XtManageChild(cascade);
		XmStringFree(labelStr);

		CreateMenuItems(win,pane,popup->item,popup->itemCount);

		last=cascade;

		popup++;		
	}

	XtVaSetValues(win->menubarWidget,XmNmenuHelpWidget,last,NULL);
}

int MPWShellWinCreate(struct MPWShellWin *win, const char *file, struct MPWToolServerMessage *msg)
{
	char *textContent=NULL;

	if (file)
	{
		int fd;
		struct stat s;

		win->filePath = realpath(file, NULL);

		if (!win->filePath)
		{
			perror(file);
			return -1;
		}

		win->baseName = MPWShellBaseName(win->filePath, '/');

		fd = open(win->filePath,O_RDONLY);

		if (fd == -1)
		{
			perror(win->filePath);
			return -1;
		}

		if (fstat(fd,&s))
		{
			close(fd);
			perror(win->filePath);
			return -1;
		}

		if (s.st_size)
		{
			int i;
			textContent=malloc(s.st_size+1);
			if (textContent)
			{
				textContent[s.st_size]=0;
				i=read(fd,textContent,s.st_size);
				if (i!=s.st_size)
				{
					close(fd);
					perror(win->filePath);
					free(textContent);
					return -1;
				}
			}
		}

		close(fd);
	}

	win->shellWidget = XtVaAppCreateShell(NULL, NULL, topLevelShellWidgetClass, win->app->display,
		XmNuserData, win,
		XmNdeleteResponse, XmDO_NOTHING,
		NULL);

	XmAddWMProtocolCallback(win->shellWidget, win->app->WM_DELETE_WINDOW, MPWShellWinClose, win);

	win->mainWidget = XtVaCreateWidget("mainWidget", xmMainWindowWidgetClass, win->shellWidget, XmNuserData, win, NULL);

	win->menubarWidget = XmCreateMenuBar(win->mainWidget, "menuBar", NULL, 0);

	CreateMenuBar(win,menuBarList,XtNumber(menuBarList));

	win->textWidget = XtVaCreateManagedWidget("textEdit",
		xmTextWidgetClass, win->mainWidget,
		XmNeditable, True,
		XmNsensitive, True,
		XmNrows, 24,
		XmNcolumns, 80,
		XmNeditMode, XmMULTI_LINE_EDIT,
		XmNuserData, win,
		NULL);

	XtAddEventHandler(win->textWidget, KeyPressMask, False, keyPressHandler, win);
	XtAddCallback(win->textWidget, XmNvalueChangedCallback, ValueChangedCB, win);

	if (msg && msg->packetType == 0x47 && msg->dataLen)
	{
		wchar_t *buf=malloc(sizeof(*buf)*(msg->dataLen+1));
		int len=msg->dataLen;
		const unsigned char *src=msg->data;
		wchar_t *dest=buf;
		while (len > 0)
		{
			int run=mbcsLen(src,len);

			if (run > 0)
			{
				*dest++=mbcsToChar(src,len);
				src+=run;
				len-=run;
			}
			else
			{
				break;
			}
		}
		*dest=0;
		XmTextSetStringWcs(win->textWidget,buf);
		win->changed=0;
		free(buf);
	}
	else
	{
		if (textContent)
		{
			XmTextSetString(win->textWidget, textContent);
			win->changed=0;

			free(textContent);
		}
	}

	return 0;
}

static void SaveYesNoCancelCB(Widget w,XtPointer client_data, XtPointer call_data)
{
	XmAnyCallbackStruct *cbs = (XmAnyCallbackStruct *) call_data;
	struct MPWShellWin *win = client_data;
	int rc = -1;

	switch (cbs->reason)
	{
		case XmCR_OK:
			if (win->filePath)
			{
				rc = MPWShellWinSave(win, win->filePath);

				if (rc != -1)
				{
					MPWShellWinCloseAnyway(win);
				}
			}
			else
			{
				XtUnmanageChild(w);
				win->closeAfterSave = 1;
				FileSaveAsCB(w,client_data,call_data);
			}

			break;
		case XmCR_CANCEL:
			rc=0;
			win->changed=0;
			MPWShellWinCloseAnyway(win);
			break;
		case XmCR_HELP:
			rc=0;
			break;
		case XmCR_ACTIVATE:
			break;
	}

	if (rc != -1)
	{
		XtUnmanageChild(w);
	}
}

void MPWShellWinOpen(struct MPWShellWin *win)
{
	Display *display=win->app->display;
	XpmAttributes attributes;
	Window window;
	char buf[512];
	char *title = win->app->appData.title;
	XmString messageText = XmStringGenerate("this page is left intentionally blank", "UTF-8", XmCHARSET_TEXT, NULL);

	if (win->baseName)
	{
		snprintf(buf, sizeof(buf), "%s - %s", title, win->baseName);
		title = buf;
	}

	XtVaSetValues(win->shellWidget, XmNtitle, title, XmNiconName, win->app->appData.title, NULL);

	win->aboutPixmap=XCreateBitmapFromData(
			win->app->display,
			XRootWindowOfScreen(XtScreen(win->shellWidget)),
			mpwshell_bits,mpwshell_width,mpwshell_height);

	{
		Arg args[10];
		int n=0;
		XmString messageText = XmStringCreateLocalized("Content has changed, save?");
		XmString yesText = XmStringCreateLocalized("Yes");
		XmString noText = XmStringCreateLocalized("No");
		XmString cancelText = XmStringCreateLocalized("Cancel");
		XtSetArg(args[n],XmNmessageString, messageText); n++;
		XtSetArg(args[n],XmNdefaultButtonType, XmDIALOG_OK_BUTTON); n++;
		XtSetArg(args[n],XmNtitle, win->app->appData.title); n++;
		XtSetArg(args[n], XmNautoUnmanage, False); n++;
		XtSetArg(args[n], XmNokLabelString, yesText); n++;
		XtSetArg(args[n], XmNcancelLabelString, noText); n++;
		XtSetArg(args[n], XmNhelpLabelString, cancelText); n++;

		win->saveYesNoCancelBox = XmCreateQuestionDialog(win->shellWidget,"helpDialog",args,n);
		XtAddCallback(win->saveYesNoCancelBox, XmNokCallback, SaveYesNoCancelCB, win);
		XtAddCallback(win->saveYesNoCancelBox, XmNcancelCallback, SaveYesNoCancelCB, win);
		XtAddCallback(win->saveYesNoCancelBox, XmNhelpCallback, SaveYesNoCancelCB, win);
		XtAddCallback(win->saveYesNoCancelBox, XmNunmapCallback, SaveYesNoCancelUnmapCB, win);
 		XmStringFree(messageText);
 		XmStringFree(yesText);
 		XmStringFree(noText);
 		XmStringFree(cancelText);
	}

	for (int m=0; m<2; m++)
	{
		Arg args[5];
		int n=0;

		XtSetArg(args[n],XmNmessageString, messageText); n++;
		XtSetArg(args[n],XmNdefaultButtonType, XmDIALOG_OK_BUTTON); n++;
		XtSetArg(args[n],XmNtitle, win->app->appData.title); n++;

		Widget messageBox = XmCreateMessageDialog(win->shellWidget,"helpDialog",args,n);

		XtAddCallback(messageBox, XmNokCallback, HelpAboutOkCB, win);
		XtAddCallback(messageBox, XmNunmapCallback, HelpAboutUnmapCB, win);

		XtUnmanageChild(XtNameToWidget(messageBox, xmDIALOG_CANCEL_BUTTON));
		XtUnmanageChild(XtNameToWidget(messageBox, xmDIALOG_HELP_BUTTON));

		n=0;
		XtSetArg(args[n], XmNdialogType, XmDIALOG_INFORMATION); n++;
		XtSetArg(args[n], XmNsymbolPixmap, m ? XmUNSPECIFIED_PIXMAP : win->aboutPixmap); n++;
		XtSetValues(messageBox, args, n);

		if (m)
		{
			win->infoMessageBox=messageBox;
		}
		else
		{
			win->aboutMessageBox=messageBox;
		}
	}

	XmStringFree(messageText);

	{
		Arg args[5];
		int n=0;
		XtSetArg(args[n], XmNtitle, win->app->appData.title); n++;
		win->selectionDialog = XmCreateFileSelectionDialog(win->shellWidget, "fileDialog", args, n);

		XtAddCallback(win->selectionDialog, XmNcancelCallback, FileDialogCancel, win);
		XtAddCallback(win->selectionDialog, XmNokCallback, FileDialogOK, win);
		XtAddCallback(win->selectionDialog, XmNunmapCallback, FileDialogUnmap, win);
	}

	memset(&attributes,0,sizeof(attributes));
	
	XtManageChild(win->menubarWidget);
	XtManageChild(win->mainWidget);
	XtRealizeWidget(win->shellWidget);

	window = XtWindow(win->shellWidget);

	XpmCreatePixmapFromData(display, window, mpwshell_xpm, &win->iconPixmap, &win->iconMask,&attributes);

	XWMHints hints;
	hints.flags = IconPixmapHint | IconMaskHint;
	hints.icon_pixmap = win->iconPixmap;
	hints.icon_mask = win->iconMask;

	XSetWMHints(display, window, &hints);
}

void MPWShellWinFree(struct MPWShellWin *win)
{
	struct MPWShellApp *app=win->app;
	Display *display=app->display;

	win->app=NULL;

	if (win == app->winList)
	{
		app->winList = win->next;
	}
	else
	{
		struct MPWShellWin *p = app->winList;
		while (p->next != win)
		{
			p = p->next;
		}
		p->next = win->next;
	}

	if (win->selectionDialog)
	{
		XtDestroyWidget(win->selectionDialog);
	}

	if (win->aboutMessageBox)
	{
		XtDestroyWidget(win->aboutMessageBox);
	}

	if (win->infoMessageBox)
	{
		XtDestroyWidget(win->infoMessageBox);
	}

	if (win->shellWidget)
	{
		XtDestroyWidget(win->shellWidget);
	}

	if (win->aboutPixmap)
	{
		XFreePixmap(display,win->aboutPixmap);
	}

	if (win->iconMask)
	{
		XFreePixmap(display,win->iconMask);
	}

	if (win->iconPixmap)
	{
		XFreePixmap(display,win->iconPixmap);
	}

	if (win->filePath)
	{
		free(win->filePath);
	}

	free(win);
}

static Boolean DestroyWindow(XtPointer closure)
{
	struct MPWShellWin *win=closure;
	struct MPWShellApp *app=win->app;

	XtRemoveWorkProc(win->deletionWorkProc);

	MPWShellWinFree(win);

	if (!app->winList)
	{
		XtAppSetExitFlag(app->appContext);
	}

	return True;
}

void MPWShellWinDestroy(struct MPWShellWin *win)
{
	if (!win->deleting)
	{
		win->deleting=1;

		win->deletionWorkProc=XtAppAddWorkProc(win->app->appContext,DestroyWindow,win);
	}
}

