# SPDX-FileCopyrightText: 2024-present Artur Drogunow <artur.drogunow@zf.com>
#
# SPDX-License-Identifier: MIT

import _winapi
from multiprocessing import reduction
import typing

from ._ext import __version__, PipeConnection, Pipe

__all__ = ["__version__", "Pipe", "PipeConnection"]


def reduce_pipe_connection(conn: PipeConnection) -> typing.Any:
    access = (_winapi.FILE_GENERIC_READ if conn.readable else 0) | (
        _winapi.FILE_GENERIC_WRITE if conn.writable else 0
    )
    dh = reduction.DupHandle(conn.fileno(), access)
    return rebuild_pipe_connection, (dh, conn.readable, conn.writable)


def rebuild_pipe_connection(
    dh: reduction.DupHandle, readable: bool, writable: bool
) -> PipeConnection:
    handle = dh.detach()
    return PipeConnection(handle, readable, writable)


reduction.register(PipeConnection, reduce_pipe_connection)