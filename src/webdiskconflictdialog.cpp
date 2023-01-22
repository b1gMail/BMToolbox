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

#include "webdiskconflictdialog.h"
#include "ui_webdiskconflictdialog.h"
#include "webdisksync.h"

WebdiskConflictDialog::WebdiskConflictDialog(QWidget *parent, const QString &localFileText, const QString &remoteFileText) :
    QDialog(parent),
    ui(new Ui::WebdiskConflictDialog)
{
    ui->setupUi(this);

    this->ui->localFileLabel->setText(localFileText);
    this->ui->remoteFileLabel->setText(remoteFileText);

    master = WD_MASTER_UNKNOWN;
    remember = false;

    this->setAttribute(Qt::WA_QuitOnClose, false);

#ifdef Q_WS_MAC
    this->adjustSize();
#endif

    this->setFixedSize(this->size());
}

WebdiskConflictDialog::~WebdiskConflictDialog()
{
    delete ui;
}

void WebdiskConflictDialog::show()
{
    QDialog::show();
    raise();
}

void WebdiskConflictDialog::acceptLocal()
{
    master = WD_MASTER_LOCAL;
    remember = ui->rememberCheckBox->checkState() == Qt::Checked;
    accept();
}

void WebdiskConflictDialog::acceptRemote()
{
    master = WD_MASTER_REMOTE;
    remember = ui->rememberCheckBox->checkState() == Qt::Checked;
    accept();
}
