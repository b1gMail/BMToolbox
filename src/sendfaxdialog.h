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

#ifndef SENDFAXDIALOG_H
#define SENDFAXDIALOG_H

#include <QDialog>
#include <QString>
#include <QPrintPreviewWidget>
#include <QPrinter>
#include <QImage>
#include <QList>
#include <QByteArray>
#include <QTimer>

#include "clientapi.h"

namespace Ui {
class SendFaxDialog;
}

class SendFaxDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SendFaxDialog(QWidget *parent, b1gMailAPI::ClientAPI *api, const QString &jobFile);
    ~SendFaxDialog();

public slots:
    void faxToPrinter(QPrinter *);
    void prevPage();
    void nextPage();
    void toAddressbook();
    void toNoChanged();
    void toNoEdited();
    void infoRetrieved(b1gMailAPI::Info&);
    void faxPriceRetrieved(int);
    void sendFax();
    void faxSent(bool success);
    virtual void show();
    virtual void reject();

private:
    void switchPage(int no);
    bool readJobFile();

private:
    Ui::SendFaxDialog *ui;
    b1gMailAPI::ClientAPI *api;
    QString jobFile, jobName;
    QPrintPreviewWidget *previewWidget;
    QList<QByteArray> pageBuffers;
    unsigned int bytesPerLine;
    QTimer *priceLookupTimer;
};

#endif // SENDFAXDIALOG_H
