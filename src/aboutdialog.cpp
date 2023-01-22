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

#include "aboutdialog.h"
#include "ui_aboutdialog.h"
#include "bmtoolboxapp.h"
#include <QDesktopServices>
#include <QUrl>
#include <QMessageBox>

AboutDialog::AboutDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::AboutDialog)
{
    ui->setupUi(this);

    this->setWindowIcon(QIcon(tbApp->resPath("app-ico.png")));
    this->ui->appIcon->setPixmap(QPixmap(tbApp->resPath("app-ico.png")));
    this->ui->appName->setText(tbApp->applicationName());
    this->ui->appVersion->setText(tr("Version %1").arg(tbApp->applicationVersion()));
    this->ui->appCopyright->setText(tr("%1 %2").arg(QDate::currentDate().year()).arg(tbApp->serviceName));
    this->ui->appCopyrightNote->setVisible(tbApp->copyrightNote);

    this->adjustSize();
    this->setFixedSize(this->size());
}

AboutDialog::~AboutDialog()
{
    delete ui;
}

void AboutDialog::showLicense()
{
#ifdef Q_WS_WIN
    QDesktopServices::openUrl(QUrl::fromLocalFile(tbApp->applicationDirPath() + "/license.html"));
#else
    QDesktopServices::openUrl(QUrl::fromLocalFile(tbApp->resPath("license.html")));
#endif
}
