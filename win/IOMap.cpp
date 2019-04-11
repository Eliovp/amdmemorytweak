/*
        AMD Memory Tweak by
        Elio VP                 <elio@eliovp.be>
        A. Solodovnikov
        Copyright (c) 2019 Eliovp, BVBA. All rights reserved.
        Permission is hereby granted, free of charge, to any person obtaining a copy
        of this software and associated documentation files (the "Software"), to deal
        in the Software without restriction, including without limitation the rights
        to use, copy, modify, merge, publish, distribute and/or sublicense
        copies of the Software, and to permit persons to whom the Software is
        furnished to do so, subject to the following conditions:
        The above copyright notice and this permission notice shall be included in
        all copies or substantial portions of the Software.
        THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
        IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
        FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
        AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
        LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
        OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
        THE SOFTWARE.
*/

#define WIN32_LEAN_AND_MEAN
#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>
#include <stdlib.h>
#include <stdio.h>
#include "IOMap.h"

typedef struct {
	WORD adapterIndex;
	WORD field_2;
	WORD field_4;
	char pad1[2];
	DWORD field_8;
	char pad2[8];
	WORD field_14;
	WORD field_16;
	WORD field_18; // 0 Byte, 1 Word, 2 Dword
	char pad3[6];
	DWORD mmioRegister;
	DWORD field_24;
	char pad4[8];
} Ioctl_83002138_In;
C_ASSERT(sizeof(Ioctl_83002138_In) == 0x30);

typedef struct {
	char pad1[4];
	DWORD field_4;
	char pad2[4];
} Ioctl_83002138_Out;
C_ASSERT(sizeof(Ioctl_83002138_Out) == 0xC);

SC_HANDLE hManager = NULL;
SC_HANDLE hService = NULL;
HANDLE g_hDevice = INVALID_HANDLE_VALUE;

// Unloading is being a b*tch, revert to EIO to load/unload driver

