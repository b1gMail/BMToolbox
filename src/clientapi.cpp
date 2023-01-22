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

#include "clientapi.h"
#include "bmtoolboxapp.h"
#include <QtNetwork/QNetworkRequest>
#include <QtNetwork/QNetworkReply>
#include <QUrl>
#include <QCryptographicHash>
#include <QMessageBox>
#include <QtXml/QDomDocument>
#include <QDateTime>

using namespace b1gMailAPI;

ClientAPI::ClientAPI(QString serviceURL) :
    QObject(NULL)
{
    this->serviceURL = QUrl(serviceURL + QString("interface/clientapi.php"));
    this->serviceURL.addQueryItem("class", "BMToolInterface");

    this->userID = 0;
    this->userEmail = "";
    this->userPassword = "";

    this->infoFor = "";

    this->netManager = new QNetworkAccessManager(this);
}

QString ClientAPI::md5(QString val)
{
    return(QCryptographicHash::hash(val.toUtf8(), QCryptographicHash::Md5).toHex());
}

QString ClientAPI::getTimezoneOffset()
{
    QDateTime t1 = QDateTime::currentDateTime(), t2 = t1.toUTC();
    t1.setTimeSpec(Qt::UTC);

    int offset = t2.secsTo(t1);

    return(QString("%1").arg(offset));
}

QNetworkRequest ClientAPI::createRequest(const QUrl &url)
{
    QNetworkRequest req;
    req.setUrl(url);
    req.setRawHeader("User-Agent", QString("BMToolbox/%1").arg(tbApp->applicationVersion()).toAscii());
    return(req);
}

QNetworkReply *ClientAPI::getURL(QUrl &url)
{
    QNetworkRequest req = this->createRequest(url);
    QNetworkReply *reply = this->netManager->get(req);

    connect(reply, SIGNAL(error(QNetworkReply::NetworkError)), this, SLOT(networkError(QNetworkReply::NetworkError)));
    connect(reply, SIGNAL(sslErrors(QList<QSslError>)), reply, SLOT(ignoreSslErrors()));        // TODO better solution?

    return(reply);
}

void ClientAPI::getInfo(bool force)
{
    if(!force && this->infoFor == this->userEmail)
    {
        emit infoRetrieved(this->info);
        return;
    }

    QUrl url = this->serviceURL;
    url.addQueryItem("method", "CheckLogin");
    url.addQueryItem("params[0]", this->userEmail);
    url.addQueryItem("params[1]", this->md5(this->userPassword));

    QNetworkReply *reply = this->getURL(url);
    connect(reply, SIGNAL(finished()), this, SLOT(getInfoFinished()));
}

QString ClientAPI::xmlVal(QDomNode &doc, const QString &path, const QString &def)
{
    QStringList pathList = path.split('/');
    QDomNode node = doc;

    for(QStringList::Iterator it = pathList.begin(); it != pathList.end(); it++)
    {
        QDomNodeList list = node.childNodes();

        bool found = false;
        for(int i=0; i<list.count(); i++)
        {
            if(list.at(i).nodeName() == *it)
            {
                found = true;
                if(it == pathList.end()-1)
                {
                    return(list.at(i).toElement().text());
                }
                else
                {
                    node = list.at(i);
                }
                break;
            }
        }

        if(!found)
            break;
    }

    return(def);
}

