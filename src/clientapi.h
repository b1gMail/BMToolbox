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

#ifndef CLIENTAPI_H
#define CLIENTAPI_H

#include <QObject>
#include <QString>
#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkRequest>
#include <QtNetwork/QNetworkReply>
#include <QUrl>
#include <QList>
#include <QDomDocument>
#include <QStringList>
#include <QDateTime>
#include <QMap>

#define BMAPI_SMSTYPE_FLAG_NOSENDER                 1

namespace b1gMailAPI
{
    class AddressbookEntry
    {
    public:
        QString firstName;
        QString lastName;
        QString cellPhone;
        QString workCellPhone;
        QString fax;
        QString workFax;
        unsigned int id;
    };

    typedef QList<AddressbookEntry> Addressbook;

    class SMSType
    {
    public:
        unsigned int id;
        QString title;
        QString typeString;
        bool defaultType;
        unsigned int price;
        unsigned int gateway;
        unsigned int flags;
        unsigned int maxLength;
    };

    typedef QList<SMSType> SMSTypes;

    class SMSOutboxEntry
    {
    public:
        unsigned int id;
        QString from;
        QString to;
        QString text;
        unsigned int price;
        QDateTime date;
    };

    typedef QList<SMSOutboxEntry> SMSOutbox;

    class Info
    {
    public:
        QString hostName;
        bool httpMailAccess;
        bool pop3Access;
        bool imapAccess;
        bool smtpAccess;
        unsigned int userID;
        unsigned int accountBalance;
        bool smsAccess;
        SMSTypes smsTypes;
        bool smsOwnFrom;
        QString smsFrom;
        QStringList smsPre;
        bool webdiskAccess;
        bool faxAccess;
        bool faxAllowOwnName;
        bool faxAllowOwnNo;
        QString faxDefaultFromNo;
        QString faxDefaultFromName;
        QString latestVersion;
    };

    class ClientAPI : public QObject
    {
        Q_OBJECT
    public:
        explicit ClientAPI(QString serviceURL);

    public:
        void checkForMails();
        void createWebSession(const QString &target);
        void getAddressbook();
        void getInfo(bool force = false);
        void sendSMS(const QString &from, const QString &to, const int type, const QString &text);
        void getSMSOutbox();
        void getFaxPrice(const QString &toNo, int pages);
        void sendFax(const QString &fromName, const QString &fromNo, const QString &toNo, const QByteArray &faxData);

    public:
        QNetworkRequest createRequest(const QUrl &url);
        QNetworkReply *getURL(QUrl &url);
        QString xmlVal(QDomNode &doc, const QString &path, const QString &def = "");
        QString md5(QString val);

    signals:
        void webSessionCreated(QString &sessionID, QString &sessionSecret, QString &target);
        void newMails(int count);
        void addressbookLoaded(b1gMailAPI::Addressbook &book);
        void infoRetrieved(b1gMailAPI::Info &info);
        void smsSent(bool success);
        void smsOutboxRetrieved(b1gMailAPI::SMSOutbox &outbox);
        void balanceChanged(int balance);
        void faxPriceRetrieved(int price);
        void errorOccured(const QString &description);
        void faxSent(bool success);

    public slots:
        void checkForMailsFinished();
        void createWebSessionFinished();
        void getAddressbookFinished();
        void getInfoFinished();
        void sendSMSFinished();
        void getSMSOutboxFinished();
        void getFaxPriceFinished();
        void sendFaxFinished();
        void networkError(QNetworkReply::NetworkError);

    public:
        QUrl serviceURL;
        QString userEmail, userPassword;
        int userID;

    private:
        QString getTimezoneOffset();

    private:
        QNetworkAccessManager *netManager;
        Info info;
        QString infoFor;
        QMap<QNetworkReply *, QString> webSessionTargets;
    };
}

#endif // CLIENTAPI_H