//void LoadDriver()
//{
//	hManager = OpenSCManagerA(NULL, NULL, SC_MANAGER_CONNECT | SC_MANAGER_CREATE_SERVICE);
//	if (!hManager)
//	{
//		printf("OpenSCManager() failed with error %u\n", GetLastError());
//		exit(EXIT_FAILURE);
//	}
//
//	hService = OpenServiceA(hManager, "IOMap", SERVICE_QUERY_STATUS | SERVICE_START | SERVICE_STOP | DELETE);
//	if (!hService)
//	{
//		DWORD errorCode = GetLastError();
//		if (errorCode == ERROR_SERVICE_DOES_NOT_EXIST)
//		{
//			char path[MAX_PATH];
//			GetModuleFileNameA(NULL, path, sizeof(path));
//			*strrchr(path, '\\') = '\0';
//			bool is64bit;
//#ifdef _WIN64
//			is64bit = true;
//#else
//			BOOL isWow64;
//			if (!IsWow64Process(GetCurrentProcess(), &isWow64))
//			{
//				printf("IsWow64Process() failed with error %u\n", GetLastError());
//				exit(EXIT_FAILURE);
//			}
//			is64bit = (isWow64 == TRUE);
//#endif
//			strcat(path, (is64bit ? "\\IOMap64.sys" : "\\IOMap.sys"));
//			hService = CreateServiceA(hManager, "IOMap", "IOMap", SERVICE_QUERY_STATUS | SERVICE_START | SERVICE_STOP | DELETE, SERVICE_KERNEL_DRIVER, SERVICE_DEMAND_START, SERVICE_ERROR_NORMAL, path, NULL, NULL, NULL, NULL, NULL);
//			if (!hService)
//			{
//				printf("CreateService() failed with error %u\n", GetLastError());
//				exit(EXIT_FAILURE);
//			}
//		}
//		else
//		{
//			printf("OpenService() failed with error %u\n", errorCode);
//			exit(EXIT_FAILURE);
//		}
//	}
//
//	SERVICE_STATUS status;
//	if (!QueryServiceStatus(hService, &status))
//	{
//		printf("QueryServiceStatus() failed with error %u\n", GetLastError());
//		exit(EXIT_FAILURE);
//	}
//
//	const char* StateNames[] = { "", "SERVICE_STOPPED", "SERVICE_START_PENDING", "SERVICE_STOP_PENDING", "SERVICE_RUNNING", "SERVICE_CONTINUE_PENDING", "SERVICE_PAUSE_PENDING", "SERVICE_PAUSED", };
////	printf("IOMap status: %s\n", StateNames[status.dwCurrentState]);
//
//	if (status.dwCurrentState == SERVICE_RUNNING)
//	{
//		// already running
//	}
//	else if (status.dwCurrentState == SERVICE_STOPPED)
//	{
//		if (!StartServiceA(hService, 0, NULL))
//		{
//			printf("StartService() failed with error %u\n", GetLastError());
//			exit(EXIT_FAILURE);
//		}
//		do
//		{
//			Sleep(1);
//		} while (QueryServiceStatus(hService, &status) && (status.dwCurrentState == SERVICE_START_PENDING));
////		printf("IOMap status: %s\n", StateNames[status.dwCurrentState]);
//	}
//	else
//	{
//		printf("Unexpected driver state: %s\n", StateNames[status.dwCurrentState]);
//		exit(EXIT_FAILURE);
//	}
//
//	g_hDevice = CreateFileA("\\\\.\\IOMap", GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
//	if (g_hDevice == INVALID_HANDLE_VALUE)
//	{
//		printf("Failed to open IOMap device with error %u\n", GetLastError());
//		exit(EXIT_FAILURE);
//	}
//}
//
//void __cdecl UnloadDriver(void)
//{
//	if (g_hDevice != INVALID_HANDLE_VALUE)
//	{
//		CloseHandle(g_hDevice);
//	}
//	if (hService)
//	{
//		SERVICE_STATUS status;
//		if (!ControlService(hService, SERVICE_CONTROL_STOP, &status))
//		{
//			printf("ControlService() failed with error %u\n", GetLastError());
//		}
//		else
//		{
//			do
//			{
//				Sleep(1);
//			} while (QueryServiceStatus(hService, &status) && (status.dwCurrentState == SERVICE_STOP_PENDING));
//		}
//		if (!DeleteService(hService))
//		{
//			DWORD errorCode = GetLastError();
//			if (errorCode != ERROR_SERVICE_MARKED_FOR_DELETE)
//			{
//				printf("DeleteService() failed with error %u\n", GetLastError());
//			}
//		}
//		CloseServiceHandle(hService);
//	}
//	if (hManager)
//	{
//		CloseServiceHandle(hManager);
//	}
//}

static WORD sub_100037C4(DWORD InBuffer)
{
	WORD OutBuffer;
	DWORD BytesReturned;

	DeviceIoControl(g_hDevice, 0x8300210C, &InBuffer, sizeof(InBuffer), &OutBuffer, sizeof(OutBuffer), &BytesReturned, NULL);
	return OutBuffer;
}
static BOOL sub_1000334B(VGAChip *chip)
{
	DWORD InBuffer[5];
	char OutBuffer[20];
	DWORD BytesReturned;

	memcpy(InBuffer, chip, 8);
	InBuffer[2] = 0xC0000;
	DeviceIoControl(g_hDevice, 0x83002104, InBuffer, sizeof(InBuffer), OutBuffer, sizeof(OutBuffer), &BytesReturned, NULL);
	return (sub_100037C4(0) == 0xAA55);
}
int EnumerateGPUs(DWORD vendorId, VGAChip *OutBuffer, DWORD OutBufferSize)
{
	DWORD BytesReturned;
	DeviceIoControl(g_hDevice, 0x830020D4, &vendorId, sizeof(vendorId), OutBuffer, OutBufferSize, &BytesReturned, NULL);
	int count = BytesReturned / sizeof(VGAChip);
	if (count > 0)
	{
		sub_1000334B(OutBuffer);
	}
	return count;
}

