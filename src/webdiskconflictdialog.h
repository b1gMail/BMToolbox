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

#ifndef WEBDISKCONFLICTDIALOG_H
#define WEBDISKCONFLICTDIALOG_H

#include <QDialog>

namespace Ui {
class WebdiskConflictDialog;
}

class WebdiskConflictDialog : public QDialog
{
    Q_OBJECT

public:
    explicit WebdiskConflictDialog(QWidget *parent,
                                   const QString &localFileText,
                                   const QString &remoteFileText);
    ~WebdiskConflictDialog();

public slots:
    void acceptLocal();
    void acceptRemote();
    virtual void show();

public:
    int master;
    bool remember;

private:
    Ui::WebdiskConflictDialog *ui;
};

#endif // WEBDISKCONFLICTDIALOG_H
