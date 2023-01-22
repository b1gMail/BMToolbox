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

#include "webdisksync.h"
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QMutexLocker>
#include <QEventLoop>
#include <QMessageBox>
#include <QTextStream>
#include <QDesktopServices>

#define MAX_LOG_FILE_SIZE       (1*1024*1024)   // 1 MB

WebdiskSync::WebdiskSync(b1gMailAPI::ClientAPI *api, const QString &syncPath) :
    QThread(NULL)
{
    this->restart           = false;
    this->abort             = false;

#ifdef Q_WS_MAC
    this->dbPath            = QDesktopServices::storageLocation(QDesktopServices::DataLocation);
    QDir().mkpath(this->dbPath);
    this->dbPath            += "/wdsync.db";
#else
    this->dbPath            = tbApp->applicationDirPath() + "/wdsync.db";
#endif
    this->syncPath          = syncPath;

    // default prefs
    this->syncHiddenItems   = true;
    this->master            = WD_MASTER_UNKNOWN;
    this->rememberMaster    = false;

    // log file
#ifdef Q_WS_MAC
    this->logFile           = new QFile(QDesktopServices::storageLocation(QDesktopServices::DataLocation)
            + "/wdsync.log");
#else
    this->logFile           = new QFile(tbApp->applicationDirPath() + "/wdsync.log");
#endif
    if(this->logFile->exists() && this->logFile->size() > MAX_LOG_FILE_SIZE)
        this->logFile->remove();
    if(!this->logFile->open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Append))
    {
        delete this->logFile;
        this->logFile = NULL;
    }
    log(QString("=").repeated(72));

    // create new ClientAPI object because it is not thread safe
    this->api               = new b1gMailAPI::ClientAPI(api->serviceURL.toString());
    this->api->userEmail    = api->userEmail;
    this->api->userPassword = api->userPassword;

    log("WebdiskSync created");
}

WebdiskSync::~WebdiskSync()
{
    mutex.lock();
    abort = true;
    condition.wakeOne();
    mutex.unlock();

    wait();

    delete this->api;

    log("WebdiskSync destroyed");
    this->logFile->flush();

    if(this->logFile != NULL)
        delete this->logFile;
}

void WebdiskSync::resetDB()
{
    QFile(tbApp->applicationDirPath() + "/wdsync.db").remove();
}

void WebdiskSync::resolveConflict(int master, bool remember)
{
    conflictMutex.lock();
    this->master = master;
    this->rememberMaster = remember;
    conflictCondition.wakeOne();
    conflictMutex.unlock();

    log(QString("Conflict resolved - master: %1, remember: %2")
        .arg(master)
        .arg(remember ? 1 : 0));
}

void WebdiskSync::sync()
{
    QMutexLocker locker(&mutex);

    if(!isRunning())
    {
        start(LowPriority);
    }
    else
    {
        restart = true;
        condition.wakeOne();
    }
}

bool WebdiskSync::isUpToDate(const QString &syncPath, const QString &userEmail, const QString &userPassword)
{
    QMutexLocker locker(&mutex);
    return(this->syncPath == syncPath
           && this->api->userEmail == userEmail
           && this->api->userPassword == userPassword);
}

bool WebdiskSync::openDB()
{
    db = QSqlDatabase::addDatabase("QSQLITE");
    db.setDatabaseName(this->dbPath);

    if(!db.open())
        return(false);

    db.exec("CREATE TABLE IF NOT EXISTS [status] ("
             "	[id] INTEGER PRIMARY KEY,"
             "	[last_sync] INTEGER NOT NULL DEFAULT 0"
             ")");
    db.exec("CREATE TABLE IF NOT EXISTS [file] ("
             "	[filename] TEXT NOT NULL PRIMARY KEY,"
             "	[type] INTEGER NOT NULL DEFAULT 0,"
             "	[id] INTEGER NOT NULL DEFAULT 0,"
             "	[date] INTEGER NOT NULL DEFAULT 0,"
             "	[local_created] INTEGER NOT NULL DEFAULT 0,"
             "	[local_modified] INTEGER NOT NULL DEFAULT 0,"
             "	[local_accessed] INTEGER NOT NULL DEFAULT 0,"
             "	[remote_created] INTEGER NOT NULL DEFAULT 0,"
             "	[remote_modified] INTEGER NOT NULL DEFAULT 0,"
             "	[remote_accessed] INTEGER NOT NULL DEFAULT 0"
             ")");

    return(true);
}

