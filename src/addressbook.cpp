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

#include "addressbook.h"
#include "ui_addressbook.h"
#include <QStandardItemModel>
#include <QStandardItem>
#include <QMessageBox>
#include <QMovie>

Addressbook::Addressbook(QWidget *parent, b1gMailAPI::ClientAPI *api, AddressbookType type) :
    QDialog(parent),
    ui(new Ui::Addressbook)
{
    ui->setupUi(this);

    QMovie *anim = new QMovie(":/icons/res/icons/load16-icon.gif");
    ui->loadingLabel->setMovie(anim);
    anim->start();

    this->api = api;
    this->type = type;
    this->selectedNo = "";

    connect(this->api, SIGNAL(addressbookLoaded(b1gMailAPI::Addressbook&)),
            this, SLOT(addressbookLoaded(b1gMailAPI::Addressbook&)));
    this->api->getAddressbook();
}

Addressbook::~Addressbook()
{
    delete ui;
}

void Addressbook::accept()
{
    if(this->ui->addressList->selectionModel()->selectedIndexes().count() == 2)
    {
        QStandardItemModel *model = dynamic_cast<QStandardItemModel *>(this->ui->addressList->model());
        this->selectedNo = model->item(this->ui->addressList->selectionModel()->selectedIndexes().at(0).row(), 1)->text();
    }
    else
        this->selectedNo = "";

    QDialog::accept();
}

void Addressbook::addressbookLoaded(b1gMailAPI::Addressbook &book)
{
    QStandardItemModel *model = new QStandardItemModel(this->ui->addressList);

    int j = 0;
    for(int i=0; i<book.count(); i++)
    {
        if(this->type == Fax)
        {
            if(book.at(i).fax.length() == 0 && book.at(i).workFax.length() == 0)
                continue;

            if(book.at(i).fax.length() > 0)
            {
                QStandardItem *item = new QStandardItem(QIcon(":/icons/res/icons/addr-priv-ico.png"), book.at(i).lastName + QString(", ") + book.at(i).firstName);
                item->setEditable(false);
                model->setItem(j, 0, item);

                item = new QStandardItem(book.at(i).fax);
                item->setEditable(false);
                model->setItem(j++, 1, item);
            }

            if(book.at(i).workFax.length() > 0)
            {
                QStandardItem *item = new QStandardItem(QIcon(":/icons/res/icons/addr-work-ico.png"), book.at(i).lastName + QString(", ") + book.at(i).firstName);
                item->setEditable(false);
                model->setItem(j, 0, item);

                item = new QStandardItem(book.at(i).workFax);
                item->setEditable(false);
                model->setItem(j++, 1, item);
            }
        }
        else if(this->type == SMS)
        {
            if(book.at(i).cellPhone.length() == 0 && book.at(i).workCellPhone.length() == 0)
                continue;

            if(book.at(i).cellPhone.length() > 0)
            {
                QStandardItem *item = new QStandardItem(QIcon(":/icons/res/icons/addr-priv-ico.png"), book.at(i).lastName + QString(", ") + book.at(i).firstName);
                item->setEditable(false);
                model->setItem(j, 0, item);

                item = new QStandardItem(book.at(i).cellPhone);
                item->setEditable(false);
                model->setItem(j++, 1, item);
            }

            if(book.at(i).workCellPhone.length() > 0)
            {
                QStandardItem *item = new QStandardItem(QIcon(":/icons/res/icons/addr-work-ico.png"), book.at(i).lastName + QString(", ") + book.at(i).firstName);
                item->setEditable(false);
                model->setItem(j, 0, item);

                item = new QStandardItem(book.at(i).workCellPhone);
                item->setEditable(false);
                model->setItem(j++, 1, item);
            }
        }
    }

    model->setHorizontalHeaderItem(0, new QStandardItem(tr("Name")));
    model->setHorizontalHeaderItem(1, new QStandardItem(tr("Number")));

    this->ui->addressList->setModel(model);
    this->ui->addressList->setColumnWidth(0, 200);
    this->ui->stackedWidget->setCurrentIndex(1);
}
