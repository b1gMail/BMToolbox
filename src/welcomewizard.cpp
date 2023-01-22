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

#include "welcomewizard.h"
#include "ui_welcomewizard.h"

#include <QPixmap>
#include <QMessageBox>
#include <QPushButton>
#include <QFileDialog>

#include "bmtoolboxapp.h"
#include "mainwindow.h"

WelcomeWizard::WelcomeWizard(QWidget *parent) :
    QWizard(parent),
    ui(new Ui::WelcomeWizard)
{
    ui->setupUi(this);

#ifdef Q_WS_WIN
    this->setWizardStyle(QWizard::ModernStyle);
    this->setPixmap(QWizard::LogoPixmap, QPixmap(tbApp->resPath("wizard-head.bmp")));
#endif

#ifdef Q_WS_MAC
    this->setWizardStyle(QWizard::MacStyle);
    this->setMacBackground();
#endif

    this->ui->loginPage->setSubTitle(this->ui->loginPage->subTitle().arg(tbApp->serviceName));
    this->ui->loginLabel->setText(this->ui->loginLabel->text().arg(tbApp->serviceName));
    this->ui->autostartLabel->setText(this->ui->autostartLabel->text().arg(tbApp->applicationName()));
    this->ui->webdiskSyncFolder->setText(tbApp->defaultWebdiskFolder());
    this->ui->wdBox->setTitle(this->ui->wdBox->title().arg(tbApp->nameWebdisk));
    this->ui->wdLabel->setText(this->ui->wdLabel->text().arg(tbApp->nameWebdisk));

    connect(this, SIGNAL(currentIdChanged(int)),
            this, SLOT(pageChanged(int)));

    this->api = new b1gMailAPI::ClientAPI(tbApp->serviceURL);
    connect(this->api, SIGNAL(infoRetrieved(b1gMailAPI::Info&)),
            this, SLOT(infoRetrieved(b1gMailAPI::Info&)));
    connect(this->api, SIGNAL(errorOccured(QString)),
            this, SLOT(apiErrorOccured(QString)));

    this->loginChecked = false;

    this->adjustSize();
}

WelcomeWizard::~WelcomeWizard()
{
    delete ui;
}

#ifdef Q_WS_MAC
#include <CoreFoundation/CoreFoundation.h>
#include <ApplicationServices/ApplicationServices.h>

void WelcomeWizard::setMacBackground()
{
    CFURLRef url;
    const int ExpectedImageWidth = 242;
    const int ExpectedImageHeight = 414;
    if (LSFindApplicationForInfo(kLSUnknownCreator, CFSTR("com.apple.KeyboardSetupAssistant"),
                                 0, 0, &url) == noErr)
    {
        CFBundleRef bundle = CFBundleCreate(kCFAllocatorDefault, url);
        if (bundle)
        {
            url = CFBundleCopyResourceURL(bundle, CFSTR("Background"), CFSTR("tif"), 0);
            if(!url) url = CFBundleCopyResourceURL(bundle, CFSTR("Background"), CFSTR("png"), 0);
            if (url)
            {
                CGImageSourceRef imageSource = CGImageSourceCreateWithURL(url, 0);
                CGImageRef image = CGImageSourceCreateImageAtIndex(imageSource, 0, 0);
                if (image)
                {
                    int width = CGImageGetWidth(image);
                    int height = CGImageGetHeight(image);
                    if (width == ExpectedImageWidth && height == ExpectedImageHeight)
                        this->setPixmap(QWizard::BackgroundPixmap, QPixmap::fromMacCGImageRef(image));
                    CFRelease(image);
                }
                CFRelease(imageSource);
            }
            CFRelease(bundle);
        }
        CFRelease(url);
    }
}
#endif

void WelcomeWizard::reject()
{
    int res = QMessageBox::question(this,
                                    tr("Cancel"),
                                    tr("Are you sure you want to cancel the setup wizard?\n\n%1 will be closed.").arg(tbApp->applicationName()),
                                    QMessageBox::Yes|QMessageBox::No,
                                    QMessageBox::NoButton);
    if(res == QMessageBox::Yes)
    {
        QWizard::reject();
        tbApp->quit();
    }
}

