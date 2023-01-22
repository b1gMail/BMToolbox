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
#include <QSettings>
#include <QUrl>
#include <QDir>
#include <QMessageBox>
#include <QCryptographicHash>
#include <QCleanlooksStyle>
#include <QPlastiqueStyle>
#include <QFile>
#include <QAction>
#include <QWidget>
#include <QIcon>
#include <QPrinter>
#include <QNetworkProxyFactory>
#include <QTranslator>
#include <QDesktopServices>
#include <stdexcept>

#ifdef Q_WS_WIN
#include <windows.h>
#include <shellapi.h>
#endif

#ifdef Q_WS_MAC
#include "macloginitemsmanager.h"
#endif

#include "mainwindow.h"

QTranslator appTrans, qtTrans;

BMToolboxApp::BMToolboxApp(int argc, char *argv[]) :
#ifdef Q_WS_WIN
    QtSingleApplication(argc, argv)
#else
    QApplication(argc, argv)
#endif
{
#ifdef Q_WS_MAC
#ifdef QT_NO_DEBUG
    QStringList libPaths;
    libPaths << this->applicationDirPath() + "/../PlugIns/";
    setLibraryPaths(libPaths);
#endif
#endif

    setQuitOnLastWindowClosed(false);

    QNetworkProxyFactory::setUseSystemConfiguration(true);

#ifdef Q_WS_WIN
    appTrans.load("bmtoolbox_" + QLocale::system().name(),
                  this->applicationDirPath());
    qtTrans.load("qt_" + QLocale::system().name(),
                 this->applicationDirPath());
#endif

#ifdef Q_WS_MAC
    appTrans.load("bmtoolbox_" + QLocale::system().name(),
                  this->applicationDirPath() + "/../Resources/");
    qtTrans.load("qt_" + QLocale::system().name(),
                  this->applicationDirPath() + "/../Resources/");
#endif

    this->installTranslator(&qtTrans);
    this->installTranslator(&appTrans);

    if(!this->loadBranding())
    {
        throw std::runtime_error("Failed to load application branding.");
    }

#ifdef Q_WS_MAC
    // check if application is installed properly
    if(this->applicationFilePath().left(9).toLower() == "/volumes/")
    {
        QMessageBox::information(0,
                                 tr("Application not installed"),
                                 tr("%1 is not installed.\n\nPlease move %1 to your Applications folder and start it from there.")
                                    .arg(this->applicationName())
                                    .arg(this->applicationName()));
        throw std::runtime_error("Application not installed.");
    }
#endif

#ifdef Q_WS_WIN
    // delete updater, if exists
    QString updaterName = tbApp->applicationDirPath() + "/updater.exe";
    if(QFile().exists(updaterName))
        QFile().remove(updaterName);
#endif

    QString cssFile = this->applicationDirPath() + "/style.css";
    if(QFile::exists(cssFile))
    {
        QFile css(cssFile);
        if(css.open(QFile::ReadOnly))
        {
            QByteArray cssData = css.readAll();
            css.close();

            this->setStyleSheet(QString::fromUtf8(cssData, cssData.size()));
        }
    }
    else
    {
#ifdef Q_WS_WIN
        this->setStyleSheet("QToolBar { background: white; spacing: 4px; padding: 4px; }");
#endif
    }

    this->userPrefs = new QSettings(QSettings::UserScope, this->organizationName(), this->applicationName(), this);

#ifdef Q_WS_WIN
    connect(this, SIGNAL(messageReceived(const QString&)),
            this, SLOT(processMessage(const QString&)));
#endif
}

void BMToolboxApp::processMessage(const QString &msg)
{
    if(msg == "close")
    {
        this->quit();
    }
    else if(msg == "smsmanager")
    {
        emit showSMSManager();
    }
}

QString BMToolboxApp::defaultWebdiskFolder()
{
    QDir homeDir = QDir::home();
    QString webdiskDir = QString("%1 %2").arg(this->serviceName).arg(this->nameWebdisk);

    int num = 0;
    while(homeDir.exists(webdiskDir))
        webdiskDir = QString("%1 %2 %3").arg(this->serviceName).arg(this->nameWebdisk).arg(++num);

    return(QDir::toNativeSeparators(homeDir.absoluteFilePath(webdiskDir)));
}