int GetVGAMMIOAddress(VGAChip *chip, WORD adapterIndex)
{
	Ioctl_83002138_In InBuffer;
	Ioctl_83002138_Out OutBuffer;
	DWORD BytesReturned;

	InBuffer.adapterIndex = adapterIndex;
	InBuffer.field_2 = chip->field_0;
	InBuffer.field_4 = chip->field_4;
	InBuffer.field_8 = chip->field_14;
	InBuffer.field_14 = 0;
	InBuffer.field_16 = 0;
	if (InBuffer.field_8)
	{
		if (DeviceIoControl(g_hDevice, 0x83002138, &InBuffer, sizeof(InBuffer), &OutBuffer, sizeof(OutBuffer), &BytesReturned, NULL))
		{
			return OutBuffer.field_4;
		}
	}
	return 0;
}

WORD GetWORDVGABIOSData(int offset, WORD adapterIndex)
{
	Ioctl_83002138_In InBuffer;
	Ioctl_83002138_Out OutBuffer;
	DWORD BytesReturned;

	InBuffer.adapterIndex = adapterIndex;
	InBuffer.field_14 = 1;
	InBuffer.field_16 = 1;
	InBuffer.field_18 = 1;
	InBuffer.mmioRegister = offset;
	DeviceIoControl(g_hDevice, 0x83002138, &InBuffer, sizeof(InBuffer), &OutBuffer, sizeof(OutBuffer), &BytesReturned, NULL);
	return (WORD) OutBuffer.field_4;
}

int GetVGABIOSAddress(VGAChip *chip, WORD adapterIndex)
{
	Ioctl_83002138_In InBuffer;
	Ioctl_83002138_Out OutBuffer;
	DWORD BytesReturned;

	InBuffer.adapterIndex = adapterIndex;
	InBuffer.field_2 = chip->field_0;
	InBuffer.field_4 = chip->field_4;
	InBuffer.field_8 = chip->field_18;
	InBuffer.field_14 = 1;
	InBuffer.field_16 = 0;
	if (InBuffer.field_8)
	{
		DeviceIoControl(g_hDevice, 0x83002138, &InBuffer, sizeof(InBuffer), &OutBuffer, sizeof(OutBuffer), &BytesReturned, NULL);
	}
	if (GetWORDVGABIOSData(0, adapterIndex) == 0xAA55)
	{
		return OutBuffer.field_4;
	}
	if (adapterIndex) return 0;
	InBuffer.field_8 = 0xC0000;
	DeviceIoControl(g_hDevice, 0x83002138, &InBuffer, sizeof(InBuffer), &OutBuffer, sizeof(OutBuffer), &BytesReturned, NULL);
	if (GetWORDVGABIOSData(0, 0) != 0xAA55) return 0;
	return OutBuffer.field_4;
}

DWORD ReadMMIODword(DWORD address, WORD adapterIndex)
{
	Ioctl_83002138_In InBuffer;
	Ioctl_83002138_Out OutBuffer;
	DWORD BytesReturned;

	InBuffer.adapterIndex = adapterIndex;
	InBuffer.field_14 = 0;
	InBuffer.field_16 = 1;
	InBuffer.field_18 = 2;
	InBuffer.mmioRegister = address;
	DeviceIoControl(g_hDevice, 0x83002138, &InBuffer, sizeof(InBuffer), &OutBuffer, sizeof(OutBuffer), &BytesReturned, NULL);
	return OutBuffer.field_4;
}

void WriteMMIODword(DWORD address, DWORD value, WORD adapterIndex)
{
	Ioctl_83002138_In InBuffer;
	Ioctl_83002138_Out OutBuffer;
	DWORD BytesReturned;

	InBuffer.adapterIndex = adapterIndex;
	InBuffer.field_14 = 0;
	InBuffer.field_16 = 2;
	InBuffer.field_18 = 2;
	InBuffer.mmioRegister = address;
	InBuffer.field_24 = value;
	DeviceIoControl(g_hDevice, 0x83002138, &InBuffer, sizeof(InBuffer), &OutBuffer, sizeof(OutBuffer), &BytesReturned, NULL);
}
