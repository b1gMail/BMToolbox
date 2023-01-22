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

#include "mainwindow.h"
#include "bmtoolboxapp.h"
#include "ui_mainwindow.h"
#include "aboutdialog.h"
#include "clientapi.h"
#include "webdisksync.h"
#include "updatenotification.h"

#include <QMessageBox>
#include <QMenu>
#include <QDesktopServices>
#include <QSound>
#include <QImage>
#include <QPrinter>
#include <QPainter>
#include <QFileDialog>
#include <QProcess>
#include <QPainter>
#include <QMovie>

#include <stdio.h>

#include "smsmanager.h"
#include "webdiskconflictdialog.h"
#include "sendfaxdialog.h"

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    connect(tbApp, SIGNAL(showSMSManager()), this, SLOT(smsManager()));

    this->wdSync = NULL;
    this->quitting = false;
    this->updateNotificationShowed = false;

    ui->setupUi(this);
    ui->actionWebdisk->setText(tbApp->nameWebdisk);
    ui->wdSyncGroup->setTitle(ui->wdSyncGroup->title().arg(tbApp->nameWebdisk));

    tbApp->addToolBarBranding(this, this->ui->mainToolBar);
    this->setWindowTitle(tr("%1: Preferences").arg(tbApp->applicationName()));

    this->createActions();
    this->createTrayIcon();
    this->switchPage(0);

    this->setWindowFlags(Qt::Dialog);

    // shield icon
    QIcon shieldIcon = QApplication::style()->standardIcon(QStyle::SP_VistaShield);
    this->ui->driverButton->setIcon(shieldIcon);

    // create API object
    this->api = new b1gMailAPI::ClientAPI(tbApp->serviceURL);
    connect(this->api, SIGNAL(newMails(int)),
            this, SLOT(newMails(int)));
    connect(this->api, SIGNAL(webSessionCreated(QString&, QString&, QString &)),
            this, SLOT(webSessionCreated(QString&, QString&, QString &)));
    connect(this->api, SIGNAL(balanceChanged(int)),
            this, SLOT(balanceChanged(int)));
    connect(this->api, SIGNAL(errorOccured(const QString&)),
            this, SLOT(apiErrorOccured(const QString&)));
    connect(this->api, SIGNAL(infoRetrieved(b1gMailAPI::Info&)),
            this, SLOT(infoRetrieved(b1gMailAPI::Info&)));

    // disable some features until services grants access
    this->ui->actionWebdisk->setVisible(false);
    this->ui->actionFax->setVisible(false);

    // sync anim
    this->syncAnim = new QMovie(":/icons/res/icons/sync-anim.gif");
    connect(this->syncAnim, SIGNAL(updated(QRect)), this, SLOT(updateSyncAnimation(QRect)));

    // mail check timer
    this->mailCheckTimer = new QTimer(this);
    connect(this->mailCheckTimer, SIGNAL(timeout()),
            this, SLOT(checkMails()));

    // wd sync timer
    this->wdSyncTimer = new QTimer(this);
    connect(this->wdSyncTimer, SIGNAL(timeout()),
            this, SLOT(syncWebdisk()));

    // info timer
    this->infoTimer = new QTimer(this);
    connect(this->infoTimer, SIGNAL(timeout()),
            this, SLOT(getInfo()));
    this->infoTimer->setInterval(5*60*1000);
    this->infoTimer->start();

    // printer info timer
    this->printerInfoTimer = new QTimer(this);
    connect(this->printerInfoTimer, SIGNAL(timeout()),
            this, SLOT(updatePrinterInfo()));
    this->printerInfoTimer->setInterval(1000);
    this->updatePrinterInfo();

    // prefs
    this->loadPrefs();

    // get first info data
    QTimer::singleShot(50, this, SLOT(getInfo()));

    this->setupFaxWatcher();

    // size
    this->adjustSize();
    this->setUnifiedTitleAndToolBarOnMac(tbApp->appStyle == "auto");

    QSize wndSize = this->size();
