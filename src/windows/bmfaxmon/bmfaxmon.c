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

#include <windows.h>
#include <winspool.h>
#include <winsplp.h>
#include <winsock2.h>
#include <wchar.h>
#include <stddef.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#ifdef DEBUG
void putLog(LPCTSTR buf)
{
#ifdef UNICODE
    int count;
    CHAR cbuf[1024];
    int UsedDefaultChar;
#endif
	DWORD cbWritten;
	HANDLE hfile;
	if (buf == NULL)
	buf = TEXT("(null)");

	if ((hfile = CreateFileA("c:\\temp\\bmfaxmon.log", GENERIC_WRITE,
	0 /* no file sharing */, NULL, OPEN_ALWAYS, 0, NULL))
		!= INVALID_HANDLE_VALUE)
	{
		SetFilePointer(hfile, 0, NULL, FILE_END);
#ifdef UNICODE
		while (lstrlen(buf))
		{
			count = min(lstrlen(buf), sizeof(cbuf));
			WideCharToMultiByte(CP_ACP, 0, buf, count,
				cbuf, sizeof(cbuf), NULL, &UsedDefaultChar);
			buf += count;
			WriteFile(hfile, cbuf, count, &cbWritten, NULL);
		}
#else
		WriteFile(hfile, buf, lstrlen(buf), &cbWritten, NULL);
#endif
		CloseHandle(hfile);
	}
}
#else
#define putLog(x)
#endif

#define STRBUFSIZE				255
#define KEY_PORTS				TEXT("Ports")
#define MONINIT()				((MONITORINIT *)hMonitor)
#define HSPOOLER()				MONINIT()->hSpooler
#define REG()					MONINIT()->pMonitorReg
#define REGROOT()				MONINIT()->hckRegistryRoot

typedef struct
{
	HANDLE hMonitor;
	HANDLE hFile;
	HANDLE hPrinter;
	DWORD dwJobId;
	TCHAR *portName;
	TCHAR *faxDir;
}
BMFaxPortHandle;