void WebdiskSync::run()
{
    if(!this->openDB())
    {
        log("Failed to open sync database");
        emit syncError(tr("Failed to open sync database."));
        return;
    }

    this->netManager = new QNetworkAccessManager;

    forever
    {
        if(abort)
            break;

        log(QString("-").repeated(72));
        log(QString("Starting Webdisk synchronization (remote folder %1 <-> local folder %2)")
            .arg(0)
            .arg(this->syncPath));
        numCreatedFiles = numCreatedFolders = 0;
        bytesReceived = bytesSent = 0;
        msecsReceived = msecsSent = 0;
        abortCurrentSync = false;
        rememberMaster = false;

        if(!QDir(this->syncPath).exists())
        {
            log(QString("Sync directory %1 does not exist. Resetting database.").arg(this->syncPath));

            QSqlQuery fileDelQuery("DELETE FROM [file]");
            fileDelQuery.exec();

            QSqlQuery statusDelQuery("DELETE FROM [status]");
            statusDelQuery.exec();
        }

        bool result = this->syncFolder(0, this->syncPath);

        log(QString("Finished Webdisk synchronization (result: %1)")
            .arg(result?1:0));
        if(this->logFile != NULL)
            this->logFile->flush();

        if(!abort)
            emit syncDone(result, numCreatedFiles, numCreatedFolders);
        else
            break;

        mutex.lock();
        if(!restart)
            condition.wait(&mutex);
        restart = false;
        mutex.unlock();
    }
    delete this->netManager;
}

QString WebdiskSync::shortPath(const QString &path)
{
    QString result = path.right(path.length() - this->syncPath.length() - 1);
    if(result.length() == 0)
        result = "/";
    return(result);
}

QString WebdiskSync::sanitizeFilename(const QString &name)
{
    QString result = name;
    const char *forbiddenChars = "";

#ifdef Q_WS_WIN
    forbiddenChars = "/?<>\\:*|";
#endif

    // TODO: remove forbidden stuff, correct charset?

    for(unsigned int i=0; i<strlen(forbiddenChars); i++)
        result = result.replace(forbiddenChars[i], '_');

    return(result);
}

void WebdiskSync::downloadProgress(qint64 bytesReceived, qint64 bytesTotal)
{
    reportSpeed(bytesReceived);
}

void WebdiskSync::uploadProgress(qint64 bytesSent, qint64 bytesTotal)
{
    reportSpeed(0, bytesSent);
}

void WebdiskSync::reportSpeed(unsigned int addDownBytes,
                              unsigned int addUpBytes)
{
    if(qAbs(QDateTime::currentDateTime().msecsTo(lastSpeedReport)) < 1000)
        return;

    unsigned int msecs = qAbs(QDateTime::currentDateTime().msecsTo(transferStart)) + msecsReceived;

    if((unsigned int)(msecs/1000) == 0)
        return;

    unsigned int downPerSec = (bytesReceived+addDownBytes)*1000 / msecs,
            upPerSec = (bytesSent+addUpBytes)*1000 / msecs;

    emit syncSpeedReport(downPerSec, upPerSec);

    lastSpeedReport = QDateTime::currentDateTime();

    /*transferStart = QDateTime::currentDateTime();
    msecsReceived = 0;
    bytesReceived = 0;
    bytesSent = 0;*/
}

