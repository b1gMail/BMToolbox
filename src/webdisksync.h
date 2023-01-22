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

#ifndef WEBDISKSYNC_H
#define WEBDISKSYNC_H

#include <QObject>
#include <QThread>
#include <QSqlDatabase>
#include <QUrl>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QWaitCondition>
#include <QMutex>
#include <QDomDocument>
#include <QFile>
#include <QIODevice>
#include "clientapi.h"
#include "bmtoolboxapp.h"

#define WD_TYPE_FOLDER          1
#define WD_TYPE_FILE            2

#define WD_MASTER_UNKNOWN       0
#define WD_MASTER_LOCAL         1
#define WD_MASTER_REMOTE        2

class WebdiskEntry
{
public:
    int id;
    int type;
    QString title;
    int size;
    unsigned int created;
    unsigned int accessed;
    unsigned int modified;
};

class WebdiskSync : public QThread
{
    Q_OBJECT
public:
    explicit WebdiskSync(b1gMailAPI::ClientAPI *api,
                         const QString &syncPath);
    virtual ~WebdiskSync();

public:
    void sync();
    bool isUpToDate(const QString &syncPath, const QString &userEmail, const QString &userPassword);
    void resolveConflict(int master, bool remember);
    static void resetDB();

private:
    // sync logic
    bool syncFolder(unsigned int remoteFolderID, const QString &localPath);
    bool didFileExist(const QString &name);
    bool didFolderExist(const QString &name);
    bool deleteWebdiskFile(int id);
    bool deleteWebdiskFolder(int id);
    bool transferFile(const WebdiskEntry &entry, const QString &destPath);
    bool transferFile(const QString &localPath, int folderID, const QString &fileName);
    unsigned int latestFileMTime(int id, const QString &path);
    unsigned int latestFileMTime(const QString &path);
    bool deleteFolder(const QString &path);
    bool deleteFile(const QString &path);
    bool createFolder(const QString &path, int folderID);
    unsigned int createWebdiskFolder(const QString &localPath, int parentFolderID, const QString &folderName);

    // helper functions
    bool openDB();
    void log(const QString &entry);
    QString shortPath(const QString &path);
    QString sanitizeFilename(const QString &name);
    QNetworkReply *getURLSynchronous(const QUrl &url);
    QNetworkReply *postURLSynchronous(const QUrl &url, QIODevice *data);
    QDomDocument getURL(const QUrl &url);
    WebdiskEntry entryFromNode(QDomNode &node);
    void reportSpeed(unsigned int addBytes = 0, unsigned int addDownBytes = 0);

protected:
    void run();

signals:
    void syncError(const QString &error);
    void syncDone(bool success, int numCreatedFiles, int numCreatedFolders);
    void syncSpeedReport(unsigned int downBytesPerSecond, unsigned int upBytesPerSecond);
    void syncConflict(const QString &localFile, const QDateTime &localMTime,
                      const QString &remoteFile, const QDateTime &remoteMTime);

public slots:
    void downloadProgress(qint64,qint64);
    void uploadProgress(qint64,qint64);

public:
    bool syncHiddenItems;

private:
    bool restart, abort, abortCurrentSync;
    b1gMailAPI::ClientAPI *api;
    QMutex mutex, conflictMutex;
    QString dbPath, syncPath;
    QSqlDatabase db;
    QNetworkAccessManager *netManager;
    QWaitCondition condition, conflictCondition;
    QFile *logFile;
    unsigned int msecsReceived, msecsSent, numCreatedFiles, numCreatedFolders;
    qint64 bytesReceived, bytesSent;
    QDateTime transferStart;
    QDateTime lastSpeedReport;
    int master;
    bool rememberMaster;
};

#endif // WEBDISKSYNC_H