#ifdef Q_WS_WIN
    if(wndSize.width() < 500)
        wndSize.setWidth(500);
#endif
    this->setFixedSize(wndSize);
}

MainWindow::~MainWindow()
{
    if(this->wdSync != NULL)
        delete this->wdSync;

    delete ui;
    delete syncAnim;
}

void MainWindow::updatePrinterInfo()
{
    // printer installed?
    if(tbApp->isFaxPrinterInstalled())
    {
        if(this->ui->driverButton->text() != tr("Uninstall Fax printer"))
        {
            this->ui->driverButton->setText(tr("Uninstall Fax printer"));
            this->ui->driverLabel->setText(tr("The Fax printer is currently installed."));
            this->printerInfoTimer->stop();
        }
    }
    else
    {
        if(this->ui->driverButton->text() != tr("Install Fax printer"))
        {
            this->ui->driverButton->setText(tr("Install Fax printer"));
            this->ui->driverLabel->setText(tr("The Fax printer is currently not installed."));
            this->printerInfoTimer->stop();
        }
    }
}

void MainWindow::setupPrinter()
{
    tbApp->installFaxPrinter(tbApp->isFaxPrinterInstalled(), api);
    this->printerInfoTimer->start();
}

void MainWindow::infoRetrieved(b1gMailAPI::Info &info)
{
    currentInfo = info;
    updateMenus();

    if(!this->updateNotificationShowed)
    {
        QStringList latestVer = info.latestVersion.split("."),
                myVer = tbApp->applicationVersion().split(".");
        bool newVersionAvailable = false;

        if(myVer.length() == latestVer.length())
        {
            for(int i=0; i<myVer.length(); i++)
            {
                int latestVal = latestVer.at(i).toInt(), myVal = myVer.at(i).toInt();

                if(latestVal > myVal)
                {
                    newVersionAvailable = true;
                    break;
                }
            }
        }

        if(newVersionAvailable)
        {
            UpdateNotification *n = new UpdateNotification(this, api, info.latestVersion);
            n->show();
            this->updateNotificationShowed = true;
        }
    }
}

void MainWindow::updateMenus()
{
    this->webdiskMenu->menuAction()->setVisible(currentInfo.webdiskAccess
                                                && tbApp->userPrefs->value("webdisk/enableSync", false).toBool());
    this->smsManagerAction->setVisible(currentInfo.smsAccess);

    this->ui->actionWebdisk->setVisible(currentInfo.webdiskAccess);
    this->ui->actionFax->setVisible(currentInfo.faxAccess);
}

void MainWindow::setupFaxWatcher()
{
    faxWatchTimer = new QTimer(this);
    faxWatchTimer->setInterval(500);
    faxWatchTimer->setSingleShot(true);

    connect(faxWatchTimer, SIGNAL(timeout()),
            this, SLOT(checkForFaxes()));

    faxWatcher = new QFileSystemWatcher(this);
    faxWatcher->addPath(tbApp->faxPath());

    connect(faxWatcher, SIGNAL(directoryChanged(QString)),
            faxWatchTimer, SLOT(start()));

    faxWatchTimer->start();
}

void MainWindow::checkForFaxes()
{
    if(!currentInfo.faxAccess)
        return;

    QDir dir(tbApp->faxPath());

    if(!dir.exists())
        return;

    foreach(QFileInfo file, dir.entryInfoList())
    {
        if(file.fileName().length() != 12)
            continue;
        if(file.fileName().right(4) != ".fax")
            continue;
        if(openFaxes.contains(file.absoluteFilePath()))
            continue;

        SendFaxDialog *dlg = new SendFaxDialog(NULL, this->api, file.absoluteFilePath());
        dlg->show();

        openFaxes.append(file.absoluteFilePath());
    }
}

void MainWindow::getInfo()
{
    this->api->getInfo(true);
}

void MainWindow::checkMails()
{
    this->api->checkForMails();
}

