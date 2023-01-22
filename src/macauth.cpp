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

#include "macauth.h"
#include <signal.h>
#include <QStringList>

MacAuth::MacAuth()
{
    this->auth = NULL;
}

MacAuth::~MacAuth()
{
    if(this->auth != NULL)
    {
        AuthorizationFree(this->auth, 0);
        this->auth = NULL;
    }
}

bool MacAuth::execAndWait(const QString &program, const QStringList &args)
{
    bool result = false;
    sig_t oldSigHandler = signal(SIGCHLD, SIG_DFL);
    char **argList = new char *[args.size()+1];

    // allocate & copy arguments
    for(int i=0; i<args.size(); i++)
    {
        std::string arg = args.at(i).toStdString();

        argList[i] = new char[ arg.length()+1 ];
        strncpy(argList[i], arg.c_str(), arg.length());
        argList[i][arg.length()] = NULL;
    }
    argList[args.size()] = NULL;

    if(AuthorizationExecuteWithPrivileges(this->auth,
                                          program.toStdString().c_str(),
                                          kAuthorizationFlagDefaults,
                                          argList,
                                          NULL) == errAuthorizationSuccess)
    {
        int status;
        pid_t pid = wait(&status);
        if(pid == -1 || !WIFEXITED(status) || WEXITSTATUS(status)!=0)
            result = false;
        else
            result = true;
    }

    signal(SIGCHLD, oldSigHandler);

    // free memory
    for(int i=0; i<args.size(); i++)
    {
        delete[] argList[i];
    }
    delete[] argList;

    return(result);
}

bool MacAuth::elevate()
{
    if(this->auth != NULL)
    {
        AuthorizationFree(this->auth, 0);
        this->auth = NULL;
    }

    OSStatus authStatus = errAuthorizationDenied;

    while(authStatus == errAuthorizationDenied)
    {
        authStatus = AuthorizationCreate(NULL,
                                         kAuthorizationEmptyEnvironment,
                                         kAuthorizationFlagDefaults,
                                         &this->auth);
    }

    return(authStatus == errAuthorizationSuccess);
}
