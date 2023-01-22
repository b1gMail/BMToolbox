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

#include "smsmanager.h"
#include "ui_smsmanager.h"
#include "addressbook.h"
#include "bmtoolboxapp.h"
#include <QMessageBox>
#include <QMovie>

SMSManager::SMSManager(QWidget *parent, b1gMailAPI::ClientAPI *api) :
    QMainWindow(parent),
    ui(new Ui::SMSManager)
{
    ui->setupUi(this);
    this->setWindowTitle(tr("%1 %2").arg(tbApp->serviceName).arg(tbApp->nameSMSManager));

    tbApp->addToolBarBranding(this, this->ui->toolBar);

    QMovie *anim = new QMovie(":/icons/res/icons/load16-icon.gif");
    ui->loadingLabel->setMovie(anim);
    anim->start();

    this->setAttribute(Qt::WA_QuitOnClose, false);
    this->setAttribute(Qt::WA_DeleteOnClose, true);
    this->api = api;

    this->switchPage(0);

    connect(this->api, SIGNAL(smsOutboxRetrieved(b1gMailAPI::SMSOutbox&)),
            this, SLOT(smsOutboxRetrieved(b1gMailAPI::SMSOutbox&)));
    connect(this->api, SIGNAL(infoRetrieved(b1gMailAPI::Info&)),
            this, SLOT(infoRetrieved(b1gMailAPI::Info&)));
    connect(this->api, SIGNAL(balanceChanged(int)),
            this, SLOT(balanceChanged(int)));
    this->api->getInfo();
    this->refreshOutbox();

    this->setUnifiedTitleAndToolBarOnMac(tbApp->appStyle == "auto");
}

SMSManager::~SMSManager()
{
    delete ui;
}

void SMSManager::show()
{
    QMainWindow::show();
    raise();
}

void SMSManager::chargeAccount()
{
    this->api->createWebSession("membership");
}

void SMSManager::refreshOutbox()
{
    this->ui->refreshOutbox->setText(tr("Refreshing..."));
    this->ui->refreshOutbox->setEnabled(false);
    this->ui->previewPanel->setVisible(false);
    this->ui->outboxList->clear();
    this->api->getSMSOutbox();
}

void SMSManager::showComposePage()
{
    this->switchPage(1);
    this->ui->toNo->setFocus();
}

void SMSManager::showOutboxPage()
{
    this->switchPage(2);
}

void SMSManager::smsOutboxRetrieved(b1gMailAPI::SMSOutbox &outbox)
{
    this->ui->outboxList->clear();
    for(b1gMailAPI::SMSOutbox::Iterator it = outbox.begin();
        it != outbox.end();
        it++)
    {
        QTreeWidgetItem *item = new QTreeWidgetItem(0);
        item->setIcon(0, QIcon(":/icons/res/icons/sms-ico.png"));
        item->setText(0, (*it).from + "  ");
        item->setText(1, (*it).to + "  ");
        item->setText(2, (*it).date.toString(Qt::SystemLocaleShortDate) + "  ");
        item->setText(3, (*it).text.replace("\n", "").replace("\r", ""));

        item->setData(2, Qt::UserRole, (*it).date);
        item->setData(3, Qt::UserRole, (*it).text);

        this->ui->outboxList->addTopLevelItem(item);
    }

    this->ui->outboxList->resizeColumnToContents(0);
    this->ui->outboxList->resizeColumnToContents(1);
    this->ui->outboxList->resizeColumnToContents(2);

    this->ui->refreshOutbox->setText(tr("Refresh"));
    this->ui->refreshOutbox->setEnabled(true);
}

void SMSManager::outboxItemSelected()
{
    if(this->ui->outboxList->selectedItems().count() != 1)
        return;

    QTreeWidgetItem *item = this->ui->outboxList->selectedItems().at(0);
    this->ui->fromLabel->setText(item->text(0));
    this->ui->toLabel->setText(item->text(1));
    this->ui->dateLabel->setText(item->data(2, Qt::UserRole).toDateTime().toString(Qt::LocalDate));
    this->ui->outboxSMSText->setPlainText(item->data(3, Qt::UserRole).toString());

    this->ui->previewPanel->setVisible(true);
}

void SMSManager::showBalancePage()
{
    this->switchPage(3);
}

void SMSManager::switchPage(int no)
{
    this->ui->toolBar->setEnabled(no > 0);

    this->ui->actionCompose_SMS->setChecked(no == 1);
    this->ui->actionSMS_Outbox->setChecked(no == 2);
    this->ui->actionAccount_Balance->setChecked(no == 3);

    this->ui->stackedWidget->setCurrentIndex(no);
}

