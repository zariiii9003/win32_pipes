/* SPDX-FileCopyrightText: 2024-present Artur Drogunow <artur.drogunow@zf.com>
#
# SPDX-License-Identifier: MIT */

#ifndef PIPECLIENT_H
#define PIPECLIENT_H

#include "./PipeConnection.h"
#include <string>

auto pipeClient(std::string address) -> PipeConnection *;

#endif
