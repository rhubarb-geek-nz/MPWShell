// Copyright (c) 2026 Roger Brown.
// Licensed under the MIT License.

#include "MPSMessageBox.h"
#include "MPSMainWindow.h"

MPSMessageBox::MPSMessageBox(MPSMainWindow *parent) : QMessageBox(parent)
{
	mainWindow=parent;
}

MPSMessageBox::~MPSMessageBox()
{
	mainWindow->alertClosed();
}
