/* SPDX-FileCopyrightText: 2024-present Artur Drogunow <artur.drogunow@zf.com>
#
# SPDX-License-Identifier: MIT */

#ifndef UTIL_H
#define UTIL_H

#include <Windows.h>
#include <system_error>

extern auto              systemErrorToOsError(const std::exception_ptr &eptr,
                                              void                     *data) -> void;
[[noreturn]] extern auto Win32ErrorExit(DWORD errNo = 0) -> void;

#endif
