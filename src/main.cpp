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

#include "bmtoolboxapp.h"
#include "mainwindow.h"
#include <stdexcept>
#include <QStringList>
#include <QTranslator>
#include <QLibraryInfo>
#include <QLocale>

#ifdef Q_WS_WIN
#include <windows.h>
#endif

#include "welcomewizard.h"

int main(int argc, char *argv[])
{
    try
    {
        QTranslator qtTrans;
        qtTrans.load("qt_" + QLocale::system().name(),
                   QLibraryInfo::location(QLibraryInfo::TranslationsPath));

        BMToolboxApp a(argc, argv);
        bool runSMSManager = false;

#ifdef Q_WS_WIN
        // args?
        QStringList args = a.arguments();
        if(args.count() == 2)
        {
            if(args.at(1) == "-uninstall")
            {
                // quit app if still running
                if(a.isRunning())
                {
                    a.sendMessage("close");
                    while(a.isRunning())
                        ::Sleep(100);
                }

                // delete fax printer
                if(a.isFaxPrinterInstalled())
                {
                    b1gMailAPI::ClientAPI api(a.serviceURL);
                    api.userEmail = a.userPrefs->value("account/emailAddress", "unknownUser").toString();
                    a.installFaxPrinter(true, &api);
                }

                // remove prefs
                a.userPrefs->clear();

                return(0);
            }

            else if(args.at(1) == "-close")
            {
                // quit app if still running
                if(a.isRunning())
                {
                    a.sendMessage("close");
                    while(a.isRunning())
                        ::Sleep(100);
                }

                return(0);
            }

            else if(args.at(1) == "-smsmanager")
            {
                // already running?
                if(a.isRunning())
                {
                    a.sendMessage("smsmanager");
                    return(0);
                }

                runSMSManager = true;
            }
        }

        // do not allow two instances
        if(a.isRunning())
        {
            return(0);
        }
#endif

        // first start?
        if(a.userPrefs->value("common/firstStart", true).toBool())
        {
            // yes. launch welcome wizard.
            WelcomeWizard *w = new WelcomeWizard();
            w->show();
        }
        else
        {
            // no. launch tray app.
            new MainWindow();

            // TODO: runSMSManager == true -> sms manager
        }

        return(a.exec());
    }
    catch(std::exception &)
    {
        return(1);
    }
}
