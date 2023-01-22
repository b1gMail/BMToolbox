/*
 * BMToolbox
 * Copyright (c) 2002-2022
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 */

#ifndef BMTOOLBOXAPP_H
#define BMTOOLBOXAPP_H

#include <QtGui/QApplication>
#include <QSettings>
#include <QToolBar>
#include "clientapi.h"

#ifdef Q_WS_WIN
#include <QtSingleApplication/QtSingleApplication>

class BMToolboxApp : public QtSingleApplication
#else
class BMToolboxApp : public QApplication
#endif
{
    Q_OBJECT
public:
    explicit BMToolboxApp(int argc, char *argv[]);

private:
    bool loadBranding();

public:
    QString resPath(QString fileName);
    QString faxPath();
    void addToolBarBranding(QWidget *owner, QToolBar *tb);
    bool isInAutostart();
    bool isFaxPrinterInstalled();
    void installFaxPrinter(bool uninstall, b1gMailAPI::ClientAPI *api);
    void setAutostart(bool enable);
    QString faxPrinterName();
    QString defaultWebdiskFolder();

public slots:
    void processMessage(const QString &msg);

signals:
    void showSMSManager();

public:
    QString appName, serviceName, serviceURL, serviceID, appStyle;
    QString nameWebdisk, nameSMSManager;
    bool copyrightNote, toolBarBranding;
    QSettings *userPrefs;
};

#define tbApp       (dynamic_cast<BMToolboxApp *>(qApp))

#endif // BMTOOLBOXAPP_H