void MainWindow::webdiskSyncConflict(const QString &localFile, const QDateTime &localMTime,
                                      const QString &remoteFile, const QDateTime &remoteMTime)
{
    if(wdSync == NULL)
        return;

    QString localText = tr("%1 (modified: %2)")
            .arg(localFile)
            .arg(localMTime.toString());
    QString remoteText = tr("%1 (modified: %2)")
            .arg(remoteFile)
            .arg(remoteMTime.toString());

    int master = WD_MASTER_UNKNOWN;
    bool remember = false;

    WebdiskConflictDialog *dlg = new WebdiskConflictDialog(NULL, localText, remoteText);
    if(dlg->exec())
    {
        master = dlg->master;
        remember = dlg->remember;
    }
    delete dlg;

    wdSync->resolveConflict(master, remember);
}

void MainWindow::syncWebdisk()
{
    if(!currentInfo.webdiskAccess)
        return;

    QString syncPath = tbApp->userPrefs->value("webdisk/targetFolder", "").toString();
    if(syncPath.length() < 3)
        return;

    this->startSyncAnimation();
    this->syncWebdiskAction->setText(tr("Syncing..."));
    this->syncWebdiskAction->setEnabled(false);

    if(wdSync != NULL && !wdSync->isUpToDate(syncPath, this->api->userEmail, this->api->userPassword))
    {
        delete wdSync;
        wdSync = NULL;
    }

    if(wdSync == NULL)
    {
        wdSync = new WebdiskSync(this->api, syncPath);
        connect(wdSync, SIGNAL(syncError(QString)),
                this, SLOT(apiErrorOccured(const QString&)));
        connect(wdSync, SIGNAL(syncDone(bool, int, int)),
                this, SLOT(webdiskSyncDone(bool, int, int)));
        connect(wdSync, SIGNAL(syncSpeedReport(uint,uint)),
                this, SLOT(webdiskSyncSpeedReport(uint,uint)));
        connect(wdSync, SIGNAL(syncConflict(QString,QDateTime,QString,QDateTime)),
                this, SLOT(webdiskSyncConflict(QString,QDateTime,QString,QDateTime)));
    }

    wdSync->syncHiddenItems = tbApp->userPrefs->value("webdisk/syncHiddenItems", false).toBool();
    wdSync->sync();
}

void MainWindow::webdiskSyncSpeedReport(unsigned int downBytesPerSecond, unsigned int upBytesPerSecond)
{
    this->syncWebdiskAction->setText(tr("Syncing... (%1 KB/s)")
                                         .arg(QString::number((float)(upBytesPerSecond + downBytesPerSecond)/1024.0f, 'f', 1)));
}


void MainWindow::webdiskSyncDone(bool success, int numCreatedFiles, int numCreatedFolders)
{
    this->stopSyncAnimation();
    this->syncWebdiskAction->setText(tr("Sync now"));
    this->syncWebdiskAction->setEnabled(true);

    if(numCreatedFiles > 0 || numCreatedFolders > 0)
    {
        if(tbApp->userPrefs->value("webdisk/showNotifications", true).toBool())
        {
            QString msg = "";

            if(numCreatedFolders > 0)
                msg += tr(", %1 new folder(s)", 0, numCreatedFolders).arg(numCreatedFolders);

            if(numCreatedFiles > 0)
                msg += tr(", %1 new file(s)", 0, numCreatedFiles).arg(numCreatedFiles);

            this->trayIcon->showMessage(tbApp->nameWebdisk,
                                        msg.mid(2),
                                        QSystemTrayIcon::Information,
                                        2500);
        }
    }
}

void MainWindow::balanceChanged(int balance)
{
    if(tbApp->userPrefs->value("common/showBalanceNotification", true).toBool())
    {
        this->trayIcon->showMessage(tr("Account balance"), tr("New account balance: %1 credit(s).", "", balance).arg(balance),
                                    QSystemTrayIcon::Information,
                                    1500);
    }
}

