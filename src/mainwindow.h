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

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QSystemTrayIcon>
#include <QAction>
#include <QCloseEvent>
#include <QTimer>
#include <QFileSystemWatcher>
#include <QStringList>
#include <QMovie>
#include "clientapi.h"
#include "webdisksync.h"

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();

private:
    void createTrayIcon();
    void createActions();
    void switchPage(int no);
    void loadPrefs();
    void updateMailCheckTimer();
    void updateWDSyncTimer();
    void setupFaxWatcher();
    void updateMenus();
    void startSyncAnimation();
    void stopSyncAnimation();

protected:
    void closeEvent(QCloseEvent *event);

public slots:
    void trayMenuActivated(QSystemTrayIcon::ActivationReason reason);
    void quitApp();
    void showCommonPage();
    void showAccountPage();
    void showEmailPage();
    void showWebdiskPage();
    void showFaxPage();
    void showAboutBox();
    void savePrefs();
    void checkMails();
    void readMail();
    void composeMail();
    void smsManager();
    void newMails(int);
    void webSessionCreated(QString&, QString&, QString &);
    void apiErrorOccured(const QString&);
    void getInfo();
    void balanceChanged(int balance);
    void syncWebdisk();
    void webdiskSyncDone(bool success, int numCreatedFiles, int numCreatedFolders);
    void webdiskSyncSpeedReport(unsigned int downBytesPerSecond, unsigned int upBytesPerSecond);
    void webdiskSyncConflict(const QString &, const QDateTime &, const QString &, const QDateTime &);
    void openWebdisk();
    void openOnlineWebdisk();
    void changeWebdiskTarget();
    void checkForFaxes();
    void infoRetrieved(b1gMailAPI::Info &info);
    void setupPrinter();
    void updatePrinterInfo();
    void updateSyncAnimation(const QRect &);
    virtual void show();

public:
    QTimer *printerInfoTimer;
    QSystemTrayIcon *trayIcon;

private:
    Ui::MainWindow *ui;
    QMenu *trayMenu, *webdiskMenu;
    QAction *quitAction, *prefsAction, *readMailAction, *composeMailAction, *smsManagerAction, *syncWebdiskAction,
            *openWebdiskAction, *openOnlineWebdiskAction;
    QTimer *mailCheckTimer, *infoTimer, *wdSyncTimer, *faxWatchTimer;
    b1gMailAPI::ClientAPI *api;
    b1gMailAPI::Info currentInfo;
    WebdiskSync *wdSync;
    QFileSystemWatcher *faxWatcher;
    QStringList openFaxes;
    QMovie *syncAnim;
    bool quitting, updateNotificationShowed;
    int syncAnimFrameNo;
};

#endif // MAINWINDOW_H
