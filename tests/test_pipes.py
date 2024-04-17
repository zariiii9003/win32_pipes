import time

import win_pipes


def test_module():
    assert hasattr(win_pipes, "__version__")
    assert hasattr(win_pipes, "Pipe")
    assert hasattr(win_pipes, "PipeConnection")


def test_context_manager():
    rx, tx = win_pipes.Pipe(False)
    with rx as rx, tx as tx:
        tx.send_bytes(b"test")
        time.sleep(0.1)
        assert rx.recv_bytes() == b"test"

    assert rx.closed
    assert tx.closed