QNetworkReply *WebdiskSync::getURLSynchronous(const QUrl &url)
{
    QEventLoop loop;
    QNetworkRequest req = api->createRequest(url);
    transferStart = QDateTime::currentDateTime();
    QNetworkReply *reply = this->netManager->get(req);

    connect(reply, SIGNAL(finished()), &loop, SLOT(quit()));
    connect(reply, SIGNAL(error(QNetworkReply::NetworkError)), &loop, SLOT(quit()));
    connect(reply, SIGNAL(sslErrors(QList<QSslError>)), reply, SLOT(ignoreSslErrors()));
    connect(reply, SIGNAL(downloadProgress(qint64,qint64)), this, SLOT(downloadProgress(qint64,qint64)));

    loop.exec();

    if(reply->error() != QNetworkReply::NoError)
    {
        delete reply;
        return(NULL);
    }

    bytesReceived += reply->size();
    msecsReceived += qAbs(QDateTime::currentDateTime().msecsTo(transferStart));
    reportSpeed();

    return(reply);
}

QNetworkReply *WebdiskSync::postURLSynchronous(const QUrl &url, QIODevice *data)
{
    QEventLoop loop;
    QNetworkRequest req = api->createRequest(url);
    req.setRawHeader("Content-Type", "application/octet-stream");       // TODO: get mime type
    req.setRawHeader("Content-Length", QString("%1").arg(data->size()).toAscii());

    transferStart = QDateTime::currentDateTime();
    QNetworkReply *reply = this->netManager->post(req, data);

    connect(reply, SIGNAL(finished()), &loop, SLOT(quit()));
    connect(reply, SIGNAL(error(QNetworkReply::NetworkError)), &loop, SLOT(quit()));
    connect(reply, SIGNAL(sslErrors(QList<QSslError>)), reply, SLOT(ignoreSslErrors()));
    connect(reply, SIGNAL(uploadProgress(qint64,qint64)), this, SLOT(uploadProgress(qint64,qint64)));

    loop.exec();

    if(reply->error() != QNetworkReply::NoError)
    {
        delete reply;
        return(NULL);
    }

    bytesSent += data->size();
    msecsSent += QDateTime::currentDateTime().msecsTo(transferStart);
    reportSpeed();

    return(reply);
}

QDomDocument WebdiskSync::getURL(const QUrl &url)
{
    QDomDocument doc;
    QNetworkReply *reply = this->getURLSynchronous(url);
    bool success = false;

    if(reply != NULL)
    {
        success = doc.setContent(reply);
        reply->deleteLater();
    }

    if(!success || api->xmlVal(doc, "response/status") != "OK")
    {
        log("getURL failed (network error or unexpected response)");
        //emit syncError(tr("The service returned an unexpected answer."));
    }

    return(doc);
}

bool WebdiskSync::didFileExist(const QString &name)
{
    bool result = false;

    QSqlQuery query(db);
    query.prepare("SELECT COUNT(*) FROM [file] WHERE [filename]=:name AND [type]=:type");
    query.bindValue("name", name);
    query.bindValue("type", WD_TYPE_FILE);
    query.exec();
    while(query.next())
    {
        result = query.value(0).toInt() > 0;
    }

    return(result);
}

bool WebdiskSync::didFolderExist(const QString &name)
{
    bool result = false;

    QSqlQuery query(db);
    query.prepare("SELECT COUNT(*) FROM [file] WHERE [filename]=:name AND [type]=:type");
    query.bindValue("name", name);
    query.bindValue("type", WD_TYPE_FOLDER);
    query.exec();
    while(query.next())
    {
        result = query.value(0).toInt() > 0;
    }

    return(result);
}

void WebdiskSync::log(const QString &entry)
{
    if(logFile == NULL)
        return;

    QTextStream out(logFile);
    out << "[" << QDateTime::currentDateTime().toString() << "] " << entry << "\n";
}

bool WebdiskSync::deleteWebdiskFile(int id)
{
    QUrl req = this->api->serviceURL;
    req.addQueryItem("method", "DeleteWebdiskFile");
    req.addQueryItem("params[0]", api->userEmail);
    req.addQueryItem("params[1]", api->md5(api->userPassword));
    req.addQueryItem("params[2]", QString("%1").arg(id));

    QDomDocument doc = this->getURL(req);
    bool success = api->xmlVal(doc, "reponse/status") == "OK";

    if(success)
    {
        QSqlQuery query(db);
        query.prepare("DELETE FROM [file] WHERE [id]=:id AND [type]=:type");
        query.bindValue(":id", id);
        query.bindValue(":type", WD_TYPE_FILE);
        query.exec();
        return(true);
    }

    return(false);
}