void ClientAPI::getInfoFinished()
{
    QNetworkReply *reply = dynamic_cast<QNetworkReply *>(sender());

    QDomDocument doc;
    bool success = doc.setContent(reply);
    reply->deleteLater();

    if(!success || xmlVal(doc, "response/status") != "OK")
    {
        // this is not worth an error message if we already have older info data available
        if(this->infoFor != this->userEmail)
            emit errorOccured(tr("The service returned an unexpected answer."));
        return;
    }

    if(xmlVal(doc, "response/result/array/loginOK", "0") != "1")
    {
        emit errorOccured(tr("Your account username/password is invalid."));
        return;
    }

    int newBalance = (unsigned int)xmlVal(doc, "response/result/array/balance", "0").toInt();
    if(this->infoFor == this->userEmail)
    {
        if(info.accountBalance != (unsigned int)newBalance)
            emit balanceChanged(newBalance);
    }

    info.accountBalance     = newBalance;
    info.hostName           = xmlVal(doc, "response/result/array/hostName");
    info.httpMailAccess     = xmlVal(doc, "response/result/array/httpmailAccess", "0") == "1";
    info.imapAccess         = xmlVal(doc, "response/result/array/imapAccess", "0") == "1";
    info.pop3Access         = xmlVal(doc, "response/result/array/pop3Access", "0") == "1";
    info.smsAccess          = xmlVal(doc, "response/result/array/smsAccess", "0") == "1";
    info.smsFrom            = xmlVal(doc, "response/result/array/smsFrom");
    info.smsOwnFrom         = xmlVal(doc, "response/result/array/smsOwnFrom", "0") == "1";
    info.smtpAccess         = xmlVal(doc, "response/result/array/smtpAccess", "0") == "1";
    info.userID             = (unsigned int)xmlVal(doc, "response/result/array/userID", "0").toInt();
#ifdef Q_WS_MAC
    info.faxAccess          = false;        // no fax support on mac at the moment
#else
    info.faxAccess          = xmlVal(doc, "response/result/array/plugins/array/FaxPlugin/array/faxAccess", "0") == "1";
#endif
    info.faxAllowOwnName    = xmlVal(doc, "response/result/array/plugins/array/FaxPlugin/array/allowOwnName", "0") == "1";
    info.faxAllowOwnNo      = xmlVal(doc, "response/result/array/plugins/array/FaxPlugin/array/allowOwnNo", "0") == "1";
    info.faxDefaultFromName = xmlVal(doc, "response/result/array/plugins/array/FaxPlugin/array/defaultFromName", "");
    info.faxDefaultFromNo   = xmlVal(doc, "response/result/array/plugins/array/FaxPlugin/array/defaultFromNo", "");
    info.webdiskAccess      = xmlVal(doc, "response/result/array/webdiskAccess", "0") == "1";
    info.latestVersion      = xmlVal(doc, "response/result/array/latestVersion", "0.0.0");

    this->userID = info.userID;

    QString smsPre = xmlVal(doc, "response/result/array/smsPre");
    if(smsPre.length() > 2)
        info.smsPre         = xmlVal(doc, "response/result/array/smsPre").split(':');
    else
        info.smsPre.clear();

    info.smsTypes.clear();
    QDomNodeList smsTypesList = doc.elementsByTagName("smsTypes");
    if(smsTypesList.count() == 1)
    {
        QDomNodeList items = smsTypesList.at(0).toElement().elementsByTagName("item");
        if(items.count() > 0)
        {
            for(int i=0; i<items.count(); i++)
            {
                QDomNode node = items.at(i);
                SMSType t;

                t.defaultType = xmlVal(node, "array/default") == "1";
                t.flags = xmlVal(node, "array/flags").toInt();
                t.gateway = xmlVal(node, "array/gateway").toInt();
                t.id = xmlVal(node, "array/id").toInt();
                t.maxLength = xmlVal(node, "array/maxlength", "160").toInt();
                t.price = xmlVal(node, "array/price").toInt();
                t.title = xmlVal(node, "array/title", "Untitled");
                t.typeString = xmlVal(node, "array/type");

                info.smsTypes.append(t);
            }
        }
    }

    this->infoFor = this->userEmail;

    emit infoRetrieved(info);
}

void ClientAPI::sendSMS(const QString &from, const QString &to, const int type, const QString &text)
{
    QUrl url = this->serviceURL;
    url.addQueryItem("method", "SendSMS");
    url.addQueryItem("params[0]", this->userEmail);
    url.addQueryItem("params[1]", this->md5(this->userPassword));
    url.addQueryItem("params[2]", from);
    url.addQueryItem("params[3]", to);
    url.addQueryItem("params[4]", QString("%1").arg(type));
    url.addQueryItem("params[5]", text);

    QNetworkReply *reply = this->getURL(url);
    connect(reply, SIGNAL(finished()), this, SLOT(sendSMSFinished()));
}

void ClientAPI::getSMSOutboxFinished()
{
    QNetworkReply *reply = dynamic_cast<QNetworkReply *>(sender());

    QDomDocument doc;
    bool success = doc.setContent(reply);
    reply->deleteLater();

    if(!success || xmlVal(doc, "response/status") != "OK")
    {
        emit errorOccured(tr("The service returned an unexpected answer."));
        return;
    }

    SMSOutbox outbox;
    QDomNodeList outboxList = doc.elementsByTagName("outbox");
    if(outboxList.count() == 1)
    {
        QDomNodeList items = outboxList.at(0).toElement().elementsByTagName("item");
        if(items.count() > 0)
        {
            for(int i=0; i<items.count(); i++)
            {
                QDomNode node = items.at(i);
                SMSOutboxEntry e;

                e.date = QDateTime::fromMSecsSinceEpoch((qint64)xmlVal(node, "array/date").toLongLong()*(quint64)1000);
                e.from = xmlVal(node, "array/from");
                e.id = xmlVal(node, "array/id").toInt();
                e.price = xmlVal(node, "array/price").toInt();
                e.text = xmlVal(node, "array/text");
                e.to = xmlVal(node, "array/to");

                outbox.append(e);
            }
        }
    }

    emit smsOutboxRetrieved(outbox);
}

