/* SPDX-FileCopyrightText: 2024-present Artur Drogunow <artur.drogunow@zf.com>
#
# SPDX-License-Identifier: MIT */

#include "./PipeListener.h"
#include "PipeListener.h"

PipeListener::PipeListener(std::string address, std::optional<size_t> backlog)
    : _address(address)
{
    _closeEvent = CreateEvent(nullptr, TRUE, FALSE, nullptr);
    _handleQueue.push(newHandle(true));
}

PipeListener::~PipeListener()
{
    if (_closed)
        close();
}

auto PipeListener::accept() -> PipeConnection
{
    _handleQueueMutex.lock();
    _handleQueue.push(newHandle(false));
    auto handle = _handleQueue.front();
    _handleQueueMutex.unlock();

    auto ov   = OVERLAPPED{0};
    ov.hEvent = CreateEvent(nullptr, TRUE, FALSE, nullptr);
    if (!ConnectNamedPipe(handle, &ov)) {
        auto errNo = GetLastError();
        switch (errNo) {
            default: {
                CloseHandle(handle);
                CloseHandle(ov.hEvent);
                cleanupAndThrowExc(errNo,
                                   "PipeListener::accept.ConnectNamedPipe");
            }
        }
    }
    HANDLE handles[2] = {_closeEvent, ov.hEvent};
    auto   waitRes    = WaitForMultipleObjects(2, handles, FALSE, INFINITE);
    switch (waitRes) {
        case WAIT_OBJECT_0:
            CloseHandle(handle);
            CloseHandle(ov.hEvent);
            throw std::runtime_error("PipeListener was closed.");
            break;
        case WAIT_OBJECT_0 + 1:
            CloseHandle(ov.hEvent);
            return PipeConnection(reinterpret_cast<size_t>(handle));
        default: {
            CloseHandle(handle);
            CloseHandle(ov.hEvent);
            cleanupAndThrowExc(0, "PipeListener::accept.ConnectNamedPipe");
        }
    }
}

auto PipeListener::close() -> void
{
    _closed = true;
    SetEvent(_closeEvent);

    auto lock = std::scoped_lock(_handleQueueMutex);
    while (!_handleQueue.empty()) {
        CloseHandle(_handleQueue.front());
        _handleQueue.pop();
    }
}

auto PipeListener::getAddress() -> std::string { return std::string(_address); }

auto PipeListener::newHandle(bool first) -> HANDLE
{
    auto flags = PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED;
    if (first) {
        flags |= FILE_FLAG_FIRST_PIPE_INSTANCE;
    }
    auto handle =
        CreateNamedPipe(_address.c_str(),
                        flags,
                        PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
                        PIPE_UNLIMITED_INSTANCES,
                        BUFSIZE,
                        BUFSIZE,
                        NMPWAIT_WAIT_FOREVER,
                        NULL);
    if (handle == INVALID_HANDLE_VALUE)
        cleanupAndThrowExc(0, "PipeListener::newHandle.CreateNamedPipe");
    return handle;
}

auto PipeListener::cleanupAndThrowExc(DWORD                      errNo,
                                      std::optional<std::string> context)
    -> void
{
    close();
    Win32ErrorExit(errNo, context);
    throw std::runtime_error("This should not be reached.");
}
