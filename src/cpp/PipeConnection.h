/* SPDX-FileCopyrightText: 2024-present Artur Drogunow <artur.drogunow@zf.com>
#
# SPDX-License-Identifier: MIT */

#ifndef PIPECONNECTION_H
#define PIPECONNECTION_H

#include <Windows.h>
#include <memory>
#include <mutex>
#include <nanobind/nanobind.h>
#include <optional>
#include <queue>
#include <vector>

#include "./util.h"

const DWORD BUFSIZE{8192};

class OverlappedData {
  public:
    OVERLAPPED        overlapped;
    std::vector<char> vector;
    OverlappedData(const char *pBuffer, const size_t len);
    OverlappedData(OverlappedData &&) = default;
};

class PipeConnection {
  public:
    PipeConnection(size_t handle, bool readable = true, bool writable = true);

    auto getHandle() const -> size_t;

    auto getReadable() const -> bool;

    auto getWritable() const -> bool;

    auto getClosed() -> bool;

    auto sendBytes(const nanobind::bytes       buffer,
                   const size_t                offset   = 0,
                   const std::optional<size_t> size     = {},
                   const bool                  blocking = true) -> void;

    auto recvBytes(std::optional<int> maxLength = {},
                   const bool         blocking  = true)
        -> std::optional<nanobind::bytes>;

    auto close() -> void;

    ~PipeConnection();

  private:
    const HANDLE                                   _handle;
    const bool                                     _readable;
    const bool                                     _writable;
    bool                                           _closed  = false;
    bool                                           _started = false;
    HANDLE                                         _completionPort;
    std::queue<std::shared_ptr<OverlappedData>>    _TxQueue;
    std::mutex                                     _TxQueueMutex;
    std::thread                                    _thread;
    DWORD                                          _threadErr{ERROR_SUCCESS};
    OVERLAPPED                                     _rxOv{0};
    std::vector<char>                              _RxBuffer{0};
    std::queue<std::shared_ptr<std::vector<char>>> _RxQueue;
    std::mutex                                     _RxQueueMutex;
    HANDLE _RxQueueEvent{INVALID_HANDLE_VALUE};

    auto              monitorIoCompletion() -> void;
    inline auto       startThread() -> void;
    inline auto       checkThread() -> void;
    [[noreturn]] auto cleanupAndThrowExc(DWORD errNo = 0) -> void;
};

#endif