void MainWindow::newMails(int count)
{
    if(tbApp->userPrefs->value("mail/showNotification", true).toBool())
    {
        this->trayIcon->showMessage(tr("New email(s)", "", count),
                                    tr("You have got %1 new email(s).", "", count).arg(count),
                                    QSystemTrayIcon::Information);
    }

    if(tbApp->userPrefs->value("mail/playSound", true).toBool())
    {
        QSound::play(tbApp->resPath("newmail.wav"));
    }

    if(tbApp->userPrefs->value("mail/showInbox", false).toBool())
    {
        this->readMail();
    }
}

void MainWindow::apiErrorOccured(const QString &msg)
{
    this->trayIcon->showMessage(tr("An error occured"),
                                msg,
                                QSystemTrayIcon::Warning,
                                2500);
}

void MainWindow::updateMailCheckTimer()
{
    QSettings *p = tbApp->userPrefs;

    this->mailCheckTimer->setInterval(qMax(p->value("mail/checkInterval", 5).toInt(), 1) * 60 * 1000);

    if(p->value("mail/enableChecking", true).toBool())
        this->mailCheckTimer->start();
    else
        this->mailCheckTimer->stop();
}

void MainWindow::updateWDSyncTimer()
{
    QSettings *p = tbApp->userPrefs;

    this->wdSyncTimer->setInterval(qMax(p->value("webdisk/syncInterval", 5).toInt(), 1) * 60 * 1000);

    if(p->value("webdisk/enableSync", false).toBool())
        this->wdSyncTimer->start();
    else
        this->wdSyncTimer->stop();
}

void MainWindow::loadPrefs()
{
    QSettings *p = tbApp->userPrefs;

    api->userEmail = p->value("account/emailAddress").toString();
    api->userPassword = p->value("account/password").toString();

    this->ui->commonAutoStart->setChecked(tbApp->isInAutostart());
    this->ui->commonShowBalanceNotification->setChecked(tbApp->userPrefs->value("common/showBalanceNotification", true).toBool());

    this->ui->accountEmailAddress->setText(api->userEmail);
    this->ui->accountPassword->setText(api->userPassword);

    this->ui->mailCheckEnable->setChecked(p->value("mail/enableChecking", true).toBool());
    this->ui->mailCheckInterval->setValue(p->value("mail/checkInterval", 5).toInt());
    this->ui->mailNotifyShowNotification->setChecked(p->value("mail/showNotification", true).toBool());
    this->ui->mailNotfiyPlaySound->setChecked(p->value("mail/playSound", true).toBool());
    this->ui->mailNotifyShowInbox->setChecked(p->value("mail/showInbox", false).toBool());

    this->ui->webdiskEnableSync->setChecked(p->value("webdisk/enableSync", false).toBool());
    this->ui->webdiskSyncInterval->setValue(p->value("webdisk/syncInterval", 30).toInt());
    this->ui->webdiskTargetFolder->setText(p->value("webdisk/targetFolder").toString());
    this->ui->webdiskSyncHiddenItems->setChecked(p->value("webdisk/syncHiddenItems", false).toBool());
    this->ui->webdiskShowNotifications->setChecked(p->value("webdisk/showNotifications", true).toBool());

    this->updateMailCheckTimer();
    this->updateWDSyncTimer();
}