bool BMToolboxApp::isInAutostart()
{
    QString appExe = QDir::toNativeSeparators(this->applicationFilePath()),
            appID = "BMToolBox-" + this->serviceID;

#ifdef Q_WS_WIN
    QSettings reg("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run", QSettings::NativeFormat);
    return(reg.value(appID, "") == appExe);
#endif

#ifdef Q_WS_MAC
    MacLoginItemsManager ml;
    return(ml.containsRunningApplication());
#endif

    return(false);
}

void BMToolboxApp::installFaxPrinter(bool uninstall, b1gMailAPI::ClientAPI *api)
{
#ifdef Q_WS_WIN
    QString appPath = QDir::toNativeSeparators(this->applicationDirPath() + "/faxdrv/BMFaxPrintInstall.exe"),
            appArgs;

    if(uninstall)
    {
        appArgs = QString("-uninstall")
                + QString(" \"%1\"").arg(this->faxPrinterName())
                + QString(" \"%1-%2\"").arg(this->serviceID).arg(api->userEmail);
    }
    else
    {
        appArgs = QString("-install")
                + QString(" \"%1\"").arg(this->faxPrinterName())
                + QString(" \"%1-%2\"").arg(this->serviceID).arg(api->userEmail)
                + QString(" \"%1\"").arg(api->userEmail)
                + QString(" \"%1\"").arg(QDir::toNativeSeparators(this->faxPath()));
    }

    QByteArray appPathStr = appPath.toUtf8(), appArgsStr = appArgs.toUtf8();

    SHELLEXECUTEINFOA exInfo;
    exInfo.cbSize		= sizeof(exInfo);
    exInfo.lpFile		= appPathStr.constData();
    exInfo.fMask		= SEE_MASK_NOCLOSEPROCESS|SEE_MASK_DOENVSUBST;
    exInfo.hwnd			= NULL;
    exInfo.lpVerb		= "open";
    exInfo.lpParameters	= appArgsStr.constData();
    exInfo.lpDirectory	= NULL;
    exInfo.nShow		= SW_SHOWNORMAL;
    exInfo.hInstApp		= (HINSTANCE)SE_ERR_DDEFAIL;

    ShellExecuteExA(&exInfo);
    if(exInfo.hInstApp == (HINSTANCE)SE_ERR_ACCESSDENIED)
    {
        exInfo.lpVerb = "runas";
        ShellExecuteExA(&exInfo);
    }

    if(exInfo.hProcess != NULL)
    {
        WaitForSingleObject(exInfo.hProcess, INFINITE);
        CloseHandle(exInfo.hProcess);
    }
#endif

#ifdef Q_WS_MAC
    QString cmd = "/usr/sbin/lpadmin";

    if(uninstall)
    {
        cmd += QString(" -x \"%1\"").arg(this->faxPrinterName());
    }
    else
    {
        // TODO: install PPD
        //       install backend

        cmd += QString(" -p \"%1\"").arg(this->faxPrinterName())
                + QString(" -m BMFaxPrint.ppd")
                + QString(" -o printer-is-shared=false")
                + QString(" -v \"%1\"").arg(this->faxPath())
                + QString(" -D \"%1 Fax\"").arg(this->serviceName)
                + QString(" -E")
                + QString(" -L \"%1\"").arg(this->serviceName);
    }

    // TODO: execute command
    QMessageBox::warning(0, "Command", cmd);
#endif
}

QString BMToolboxApp::faxPrinterName()
{
    return(tr("%1 Fax").arg(this->serviceName));
}

bool BMToolboxApp::isFaxPrinterInstalled()
{
    QPrinter p;
    p.setPrinterName(this->faxPrinterName());
    return(p.isValid());
}

void BMToolboxApp::setAutostart(bool enable)
{
    QString appExe = QDir::toNativeSeparators(this->applicationFilePath()),
            appID = "BMToolBox-" + this->serviceID;

#ifdef Q_WS_WIN
    QSettings reg("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run", QSettings::NativeFormat);

    if(!enable)
        reg.remove(appID);
    else
        reg.setValue(appID, appExe);
#endif

#ifdef Q_WS_MAC
    MacLoginItemsManager ml;
    if(!enable && ml.containsRunningApplication())
        ml.removeRunningApplication();
    else if(enable && !ml.containsRunningApplication())
        ml.appendRunningApplication();
#endif
}