void ClientAPI::getSMSOutbox()
{
    QUrl url = this->serviceURL;
    url.addQueryItem("method", "GetSMSOutbox");
    url.addQueryItem("params[0]", this->userEmail);
    url.addQueryItem("params[1]", this->md5(this->userPassword));

    QNetworkReply *reply = this->getURL(url);
    connect(reply, SIGNAL(finished()), this, SLOT(getSMSOutboxFinished()));
}

void ClientAPI::sendSMSFinished()
{
    QNetworkReply *reply = dynamic_cast<QNetworkReply *>(sender());

    QDomDocument doc;
    bool success = doc.setContent(reply);
    reply->deleteLater();

    if(!success || xmlVal(doc, "response/status") != "OK")
    {
        emit errorOccured(tr("The service returned an unexpected answer."));
        emit smsSent(false);
        return;
    }

    success = xmlVal(doc, "response/result/array/sendOK") == "1";
    int newBalance = xmlVal(doc, "response/result/array/balance", QString("%1").arg(this->info.accountBalance)).toInt();

    if(this->info.accountBalance != (unsigned int)newBalance)
    {
        this->info.accountBalance = newBalance;
        emit balanceChanged(newBalance);
    }

    emit smsSent(success);
}

void ClientAPI::getAddressbook()
{
    QUrl url = this->serviceURL;
    url.addQueryItem("method", "GetSMSAddressbook");
    url.addQueryItem("params[0]", this->userEmail);
    url.addQueryItem("params[1]", this->md5(this->userPassword));

    QNetworkReply *reply = this->getURL(url);
    connect(reply, SIGNAL(finished()), this, SLOT(getAddressbookFinished()));
}

void ClientAPI::getAddressbookFinished()
{
    QNetworkReply *reply = dynamic_cast<QNetworkReply *>(sender());

    QDomDocument doc;
    bool success = doc.setContent(reply);
    reply->deleteLater();

    if(!success || xmlVal(doc, "response/status") != "OK")
    {
        emit errorOccured(tr("The service returned an unexpected answer."));
        return;
    }

    Addressbook book;
    QDomNodeList addresses = doc.elementsByTagName("item");
    for(int i=0; i<addresses.count(); i++)
    {
         if(addresses.at(i).childNodes().count() != 1)
            continue;

        AddressbookEntry entry;

        QDomNode array = addresses.at(i).childNodes().at(0);
        for(int j=0; j<array.childNodes().count(); j++)
        {
            QDomNode node = array.childNodes().at(j);

            if(node.nodeName() == "firstname")
                entry.firstName = node.toElement().text();
            else if(node.nodeName() == "lastname")
                entry.lastName = node.toElement().text();
            else if(node.nodeName() == "handy")
                entry.cellPhone = node.toElement().text();
            else if(node.nodeName() == "work_handy")
                entry.workCellPhone = node.toElement().text();
            else if(node.nodeName() == "fax")
                entry.fax = node.toElement().text();
            else if(node.nodeName() == "work_fax")
                entry.workFax = node.toElement().text();
        }

        book.append(entry);
    }

    emit addressbookLoaded(book);
}

void ClientAPI::getFaxPrice(const QString &toNo, int pages)
{
    QUrl url = this->serviceURL;
    url.addQueryItem("method", "GetFaxPrice");
    url.addQueryItem("params[0]", this->userEmail);
    url.addQueryItem("params[1]", this->md5(this->userPassword));
    url.addQueryItem("params[2]", toNo);
    url.addQueryItem("params[3]", QString("%1").arg(pages));

    QNetworkReply *reply = this->getURL(url);
    connect(reply, SIGNAL(finished()), this, SLOT(getFaxPriceFinished()));
}

void ClientAPI::getFaxPriceFinished()
{
    QNetworkReply *reply = dynamic_cast<QNetworkReply *>(sender());

    QDomDocument doc;
    bool success = doc.setContent(reply);
    reply->deleteLater();

    if(!success || xmlVal(doc, "response/status") != "OK")
    {
        emit errorOccured(tr("The service returned an unexpected answer."));
        return;
    }

    if(xmlVal(doc, "response/result/array/status") != "OK")
    {
        // cannot send fax to this destination number or with this count of pages
        emit faxPriceRetrieved(-1);
    }

    int price = xmlVal(doc, "response/result/array/price", "-1").toInt();
    emit faxPriceRetrieved(price);
}

