/* SPDX-FileCopyrightText: 2024-present Artur Drogunow <artur.drogunow@zf.com>
#
# SPDX-License-Identifier: MIT */

#include <nanobind/nanobind.h>

#include "./PipeConnection.h"
#include "./util.h"

PipeConnection::PipeConnection(const size_t handle, const bool readable, const bool writable)
    : _handle{reinterpret_cast<const HANDLE>(handle)}, _readable{readable}, _writable{writable}
{
    if (!readable && !writable)
        throw nanobind::value_error("at least one of `readable` and `writable` must be True");

    _completionPort = CreateIoCompletionPort(_handle, nullptr, 0, 0);
    if (_completionPort == NULL)
        Win32ErrorExit();

    _thread = std::thread(&PipeConnection::MonitorIoCompletion, this);
};

PipeConnection::~PipeConnection() { Close(); }
auto PipeConnection::Close() -> void
{
    if (!_closed) {
        _closed = true;
        CloseHandle(_completionPort);
        CloseHandle(_handle);
        if (_thread.joinable())
            _thread.join();
    }
}

auto PipeConnection::GetHandle() const -> size_t { return reinterpret_cast<size_t>(_handle); }

auto PipeConnection::GetReadable() const -> bool { return _readable; }

auto PipeConnection::GetWritable() const -> bool { return _writable; }

auto PipeConnection::GetClosed() -> bool { return _closed; }

auto PipeConnection::SendBytes(
    const nanobind::bytes buffer,
    const size_t offset,
    const std::optional<size_t> size) -> void
{
    if (_closed)
        throw std::exception("handle is closed");
    if (!_writable)
        throw std::exception("connection is read-only");

    auto bufferLength = buffer.size();
    if (bufferLength <= offset)
        throw nanobind::value_error("buffer length <= offset");

    auto _size = bufferLength - offset;
    if (size.has_value()) {
        _size = size.value();
        if (offset + _size > bufferLength)
            throw nanobind::value_error("buffer length < offset + size");
    }

    // create OverlappedObject and push it to the queue before starting
    // WriteFile(), otherwise the monitor thread might try to clean up before it
    // is inserted
    auto pOd = OverlappedData::copy((buffer.c_str() + offset), _size);
    {
        std::scoped_lock lock(_TxQueueMutex);
        _TxQueue.push(pOd);
    }

    if (!WriteFile(_handle, &pOd->vector.front(), _size, 0, &pOd->overlapped)) {
        auto errNo = GetLastError();
        switch (errNo) {
        case ERROR_SUCCESS:
        case ERROR_IO_PENDING:
        case ERROR_IO_INCOMPLETE:
            break;
        default:
            Win32ErrorExit();
        }
    }
}

auto PipeConnection::RecvBytes() -> std::optional<nanobind::bytes>
{
    if (_closed)
        throw std::exception("handle is closed");
    if (!_readable)
        throw std::exception("connection is write-only");

    // Get RxQueue content and release lock asap. Return if empty.
    std::vector<char> *rxMessage;
    {
        std::scoped_lock lock(_RxQueueMutex);
        if (_RxQueue.empty())
            return {};
        rxMessage = _RxQueue.front();
        _RxQueue.pop();
    }

    // create python bytes object from vector
    auto nbbytes = new nanobind::bytes(&rxMessage->front(), rxMessage->size());
    delete rxMessage;
    return std::move(*nbbytes);
}

auto PipeConnection::MonitorIoCompletion() -> void
{
    ULONG_PTR completionKey{0};
    size_t bytesReadTotal{0};

    if (_readable) {
        // initialize read buffer
        _RxBuffer.resize(static_cast<size_t>(BUFSIZE));
        // start first read operation
        if (!ReadFile(_handle, &_RxBuffer.front(), BUFSIZE, nullptr, &rxOv))
            Win32ErrorExit();
    }

    while (!_closed) {
        // wait for completed operation
        DWORD numberOfBytesTransferred{0};
        LPOVERLAPPED pOv = nullptr;
        if (!GetQueuedCompletionStatus(
                _completionPort, &numberOfBytesTransferred, &completionKey, &pOv, INFINITE)) {
            DWORD errNo = GetLastError();
            switch (errNo) {
            case ERROR_SUCCESS:
            case ERROR_MORE_DATA:
                break;
            case ERROR_ABANDONED_WAIT_0:
            case ERROR_BROKEN_PIPE:
                return;
            default:
                Win32ErrorExit(errNo);
            }
        };

        if (pOv == &rxOv) {
            // receive operation completed
            size_t msgSize = static_cast<size_t>(numberOfBytesTransferred);
            auto ovRes = GetOverlappedResult(_handle, pOv, &numberOfBytesTransferred, false);
            bytesReadTotal += static_cast<size_t>(numberOfBytesTransferred);

            if (!ovRes) {
                if (GetLastError() == ERROR_MORE_DATA) {
                    // check how much data of the message is missing
                    DWORD bytesLeftThisMessage;
                    PeekNamedPipe(_handle, nullptr, 0, nullptr, nullptr, &bytesLeftThisMessage);

                    // check if rxBuffer size is sufficient
                    size_t msgSize = bytesReadTotal + static_cast<size_t>(bytesLeftThisMessage);
                    if (msgSize > _RxBuffer.size())
                        _RxBuffer.resize(msgSize);

                    // Reset OVERLAPPED and read the rest of the message
                    rxOv = OVERLAPPED{0};
                    if (!ReadFile(
                            _handle, &_RxBuffer[bytesReadTotal], bytesLeftThisMessage, nullptr, &rxOv))
                        Win32ErrorExit();
                    continue;
                }
                else
                    Win32ErrorExit();
            }

            // create new vector, which will be saved in RxQueue
            auto rxMessageOut = new std::vector<char>(bytesReadTotal);
            rxMessageOut->swap(_RxBuffer);
            rxMessageOut->resize(bytesReadTotal);
            bytesReadTotal = 0; // reset bytesReadTotal

            // push the new vector to the queue
            std::scoped_lock lock(_RxQueueMutex);
            _RxQueue.push(rxMessageOut);

            // start next receive operation
            if (!ReadFile(_handle, &_RxBuffer.front(), BUFSIZE, nullptr, &rxOv))
                Win32ErrorExit();
        }
        else {
            // send operation completed
            std::scoped_lock lock(_TxQueueMutex);
            if (!_TxQueue.empty() && (&_TxQueue.front()->overlapped == pOv)) {
                delete _TxQueue.front();
                _TxQueue.pop();
            }
            else {
            }
        }
    }
};

auto OverlappedData::allocate(const size_t len) -> OverlappedData *
{
    return new OverlappedData{OVERLAPPED{0}, std::vector<char>(len)};
};

auto OverlappedData::copy(const char *pBuffer, const size_t len) -> OverlappedData *
{
    return new OverlappedData{OVERLAPPED{0}, std::vector<char>(pBuffer, pBuffer + len)};
};