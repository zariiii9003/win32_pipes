import multiprocessing
import os
import re
import time
from concurrent.futures import ThreadPoolExecutor
from typing import List

import pytest

import win32_pipes


def test_module():
    assert hasattr(win32_pipes, "__version__")
    assert hasattr(win32_pipes, "Pipe")
    assert hasattr(win32_pipes, "PipeConnection")
    assert hasattr(win32_pipes, "PipeClient")
    assert hasattr(win32_pipes, "PipeListener")
    assert hasattr(win32_pipes, "generate_pipe_address")


def test_duplex():
    c1, c2 = win32_pipes.Pipe(duplex=True, start_thread=True)
    assert c1.readable
    assert c1.writable
    assert c2.readable
    assert c2.writable
    assert not c1.closed
    assert not c2.closed
    assert c1.fileno() > 0
    assert c2.fileno() > 0

    tx_data_256b = bytes(list(range(256)))
    tx_data_1kb = tx_data_256b * 4
    tx_data_1mb = tx_data_1kb * 1024

    # send from c1 to c2
    c1.send_bytes(tx_data_1kb)
    c1.send_bytes(tx_data_1mb)
    c1.send_bytes(tx_data_256b)
    time.sleep(0.5)
    assert c2.recv_bytes() == tx_data_1kb
    assert c2.recv_bytes() == tx_data_1mb
    assert c2.recv_bytes() == tx_data_256b
    assert c1.recv_bytes() is None
    assert c2.recv_bytes() is None

    # send from c2 to c1
    c2.send_bytes(tx_data_1kb)
    c2.send_bytes(tx_data_1mb)
    c2.send_bytes(tx_data_256b)
    time.sleep(0.5)
    assert c1.recv_bytes() == tx_data_1kb
    assert c1.recv_bytes() == tx_data_1mb
    assert c1.recv_bytes() == tx_data_256b
    assert c1.recv_bytes() is None
    assert c2.recv_bytes() is None

    c1.close()
    c2.close()
    assert c1.closed
    assert c2.closed


def test_non_duplex():
    c1, c2 = win32_pipes.Pipe(duplex=False, start_thread=True)
    assert c1.readable
    assert not c1.writable
    assert not c2.readable
    assert c2.writable
    assert not c1.closed
    assert not c2.closed
    assert c1.fileno() > 0
    assert c2.fileno() > 0

    tx_data_256b = bytes(list(range(256)))
    tx_data_1kb = tx_data_256b * 4
    tx_data_1mb = tx_data_1kb * 1024
    tx_data_10mb = tx_data_1mb * 10

    # send from c2 to c1
    c2.send_bytes(tx_data_1kb)
    c2.send_bytes(tx_data_1mb)
    c2.send_bytes(tx_data_256b)
    c2.send_bytes(tx_data_10mb)
    c2.send_bytes(tx_data_256b)
    time.sleep(0.5)
    assert c1.recv_bytes() == tx_data_1kb
    assert c1.recv_bytes() == tx_data_1mb
    assert c1.recv_bytes() == tx_data_256b
    assert c1.recv_bytes() == tx_data_10mb
    assert c1.recv_bytes() == tx_data_256b
    assert c1.recv_bytes() is None

    with pytest.raises(RuntimeError):
        c1.send_bytes(tx_data_256b)
    with pytest.raises(RuntimeError):
        c2.recv_bytes()

    c1.close()
    c2.close()
    assert c1.closed
    assert c2.closed


def test_context_manager():
    rx, tx = win32_pipes.Pipe(False)
    with rx as rx, tx as tx:
        tx.send_bytes(b"test")
        time.sleep(0.1)
        assert rx.recv_bytes() == b"test"

    assert rx.closed
    assert tx.closed


def test_broken_pipe_rx():
    rx, tx = win32_pipes.Pipe(False, start_thread=True)
    tx.close()
    time.sleep(0.001)

    with pytest.raises(BrokenPipeError):
        rx.recv_bytes()

    rx.close()
    assert rx.closed
    assert tx.closed


def test_broken_pipe_tx():
    rx, tx = win32_pipes.Pipe(False, start_thread=True)
    rx.close()

    with pytest.raises(BrokenPipeError):
        tx.send_bytes(b"data")

    tx.close()
    assert rx.closed
    assert tx.closed


def test_generate_pipe_address():
    address = win32_pipes.generate_pipe_address()
    assert re.match(rf"\\\\.\\pipe\\win32_pipes-{os.getpid()}-\d+-", address)


def test_pipe_listener():
    def listen(_listener: win32_pipes.PipeListener):
        server = _listener.accept()
        server.start_thread()
        server.send_bytes(b"Hello", blocking=True)
        server.close()
        _listener.close()

    address = win32_pipes.generate_pipe_address()
    listener = win32_pipes.PipeListener(address)
    with ThreadPoolExecutor(max_workers=1) as executor:
        future = executor.submit(listen, listener)

        # connect to listener
        client = win32_pipes.PipeClient(address)
        client.start_thread()
        while True:
            rx_data = client.recv_bytes()
            if rx_data:
                break
        assert rx_data == b"Hello"
        client.close()
        future.result()


def test_pipe_listener_close():
    def listen(_listener: win32_pipes.PipeListener):
        _listener.accept()

    address = win32_pipes.generate_pipe_address()
    listener = win32_pipes.PipeListener(address)
    with ThreadPoolExecutor(max_workers=1) as executor:
        future = executor.submit(listen, listener)
        listener.close()
        with pytest.raises(RuntimeError):
            future.result()


def _send_from_subprocess(c: win32_pipes.PipeConnection, messages: List[bytes]):
    c.start_thread()
    for msg in messages:
        c.send_bytes(msg, blocking=True)
    time.sleep(0.5)
    c.close()


def test_multiprocessing():
    c1, c2 = win32_pipes.Pipe(duplex=True)
    c1.start_thread()

    messages = [b"Message1", b"Message1", b"Message3"]

    p = multiprocessing.Process(target=_send_from_subprocess, args=(c2, messages))
    p.start()

    for tx_data in messages:
        while True:
            rx_data = c1.recv_bytes()
            if rx_data:
                break
        assert rx_data == tx_data

    p.join()
    assert p.exitcode == 0
    c1.close()
