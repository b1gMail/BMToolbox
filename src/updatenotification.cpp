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

#include "updatenotification.h"
#include "ui_updatenotification.h"
#include "bmtoolboxapp.h"

#include <QMessageBox>
#include <QCryptographicHash>
#include <QFile>
#include <QDir>
#include <QDesktopServices>
#include <QProcess>

#include <openssl/ssl.h>

#ifdef Q_WS_MAC
#include "macauth.h"
#include "macloginitemsmanager.h"
#endif

#ifdef Q_WS_WIN
#include <windows.h>
#endif

unsigned char updatePublicKey[] =
        "-----BEGIN PUBLIC KEY-----\n"
        "MIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEAq5doHkaFhCcrHoQyOZe0\n"
        "sRSNf6/glGX9Vjiw92+CMUPRA0hroufh9oBIzI1S1/ziacgzVL8gcrd79WTmodjh\n"
        "j/mqle56+lrQ0WTnjg8Rh67eds0m7ycFqM19LgPwH/9J09yU1UhED/Ah49FsoVxS\n"
        "NNlwSxHe4G2aZ0RDX9S5cZMFntR22dp+u6EqojEnDWU5WKgduuOtHYQanJylqru+\n"
        "h4FOycIc+kwJ0fZ3ddEvwzCLHq9uG4bZYLupw4LbWWSV4/a3M7TIuTDJS593+JzO\n"
        "EY4c67STPgCUZBra1IXupuAR/3rPUlWkLLwkgbAeOnnD9irZ4enC2PfjhvlB3UEo\n"
        "/QIDAQAB\n"
        "-----END PUBLIC KEY-----\n";

UpdateNotification::UpdateNotification(QWidget *parent, b1gMailAPI::ClientAPI *api, const QString &newVersion) :
    QDialog(parent),
    ui(new Ui::UpdateNotification)
{
    this->api = api;
    this->downloading = false;
    this->netManager = new QNetworkAccessManager;;

    this->setAttribute(Qt::WA_DeleteOnClose, true);
    this->setAttribute(Qt::WA_QuitOnClose, true);

    ui->setupUi(this);
    ui->progressBar->setValue(0);

    ui->introLabel->setText(ui->introLabel->text().arg(tbApp->applicationName()));
    ui->latestVersion->setText(newVersion);
    ui->myVersion->setText(tbApp->applicationVersion());

    this->setWindowTitle(tr("%1: Update available").arg(tbApp->applicationName()));
    this->adjustSize();
    this->setFixedSize(this->size());

    ui->stackedWidget->setCurrentIndex(0);
}

UpdateNotification::~UpdateNotification()
{
    delete ui;
    delete netManager;
}

void UpdateNotification::show()
{
    QDialog::show();
    raise();
}

void UpdateNotification::updateNow()
{
    ui->stackedWidget->setCurrentIndex(1);
    this->startDownload();
}

void UpdateNotification::downloadProgress(qint64 bytesReceived, qint64 bytesTotal)
{
    ui->progressBar->setMaximum((int)bytesTotal);
    ui->progressBar->setValue((int)bytesReceived);
}

void UpdateNotification::downloadFailed()
{
    QMessageBox::critical(this, tr("Error"),
                          tr("Failed to download the update package.\n\nPlease try again later."));
    reject();
}

void UpdateNotification::downloadFinished()
{
    QNetworkReply *reply = dynamic_cast<QNetworkReply *>(sender());

    if(reply->error() != QNetworkReply::NoError)
        return;

    reply->deleteLater();

    QByteArray updateData = reply->readAll();
    bool sigOK = false;

    if(reply->hasRawHeader("X-b1gMail-File-Signature"))
    {
        QByteArray sentRawSig = QByteArray::fromBase64(reply->rawHeader("X-b1gMail-File-Signature"));

        BIO *mem = BIO_new_mem_buf(updatePublicKey, sizeof(updatePublicKey)-1);
        RSA *RSAPubKey = PEM_read_bio_RSA_PUBKEY(mem, NULL, NULL, NULL);

        if(RSAPubKey != NULL)
        {
            QCryptographicHash hash(QCryptographicHash::Sha1);
            hash.addData(updateData);
            QByteArray hashData = hash.result();

            int res = RSA_verify(NID_sha1,
                                 (const unsigned char *)hashData.constData(),
                                 hashData.size(),
                                 (unsigned char *)sentRawSig.constData(),
                                 sentRawSig.size(),
                                 RSAPubKey);

            sigOK = (res == 1);

            RSA_free(RSAPubKey);
        }

        BIO_free_all(mem);
    }

    if(!sigOK)
    {
        QMessageBox::critical(this, tr("Error"),
                              tr("The update package is corrupt.\n\nPlease try again."));
        close();
        return;
    }

    this->installUpdate(updateData);

    close();
}

#ifdef Q_WS_WIN
void UpdateNotification::installUpdate(const QByteArray &data)
{
    QString installerName = tbApp->applicationDirPath() + "/updater.exe";

    // copy updater to temporary location
    QString updaterDestName = QDesktopServices::storageLocation(QDesktopServices::DataLocation) + "/bmToolboxUpdater";
    if(QFile().exists(installerName))
        QFile().remove(installerName);
    QFile updateFile(installerName);
    if(!updateFile.open(QIODevice::WriteOnly))
    {
        QMessageBox::critical(this,
                              tr("Error"),
                              tr("Failed to save update package."));
        return;
    }
    updateFile.write(data);
    updateFile.close();

    // launch
    QByteArray installerNameStr = installerName.toUtf8();
    ShellExecuteA(NULL, "open", installerNameStr.constData(), "/S", NULL, SW_SHOWNORMAL);
    tbApp->quit();
}
#endif

