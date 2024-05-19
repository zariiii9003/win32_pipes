# win32_pipes

[![PyPI - Version](https://img.shields.io/pypi/v/win32_pipes.svg)](https://pypi.org/project/win32_pipes)
[![PyPI - Python Version](https://img.shields.io/pypi/pyversions/win32_pipes.svg)](https://pypi.org/project/win32_pipes)
[![CI](https://github.com/zariiii9003/win32_pipes/actions/workflows/wheels.yml/badge.svg)](https://github.com/zariiii9003/win32_pipes/actions/workflows/wheels.yml)

## Installation

```console
pip install win32-pipes
```

## Description

This library reimplements the builtin [`PipeListener`](https://github.com/python/cpython/blob/5307f44fb983f2a17727fb43602f5dfa63e93311/Lib/multiprocessing/connection.py#L656),
[`PipeClient`](https://github.com/python/cpython/blob/5307f44fb983f2a17727fb43602f5dfa63e93311/Lib/multiprocessing/connection.py#L712)
and [`PipeConnection`](https://github.com/python/cpython/blob/5307f44fb983f2a17727fb43602f5dfa63e93311/Lib/multiprocessing/connection.py#L268).

The main difference to the builtin versions is the `blocking` parameter
in the `PipeConnection.recv_bytes()` and `PipeConnection.send_bytes()` methods.
The `PipeConnection` internally uses a thread to handle the asynchronous I/O
without the acquiring the global interpreter lock.

Once the `PipeConnection.recv_bytes()` or `PipeConnection.send_bytes()`
methods were called, the `PipeConnection` can not be moved to another
process anymore.

## License

`win32_pipes` is distributed under the terms of the [MIT](https://spdx.org/licenses/MIT.html) license.
