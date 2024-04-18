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
    PipeConnection(size_t handle,
                   bool   readable    = true,
                   bool   writable    = true,
                   bool   startThread = false);

    auto StartThread() -> void;

    auto GetHandle() const -> size_t;

    auto GetReadable() const -> bool;

    auto GetWritable() const -> bool;

    auto GetClosed() -> bool;

    auto SendBytes(const nanobind::bytes       buffer,
                   const size_t                offset = 0,
                   const std::optional<size_t> size   = {}) -> void;

    auto RecvBytes() -> std::optional<nanobind::bytes>;

    auto Close() -> void;

    ~PipeConnection();

  private:
    const HANDLE                                   _handle;
    const bool                                     _readable;
    const bool                                     _writable;
    bool                                           _closed  = false;
    bool                                           _started = false;
    HANDLE                                         _completionPort;
    std::queue<std::unique_ptr<OverlappedData>>    _TxQueue;
    std::mutex                                     _TxQueueMutex;
    std::thread                                    _thread;
    DWORD                                          _threadErr{0};
    std::optional<std::string>                     _threadErrContext;
    OVERLAPPED                                     rxOv{0};
    std::vector<char>                              _RxBuffer{0};
    std::queue<std::unique_ptr<std::vector<char>>> _RxQueue;
    std::mutex                                     _RxQueueMutex;

    void        MonitorIoCompletion();
    inline void CheckThread();
    void        CleanupAndThrow(DWORD                      errNo   = 0,
                                std::optional<std::string> context = {});
};

#endif