bool WebdiskSync::deleteWebdiskFolder(int id)
{
    QUrl req = this->api->serviceURL;
    req.addQueryItem("method", "DeleteWebdiskFolder");
    req.addQueryItem("params[0]", api->userEmail);
    req.addQueryItem("params[1]", api->md5(api->userPassword));
    req.addQueryItem("params[2]", QString("%1").arg(id));

    QDomDocument doc = this->getURL(req);
    bool success = api->xmlVal(doc, "reponse/status") == "OK";

    if(success)
    {
        QSqlQuery query(db);
        query.prepare("DELETE FROM [file] WHERE [id]=:id AND [type]=:type");
        query.bindValue(":id", id);
        query.bindValue(":type", WD_TYPE_FOLDER);
        query.exec();

        // TODO: delete child items of folder from DB?

        return(true);
    }

    return(false);
}

bool WebdiskSync::transferFile(const QString &localPath, int folderID, const QString &fileName)
{
    bool success = false;
    QFile localFile(localPath);

    if(!localFile.open(QIODevice::ReadOnly))
    {
        log(QString("Failed to open file %1 for read access")
            .arg(shortPath(localPath)));
        return(false);
    }

    QUrl req = this->api->serviceURL;
    req.addQueryItem("method", "CreateWebdiskFile");
    req.addQueryItem("params[0]", api->userEmail);
    req.addQueryItem("params[1]", api->md5(api->userPassword));
    req.addQueryItem("params[2]", QString("%1").arg(folderID));
    req.addQueryItem("params[3]", fileName);

    unsigned int fileCTime, fileMTime, fileATime, fileID;
    QNetworkReply *reply = postURLSynchronous(req, &localFile);
    if(reply != NULL)
    {
        QDomDocument doc;
        if(doc.setContent(reply))
        {
            fileID = api->xmlVal(doc, "response/result/array/info/array/id").toUInt();
            fileCTime = api->xmlVal(doc, "response/result/array/info/array/created").toUInt();
            fileMTime = api->xmlVal(doc, "response/result/array/info/array/modified").toUInt();
            fileATime = api->xmlVal(doc, "response/result/array/info/array/accessed").toUInt();

            success = true;
        }

        reply->deleteLater();
    }

    localFile.close();

    if(success)
    {
        QFileInfo localFileInfo(localPath);

        QSqlQuery query(db);
        query.prepare("REPLACE INTO [file]([filename],[type],[id],[date],[local_created],[local_modified],[local_accessed],[remote_created],[remote_modified],[remote_accessed]) VALUES "
                      "(:filename,:type,:id,:date,:localCreated,:localModified,:localAccessed,:remoteCreated,:remoteModified,:remoteAccessed)");
        query.bindValue(":filename", shortPath(localPath));
        query.bindValue(":type", WD_TYPE_FILE);
        query.bindValue(":id", fileID);
        query.bindValue(":date", (unsigned int)(QDateTime::currentDateTime().toMSecsSinceEpoch()/1000));
        query.bindValue(":localCreated", (unsigned int)(localFileInfo.created().toMSecsSinceEpoch()/1000));
        query.bindValue(":localModified", (unsigned int)(localFileInfo.lastModified().toMSecsSinceEpoch()/1000));
        query.bindValue(":localAccessed", (unsigned int)(localFileInfo.lastRead().toMSecsSinceEpoch()/1000));
        query.bindValue(":remoteCreated", fileCTime);
        query.bindValue(":remoteModified", fileMTime);
        query.bindValue(":remoteAccessed", fileATime);
        if(!query.exec())
        {
            log(QString("transferFile (upload) REPLACE query failed: %1")
                .arg(query.lastError().text()));
        }
        return(true);
    }

    return(false);
}

