/* SPDX-FileCopyrightText: 2024-present Artur Drogunow <artur.drogunow@zf.com>
#
# SPDX-License-Identifier: MIT */

#include "./util.h"
#include <iostream>
#include <stdexcept>

// Convert ANSI string to UTF-8
auto AnsiToUtf8(const char *ansiString) -> std::string
{
    int numWideChars = MultiByteToWideChar(CP_ACP, 0, ansiString, -1, NULL, 0);
    if (numWideChars == 0) {
        // Handle error
        return "";
    }

    std::wstring wideString(numWideChars, L'\0');
    MultiByteToWideChar(CP_ACP,
                        0,
                        ansiString,
                        -1,
                        &wideString[0],
                        numWideChars);

    int numUtf8Bytes = WideCharToMultiByte(CP_UTF8,
                                           0,
                                           wideString.c_str(),
                                           -1,
                                           NULL,
                                           0,
                                           NULL,
                                           NULL);
    if (numUtf8Bytes == 0) {
        // Handle error
        return "";
    }

    std::string utf8String(numUtf8Bytes, '\0');
    WideCharToMultiByte(CP_UTF8,
                        0,
                        wideString.c_str(),
                        -1,
                        &utf8String[0],
                        numUtf8Bytes,
                        NULL,
                        NULL);

    return utf8String;
}

extern void Win32ErrorExit(DWORD errNo)
{
    DWORD _errNo = errNo == 0 ? GetLastError() : errNo;
    if (errNo != ERROR_SUCCESS) {
        DWORD n;
        CHAR  msgBuf[1000]{0};

        n           = FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM |
                              FORMAT_MESSAGE_IGNORE_INSERTS,
                          nullptr,
                          _errNo,
                          MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                          msgBuf,
                          sizeof(msgBuf),
                          nullptr);
        auto errMsg = AnsiToUtf8(msgBuf);
        std::cout << errMsg << std::endl;
        throw std::runtime_error(errMsg.c_str());
    }
};