void SMSManager::infoRetrieved(b1gMailAPI::Info &info)
{
    disconnect(this->api, SIGNAL(infoRetrieved(b1gMailAPI::Info&)), this, 0);

    this->apiInfo.smsTypes.clear();
    this->apiInfo.smsPre.clear();
    this->apiInfo = info;

    // pres
    if(this->apiInfo.smsPre.count() > 0)
    {
        this->ui->fromNoPre->clear();
        this->ui->toNoPre->clear();

        for(QStringList::Iterator it = this->apiInfo.smsPre.begin();
            it != this->apiInfo.smsPre.end();
            it++)
        {
            this->ui->fromNoPre->addItem(*it);
            this->ui->toNoPre->addItem(*it);
        }

        this->ui->fromNoPre->adjustSize();
        this->ui->toNoPre->adjustSize();
    }
    else
    {
        this->ui->fromNoPre->setVisible(false);
        this->ui->toNoPre->setVisible(false);
    }

    // own sender?
    this->ui->fromNoPre->setEnabled(this->apiInfo.smsOwnFrom);
    this->ui->toNoPre->setEnabled(this->apiInfo.smsOwnFrom);

    // set sender
    this->setNo(this->ui->fromNoPre,
                this->ui->fromNo,
                this->apiInfo.smsFrom);

    // types
    this->ui->typeComboBox->clear();
    for(int i = 0; i < this->apiInfo.smsTypes.count(); i++)
    {
        const b1gMailAPI::SMSType *type = &this->apiInfo.smsTypes.at(i);

        this->ui->typeComboBox->addItem(tr("%1 (%2 credit(s))", "", type->price).arg(type->title).arg(type->price));
        if(type->defaultType)
            this->ui->typeComboBox->setCurrentIndex(this->ui->typeComboBox->count()-1);
    }

    // text
    this->smsTextChanged();

    // balance
    this->balanceChanged(this->apiInfo.accountBalance);

    this->showComposePage();
}

void SMSManager::balanceChanged(int balance)
{
    this->ui->balanceLabel->setText(QString("%1").arg(balance));
}

void SMSManager::setNo(QComboBox *preBox, QLineEdit *noEdit, const QString &no)
{
    noEdit->setText("");

    if(no.length() < 2)
        return;

    if(preBox->count() > 0)
    {
        for(int i=0; i<preBox->count(); i++)
        {
            QString pre = preBox->itemText(i);
            if(pre.length() > no.length())
                continue;

            if(no.left(pre.length()) == pre)
            {
                preBox->setCurrentIndex(i);
                noEdit->setText(no.mid(pre.length()));
                return;
            }
        }
    }
    else
    {
        noEdit->setText(no);
    }
}

void SMSManager::smsTypeChanged(int i)
{
    const b1gMailAPI::SMSType *type = &this->apiInfo.smsTypes.at(i);

    this->ui->fromNoLabel->setVisible((type->flags & BMAPI_SMSTYPE_FLAG_NOSENDER) == 0);
    this->ui->fromNoPre->setVisible((type->flags & BMAPI_SMSTYPE_FLAG_NOSENDER) == 0
                                    && this->apiInfo.smsPre.count() > 0);
    this->ui->fromNo->setVisible((type->flags & BMAPI_SMSTYPE_FLAG_NOSENDER) == 0);

    this->ui->charBar->setMaximum(type->maxLength);

    if(this->ui->smsText->toPlainText().length() > (int)type->maxLength)
    {
        this->ui->smsText->setPlainText(this->ui->smsText->toPlainText().left(type->maxLength));
        this->smsTextChanged();
    }
}

void SMSManager::smsTextChanged()
{
    int length = this->ui->smsText->toPlainText().length();
    this->ui->charBar->setValue(length);

    if(length > this->ui->charBar->maximum())
        this->ui->smsText->textCursor().deletePreviousChar();
}

void SMSManager::toAddressbook()
{
    Addressbook *book = new Addressbook(this, this->api, SMS);
    if(book->exec() == QDialog::Accepted)
    {
        this->setNo(this->ui->toNoPre,
                    this->ui->toNo,
                    book->selectedNo);
    }
    delete book;
}

void SMSManager::smsSent(bool success)
{
    disconnect(this->api, SIGNAL(smsSent(bool)), this, 0);

    if(!success)
    {
        QMessageBox::warning(this,
                             tr("Error"),
                             tr("The SMS could not be sent.\n\n"
                                "Possible reasons are an insufficient account balance or internet connection issues."));
    }
    else
    {
        this->ui->smsText->setPlainText("");
        this->ui->toNo->setText("");

        if(this->ui->toNoPre->count() > 0)
            this->ui->toNoPre->setCurrentIndex(0);

        this->refreshOutbox();
    }

    this->ui->sendSMSButton->setEnabled(true);
    this->ui->sendSMSButton->setText(tr("Send SMS"));
}

void SMSManager::sendSMS()
{
    this->ui->sendSMSButton->setEnabled(false);
    this->ui->sendSMSButton->setText(tr("Sending..."));

    QString fromNo = "", toNo = "", text = this->ui->smsText->toPlainText();
    const b1gMailAPI::SMSType *type = &this->apiInfo.smsTypes.at(this->ui->typeComboBox->currentIndex());

    // from no
    if((type->flags & BMAPI_SMSTYPE_FLAG_NOSENDER) == 0 && this->apiInfo.smsOwnFrom)
    {
        if(this->ui->fromNoPre->count() > 0)
            fromNo = this->ui->fromNoPre->itemText(this->ui->fromNoPre->currentIndex());
        fromNo += this->ui->fromNo->text();
    }
    else
        fromNo = this->apiInfo.smsFrom;

    // to no
    if(this->ui->toNoPre->count() > 0)
        toNo = this->ui->toNoPre->itemText(this->ui->toNoPre->currentIndex());
    toNo += this->ui->toNo->text();

    // text
    if(text.length() > (int)type->maxLength)
        text = text.left(type->maxLength);

    // send
    connect(this->api,
            SIGNAL(smsSent(bool)),
            this,
            SLOT(smsSent(bool)));
    this->api->sendSMS(fromNo, toNo, type->id, text);
}