void MainWindow::savePrefs()
{
    QSettings *p = tbApp->userPrefs;

    this->api->userEmail = this->ui->accountEmailAddress->text();
    this->api->userPassword = this->ui->accountPassword->text();

    p->setValue("common/showBalanceNotification",
                                            this->ui->commonShowBalanceNotification->checkState() == Qt::Checked);

    p->setValue("account/emailAddress",     this->api->userEmail);
    p->setValue("account/password",         this->api->userPassword);

    p->setValue("mail/enableChecking",      this->ui->mailCheckEnable->checkState() == Qt::Checked);
    p->setValue("mail/checkInterval",       this->ui->mailCheckInterval->value());
    p->setValue("mail/showNotification",    this->ui->mailNotifyShowNotification->checkState() == Qt::Checked);
    p->setValue("mail/playSound",           this->ui->mailNotfiyPlaySound->checkState() == Qt::Checked);
    p->setValue("mail/showInbox",           this->ui->mailNotifyShowInbox->checkState() == Qt::Checked);

    p->setValue("webdisk/enableSync",       this->ui->webdiskEnableSync->checkState() == Qt::Checked);
    p->setValue("webdisk/syncInterval",     this->ui->webdiskSyncInterval->value());
    p->setValue("webdisk/syncHiddenItems",  this->ui->webdiskSyncHiddenItems->checkState() == Qt::Checked);
    p->setValue("webdisk/showNotifications",this->ui->webdiskShowNotifications->checkState() == Qt::Checked);

    if(this->ui->webdiskTargetFolder->text() != p->value("webdisk/targetFolder").toString())
    {
        if(!this->syncWebdiskAction->isEnabled())
        {
            QMessageBox::critical(this,
                                  tr("Error"),
                                  tr("Failed to change %1 target: Sync in progress.").arg(tbApp->nameWebdisk));
        }
        else
        {
            p->setValue("webdisk/targetFolder", this->ui->webdiskTargetFolder->text());

            if(wdSync != NULL)
            {
                delete wdSync;
                wdSync = NULL;
            }

            WebdiskSync::resetDB();
        }
    }

    tbApp->setAutostart(this->ui->commonAutoStart->checkState() == Qt::Checked);

    this->updateMenus();

    this->updateMailCheckTimer();
    this->updateWDSyncTimer();

    this->hide();
}

void MainWindow::changeWebdiskTarget()
{
    QString newDir = QFileDialog::getExistingDirectory(this,
                                      tr("Choose %1 target").arg(tbApp->nameWebdisk),
                                      this->ui->webdiskTargetFolder->text(),
                                      QFileDialog::ShowDirsOnly);

    if(newDir.length() == 0
            || newDir == this->ui->webdiskTargetFolder->text())
        return;

    bool isEmpty = true;

    QDir dir(newDir);
    QFileInfoList fl = dir.entryInfoList();
    foreach(QFileInfo info, fl)
    {
        if(info.fileName() == "." || info.fileName() == ".."
                || info.isHidden())
            continue;

        isEmpty = false;
        break;
    }

    if(!isEmpty)
    {
        QMessageBox::warning(this,
                              tr("Error"),
                              tr("The target folder must be empty."));
        return;
    }

    if(QMessageBox::question(this,
                             tr("Change %1 target").arg(tbApp->nameWebdisk),
                             tr("Do you really want to change the %1 target?\n\nAfterwards, the %2 will have to be re-synchronized.")
                                .arg(tbApp->nameWebdisk)
                                .arg(tbApp->nameWebdisk),
                             QMessageBox::Yes,
                             QMessageBox::No) == QMessageBox::No)
        return;

    this->ui->webdiskTargetFolder->setText(newDir);
}

void MainWindow::switchPage(int no)
{
    this->ui->actionCommon->setChecked(no == 0);
    this->ui->actionAccount->setChecked(no == 1);
    this->ui->actionEmail->setChecked(no == 2);
    this->ui->actionWebdisk->setChecked(no == 3);
    this->ui->actionFax->setChecked(no == 4);

    this->ui->stackedWidget->setCurrentIndex(no);
}

void MainWindow::showCommonPage()
{
    switchPage(0);
}

void MainWindow::showAccountPage()
{
    switchPage(1);
}

void MainWindow::showEmailPage()
{
    switchPage(2);
}

void MainWindow::showWebdiskPage()
{
    switchPage(3);
}

void MainWindow::showFaxPage()
{
    switchPage(4);
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    if(this->quitting)
        return;

    this->hide();
    event->ignore();
}

void MainWindow::quitApp()
{
    int res = QMessageBox::question(this, tr("Quit"), tr("Do you really want to quit %1?\n\nYou will not receive notifications from %2 anymore.")
                                 .arg(tbApp->applicationName())
                                 .arg(tbApp->serviceName),
                                 QMessageBox::Yes | QMessageBox::No,
                                 QMessageBox::NoButton);

    if(res == QMessageBox::Yes)
    {
        this->quitting = true;
        this->trayIcon->hide();
        this->close();
        tbApp->quit();
    }
}

