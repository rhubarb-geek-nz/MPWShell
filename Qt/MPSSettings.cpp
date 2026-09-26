// Copyright (c) 2026 Roger Brown.
// Licensed under the MIT License.

#include "MPSSettings.h"
#include "MPSApplication.h"
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

static const char *szToolServerProgram="ToolServerProgram";
static const char *szToolServerArguments="ToolServerArguments";
static const char *szToolServerEnvironment="ToolServerEnvironment";
static const char *szInitialScript="InitialScript";

MPSSettings::MPSSettings(MPSApplication *app) : QSettings("rhubarb.geek.nz","mpwshell")
{
	QFile settingsFile(":/mpwshell.json");
	if (settingsFile.open(QIODevice::ReadOnly | QIODevice::Text))
	{
		QByteArray jsonData=settingsFile.readAll();
		settingsFile.close();

		QJsonParseError parseError;
		QJsonDocument jsonDoc = QJsonDocument::fromJson(jsonData,&parseError);
		QJsonObject jsonObj = jsonDoc.object();

		toolServerProgram=value(szToolServerProgram,jsonObj[szToolServerProgram].toString()).toString();

		QStringList argumentsFromJson;
		
		for (const QJsonValue &value : jsonObj[szToolServerArguments].toArray())
		{
			argumentsFromJson.append(value.toString());
		}

		toolServerArguments = value(szToolServerArguments, argumentsFromJson).toStringList();

		app->initialScript = value(szInitialScript,jsonObj[szInitialScript].toString()).toString();

		toolServerEnvironment = value(szToolServerEnvironment,jsonObj[szToolServerEnvironment].toObject().toVariantMap()).toMap();
	}
}
