/* SPDX-FileCopyrightText: 2024-present Artur Drogunow <artur.drogunow@zf.com>
#
# SPDX-License-Identifier: MIT */

#ifndef PIPECONNECTION_H
#define PIPECONNECTION_H

#include <Windows.h>
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

    static auto allocate(const size_t len) -> OverlappedData *;
    static auto copy(const char *pBuffer, const size_t len) -> OverlappedData *;
};

class PipeConnection {
  public:
    PipeConnection(const size_t handle,
                   const bool   readable = true,
                   const bool   writable = true);

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
    const HANDLE                    _handle;
    const bool                      _readable;
    const bool                      _writable;
    bool                            _closed = false;
    HANDLE                          _completionPort;
    std::queue<OverlappedData *>    _TxQueue;
    std::mutex                      _TxQueueMutex;
    std::thread                     _thread;
    DWORD                           _threadErr{0};
    OVERLAPPED                      rxOv{0};
    std::vector<char>               _RxBuffer{0};
    std::queue<std::vector<char> *> _RxQueue;
    std::mutex                      _RxQueueMutex;

    void MonitorIoCompletion();
};

#endif
