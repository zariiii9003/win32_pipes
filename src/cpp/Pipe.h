/* SPDX-FileCopyrightText: 2024-present Artur Drogunow <artur.drogunow@zf.com>
#
# SPDX-License-Identifier: MIT */

#ifndef PIPE_H
#define PIPE_H

#include "./PipeConnection.h"
#include <format>
#include <mutex>
#include <string>
#include <tuple>

auto generatePipeAddress() -> std::string;
auto pipe(bool duplex = true) -> std::tuple<PipeConnection *, PipeConnection *>;

#endif