void MainWindow::webSessionCreated(QString &sessionID, QString &sessionSecret, QString &target)
{
    QUrl url(tbApp->serviceURL + QString("index.php"));
    url.addQueryItem("action", "initiateSession");
    url.addQueryItem("sid", sessionID);
    url.addQueryItem("secret", sessionSecret);
    url.addQueryItem("target", target);
    QDesktopServices::openUrl(url);
}

void MainWindow::readMail()
{
    this->api->createWebSession("inbox");
}

void MainWindow::composeMail()
{
    this->api->createWebSession("compose");
}

void MainWindow::smsManager()
{
    if(!currentInfo.smsAccess)
        return;
    SMSManager *mgr = new SMSManager(0, this->api);
    mgr->show();
}

void MainWindow::openWebdisk()
{
    QString webdiskFolder = tbApp->userPrefs->value("webdisk/targetFolder", "").toString();

    if(!QDir().exists(webdiskFolder))
        QDir().mkdir(webdiskFolder);

    QUrl folderURL = QUrl::fromLocalFile(webdiskFolder);
    QDesktopServices::openUrl(folderURL);
}

void MainWindow::openOnlineWebdisk()
{
    this->api->createWebSession("webdisk");
}

void MainWindow::createActions()
{
    this->readMailAction = new QAction(tr("Read email"), this);
    this->readMailAction->setIcon(QIcon(QPixmap(":/icons/res/icons/email-ico.png").scaled(16, 16, Qt::KeepAspectRatio, Qt::SmoothTransformation)));
    connect(this->readMailAction, SIGNAL(triggered()),
            this, SLOT(readMail()));

    this->composeMailAction = new QAction(tr("Compose email"), this);
    this->composeMailAction->setIcon(QIcon(QPixmap(":/icons/res/icons/compose-ico.png").scaled(16, 16, Qt::KeepAspectRatio, Qt::SmoothTransformation)));
    connect(this->composeMailAction, SIGNAL(triggered()),
            this, SLOT(composeMail()));

    this->smsManagerAction = new QAction(tbApp->nameSMSManager, this);
    this->smsManagerAction->setIcon(QIcon(QPixmap(":/icons/res/icons/sms-ico.png").scaled(16, 16, Qt::KeepAspectRatio, Qt::SmoothTransformation)));
    connect(this->smsManagerAction, SIGNAL(triggered()),
            this, SLOT(smsManager()));

    this->openWebdiskAction = new QAction(tr("Open local %1 folder").arg(tbApp->nameWebdisk), this);
    this->openWebdiskAction->setIcon(QIcon(QPixmap(":/icons/res/icons/folder-ico.png").scaled(16, 16, Qt::KeepAspectRatio, Qt::SmoothTransformation)));
    connect(this->openWebdiskAction, SIGNAL(triggered()),
            this, SLOT(openWebdisk()));

    this->openOnlineWebdiskAction = new QAction(tr("Open online %1").arg(tbApp->nameWebdisk), this);
    this->openOnlineWebdiskAction->setIcon(QIcon(QPixmap(":/icons/res/icons/online-webdisk-ico.png").scaled(16, 16, Qt::KeepAspectRatio, Qt::SmoothTransformation)));
    connect(this->openOnlineWebdiskAction, SIGNAL(triggered()),
            this, SLOT(openOnlineWebdisk()));

    this->syncWebdiskAction = new QAction(tr("Sync now"), this);
    this->syncWebdiskAction->setIcon(QIcon(QPixmap(":/icons/res/icons/sync-ico.png").scaled(16, 16, Qt::KeepAspectRatio, Qt::SmoothTransformation)));
    connect(this->syncWebdiskAction, SIGNAL(triggered()),
            this, SLOT(syncWebdisk()));

    this->prefsAction = new QAction(tr("Preferences..."), this);
    this->prefsAction->setIcon(QIcon(QPixmap(":/icons/res/icons/prefs-ico.png").scaled(16, 16, Qt::KeepAspectRatio, Qt::SmoothTransformation)));
    connect(this->prefsAction, SIGNAL(triggered()),
            this, SLOT(show()));

    this->quitAction = new QAction(tr("Quit"), this);
    this->quitAction->setIcon(QIcon(QPixmap(":/icons/res/icons/exit-ico.png").scaled(16, 16, Qt::KeepAspectRatio, Qt::SmoothTransformation)));
    connect(this->quitAction, SIGNAL(triggered()),
            this, SLOT(quitApp()));
}

