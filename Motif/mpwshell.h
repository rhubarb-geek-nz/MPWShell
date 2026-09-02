// Copyright (c) 2026 Roger Brown.
// Licensed under the MIT License.

#define MPWTOOLS_CLIENT_USERTYPE struct MPWShellApp

#include <mpwtools.h>

typedef struct {
	String toolServer, initialScript, title;
} AppData;

struct MPWShellApp
{
	const char *appName;
	int writePipeAdded,readPipeAdded,nextClientId;
	XtInputId idReader,idWriter;
	XtAppContext appContext;
	struct MPWToolServer *toolServer;
	Display *display;
	struct MPWShellWin *winList;
	AppData appData;
	char useColourIcon;
	Atom WM_DELETE_WINDOW, _NET_SUPPORTED;
};

struct MPWShellWin
{
	struct MPWShellWin *next;
	struct MPWShellApp *app;
	int state, clientId, runspaceId;
	Widget shellWidget, textWidget, menubarWidget, mainWidget, aboutMessageBox, infoMessageBox, selectionDialog, saveYesNoCancelBox;
	Widget fileSaveGadget,editCutGadget, editCopyGadget, editSelectAllGadget, editClearGadget;
	Pixmap iconPixmap, iconMask, aboutPixmap;
	char *filePath;
	const char *baseName;
	char deleting, aboutMapped, changed, selectionDialogMode, yesNoCancelMapped, closeAfterSave;
	struct MPWToolServerMessage *messageQueue;
	XtWorkProcId deletionWorkProc, messageWorkProc;
};

extern struct MPWShellWin *MPWShellWinNew(struct MPWShellApp *app);
extern void XmTextInsertUTF8(Widget w,XmTextPosition pos,const unsigned char *str,int len);
extern void MPWShellAppSend(struct MPWShellApp *app,struct MPWToolServerMessage *msg);
extern int MPWShellWinCreate(struct MPWShellWin *win,const char *file,struct MPWToolServerMessage *msg);
extern void MPWShellWinOpen(struct MPWShellWin *win);
extern void MPWShellWinDestroy(struct MPWShellWin *win);
extern void MPWShellWinFree(struct MPWShellWin *win);
extern void MPWShellWinMessageBox(struct MPWShellWin *win);
extern const char *MPWShellBaseName(const char *,char);
extern void MPWShellWinObituary(struct MPWShellWin *win);