void WelcomeWizard::accept()
{
    // login
    tbApp->userPrefs->setValue("account/emailAddress", this->ui->accUsername->text());
    tbApp->userPrefs->setValue("account/password", this->ui->accPassword->text());

    // mail checking
    tbApp->userPrefs->setValue("mail/enableChecking", this->ui->mailcheckBox->isChecked());

    // webdisk
    if(accInfo.webdiskAccess && this->ui->wdBox->isChecked())
    {
        QDir webdiskDir(this->ui->webdiskSyncFolder->text());
        if(!webdiskDir.exists())
            QDir().mkdir(webdiskDir.absolutePath());
        tbApp->userPrefs->setValue("webdisk/enableSync",    true);
        tbApp->userPrefs->setValue("webdisk/targetFolder",  QDir::toNativeSeparators(webdiskDir.absolutePath()));
    }
    else
    {
        tbApp->userPrefs->setValue("webdisk/enableSync",    false);
        tbApp->userPrefs->setValue("webdisk/targetFolder",  tbApp->defaultWebdiskFolder());
    }

    // fax
    if(accInfo.faxAccess && this->ui->faxBox->isChecked())
    {
        tbApp->installFaxPrinter(false, api);
    }

    // autostart
    if(this->ui->autostartBox->isChecked())
    {
        tbApp->setAutostart(true);
    }

    // unset first start
    tbApp->userPrefs->setValue("common/firstStart",      false);

    // start main app
    MainWindow *w = new MainWindow();
    if(accInfo.faxAccess && this->ui->faxBox->isChecked())
        w->printerInfoTimer->start();

#ifdef Q_WS_WIN
    w->trayIcon->showMessage(tr("Welcome"), tr("You can find %1 here.").arg(tbApp->applicationName()));
#endif

    // accept
    QWizard::accept();
}

void WelcomeWizard::changeWebdiskFolder()
{
    QString newDir = QFileDialog::getExistingDirectory(this,
                                      tr("Choose %1 target").arg(tbApp->nameWebdisk),
                                      this->ui->webdiskSyncFolder->text(),
                                      QFileDialog::ShowDirsOnly);

    if(newDir.length() == 0
            || newDir == this->ui->webdiskSyncFolder->text())
        return;

    bool isEmpty = true;

    QDir dir(newDir);
    QFileInfoList fl = dir.entryInfoList();
    foreach(QFileInfo info, fl)
    {
        if(info.fileName() == "." || info.fileName() == ".."
                || info.isHidden())
            continue;

        isEmpty = false;
        break;
    }

    if(!isEmpty)
    {
        QMessageBox::warning(this,
                              tr("Error"),
                              tr("The target folder must be empty."));
        return;
    }

    this->ui->webdiskSyncFolder->setText(newDir);
}

void WelcomeWizard::infoRetrieved(b1gMailAPI::Info &info)
{
    accInfo = info;

    // show/hide feature boxes
    this->ui->wdBox->setVisible(info.webdiskAccess);
    this->ui->faxBox->setVisible(info.faxAccess);

    // show features page
    this->loginChecked = true;
    dynamic_cast<QPushButton *>(this->button(QWizard::NextButton))->setEnabled(true);
    next();
}

void WelcomeWizard::apiErrorOccured(QString msg)
{
    QMessageBox::warning(this, tr("Error"), tr("An error occured: %1\n\nPlease try again.").arg(msg));
    dynamic_cast<QPushButton *>(this->button(QWizard::NextButton))->setEnabled(true);
}

void WelcomeWizard::pageChanged(int id)
{
    if(id == 0)
    {
        this->loginChecked = false;
    }
}

bool WelcomeWizard::validateCurrentPage()
{
    if(currentId() == 0)
    {
        if(!loginChecked)
        {
            // disable next button
            dynamic_cast<QPushButton *>(this->button(QWizard::NextButton))->setEnabled(false);

            // verify login
            this->api->userEmail = this->ui->accUsername->text();
            this->api->userPassword = this->ui->accPassword->text();
            this->api->getInfo(true);

            // block operation (wait until login has been checked)
            return(false);
        }
    }

    return(true);
}
