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

#ifndef ADDRESSBOOK_H
#define ADDRESSBOOK_H

#include <QDialog>
#include "clientapi.h"

namespace Ui {
class Addressbook;
}

enum AddressbookType
{
    SMS,
    Fax
};

class Addressbook : public QDialog
{
    Q_OBJECT

public:
    explicit Addressbook(QWidget *parent, b1gMailAPI::ClientAPI *api, AddressbookType type);
    ~Addressbook();

public slots:
    void addressbookLoaded(b1gMailAPI::Addressbook&);
    virtual void accept();

public:
    QString selectedNo;

private:
    Ui::Addressbook *ui;
    b1gMailAPI::ClientAPI *api;
    AddressbookType type;
};

#endif // ADDRESSBOOK_H
