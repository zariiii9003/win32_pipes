# SPDX-FileCopyrightText: 2024-present Artur Drogunow <artur.drogunow@zf.com>
#
# SPDX-License-Identifier: MIT

import _winapi
import typing
from multiprocessing import reduction

from win32_pipes._ext import (
    Pipe,
    PipeClient,
    PipeConnection,
    PipeListener,
    generate_pipe_address,
)
from win32_pipes._version import __version__

__all__ = [
    "Pipe",
    "PipeClient",
    "PipeConnection",
    "PipeListener",
    "__version__",
    "generate_pipe_address",
]


def reduce_pipe_connection(conn: PipeConnection) -> typing.Any:
    access = (_winapi.FILE_GENERIC_READ if conn.readable else 0) | (
        _winapi.FILE_GENERIC_WRITE if conn.writable else 0
    )
    dh = reduction.DupHandle(conn.fileno(), access)
    conn.close()
    return rebuild_pipe_connection, (dh, conn.readable, conn.writable)


def rebuild_pipe_connection(
    dh: reduction.DupHandle, readable: bool, writable: bool
) -> PipeConnection:
    handle = dh.detach()
    return PipeConnection(handle, readable, writable)


reduction.register(PipeConnection, reduce_pipe_connection)
