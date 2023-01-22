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

#ifndef WELCOMEWIZARD_H
#define WELCOMEWIZARD_H

#include <QWizard>

#include "clientapi.h"

namespace Ui {
class WelcomeWizard;
}

class WelcomeWizard : public QWizard
{
    Q_OBJECT

public:
    explicit WelcomeWizard(QWidget *parent = 0);
    ~WelcomeWizard();

public:
    virtual bool validateCurrentPage();

private:
    virtual void reject();
    virtual void accept();

#ifdef Q_WS_MAC
    void setMacBackground();
#endif

public slots:
    void infoRetrieved(b1gMailAPI::Info &info);
    void apiErrorOccured(QString msg);
    void pageChanged(int id);
    void changeWebdiskFolder();

private:
    Ui::WelcomeWizard *ui;
    b1gMailAPI::ClientAPI *api;
    b1gMailAPI::Info accInfo;
    bool loginChecked;
};

#endif // WELCOMEWIZARD_H