void BMToolboxApp::addToolBarBranding(QWidget *owner, QToolBar *tb)
{
    if(!this->toolBarBranding)
        return;

    QWidget *tbSpacer = new QWidget(owner);
    tbSpacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    tb->addWidget(tbSpacer);
    tb->addSeparator();

    QIcon theIcon;
    theIcon.addPixmap(QPixmap(this->resPath("app-ico.png")), QIcon::Disabled);

    QAction *serviceAction = new QAction(this->serviceName, owner);
    serviceAction->setIcon(theIcon);
    serviceAction->setCheckable(false);
    serviceAction->setDisabled(true);
    tb->addAction(serviceAction);
}

bool BMToolboxApp::loadBranding()
{
#ifdef Q_WS_WIN
    QSettings brand(this->applicationDirPath() + "/branding.ini", QSettings::IniFormat);
#endif

#ifdef Q_WS_MAC
    QSettings brand(this->applicationDirPath() + "/../Resources/branding.ini", QSettings::IniFormat);
#endif

    this->appName           = brand.value("branding/appName", "Unknown App").toString();
    this->serviceName       = brand.value("branding/serviceName", "Unknown Service").toString();
    this->serviceURL        = brand.value("branding/serviceURL", "http://mail.unknown-service.xy/").toString();
    this->serviceID         = brand.value("branding/serviceID", "00000000").toString();
    this->copyrightNote     = brand.value("branding/copyrightNote", true).toBool();
    this->toolBarBranding   = brand.value("branding/toolBarBranding", true).toBool();
    this->appStyle          = brand.value("branding/appStyle", "default").toString();
    this->nameWebdisk       = brand.value("names/webdisk", "Webdisk").toString();
    this->nameSMSManager    = brand.value("names/smsManager", "SMS Manager").toString();
    this->setApplicationVersion(brand.value("common/version", "0.0.0").toString());

    // very simple branding file protection
    QString sep = ";", key = "vWeW>e!L{rtCE&E_aPpqN894pNlc/MRz";
    QString hashData = this->applicationVersion() + sep
            + this->appName + sep
            + this->serviceName + sep
            + this->serviceURL + sep
            + this->serviceID + sep
            + (this->copyrightNote ? "1" : "0") + sep
            + (this->toolBarBranding ? "1" : "0") + sep
            + this->appStyle + sep
            + this->nameWebdisk + sep
            + this->nameSMSManager + sep
            + key;
    QString hash = QCryptographicHash::hash(hashData.toUtf8(), QCryptographicHash::Md5).toHex().toLower();

    if(brand.value("file/checksum").toString() != hash)
    {
        QMessageBox::critical(0,
                              tr("Error"),
                              tr("The application files are corrupt.\n\nPlease re-install the application or contact the application vendor."));
        return(false);
    }

    if(this->serviceID.length() != 8 || this->serviceID == "00000000")
    {
        QMessageBox::critical(0,
                              tr("Error"),
                              tr("Invalid service ID.\n\nPlease re-install the application or contact the application vendor."));
        return(false);
    }

    this->setApplicationName(this->appName);
    this->setOrganizationName(this->serviceName);
    this->setOrganizationDomain(QUrl(this->serviceURL).host());

    if(this->appStyle == "cleanlooks")
    {
        this->setStyle(new QCleanlooksStyle());
    }
    else if(this->appStyle == "plastique")
    {
        this->setStyle(new QPlastiqueStyle());
    }

    return(true);
}

QString BMToolboxApp::resPath(QString fileName)
{
#ifdef Q_WS_WIN
    return(this->applicationDirPath() + "/res/" + fileName);
#endif

#ifdef Q_WS_MAC
    return(this->applicationDirPath() + "/../Resources/" + fileName);
#endif
}

QString BMToolboxApp::faxPath()
{
#ifdef Q_WS_MAC
    QString dir = QDesktopServices::storageLocation(QDesktopServices::DataLocation) + "/fax";
    QDir().mkpath(dir);
    return(dir);
#else
    return(this->applicationDirPath() + "/fax");
#endif
}
