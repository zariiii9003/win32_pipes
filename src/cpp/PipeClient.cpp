#include "PipeClient.h"
/* SPDX-FileCopyrightText: 2024-present Artur Drogunow <artur.drogunow@zf.com>
#
# SPDX-License-Identifier: MIT */

#include "./PipeConnection.h"
#include "./util.h"
#include <Windows.h>
#include <string>
#include <utility>

auto pipeClient(std::string address) -> PipeConnection *
{
    auto handle = CreateFile(address.c_str(),
                             GENERIC_READ | GENERIC_WRITE,
                             0,
                             NULL,
                             OPEN_EXISTING,
                             FILE_FLAG_OVERLAPPED,
                             NULL);
    if (handle == INVALID_HANDLE_VALUE)
        Win32ErrorExit();

    DWORD mode{PIPE_READMODE_MESSAGE};
    if (!SetNamedPipeHandleState(handle, &mode, nullptr, nullptr))
        Win32ErrorExit();

    return new PipeConnection(reinterpret_cast<size_t>(handle));
}