#ifdef Q_WS_MAC
void UpdateNotification::installUpdate(const QByteArray &data)
{
    QString dir = QDesktopServices::storageLocation(QDesktopServices::DataLocation) + "/update_temp/",
            deleteDir = dir +  "/update_temp/old_app/",
            zipFileName = dir + "BMToolbox.zip";
    QDir().mkpath(dir);
    QDir().mkpath(deleteDir);

    // copy updater to temporary location
    QString updaterDestName = QDesktopServices::storageLocation(QDesktopServices::DataLocation) + "/bmToolboxUpdater";
    if(QFile().exists(updaterDestName))
        QFile().remove(updaterDestName);
    if(!QFile().copy(tbApp->resPath("updater"), updaterDestName))
    {
        QMessageBox::critical(this,
                              tr("Error"),
                              tr("Failed to copy update helper application."));
        return;
    }

    // save file
    QFile updateFile(zipFileName);
    if(!updateFile.open(QIODevice::WriteOnly))
    {
        QMessageBox::critical(this,
                              tr("Error"),
                              tr("Failed to save update package."));
        return;
    }
    updateFile.write(data);
    updateFile.close();

    // unzip
    QStringList args;
    args.append("-qq");
    args.append("-o");
    args.append("BMToolbox.zip");
    QProcess zipProc(this);
    zipProc.setWorkingDirectory(dir);
    zipProc.start("/usr/bin/unzip", args);
    if(!zipProc.waitForStarted())
    {
        QMessageBox::critical(this,
                              tr("Error"),
                              tr("Failed to start update package extraction."));
        return;
    }
    while(!zipProc.waitForFinished(100))
        tbApp->processEvents();

    // success?
    if(zipProc.exitCode() != 0)
    {
        QMessageBox::critical(this,
                              tr("Error"),
                              tr("Failed to extract update package."));
        return;
    }

    // find app folder
    QString newAppDir;
    QDir tempDir(dir);
    QFileInfoList dirEntries = tempDir.entryInfoList();
    foreach(QFileInfo dirEntry, dirEntries)
    {
        if(dirEntry.fileName() == "." || dirEntry.fileName() == ".."
                || dirEntry.fileName().length() == 0)
            continue;

        if(!dirEntry.isDir() || dirEntry.fileName().length() < 4)
            continue;

        if(dirEntry.fileName().right(4) != ".app")
            continue;

        newAppDir = dirEntry.filePath();
        break;
    }

    // found?
    if(newAppDir.isEmpty())
    {
        QMessageBox::critical(this,
                              tr("Error"),
                              tr("Unexpected update package contents."));
        return;
    }

    // get app dir
    QString appDir = tbApp->applicationDirPath();
    int postfixPos = appDir.indexOf("/Contents/MacOS");
    if(postfixPos == -1)
    {
        QMessageBox::critical(this,
                              tr("Error"),
                              tr("Failed to determine package destination."));
        return;
    }
    appDir = appDir.left(postfixPos);

    // elevate
    MacAuth ma;
    if(!ma.elevate())
    {
        QMessageBox::critical(this,
                              tr("Error"),
                              tr("Permission denied."));
        return;
    }

    // move away old app
    args.clear();
    args << "-f";
    args << appDir;
    args << deleteDir;
    if(!ma.execAndWait("/bin/mv", args))
    {
        QMessageBox::critical(this,
                              tr("Error"),
                              tr("Failed to remove old application."));
        return;
    }

    // copy new app
    args.clear();
    args << "-f";
    args << newAppDir;
    args << appDir;
    if(!ma.execAndWait("/bin/mv", args))
    {
        QMessageBox::critical(this,
                              tr("Error"),
                              tr("Failed to move new application."));
        return;
    }

    // TODO:
    //          set permissions

    // remove old stuff
    args.clear();
    args << "-rf";
    args << dir;
    ma.execAndWait("/bin/rm", args);

    // delete file
    updateFile.remove();
    QDir().remove(dir);

    // launch updater and quit
    MacHelper::launchUpdater(updaterDestName.toStdString().c_str());
    tbApp->quit();
}
#endif

void UpdateNotification::startDownload()
{
    this->downloading = true;

    QUrl url = this->api->serviceURL;
    url.addQueryItem("method", "DownloadCurrentVersion");
    url.addQueryItem("params[0]", api->userEmail);
    url.addQueryItem("params[1]", api->md5(api->userPassword));

#ifdef Q_WS_WIN
    url.addQueryItem("params[2]", "win");
#endif

#ifdef Q_WS_MAC
    url.addQueryItem("params[2]", "mac");
#endif

    QNetworkRequest req = api->createRequest(url);
    QNetworkReply *reply = this->netManager->get(req);

    connect(reply, SIGNAL(finished()), this, SLOT(downloadFinished()));
    connect(reply, SIGNAL(error(QNetworkReply::NetworkError)), this, SLOT(downloadFailed()));
    connect(reply, SIGNAL(sslErrors(QList<QSslError>)), reply, SLOT(ignoreSslErrors()));
    connect(reply, SIGNAL(downloadProgress(qint64,qint64)), this, SLOT(downloadProgress(qint64,qint64)));
}
