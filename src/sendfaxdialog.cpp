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

#include "sendfaxdialog.h"
#include "ui_sendfaxdialog.h"
#include "bmtoolboxapp.h"
#include <QFile>
#include <QImage>
#include <QPainter>
#include <QMessageBox>
#include <QByteArray>
#include <QMap>
#include <QPrintPreviewDialog>
#include <QMovie>

#include "addressbook.h"

#ifdef Q_WS_WIN
#include "hpdf.h"
#endif

// TODO: zoom buttons, prev next update at scroll?

SendFaxDialog::SendFaxDialog(QWidget *parent, b1gMailAPI::ClientAPI *api, const QString &jobFile) :
    QDialog(parent),
    ui(new Ui::SendFaxDialog)
{
    ui->setupUi(this);

    this->api = api;

    QMovie *anim = new QMovie(":/icons/res/icons/load16-icon.gif");
    ui->loadingLabel->setMovie(anim);
    anim->start();

    this->setAttribute(Qt::WA_DeleteOnClose, true);
    this->setAttribute(Qt::WA_QuitOnClose, false);

    this->switchPage(0);

    connect(this->api, SIGNAL(infoRetrieved(b1gMailAPI::Info&)),
            this, SLOT(infoRetrieved(b1gMailAPI::Info&)));
    connect(this->api, SIGNAL(faxPriceRetrieved(int)),
            this, SLOT(faxPriceRetrieved(int)));
    connect(this->api, SIGNAL(faxSent(bool)),
            this, SLOT(faxSent(bool)));
    this->api->getInfo();

    this->jobFile = jobFile;
    this->bytesPerLine = 0;

    if(!this->readJobFile())
    {
        QMessageBox::critical(this, tr("Error"), tr("Fax job file has invalid format."));
    }

    this->ui->previewBox->setTitle(this->jobName);
    this->ui->pagesLabel->setText(tr("%1 page(s)", 0, this->pageBuffers.count()).arg(this->pageBuffers.count()));
    this->setWindowTitle(tr("Send fax: %1").arg(this->jobName));

    this->previewWidget = new QPrintPreviewWidget(this);
    this->previewWidget->setZoomMode(QPrintPreviewWidget::FitToWidth);
    connect(this->previewWidget, SIGNAL(paintRequested(QPrinter*)), this, SLOT(faxToPrinter(QPrinter *)));

    this->ui->verticalLayout->insertWidget(0, this->previewWidget, 1);
    this->ui->prevPageButton->setEnabled(false);
    this->ui->nextPageButton->setEnabled(this->pageBuffers.count() > 1);

    this->priceLookupTimer = new QTimer(this);
    this->priceLookupTimer->setInterval(500);
    this->priceLookupTimer->setSingleShot(true);
    connect(this->priceLookupTimer, SIGNAL(timeout()),
            this, SLOT(toNoChanged()));
}

SendFaxDialog::~SendFaxDialog()
{
    delete ui;
}

void SendFaxDialog::faxSent(bool success)
{
    if(success)
    {
        QFile(this->jobFile).remove();
        api->getInfo(true);
        accept();
        QMessageBox::information(this, tr("Send fax"),
                                 tr("The fax has been sent successfully."));
    }
    else
    {
        QMessageBox::critical(this, tr("Send fax"),
                              tr("The fax could not be sent.\n\nPlease check your input, your credit balance and your internet connection and try again."));
        this->switchPage(1);
    }
}

