// Copyright (c) 2026 Roger Brown.
// Licensed under the MIT License.

#include "MPSApplication.h"
#include "MPSDocumentManager.h"
#include "MPSDocument.h"
#include "MPSMainWindow.h"
#include <signal.h>

static void MPWShellSignalHandler(int)
{
    exit(127);
}

int main(int argc, char *argv[]) 
{
    struct sigaction sa;

    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = MPWShellSignalHandler;
    sigaction(SIGABRT, &sa, NULL);

    MPSApplication app(argc, argv);
    int i=0,j=0;

    for (const QString &filePath : app.arguments())
    {
        if (i)
        {
            if (app.documentManager->openDocument(filePath, nullptr))
            {
                j++;
            }
            else
            {
                return 1;
            }
        }

        i++;
    }

    if (j==0)
    {
        QString str;
        app.documentManager->newDocument(str);
    }

    for (const auto &item : app.documentManager->documentList)
    {
        item->newMainWindow()->show();
    }

    app.startup();

    return app.exec();
}
