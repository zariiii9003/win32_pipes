/* SPDX-FileCopyrightText: 2024-present Artur Drogunow <artur.drogunow@zf.com>
#
# SPDX-License-Identifier: MIT */

#ifndef UTIL_H
#define UTIL_H

#include <Windows.h>
#include <string>

extern auto AnsiToUtf8(const char *ansiString) -> std::string;
extern auto Win32ErrorExit(DWORD errNo = 0) -> void;

#endif
