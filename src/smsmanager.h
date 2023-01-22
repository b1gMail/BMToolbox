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

#ifndef SMSMANAGER_H
#define SMSMANAGER_H

#include <QMainWindow>
#include <QComboBox>
#include <QLineEdit>
#include "clientapi.h"

namespace Ui {
class SMSManager;
}

class SMSManager : public QMainWindow
{
    Q_OBJECT

public:
    explicit SMSManager(QWidget *parent, b1gMailAPI::ClientAPI *api);
    ~SMSManager();

public slots:
    void toAddressbook();
    void infoRetrieved(b1gMailAPI::Info&);
    void showComposePage();
    void showOutboxPage();
    void showBalancePage();
    void smsTypeChanged(int);
    void smsTextChanged();
    void sendSMS();
    void smsSent(bool success);
    void smsOutboxRetrieved(b1gMailAPI::SMSOutbox&);
    void outboxItemSelected();
    void refreshOutbox();
    void balanceChanged(int balance);
    void chargeAccount();
    virtual void show();

private:
    void switchPage(int no);
    void setNo(QComboBox *preBox, QLineEdit *noEdit, const QString &no);

private:
    Ui::SMSManager *ui;
    b1gMailAPI::ClientAPI *api;
    b1gMailAPI::Info apiInfo;
};

#endif // SMSMANAGER_H
