import time

import pytest

import win_pipes


def test_module():
    assert hasattr(win_pipes, "__version__")
    assert hasattr(win_pipes, "Pipe")
    assert hasattr(win_pipes, "PipeConnection")


def test_duplex():
    c1, c2 = win_pipes.Pipe(duplex=True)
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
    c1, c2 = win_pipes.Pipe(duplex=False)
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

    # send from c2 to c1
    c2.send_bytes(tx_data_1kb)
    c2.send_bytes(tx_data_1mb)
    c2.send_bytes(tx_data_256b)
    time.sleep(0.5)
    assert c1.recv_bytes() == tx_data_1kb
    assert c1.recv_bytes() == tx_data_1mb
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
    rx, tx = win_pipes.Pipe(False)
    with rx as rx, tx as tx:
        tx.send_bytes(b"test")
        time.sleep(0.1)
        assert rx.recv_bytes() == b"test"

    assert rx.closed
    assert tx.closed
