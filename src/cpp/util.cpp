/* SPDX-FileCopyrightText: 2024-present Artur Drogunow <artur.drogunow@zf.com>
#
# SPDX-License-Identifier: MIT */

#include "./util.h"
#include <Python.h>
#include <Windows.h>
#include <system_error>

[[noreturn]] extern auto Win32ErrorExit(DWORD errNo) -> void
{
    DWORD _errNo = errNo == 0 ? GetLastError() : errNo;
    auto  ec     = std::error_code(_errNo, std::system_category());
    throw std::system_error(ec);
}

extern auto systemErrorToOsError(const std::exception_ptr &eptr,
                                 void                     *data) -> void
{
    try {
        std::rethrow_exception(eptr);
    }
    catch (const std::system_error &e) {
        PyErr_SetFromWindowsErr(e.code().value());
    }
}
