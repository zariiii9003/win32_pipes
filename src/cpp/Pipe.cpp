/* SPDX-FileCopyrightText: 2024-present Artur Drogunow <artur.drogunow@zf.com>
#
# SPDX-License-Identifier: MIT */

#include "./Pipe.h"

static size_t _mmap_counter{0};
static std::mutex _mmap_counter_lock;

auto GeneratePipeAddress() -> std::string
{
    std::scoped_lock lock(_mmap_counter_lock);
    auto newAddress = std::format("\\\\.\\pipe\\pyc-{}-{}-", GetCurrentProcessId(), _mmap_counter);
    _mmap_counter++;
    return newAddress;
}

auto Pipe(bool duplex) -> std::tuple<PipeConnection *, PipeConnection *>
{
    auto address = GeneratePipeAddress();
    auto openmode = PIPE_ACCESS_DUPLEX;
    auto access = GENERIC_READ | GENERIC_WRITE;
    auto obsize = BUFSIZE;
    auto ibsize = BUFSIZE;
    if (!duplex) {
        openmode = PIPE_ACCESS_INBOUND;
        access = GENERIC_WRITE;
        obsize = 0;
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
        Win32ErrorExit();

    HANDLE h2 =
        CreateFile(address.c_str(), access, 0, nullptr, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, nullptr);
    if (h2 == INVALID_HANDLE_VALUE)
        Win32ErrorExit();

    DWORD mode = PIPE_READMODE_MESSAGE;
    if (!SetNamedPipeHandleState(h2, &mode, nullptr, nullptr))
        Win32ErrorExit();

    OVERLAPPED ov{0};
    DWORD NumberOfBytesTransferred{0};
    ConnectNamedPipe(h1, &ov);
    switch (GetLastError()) {
    case ERROR_PIPE_CONNECTED:
        break;
    case ERROR_IO_PENDING:
        if (!GetOverlappedResult(h1, &ov, &NumberOfBytesTransferred, TRUE))
            Win32ErrorExit();
        break;
    default:
        Win32ErrorExit();
    }

    return std::make_tuple<PipeConnection *, PipeConnection *>(
        new PipeConnection(reinterpret_cast<size_t>(h1), true, duplex),
        new PipeConnection(reinterpret_cast<size_t>(h2), duplex, true));
};