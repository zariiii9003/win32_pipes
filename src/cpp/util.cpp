/* SPDX-FileCopyrightText: 2024-present Artur Drogunow <artur.drogunow@zf.com>
#
# SPDX-License-Identifier: MIT */

#include "./util.h"
#include <format>
#include <iostream>
#include <optional>
#include <stdexcept>

// Convert ANSI string to UTF-8
auto AnsiToUtf8(const std::string &ansiString) -> std::string
{
    int numWideChars =
        MultiByteToWideChar(CP_ACP, 0, ansiString.c_str(), -1, NULL, 0);
    if (numWideChars == 0) {
        // Handle error
        return "";
    }

    std::wstring wideString(numWideChars, L'\0');
    MultiByteToWideChar(CP_ACP,
                        0,
                        ansiString.c_str(),
                        -1,
                        &wideString.front(),
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

    return std::move(utf8String);
}

extern auto AnsiFormatMessage(DWORD errNo) -> std::string
{
    auto ansiString = std::string(256, '\0');
    auto n          = FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM |
                               FORMAT_MESSAGE_IGNORE_INSERTS,
                           nullptr,
                           errNo,
                           MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US),
                           &ansiString.front(),
                           ansiString.size(),
                           nullptr);

    // remove trailing whitespace
    while (isspace(ansiString.at(n - 1))) {
        --n;
    }
    ansiString.resize(n);

    return std::move(ansiString);
}

extern void Win32ErrorExit(DWORD errNo, std::optional<std::string> context)
{
    DWORD _errNo = errNo == 0 ? GetLastError() : errNo;
    if (_errNo != ERROR_SUCCESS) {
        auto        ansiString = AnsiFormatMessage(_errNo);
        auto        utf8String = AnsiToUtf8(ansiString);
        std::string errMsg{};

        if (context.has_value()) {
            errMsg = std::format("{} [WinError {}: {}]",
                                 context.value(),
                                 _errNo,
                                 utf8String.c_str());
        }
        else {
            errMsg = std::format("[WinError {}: {}]", _errNo, utf8String);
        }
        std::cerr << errMsg << std::endl;
        throw std::runtime_error(errMsg);
    }
};