void SendFaxDialog::sendFax()
{
    this->switchPage(0);

#ifdef Q_WS_WIN
    // create PDF file
    HPDF_Doc pdf;
    pdf = HPDF_New(NULL, NULL);
    if(!pdf)
    {
        QMessageBox::warning(this, tr("Error"), tr("Failed to create fax document."));
        return;
    }

    HPDF_SetInfoAttr(pdf, HPDF_INFO_CREATOR, QString("BMToolbox/%1").arg(tbApp->applicationVersion()).toAscii());

    foreach(QByteArray buffer, pageBuffers)
    {
        HPDF_Image image = HPDF_Image_LoadRawImageFromMem(pdf->mmgr,
                                                          (const HPDF_BYTE *)buffer.data(),
                                                          pdf->xref,
                                                          bytesPerLine*8,
                                                          buffer.size()/bytesPerLine,
                                                          HPDF_CS_DEVICE_GRAY,
                                                          1);
        image->filter = HPDF_STREAM_FILTER_FLATE_DECODE;

        HPDF_Page page = HPDF_AddPage(pdf);
        HPDF_Page_SetSize(page, HPDF_PAGE_SIZE_A4, HPDF_PAGE_PORTRAIT);
        HPDF_Page_DrawImage(page, image, 0, 0, HPDF_Page_GetWidth(page), HPDF_Page_GetHeight(page));
    }

    if(HPDF_SaveToStream(pdf) == HPDF_OK)
    {
        unsigned int pdfSize = HPDF_GetStreamSize(pdf), outSize;
        outSize = pdfSize;

        char *pdfBuffer = new char[ pdfSize ];
        HPDF_ReadFromStream(pdf, (HPDF_BYTE *)pdfBuffer, &outSize);

        if(outSize == pdfSize)
        {
            QByteArray *pdfData = new QByteArray(pdfBuffer, pdfSize);

            this->api->sendFax(this->ui->fromName->text(),
                               this->ui->fromNo->text(),
                               this->ui->toNo->text(),
                               *pdfData);

            delete pdfData;
        }
        else
        {
            QMessageBox::warning(this, tr("Error"), tr("Failed to read fax document stream."));
        }

        delete[] pdfBuffer;
    }
    else
    {
        QMessageBox::warning(this, tr("Error"), tr("Failed to create fax document stream."));
    }

    HPDF_Free(pdf);
#endif
}

void SendFaxDialog::reject()
{
    int res = QMessageBox::question(this,
                                    tr("Cancel"),
                                    tr("Do you really want to cancel?\n\nThe current fax will be discarded."),
                                    QMessageBox::Yes|QMessageBox::No,
                                    QMessageBox::No);
    if(res == QMessageBox::No)
        return;

    QFile(this->jobFile).remove();
    QDialog::reject();
}

void SendFaxDialog::infoRetrieved(b1gMailAPI::Info &info)
{
    this->ui->fromName->setText(info.faxDefaultFromName);
    this->ui->fromName->setEnabled(info.faxAllowOwnName);

    this->ui->fromNo->setText(info.faxDefaultFromNo);
    this->ui->fromNo->setEnabled(info.faxAllowOwnNo);

    this->switchPage(1);
}

void SendFaxDialog::faxPriceRetrieved(int price)
{
    this->ui->sendButton->setEnabled(price >= 0);

    if(price >= 0)
        this->ui->priceLabel->setText(tr("%1 credit(s)", "", price).arg(price));
    else
        this->ui->priceLabel->setText("-");
}

void SendFaxDialog::switchPage(int no)
{
    this->ui->stackedWidget->setCurrentIndex(no);
}

void SendFaxDialog::show()
{
    QDialog::show();
    raise();
}

void SendFaxDialog::toAddressbook()
{
    Addressbook *book = new Addressbook(this, this->api, Fax);
    if(book->exec() == QDialog::Accepted)
    {
        this->ui->toNo->setText(book->selectedNo);
        toNoChanged();
    }
    delete book;
}

void SendFaxDialog::toNoChanged()
{
    QString toNo = this->ui->toNo->text();
    api->getFaxPrice(toNo, this->pageBuffers.count());
}

void SendFaxDialog::toNoEdited()
{
    this->ui->sendButton->setEnabled(false);
    this->ui->priceLabel->setText("-");

    this->priceLookupTimer->stop();
    this->priceLookupTimer->start();
}