bool WebdiskSync::transferFile(const WebdiskEntry &entry, const QString &destPath)
{
    QFile destFile(destPath);
    bool success = false;

    QUrl req = this->api->serviceURL;
    req.addQueryItem("method", "GetWebdiskFile");
    req.addQueryItem("params[0]", api->userEmail);
    req.addQueryItem("params[1]", api->md5(api->userPassword));
    req.addQueryItem("params[2]", QString("%1").arg(entry.id));

    QNetworkReply *reply = getURLSynchronous(req);
    if(reply != NULL)
    {
        if(destFile.open(QIODevice::WriteOnly))
        {
            destFile.write(reply->readAll());
            destFile.close();

            success = true;
            numCreatedFiles++;
        }
        else
        {
            log(QString("transferFile: Failed to write to destination file %1: %2")
                .arg(destPath)
                .arg(destFile.errorString()));
            emit syncError(tr("Failed to synchronize file %1: %2")
                           .arg(shortPath(destPath))
                           .arg(destFile.errorString()));
        }

        reply->deleteLater();
    }
    else
        log(QString("transferFile(): Failed to get reply from service"));

    if(success)
    {
        QFileInfo destFileInfo(destFile);

        QSqlQuery query(db);
        query.prepare("REPLACE INTO [file]([filename],[type],[id],[date],[local_created],[local_modified],[local_accessed],[remote_created],[remote_modified],[remote_accessed]) VALUES "
                      "(:filename,:type,:id,:date,:localCreated,:localModified,:localAccessed,:remoteCreated,:remoteModified,:remoteAccessed)");
        query.bindValue(":filename", shortPath(destPath));
        query.bindValue(":type", WD_TYPE_FILE);
        query.bindValue(":id", entry.id);
        query.bindValue(":date", (unsigned int)(QDateTime::currentDateTime().toMSecsSinceEpoch()/1000));
        query.bindValue(":localCreated", (unsigned int)(destFileInfo.created().toMSecsSinceEpoch()/1000));
        query.bindValue(":localModified", (unsigned int)(destFileInfo.lastModified().toMSecsSinceEpoch()/1000));
        query.bindValue(":localAccessed", (unsigned int)(destFileInfo.lastRead().toMSecsSinceEpoch()/1000));
        query.bindValue(":remoteCreated", entry.created);
        query.bindValue(":remoteModified", entry.modified);
        query.bindValue(":remoteAccessed", entry.accessed);
        if(!query.exec())
        {
            log(QString("transferFile (download) REPLACE query failed: %1")
                .arg(query.lastError().text()));
        }
        return(true);
    }

    return(false);
}

unsigned int WebdiskSync::latestFileMTime(const QString &path)
{
    QSqlQuery query(db);
    query.prepare("SELECT [local_created],[local_modified] FROM [file] WHERE [filename]=:path AND [type]=:type");
    query.bindValue(":path", path);
    query.bindValue(":type", WD_TYPE_FILE);
    query.exec();
    while(query.next())
    {
        unsigned int cTime = query.value(0).toUInt(), mTime = query.value(1).toUInt();
        return(qMax(cTime, mTime));
    }

    return(0);
}

unsigned int WebdiskSync::latestFileMTime(int id, const QString &path)
{
    QSqlQuery query(db);
    query.prepare("SELECT [remote_created],[remote_modified] FROM [file] WHERE [id]=:id AND [type]=:type AND [filename]=:path");
    query.bindValue(":id", id);
    query.bindValue(":type", WD_TYPE_FILE);
    query.bindValue(":path", path);
    query.exec();
    while(query.next())
    {
        unsigned int cTime = query.value(0).toUInt(), mTime = query.value(1).toUInt();

        return(qMax(cTime, mTime));
    }

    return(0);
}

