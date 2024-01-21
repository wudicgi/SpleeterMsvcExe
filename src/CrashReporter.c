/*
 * MIT License
 *
 * Copyright (c) 2018 Wudi <wudi@wudilabs.org>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdlib.h>
#include <tchar.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <Windows.h>
#include <DbgHelp.h>
#include "Debug.h"

typedef BOOL(WINAPI *MiniDumpWriteDumpFunc)(
        HANDLE hProcess,
        DWORD ProcessId,
        HANDLE hFile,
        MINIDUMP_TYPE DumpType,
        const MINIDUMP_EXCEPTION_INFORMATION *ExceptionInfo,
        const MINIDUMP_USER_STREAM_INFORMATION *UserStreamInfo,
        const MINIDUMP_CALLBACK_INFORMATION *Callback
);

static LPTOP_LEVEL_EXCEPTION_FILTER _lastExceptionFilter = NULL;

static LONG WINAPI _unhandledExceptionFilter(struct _EXCEPTION_POINTERS *exceptionPtr) {
    int rc = MessageBox(NULL, _T("An unexpected error occurred.\n\nDo you want to save the debug information?"),
            _T("Application Crashed"), MB_YESNO);
    if (rc != IDYES) {
        return EXCEPTION_CONTINUE_SEARCH;
    }

    // We load DbgHelp.dll dynamically, to support Windows 2000
    HMODULE hModule = LoadLibrary(_T("DbgHelp.dll"));
    if (hModule) {
        MiniDumpWriteDumpFunc miniDumpWriteDump = (MiniDumpWriteDumpFunc)GetProcAddress(hModule, "MiniDumpWriteDump");
        if (miniDumpWriteDump) {
            // fetch system time for dump-file name
            SYSTEMTIME systemTime = { 0 };
            GetLocalTime(&systemTime);

            // choose proper path for dump-file
            TCHAR dumpFilePath[MAX_PATH] = { 0 };
            _sntprintf(dumpFilePath, MAX_PATH, _T("SpleeterMsvcExe_crash_%04d%02d%02d_%02d%02d%02d.dmp"),
                    systemTime.wYear, systemTime.wMonth, systemTime.wDay,
                    systemTime.wHour, systemTime.wMinute, systemTime.wSecond);

            _tprintf(_T("\ndumpFilePath = %s\n"), dumpFilePath);
            fflush(stdout);

            // create and open the dump-file
            HANDLE hFile = CreateFile(dumpFilePath, GENERIC_WRITE,
                    FILE_SHARE_WRITE,
                    NULL,
                    CREATE_ALWAYS,
                    FILE_ATTRIBUTE_NORMAL | FILE_ATTRIBUTE_HIDDEN,
                    NULL
            );
            if (hFile != INVALID_HANDLE_VALUE) {
                MINIDUMP_EXCEPTION_INFORMATION exceptionInfo = { 0 };
                exceptionInfo.ThreadId = GetCurrentThreadId();
                exceptionInfo.ExceptionPointers = exceptionPtr;
                exceptionInfo.ClientPointers = FALSE;

                // at last write crash-dump to file
                bool succeeded = miniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), hFile,
                        MiniDumpNormal, &exceptionInfo, NULL, NULL);
                // dump-data is written, and we can close the file
                CloseHandle(hFile);
                if (succeeded) {
                    TCHAR buf[MAX_PATH + 100] = { 0 };
                    _sntprintf(buf, (MAX_PATH + 100), _T("The debug information has been successfully saved to file %s"), dumpFilePath);

                    MessageBox(NULL, buf, _T("Application Crashed"), MB_OK);

                    // Return from UnhandledExceptionFilter and execute the associated exception handler.
                    //   This usually results in process termination.
                    return EXCEPTION_EXECUTE_HANDLER;
                }
            }
        }
    }

    // Proceed with normal execution of UnhandledExceptionFilter.
    //   That means obeying the SetErrorMode flags,
    //   or invoking the Application Error pop-up message box.
    return EXCEPTION_CONTINUE_SEARCH;
}

void CrashReporter_register(void) {
    if (_lastExceptionFilter != NULL) {
        DEBUG_INFO("CrashReporter: is already registered\n");
    }
    SetErrorMode(SEM_FAILCRITICALERRORS);
    _lastExceptionFilter = SetUnhandledExceptionFilter(_unhandledExceptionFilter);
}

void CrashReporter_unregister(void) {
    SetUnhandledExceptionFilter(_lastExceptionFilter);
}