BOOL OpenFaxDestFile(BMFaxPortHandle *handle)
{
	TCHAR fileName[MAX_PATH+32];

	// build file name
	wsprintf(fileName, TEXT("%s\\%08x.fax.active"), handle->faxDir, handle->dwJobId);

	// open
	handle->hFile = CreateFile(fileName, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
	if(handle->hFile == INVALID_HANDLE_VALUE)
	{
		putLog(TEXT("Failed to create file "));
		putLog(fileName);
		putLog(TEXT("\r\n"));
		handle->hFile = NULL;
		return(FALSE);
	}

	return(TRUE);
}

BOOL CloseFaxDestFile(BMFaxPortHandle *handle)
{
	TCHAR fileNameSrc[MAX_PATH+32], fileNameDest[MAX_PATH+16];
	TCHAR hexChars[] = TEXT("0123456789ABCDEF");

	// close file
	CloseHandle(handle->hFile);
	handle->hFile = NULL;

	// build src/dest file name
	wsprintf(fileNameSrc, TEXT("%s\\%08x.fax.active"), handle->faxDir, handle->dwJobId);
	wsprintf(fileNameDest, TEXT("%s\\%08x.fax"), handle->faxDir, handle->dwJobId);

	// rename
	return(MoveFile(fileNameSrc, fileNameDest));
}

BMFaxPortHandle *CreateBMFaxPortHandle(HANDLE hMonitor, TCHAR *portName, TCHAR *faxDir)
{
	BMFaxPortHandle *res = (BMFaxPortHandle *)malloc(sizeof(BMFaxPortHandle));
	if(res == NULL)
		return(NULL);

	res->hMonitor			= hMonitor;
	res->hFile				= NULL;
	res->hPrinter 			= NULL;

	res->portName			= (TCHAR *)malloc((lstrlen(portName)+1) * sizeof(TCHAR));
	if(res->portName == NULL)
	{
		free(res);
		return(NULL);
	}

	res->faxDir				= (TCHAR *)malloc((lstrlen(faxDir)+1) * sizeof(TCHAR));
	if(res->faxDir == NULL)
	{
		free(res->portName);
		free(res);
		return(NULL);
	}

	lstrcpy(res->portName,	portName);
	lstrcpy(res->faxDir,	faxDir);

	return(res);
}

void DestroyBMFaxPortHandle(BMFaxPortHandle *handle)
{
	free(handle->portName);
	free(handle->faxDir);
	free(handle);
}

BOOL BMFaxMonEnumPorts(
    __in                        HANDLE  hMonitor,
    __in_opt                    LPWSTR  pName,
                                DWORD   Level,
    __out_bcount_opt(cbBuf)     LPBYTE  pPorts,
                                DWORD   cbBuf,
    __out                       LPDWORD pcbNeeded,
    __out                       LPDWORD pcReturned
    )
{
	HKEY hKey, hSubKey;
	TCHAR monitorName[] = TEXT("BMFaxMonitor"), portName[STRBUFSIZE], portDesc[STRBUFSIZE];
	DWORD dwNeeded = 0, cbData, i;
	PORT_INFO_1 *pi1;
	PORT_INFO_2 *pi2;
	LPTSTR str;
	LONG res;

	putLog(TEXT("BMFaxMonEnumPorts\r\n"));

	*pcbNeeded = 0;
	*pcReturned = 0;

	//
	// check input variables
	//
	if(hMonitor == NULL)
	{
		putLog(TEXT("hMonitor == NULL\r\n"));
		return(FALSE);
	}
	if(Level != 1 && Level != 2)
	{
		putLog(TEXT("Level unsupported\r\n"));
		SetLastError(ERROR_INVALID_LEVEL);
		return(FALSE);
	}

	//
	// reg key existant?
	//
	if(REG()->fpOpenKey(REGROOT(), KEY_PORTS, KEY_READ, &hKey, HSPOOLER()) != ERROR_SUCCESS)
		return(TRUE);

	//
	// determine size
	//
	cbData = sizeof(portName);
	res = REG()->fpEnumKey(hKey, i = 0, portName, &cbData, NULL, HSPOOLER());
	while(res == ERROR_SUCCESS)
	{
		dwNeeded += (lstrlen(portName)+1) * sizeof(TCHAR);

		if(Level == 1)
		{
			dwNeeded += sizeof(PORT_INFO_1);
		}
		else if(Level == 2)
		{
			dwNeeded += sizeof(PORT_INFO_2);
			dwNeeded += (lstrlen(monitorName)+1) * sizeof(TCHAR);

			// get user name as description
			res = REG()->fpOpenKey(hKey, portName, KEY_READ, &hSubKey, HSPOOLER());
			if(res == ERROR_SUCCESS)
			{
				cbData = sizeof(portDesc);
				res = REG()->fpQueryValue(hSubKey, TEXT("User"), NULL, (LPBYTE)portDesc, &cbData, HSPOOLER());
				if(res == ERROR_SUCCESS)
					dwNeeded += (lstrlen(portDesc)+1) * sizeof(TCHAR);
				REG()->fpCloseKey(hSubKey, HSPOOLER());
			}
			if(res != ERROR_SUCCESS)
				dwNeeded += sizeof(TCHAR);
		}

		cbData = sizeof(portName);
		res = REG()->fpEnumKey(hKey, ++i, portName, &cbData, NULL, HSPOOLER());
	}
	*pcbNeeded = dwNeeded;

	//
	// buffer large enough?
	//
	if(pPorts == NULL || dwNeeded > cbBuf)
	{
		putLog(TEXT("cbBuf < dwNeeded\r\n"));
		REG()->fpCloseKey(hKey, HSPOOLER());
		SetLastError(ERROR_INSUFFICIENT_BUFFER);
		return(FALSE);
	}

	//
	// copy data
	//

	str = (LPTSTR)(pPorts + cbBuf);
	pi1 = (PORT_INFO_1 *)pPorts;
	pi2 = (PORT_INFO_2 *)pPorts;

	// enum
	cbData = sizeof(portName);
	res = REG()->fpEnumKey(hKey, i = 0, portName, &cbData, NULL, HSPOOLER());
	while(res == ERROR_SUCCESS)
	{
		if(Level == 1)
		{
			// copy port name
			str -= lstrlen(portName) + 1;
			lstrcpy(str, portName);
			pi1[i].pName = str;
		}
		else if(Level == 2)
		{
			pi2[i].fPortType = PORT_TYPE_WRITE;
			pi2[i].Reserved = 0;

			// copy port name
			str -= lstrlen(portName) + 1;
			lstrcpy(str, portName);
			pi2[i].pPortName = str;

			// copy monitor name
			str -= lstrlen(monitorName)+1;
			lstrcpy(str, monitorName);
			pi2[i].pMonitorName = str;

			// get user name as description
			res = REG()->fpOpenKey(hKey, portName, KEY_READ, &hSubKey, HSPOOLER());
			if(res == ERROR_SUCCESS)
			{
				cbData = sizeof(portDesc);
				res = REG()->fpQueryValue(hSubKey, TEXT("User"), NULL, (LPBYTE)portDesc, &cbData, HSPOOLER());
				REG()->fpCloseKey(hSubKey, HSPOOLER());
			}
			if(res != ERROR_SUCCESS)
				lstrcpy(portDesc, TEXT(""));

			// copy port desc
			str -= lstrlen(portDesc) + 1;
			lstrcpy(str, portDesc);
			pi2[i].pDescription = str;
		}

		cbData = sizeof(portName);
		res = REG()->fpEnumKey(hKey, ++i, portName, &cbData, NULL, HSPOOLER());
	}

	*pcReturned = i;
	REG()->fpCloseKey(hKey, HSPOOLER());

	return(TRUE);
}

BOOL BMFaxMonOpenPort(
    __in    HANDLE  hMonitor,
    __in    LPWSTR  pName,
    __out   PHANDLE pHandle
    )
{
	TCHAR faxDir[MAX_PATH], buf[STRBUFSIZE];
	HKEY hKey;
	DWORD cbData;
	BMFaxPortHandle *handle;

	putLog(TEXT("BMFaxMonOpenPort\r\n"));

	// look up fax dir
	lstrcpy(buf, KEY_PORTS);
	lstrcat(buf, TEXT("\\"));
	lstrcat(buf, pName);
	if(REG()->fpOpenKey(REGROOT(), buf, KEY_READ, &hKey, HSPOOLER()) != ERROR_SUCCESS)
	{
		putLog(TEXT("Port not found: "));
		putLog(pName);
		putLog(TEXT("\r\n"));
		return(FALSE);
	}
	cbData = sizeof(faxDir);
	if(REG()->fpQueryValue(hKey, TEXT("Dir"), NULL, (LPBYTE)faxDir, &cbData, HSPOOLER()) != ERROR_SUCCESS)
	{
		putLog(TEXT("Port has no Dir registry entry\r\n"));
		REG()->fpCloseKey(hKey, HSPOOLER());
		return(FALSE);
	}
	REG()->fpCloseKey(hKey, HSPOOLER());

	// create handle
	handle = CreateBMFaxPortHandle(hMonitor, pName, faxDir);
	if(handle != NULL)
	{
		*pHandle = (PHANDLE)handle;
		return(TRUE);
	}

	return(FALSE);
}

BOOL BMFaxMonClosePort(
    __in    HANDLE  hPort
    )
{
	putLog(TEXT("BMFaxMonClosePort\r\n"));

	DestroyBMFaxPortHandle((BMFaxPortHandle *)hPort);

	return(TRUE);
}

BOOL BMFaxMonStartDocPort(
    __in    HANDLE  hPort,
    __in    LPWSTR  pPrinterName,
            DWORD   JobId,
            DWORD   Level,
    __in    LPBYTE  pDocInfo)
{
	BMFaxPortHandle *handle = (BMFaxPortHandle *)hPort;
	DWORD dwBytesWritten = 0;
	TCHAR *jobName = NULL;
	const char strPreamble[] = "BMFaxFile:JobName=", strPreamble2[] = ";\n";

	putLog(TEXT("BMFaxMonStartDocPort\r\n"));

	if(handle->hFile != NULL)
	{
		putLog(TEXT("Handle has already a document open\r\n"));
		return(FALSE);
	}

	if(Level == 1)
		jobName = ((DOC_INFO_1 *)pDocInfo)->pDocName;
	else if(Level == 2)
		jobName = ((DOC_INFO_2 *)pDocInfo)->pDocName;

	if(OpenPrinter(pPrinterName, &handle->hPrinter, NULL))
	{
		handle->dwJobId = JobId;

		if(OpenFaxDestFile(handle))
		{
			WriteFile(handle->hFile, (LPBYTE)strPreamble, sizeof(strPreamble)-1, &dwBytesWritten, NULL);
			if(jobName != NULL)
				WriteFile(handle->hFile, (LPBYTE)jobName, lstrlen(jobName)*sizeof(TCHAR), &dwBytesWritten, NULL);
			WriteFile(handle->hFile, (LPBYTE)strPreamble2, sizeof(strPreamble2)-1, &dwBytesWritten, NULL);

			return(TRUE);
		}

		ClosePrinter(handle->hPrinter);
	}

	return(FALSE);
}

BOOL BMFaxMonWritePort(
            __in                HANDLE  hPort,
            __in_bcount(cbBuf)  LPBYTE  pBuffer,
            DWORD   			cbBuf,
            __out               LPDWORD pcbWritten)
{
	BMFaxPortHandle *handle = (BMFaxPortHandle *)hPort;

	putLog(TEXT("BMFaxMonWritePort\r\n"));

	if(handle->hFile == NULL)
	{
		putLog(TEXT("No document open!\r\n"));
		return(FALSE);
	}

	return(WriteFile(handle->hFile, pBuffer, cbBuf, pcbWritten, NULL));
}

BOOL BMFaxMonReadPort(
    __in                HANDLE      hPort,
    __out_bcount(cbBuf) LPBYTE      pBuffer,
                        DWORD       cbBuf,
    __out               LPDWORD     pcbRead)
{
	putLog(TEXT("BMFaxMonReadPort\r\n"));

	return(FALSE);
}

BOOL BMFaxMonEndDocPort(
    __in    HANDLE   hPort
    )
{
	BMFaxPortHandle *handle = (BMFaxPortHandle *)hPort;

	putLog(TEXT("BMFaxMonEndDocPort\r\n"));

	if(handle->hFile != NULL)
	{
		CloseFaxDestFile(handle);
	}

	SetJob(handle->hPrinter, handle->dwJobId, 0, NULL, JOB_CONTROL_SENT_TO_PRINTER);

	if(handle->hPrinter != NULL)
	{
		ClosePrinter(handle->hPrinter);
		handle->hPrinter = NULL;
	}

	return(TRUE);
}

void BMFaxMonShutdown(__in HANDLE hMonitor)
{
	putLog(TEXT("BMFaxMonShutdown\r\n"));
}

MONITOR2 Monitor2 =
{
    sizeof(MONITOR2),
    BMFaxMonEnumPorts,
    BMFaxMonOpenPort,
    NULL,
    BMFaxMonStartDocPort,
    BMFaxMonWritePort,
    BMFaxMonReadPort,
    BMFaxMonEndDocPort,
    BMFaxMonClosePort,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
	NULL,
    BMFaxMonShutdown
};

LPMONITOR2 InitializePrintMonitor2(
    __in    PMONITORINIT pMonitorInit,
    __out   PHANDLE phMonitor
    )
{
	putLog(TEXT("InitializePrintMonitor2\r\n"));
	*phMonitor = (PHANDLE)pMonitorInit;
    return &Monitor2;
}

BOOL DllMain(HINSTANCE hModule, DWORD dwReason, LPVOID lpRes)
{
	putLog(TEXT("DllMain\r\n"));

    switch(dwReason)
    {
    case DLL_PROCESS_ATTACH:
        DisableThreadLibraryCalls(hModule);
        return TRUE;

    case DLL_PROCESS_DETACH:
        return TRUE;
    }

    return TRUE;
}