WebdiskEntry WebdiskSync::entryFromNode(QDomNode &node)
{
    WebdiskEntry e;

    e.accessed = api->xmlVal(node, "array/accessed").toUInt();
    e.created = api->xmlVal(node, "array/created").toUInt();
    e.id = api->xmlVal(node, "array/id").toInt();
    e.modified = api->xmlVal(node, "array/modified").toUInt();
    e.size = api->xmlVal(node, "array/size").toInt();
    e.title = api->xmlVal(node, "array/title");
    e.type = api->xmlVal(node, "array/type").toInt();

    return(e);
}

bool WebdiskSync::deleteFile(const QString &path)
{
    QFile file(path);

    if(file.remove())
    {
        QSqlQuery query(db);
        query.prepare("DELETE FROM [file] WHERE [filename]=:filename AND [type]=:type");
        query.bindValue(":filename", shortPath(path));
        query.bindValue(":type", WD_TYPE_FILE);
        query.exec();
        return(true);
    }
    else
    {
        log(QString("Failed to delete file %1")
            .arg(shortPath(path)));
    }

    return(false);
}

bool WebdiskSync::deleteFolder(const QString &path)
{
    QDir dir(path);
    QFileInfoList dirEntries = dir.entryInfoList();

    if(path.indexOf("../") != -1 || path.indexOf("/..") != -1)
    {
        log(QString("Cowardly refusing to delete local folder %1 containing ../ or /..").arg(path));
        return(false);
    }

    if(QDir::toNativeSeparators(path).indexOf( QDir::toNativeSeparators(this->syncPath) ) != 0)
    {
        log(QString("Cowardly refusing to delete local folder %1 which does not begin with %2").arg(path).arg(this->syncPath));
        return(false);
    }

    foreach(QFileInfo dirEntry, dirEntries)
    {
        if(dirEntry.fileName() == "." || dirEntry.fileName() == ".."
                || dirEntry.fileName().length() == 0)
            continue;

        QString entryFileName = path + "/" + dirEntry.fileName();
        if(dirEntry.isFile())
        {
            deleteFile(entryFileName);
        }
        else if(dirEntry.isDir())
        {
            deleteFolder(entryFileName);
        }
    }

    if(QDir().rmpath(path))
    {
        QSqlQuery query(db);
        query.prepare("DELETE FROM [file] WHERE [filename]=:filename AND [type]=:type");
        query.bindValue(":filename", shortPath(path));
        query.bindValue(":type", WD_TYPE_FOLDER);
        query.exec();
        return(true);
    }
    else
    {
        log(QString("Failed to delete folder %1")
            .arg(shortPath(path)));
    }

    return(false);
}

bool WebdiskSync::createFolder(const QString &path, int folderID)
{
    QDir dir;

    if(dir.mkdir(path))
    {
        QSqlQuery query(db);
        query.prepare("REPLACE INTO [file]([filename],[type],[id],[date]) VALUES(:filename,:type,:id,:date)");
        query.bindValue(":filename", shortPath(path));
        query.bindValue(":type", WD_TYPE_FOLDER);
        query.bindValue(":id", folderID);
        query.bindValue(":date", (unsigned int)QDateTime::currentDateTime().toMSecsSinceEpoch()/1000);
        query.exec();

        numCreatedFolders++;

        return(true);
    }
    else
    {
        log(QString("Failed to create folder %1")
            .arg(shortPath(path)));
    }

    return(false);
}

unsigned int WebdiskSync::createWebdiskFolder(const QString &localPath, int parentFolderID, const QString &folderName)
{
    QUrl req = this->api->serviceURL;
    req.addQueryItem("method", "CreateWebdiskFolder");
    req.addQueryItem("params[0]", api->userEmail);
    req.addQueryItem("params[1]", api->md5(api->userPassword));
    req.addQueryItem("params[2]", QString("%1").arg(parentFolderID));
    req.addQueryItem("params[3]", folderName);

    QDomDocument doc = this->getURL(req);
    bool success = api->xmlVal(doc, "response/status") == "OK";

    if(success)
    {
        unsigned int folderID = api->xmlVal(doc, "response/result/array/folderID").toUInt();

        QSqlQuery query(db);
        query.prepare("REPLACE INTO [file]([filename],[type],[id],[date]) VALUES(:filename,:type,:id,:date)");
        query.bindValue(":filename", shortPath(localPath));
        query.bindValue(":type", WD_TYPE_FOLDER);
        query.bindValue(":id", folderID);
        query.bindValue(":date", (unsigned int)QDateTime::currentDateTime().toMSecsSinceEpoch()/1000);
        query.exec();

        return(folderID);
    }
    else
    {
        log(QString("Failed to create Webdisk folder %1 (parent %2) for local folder %3")
            .arg(folderName)
            .arg(parentFolderID)
            .arg(shortPath(localPath)));
    }

    return(0);
}

