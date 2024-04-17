/* SPDX-FileCopyrightText: 2024-present Artur Drogunow <artur.drogunow@zf.com>
#
# SPDX-License-Identifier: MIT */

#ifndef UTIL_H
#define UTIL_H

#include <Windows.h>
#include <optional>
#include <string>

extern auto AnsiFormatMessage(DWORD errNo) -> std::string;
extern auto AnsiToUtf8(const std::string &ansiString) -> std::string;
extern auto Win32ErrorExit(DWORD                      errNo   = 0,
                           std::optional<std::string> context = {}) -> void;

#endif
