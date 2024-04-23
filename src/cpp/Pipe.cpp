/* SPDX-FileCopyrightText: 2024-present Artur Drogunow <artur.drogunow@zf.com>
#
# SPDX-License-Identifier: MIT */

#include "./Pipe.h"

static size_t     _mmap_counter{0};
static std::mutex _mmap_counter_lock;

auto generatePipeAddress() -> std::string
{
    std::scoped_lock lock(_mmap_counter_lock);
    return std::format("\\\\.\\pipe\\win32_pipes-{}-{}-",
                       GetCurrentProcessId(),
                       _mmap_counter++);
}

auto pipe(bool duplex) -> std::tuple<PipeConnection *, PipeConnection *>
{
    auto address  = generatePipeAddress();
    auto openmode = PIPE_ACCESS_DUPLEX;
    auto access   = GENERIC_READ | GENERIC_WRITE;
    auto obsize   = BUFSIZE;
    auto ibsize   = BUFSIZE;
    if (!duplex) {
        openmode = PIPE_ACCESS_INBOUND;
        access   = GENERIC_WRITE;
        obsize   = 0;
    }
    HANDLE h1 = CreateNamedPipe(
        address.c_str(),
        openmode | FILE_FLAG_OVERLAPPED | FILE_FLAG_FIRST_PIPE_INSTANCE,
        PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
        1,
        obsize,
        ibsize,
        NMPWAIT_WAIT_FOREVER,
        nullptr);
    if (h1 == INVALID_HANDLE_VALUE)
        Win32ErrorExit(0);

    HANDLE h2 = CreateFile(address.c_str(),
                           access,
                           0,
                           nullptr,
                           OPEN_EXISTING,
                           FILE_FLAG_OVERLAPPED,
                           nullptr);
    if (h2 == INVALID_HANDLE_VALUE)
        Win32ErrorExit(0);

    DWORD mode = PIPE_READMODE_MESSAGE;
    if (!SetNamedPipeHandleState(h2, &mode, nullptr, nullptr))
        Win32ErrorExit(0);

    OVERLAPPED ov{0};
    DWORD      NumberOfBytesTransferred{0};
    ConnectNamedPipe(h1, &ov);
    switch (GetLastError()) {
        case ERROR_PIPE_CONNECTED:
            break;
        case ERROR_IO_PENDING:
            if (!GetOverlappedResult(h1, &ov, &NumberOfBytesTransferred, TRUE))
                Win32ErrorExit(0);
            break;
        default:
            Win32ErrorExit(0);
    }

    return std::make_tuple<PipeConnection *, PipeConnection *>(
        new PipeConnection(reinterpret_cast<size_t>(h1), true, duplex),
        new PipeConnection(reinterpret_cast<size_t>(h2), duplex, true));
};