bool WebdiskSync::syncFolder(unsigned int remoteFolderID, const QString &localPath)
{
    QDir localDir(localPath);
    log(QString("Processing folder: remoteID: %1, localPath: %2")
        .arg(remoteFolderID)
        .arg(localPath));

    if(!localDir.exists() && !createFolder(localPath, remoteFolderID))
    {
        emit syncError(tr("Failed to synchronize folder %1: Failed to create folder.").arg(shortPath(localPath)));
        return(false);
    }

    // remote -> local
    // TODO: check if something changed remotely by querying the API to reduce data transfer?
    // TODO: timezone stuff
    QStringList processedRemoteFileNames;
    QUrl req = this->api->serviceURL;
    req.addQueryItem("method", "GetWebdiskFolder");
    req.addQueryItem("params[0]", api->userEmail);
    req.addQueryItem("params[1]", api->md5(api->userPassword));
    req.addQueryItem("params[2]", QString("%1").arg(remoteFolderID));

    QDomDocument doc = this->getURL(req);
    QDomNodeList itemList = doc.elementsByTagName("contents");
    if(itemList.count() != 1)
    {
        // request failed
        return(false);
    }
    else
    {
        QDomNodeList items = itemList.at(0).toElement().elementsByTagName("item");
        for(int i=0; i<items.count(); i++)
        {
            QDomNode node = items.at(i);
            WebdiskEntry entry = entryFromNode(node);

            QString fileName = sanitizeFilename(entry.title);
            if(fileName.length() == 0)
                continue;
            if(fileName.at(0) == '.' && !this->syncHiddenItems)
                continue;

            QString localFileName = localPath + "/" + fileName,
                    shortFileName = shortPath(localFileName);

            processedRemoteFileNames.append(fileName);

            // file?
            if(entry.type == WD_TYPE_FILE)
            {
                QFile localFile(localFileName);

                // does file exist?
                if(!localFile.exists())
                {
                    // no. did the file exist at last sync?
                    if(didFileExist(shortFileName))
                    {
                        // yes => delete on webdisk
                        log(QString("Deleting Webdisk file %1 with path %2 (deleted locally)")
                            .arg(entry.id)
                            .arg(shortFileName));
                        deleteWebdiskFile(entry.id);
                    }
                    else
                    {
                        // no => file is new on Webdisk => transfer
                        log(QString("Downloading Webdisk file %1 with path %2 (new on Webdisk)")
                            .arg(entry.id)
                            .arg(shortFileName));
                        transferFile(entry, localFileName);
                    }
                }
                else
                {
                    // yes
                    QFileInfo localFileInfo(localFile);
                    bool changedLocally = (unsigned int)(localFileInfo.lastModified().toMSecsSinceEpoch()/1000) > latestFileMTime(shortFileName),
                            changedRemotely = qMax(entry.modified, entry.created) > latestFileMTime(entry.id, shortFileName);

                    if(changedLocally && changedRemotely)
                    {
                        // changed on both sides => conflict
                        log(QString("CONFLICT: File %1 with path %2 changed both locally and remotely")
                            .arg(entry.id)
                            .arg(shortFileName));

                        if(!rememberMaster)
                        {
                            emit syncConflict(shortFileName, localFileInfo.lastModified(),
                                              shortFileName, QDateTime::fromMSecsSinceEpoch((qint64)entry.modified*1000));
                            conflictMutex.lock();
                            conflictCondition.wait(&conflictMutex);
                        }

                        if(master == WD_MASTER_LOCAL)
                        {
                            changedRemotely = false;
                        }
                        else if(master == WD_MASTER_REMOTE)
                        {
                            changedLocally = false;
                        }
                        else
                        {
                            log("Aborting sync on user request after conflict");
                            abortCurrentSync = true;
                            return(false);
                        }
                    }

                    if(changedLocally)
                    {
                        // changed locally => transfer
                        log(QString("Uploading file %1 with path %2 (changed locally)")
                            .arg(entry.id)
                            .arg(shortFileName));
                        transferFile(localFileName, remoteFolderID, entry.title);
                    }

                    else if(changedRemotely)
                    {
                        // changed remotely => transfer
                        log(QString("Downloading Webdisk file %1 with path %2 (changed remotely)")
                            .arg(entry.id)
                            .arg(shortFileName));
                        transferFile(entry, localFileName);
                    }
                }
            }
            else if(entry.type == WD_TYPE_FOLDER)
            {
                QDir localDir(localFileName);

                // does folder exist?
                if(!localDir.exists())
                {
                    // no. did it exist at last sync?
                    if(didFolderExist(shortFileName))
                    {
                        // yes => deleted locally => delete on webdisk
                        log(QString("Deleting Webdisk folder %1 with path %2 (delete locally)")
                            .arg(entry.id)
                            .arg(shortFileName));
                        deleteWebdiskFolder(entry.id);
                    }
                    else
                    {
                        // no => sync it
                        syncFolder(entry.id, localFileName);
                    }
                }
                else
                {
                    // yes => sync it
                    syncFolder(entry.id, localFileName);
                }
            }
        }

        if(abortCurrentSync)
            return(false);
    }

    // local => remote
    QFileInfoList localDirContents = localDir.entryInfoList();
    foreach(QFileInfo dirEntry, localDirContents)
    {
        if(dirEntry.fileName() == "." || dirEntry.fileName() == ".."
                || dirEntry.fileName().length() == 0)
            continue;

        if(dirEntry.fileName().at(0) == '.'
                && !this->syncHiddenItems)
            continue;

        if(dirEntry.isHidden()
                && !this->syncHiddenItems)
            continue;

        // skip items already procesed in remote => local sync
        if(processedRemoteFileNames.contains(dirEntry.fileName(), Qt::CaseInsensitive))
            continue;

        // build local filename
        QString localFileName = localPath +  "/" + dirEntry.fileName(),
                shortFileName = shortPath(localFileName);

        // file?
        if(dirEntry.isFile())
        {
            // did the file exist at last sync?
            if(didFileExist(shortFileName))
            {
                // yes. deleted on Webdisk => delete locally
                log(QString("Deleting local file %1 (deleted on Webdisk)")
                    .arg(shortFileName));
                deleteFile(localFileName);
            }
            else
            {
                // no. new file => transfer
                log(QString("Uploading local file %1 (created locally)")
                    .arg(shortFileName));
                transferFile(localFileName, remoteFolderID, dirEntry.fileName());
            }
        }

        // folder?
        else if(dirEntry.isDir())
        {
            // did the folder exist at last sync?
            if(didFolderExist(shortFileName))
            {
                // yes. deleted on Webdisk => delete locally
                log(QString("Deleting local folder %1 (deleted on Webdisk)")
                    .arg(shortFileName));
                deleteFolder(localFileName);
            }
            else
            {
                // no. new folder => transfer
                log(QString("Creating Webdisk folder %1 (parent %2) for local folder %3 (created locally)")
                    .arg(dirEntry.fileName())
                    .arg(remoteFolderID)
                    .arg(shortPath(localFileName)));
                unsigned int newFolderID = createWebdiskFolder(localFileName, remoteFolderID, dirEntry.fileName());
                if(newFolderID > 0)
                    syncFolder(newFolderID, localFileName);
            }
        }

        if(abortCurrentSync)
            return(false);
    }

    return(true);
}
