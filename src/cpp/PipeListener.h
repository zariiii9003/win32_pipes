/* SPDX-FileCopyrightText: 2024-present Artur Drogunow <artur.drogunow@zf.com>
#
# SPDX-License-Identifier: MIT */

#ifndef PIPELISTENER_H
#define PIPELISTENER_H

#include "./PipeConnection.h"
#include "Windows.h"
#include <mutex>
#include <optional>
#include <string>

class PipeListener {
  private:
    std::string        _address;
    std::queue<HANDLE> _handleQueue{};
    std::mutex         _handleQueueMutex{};
    bool               _closed{false};
    HANDLE             _closeEvent{};

    auto              newHandle(bool first) -> HANDLE;
    [[noreturn]] auto cleanupAndThrowExc(DWORD errNo = 0) -> void;

  public:
    PipeListener(std::string address, std::optional<size_t> backlog = {});
    ~PipeListener();
    auto accept() -> PipeConnection *;
    auto close() -> void;
    auto getAddress() -> std::string;
};

#endif
