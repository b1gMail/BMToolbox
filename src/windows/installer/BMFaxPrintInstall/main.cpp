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

#undef UNICODE
#include <Windows.h>
#include <SetupAPI.h>
#include <WinSpool.h>
#include <stdio.h>

TCHAR appDir[MAX_PATH], driverDir[MAX_PATH], sysDir[MAX_PATH];
typedef BOOL (__stdcall *pIsWow64)(HANDLE hProcess, PBOOL Wow64Process);
typedef BOOL (__stdcall *pWow64DisableWow64FsRedirection)(PVOID *oldValue);

bool Is64Bit()
{
	BOOL is64Bit = false;

	HMODULE hKernel32 = LoadLibrary("kernel32.dll");
	if(hKernel32 != NULL)
	{
		pIsWow64 isWow64 = (pIsWow64)GetProcAddress(hKernel32, "IsWow64Process");

		if(isWow64 != NULL)
		{
			isWow64(GetCurrentProcess(), &is64Bit);
		}

		FreeLibrary(hKernel32);
	}

	return(is64Bit != false);
}

bool DisableWow64Redirect()
{
	PVOID oldValue;

	HMODULE hKernel32 = LoadLibrary("kernel32.dll");
	if(hKernel32 != NULL)
	{
		pWow64DisableWow64FsRedirection disFunc = (pWow64DisableWow64FsRedirection)GetProcAddress(hKernel32, "Wow64DisableWow64FsRedirection");

		if(disFunc != NULL)
		{
			return(disFunc(&oldValue) != 0);
		}

		FreeLibrary(hKernel32);
	}

	return(true);
}

bool FileExists(TCHAR *fileName)
{
	return(GetFileAttributes(fileName) != INVALID_FILE_ATTRIBUTES);
}

bool InstallPortMonitor()
{
	TCHAR destFileName[MAX_PATH*2], srcFileName[MAX_PATH*2];
	strcpy(destFileName, sysDir);
	strcat(destFileName, Is64Bit() ? "bmfaxmon64.dll" : "bmfaxmon32.dll");
	strcpy(srcFileName, appDir);
	strcat(srcFileName, Is64Bit() ? "bmfaxmon64.dll" : "bmfaxmon32.dll");

	if(!FileExists(destFileName) && !CopyFile(srcFileName, destFileName, true))
	{
		MessageBox(NULL, "Failed to install port monitor DLL.", "Error", MB_ICONERROR|MB_OK);
		return(false);
	}

	MONITOR_INFO_2 mon;
	mon.pName			= "BMToolbox Fax Port Monitor";
	mon.pDLLName		= Is64Bit() ? "bmfaxmon64.dll" : "bmfaxmon32.dll";
	mon.pEnvironment	= Is64Bit() ? "Windows x64" : "Windows NT x86";

	AddMonitor(NULL, 2, (LPBYTE)&mon);

	return(true);
}

void RemovePortMonitorReg(TCHAR *portName)
{
	HKEY hKey;

	if(RegCreateKeyEx(HKEY_LOCAL_MACHINE, "SYSTEM\\CurrentControlSet\\Control\\Print\\Monitors\\BMToolbox Fax Port Monitor\\Ports",
		0, NULL, 0, KEY_ALL_ACCESS, NULL, &hKey, NULL) == ERROR_SUCCESS)
	{
		RegDeleteKey(hKey, portName);
		RegCloseKey(hKey);
	}
}

void SetupPortMonitorReg(TCHAR *portName, TCHAR *userName, TCHAR *faxDir)
{
	HKEY hKey, hKeySub;

	if(RegCreateKeyEx(HKEY_LOCAL_MACHINE, "SYSTEM\\CurrentControlSet\\Control\\Print\\Monitors\\BMToolbox Fax Port Monitor\\Ports",
		0, NULL, 0, KEY_ALL_ACCESS, NULL, &hKey, NULL) == ERROR_SUCCESS)
	{
		if(RegCreateKeyEx(hKey, portName, 0, NULL, 0, KEY_ALL_ACCESS, NULL, &hKeySub, NULL) == ERROR_SUCCESS)
		{
			RegSetValueEx(hKeySub, "User", 0, REG_SZ, (const BYTE *)userName, strlen(userName));
			RegSetValueEx(hKeySub, "Dir", 0, REG_SZ, (const BYTE *)faxDir, strlen(faxDir));

			RegCloseKey(hKeySub);
		}

		RegCloseKey(hKey);
	}
}

int WINAPI WinMain(HINSTANCE hInstance,
	HINSTANCE hPrevInstance,
	LPSTR lpCmdLine,
	int nCmdShow)
{
	DWORD cbNeeded;

	// get app dir
	GetModuleFileName(GetModuleHandle(NULL), appDir, sizeof(appDir));
	TCHAR *ptr = appDir+strlen(appDir);
	while(*ptr != '\\' && ptr > appDir)
		*ptr-- = '\0';

	// get win sys dir
	GetSystemDirectory(sysDir, sizeof(sysDir)-1);
	strcat(sysDir, "\\");

	// get printer driver dir
	GetPrinterDriverDirectory(NULL, NULL, 1, (LPBYTE)driverDir, sizeof(driverDir)-1, &cbNeeded);
	strcat(driverDir, "\\");

	// disable file system redirection
	DisableWow64Redirect();

	// uninstall
	if(__argc == 4 && strcmp(__argv[1], "-uninstall") == 0
		&& strlen(__argv[2]) < 256 && strlen(__argv[3]) < 256)
	{
		TCHAR cmdLine[1024];

		sprintf(cmdLine, "printui.dll,PrintUIEntry /dl /n \"%s\"",
			__argv[2],
			appDir);

		ShellExecute(NULL, "open", "rundll32", cmdLine, NULL, SW_SHOWNORMAL);

		RemovePortMonitorReg(__argv[3]);

		return(0);
	}

	// install
	else if(__argc == 6 && strcmp(__argv[1], "-install") == 0
		&& strlen(__argv[2]) < 256 && strlen(__argv[3]) < 256
		&& strlen(__argv[4]) < 256 && strlen(__argv[5]) < 256)
	{
		InstallPortMonitor();
		SetupPortMonitorReg(__argv[3], __argv[4], __argv[5]);

		TCHAR cmdLine[1024];

		sprintf(cmdLine, "printui.dll,PrintUIEntry /if /b \"%s\" /r \"%s\" /f \"%sbmfaxprint.inf\" /m \"BMToolbox Fax Printer\"",
			__argv[2],
			__argv[3],
			appDir);

		ShellExecute(NULL, "open", "rundll32", cmdLine, NULL, SW_SHOWNORMAL);
		return(0);
	}

	return(1);
}
