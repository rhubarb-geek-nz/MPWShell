// Copyright (c) 2026 Roger Brown.
// Licensed under the MIT License.

#ifndef MPSSETTINGS_H
#define MPSSETTINGS_H

#include <QSettings>

class MPSApplication;

class MPSSettings : public QSettings
{
    Q_OBJECT
public:
    MPSSettings(MPSApplication *app);
	QString toolServerProgram;
	QStringList toolServerArguments;
	QVariantMap toolServerEnvironment;
};
#endif