void ClientAPI::createWebSession(const QString &target)
{
    QUrl url = this->serviceURL;
    url.addQueryItem("method", "CreateWebSession");
    url.addQueryItem("params[0]", this->userEmail);
    url.addQueryItem("params[1]", this->userPassword);
    url.addQueryItem("params[2]", this->getTimezoneOffset());

    QNetworkReply *reply = this->getURL(url);
    connect(reply, SIGNAL(finished()), this, SLOT(createWebSessionFinished()));

    this->webSessionTargets[reply] = target;
}

void ClientAPI::createWebSessionFinished()
{
    QNetworkReply *reply = dynamic_cast<QNetworkReply *>(sender());
    QString target = this->webSessionTargets[reply];
    this->webSessionTargets.remove(reply);

    QDomDocument doc;
    bool success = doc.setContent(reply);
    reply->deleteLater();

    if(!success || xmlVal(doc, "response/status") != "OK")
    {
        emit errorOccured(tr("The service returned an unexpected answer."));
        return;
    }

    QString sessionID = "", sessionSecret = "";

    QDomNodeList l = doc.elementsByTagName("sessionID");
    if(l.count() == 1)
        sessionID = l.at(0).toElement().text();

    l = doc.elementsByTagName("sessionSecret");
    if(l.count() == 1)
        sessionSecret = l.at(0).toElement().text();

    if(sessionID.length() > 0)
        emit webSessionCreated(sessionID, sessionSecret, target);
}

void ClientAPI::checkForMails()
{
    QUrl url = this->serviceURL;
    url.addQueryItem("method", "CheckForMails");
    url.addQueryItem("params[0]", this->userEmail);
    url.addQueryItem("params[1]", this->md5(this->userPassword));

    QNetworkReply *reply = this->getURL(url);
    connect(reply, SIGNAL(finished()), this, SLOT(checkForMailsFinished()));
}

void ClientAPI::networkError(QNetworkReply::NetworkError)
{
    // Do not annoy user with error messages in case the network is down.
    //emit errorOccured(tr("Failed to contact %1. Please ensure that you are connected to the internet.").arg(tbApp->serviceName));
}

void ClientAPI::checkForMailsFinished()
{
    QNetworkReply *reply = dynamic_cast<QNetworkReply *>(sender());

    QDomDocument doc;
    bool success = doc.setContent(reply);
    reply->deleteLater();

    if(!success || xmlVal(doc, "response/status") != "OK")
    {
        // do not issue an error message here - this is not critical, the internet connection may just be
        // interrupted
        return;
    }

    QDomNodeList l = doc.elementsByTagName("recentMails");
    if(l.count() == 1)
    {
        int newMailCount = l.at(0).toElement().text().toInt();
        if(newMailCount > 0)
            emit newMails(newMailCount);
    }
}

void ClientAPI::sendFax(const QString &fromName, const QString &fromNo, const QString &toNo, const QByteArray &faxData)
{
    QUrl url = this->serviceURL;
    url.addQueryItem("method", "SendFax");
    url.addQueryItem("params[0]", this->userEmail);
    url.addQueryItem("params[1]", this->md5(this->userPassword));
    url.addQueryItem("params[2]", fromName);
    url.addQueryItem("params[3]", fromNo);
    url.addQueryItem("params[4]", toNo);
    url.addQueryItem("params[5]", QString("%1").arg(faxData.size()));

    QNetworkRequest req = this->createRequest(url);
    req.setRawHeader("Content-Type", "application/pdf");
    req.setRawHeader("Content-Length", QString("%1").arg(faxData.size()).toAscii());

    QNetworkReply *reply = this->netManager->post(req, faxData);
    connect(reply, SIGNAL(finished()), this, SLOT(sendFaxFinished()));
}

void ClientAPI::sendFaxFinished()
{
    QNetworkReply *reply = dynamic_cast<QNetworkReply *>(sender());

    QDomDocument doc;
    bool success = doc.setContent(reply);
    reply->deleteLater();

    if(!success || xmlVal(doc, "response/status") != "OK")
    {
        emit errorOccured(tr("The service returned an unexpected answer."));
        emit faxSent(false);
        return;
    }

    emit faxSent(xmlVal(doc, "response/result/array/status") == "OK");
}