void MainWindow::show()
{
    QMainWindow::show();
    raise();
}

void MainWindow::trayMenuActivated(QSystemTrayIcon::ActivationReason reason)
{
    if(reason == QSystemTrayIcon::Trigger)
    {
#ifdef Q_WS_WIN
        this->trayMenu->popup(QCursor::pos());
        this->trayMenu->activateWindow();
#endif
    }

    else if(reason == QSystemTrayIcon::DoubleClick)
    {
        this->readMail();
    }
}

void MainWindow::createTrayIcon()
{
    this->trayMenu = new QMenu(this);

    this->trayMenu->addAction(readMailAction);
    this->trayMenu->addAction(composeMailAction);
    this->trayMenu->addSeparator();

    this->trayMenu->addAction(smsManagerAction);
    smsManagerAction->setVisible(false);

    webdiskMenu = this->trayMenu->addMenu(QIcon(QPixmap(":/icons/res/icons/webdisk-ico.png").scaled(16, 16, Qt::KeepAspectRatio, Qt::SmoothTransformation)),
                                          tbApp->nameWebdisk);
    webdiskMenu->addAction(openWebdiskAction);
    webdiskMenu->addAction(openOnlineWebdiskAction);
    webdiskMenu->addSeparator();
    webdiskMenu->addAction(syncWebdiskAction);
    webdiskMenu->menuAction()->setVisible(false);

    this->trayMenu->addSeparator();

    this->trayMenu->addAction(prefsAction);
    this->trayMenu->addSeparator();
    this->trayMenu->addAction(quitAction);

    this->trayMenu->setDefaultAction(readMailAction);

    this->trayIcon = new QSystemTrayIcon(this);
    this->trayIcon->setToolTip(tbApp->applicationName());
    this->trayIcon->setContextMenu(this->trayMenu);
    this->trayIcon->setIcon(QIcon(tbApp->resPath("app-ico.png")));
    this->trayIcon->show();

    connect(this->trayIcon, SIGNAL(activated(QSystemTrayIcon::ActivationReason)),
            this, SLOT(trayMenuActivated(QSystemTrayIcon::ActivationReason)));
    connect(this->trayIcon, SIGNAL(messageClicked()),
            this, SLOT(readMail()));
}

void MainWindow::updateSyncAnimation(const QRect &)
{
    // get original ico
    QImage ico(tbApp->resPath("app-ico.png"));

    // determine load img width
    int w = (int)(0.625f * (float)ico.width());

    // get frame image
    QImage frameImg = this->syncAnim->currentImage().scaled(w, w, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);

    // create new icon
    QPainter p(&ico);
    p.drawImage(ico.width()-w, ico.height()-w, frameImg);

    // set icon
    this->trayIcon->setIcon(QIcon(QPixmap::fromImage(ico)));
}

void MainWindow::startSyncAnimation()
{
    this->syncAnim->setSpeed(75);
    this->syncAnim->jumpToFrame(0);
    this->syncAnim->start();
}

void MainWindow::stopSyncAnimation()
{
    this->syncAnim->stop();
    this->trayIcon->setIcon(QIcon(tbApp->resPath("app-ico.png")));
}

void MainWindow::showAboutBox()
{
    AboutDialog dlg(this);
    dlg.exec();
}