void SendFaxDialog::prevPage()
{
    int page = this->previewWidget->currentPage();

    if(page <= 1)
        return;

    page--;

    this->previewWidget->setCurrentPage(page);
    this->ui->prevPageButton->setEnabled(page > 1);
    this->ui->nextPageButton->setEnabled(page < this->previewWidget->pageCount());
}

void SendFaxDialog::nextPage()
{
    int page = this->previewWidget->currentPage();

    if(page >= this->previewWidget->pageCount())
        return;

    page++;

    this->previewWidget->setCurrentPage(page);
    this->ui->prevPageButton->setEnabled(page > 1);
    this->ui->nextPageButton->setEnabled(page < this->previewWidget->pageCount());
}

void SendFaxDialog::faxToPrinter(QPrinter *printer)
{
    printer->setPaperSize(QPrinter::A4);
    printer->setPageMargins(0, 0, 0, 0, QPrinter::Millimeter);

    QPainter painter;
    painter.begin(printer);

    int i = 0;
    foreach(QByteArray buffer, pageBuffers)
    {
        if(i++ > 0)
            printer->newPage();

        QImage img((const uchar *)buffer.data(), bytesPerLine*8, buffer.size()/bytesPerLine, bytesPerLine, QImage::Format_Mono);

        if(sender() == this->previewWidget)
        {
            painter.drawImage(0, 0,
                              img.scaled(painter.device()->width(), painter.device()->height(),
                                         Qt::IgnoreAspectRatio, Qt::SmoothTransformation));
        }
        else
        {
            painter.drawImage(0, 0, img);
        }
    }

    painter.end();
}

bool SendFaxDialog::readJobFile()
{
    QFile jobFile(this->jobFile);

    pageBuffers.clear();

    if(!jobFile.open(QIODevice::ReadOnly))
    {
        //QMessageBox::warning(this, "Err", "cannot open file");
        return(false);
    }

    if(jobFile.size() < 32)
    {
        //QMessageBox::warning(this, "Err", "invalid size");
        return(false);
    }

    // process first line
    QByteArray firstLine = jobFile.readLine();
    if(firstLine.left(18) != "BMFaxFile:JobName="
            || firstLine.right(2) != ";\n")
    {
        //QMessageBox::warning(this, "Err", "invalid preamble");
        return(false);
    }
    if(firstLine.length() == 18+2)
    {
        jobName = tr("Unnamed document");
    }
    else
    {
        jobName = QString::fromUtf16((const ushort *)firstLine.mid(18, firstLine.length()-18-2).data(),
                                     (firstLine.length()-18-2)/2);
    }

    // read commands
    QByteArray pageData;
    while(!jobFile.atEnd())
    {
        QByteArray lineData = jobFile.readLine();
        QString line = QString::fromAscii(lineData);

        int cPos = line.indexOf(':');
        if(cPos == -1)
            cPos = line.indexOf(';');
        if(cPos == -1)
            continue;

        // parse cmd
        QString command = line.left(cPos);
        QStringList paramList = line.mid(cPos+1).split(';');
        QMap<QString, QString> params;
        foreach(QString param, paramList)
        {
            int eqPos = param.indexOf('=');
            if(eqPos == -1)
                params[param] = "1";
            else
                params[param.left(eqPos)] = param.mid(eqPos+1);
        }

        // process command
        if(command == "SP")
        {
            pageData.clear();
        }
        else if(command == "BD")
        {
            bytesPerLine = params["L"].toInt();

            QByteArray blockData = jobFile.read(params["N"].toInt());
            for(int i=0; i<blockData.size(); i++)
                blockData[i] = ~blockData[i];
            pageData.append(blockData);
        }
        else if(command == "EP")
        {
            if(pageData.size() % bytesPerLine != 0)
            {
                //QMessageBox::warning(this, "Err", "invalid data size");
                return(false);
            }

            pageBuffers.append(pageData);
        }
    }

    jobFile.close();

    return(true);
}
