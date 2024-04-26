/* SPDX-FileCopyrightText: 2024-present Artur Drogunow <artur.drogunow@zf.com>
#
# SPDX-License-Identifier: MIT */

#include "./PipeConnection.h"
#include "./util.h"
#include <nanobind/nanobind.h>

PipeConnection::PipeConnection(size_t handle, bool readable, bool writable)
    : _handle{reinterpret_cast<const HANDLE>(handle)},
      _readable{readable},
      _writable{writable}
{
    if (!readable && !writable)
        throw nanobind::value_error(
            "at least one of `readable` and `writable` must be True");
};

PipeConnection::~PipeConnection() { close(); }
auto PipeConnection::close() -> void
{
    if (!_closed) {
        _closed = true;

        if (_readable) {
            SetEvent(_RxQueueEvent);
        }

        // close handles
        if (!CloseHandle(_handle))
            Win32ErrorExit(0);
        if (_started && !CloseHandle(_completionPort))
            Win32ErrorExit(0);
        if (_readable) {
            CloseHandle(_RxQueueEvent);
        }

        // wait until thread stops
        if (_thread.joinable())
            _thread.join();
    }
}

inline auto PipeConnection::startThread() -> void
{
    if (!_started) {
        _completionPort = CreateIoCompletionPort(_handle, nullptr, 0, 0);
        if (_completionPort == NULL)
            cleanupAndThrowExc(0);

        if (_readable) {
            // Create event to signal, that RxQueue contains value
            _RxQueueEvent = CreateEvent(nullptr, TRUE, FALSE, nullptr);

            // initialize read buffer
            _RxBuffer.resize(static_cast<size_t>(BUFSIZE));

            // start first read operation
            if (!ReadFile(_handle,
                          &_RxBuffer.front(),
                          _RxBuffer.size(),
                          nullptr,
                          &_rxOv)) {
                auto errNo = GetLastError();
                if (errNo != ERROR_IO_PENDING) {
                    cleanupAndThrowExc();
                }
            }
        }

        _thread  = std::thread(&PipeConnection::monitorIoCompletion, this);
        _started = true;
    }
}

auto PipeConnection::getHandle() const -> size_t
{
    return reinterpret_cast<size_t>(_handle);
}

auto PipeConnection::getReadable() const -> bool { return _readable; }

auto PipeConnection::getWritable() const -> bool { return _writable; }

auto PipeConnection::getClosed() -> bool { return _closed; }

auto PipeConnection::sendBytes(const nanobind::bytes       buffer,
                               const size_t                offset,
                               const std::optional<size_t> size,
                               const bool                  blocking) -> void
{
    if (_closed) [[unlikely]]
        throw std::exception("handle is closed");
    if (!_writable) [[unlikely]]
        throw std::exception("connection is read-only");

    checkThread();

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
    auto pOd = std::shared_ptr<OverlappedData>(
        new OverlappedData(buffer.c_str() + offset, _size));
    {
        std::scoped_lock lock(_TxQueueMutex);
        _TxQueue.push(pOd);
    }
    if (!WriteFile(_handle,
                   &pOd->vector.front(),
                   pOd->vector.size(),
                   NULL,
                   &pOd->overlapped)) {
        auto errNo = GetLastError();
        switch (errNo) {
            case ERROR_SUCCESS:
            case ERROR_IO_INCOMPLETE:
            case ERROR_IO_PENDING:
                break;
            default:
                cleanupAndThrowExc(errNo);
        }
    }

    if (blocking) {
        auto  nogil = nanobind::gil_scoped_release();
        DWORD numberOfBytesTransferred;
        if (!GetOverlappedResult(_handle,
                                 &pOd->overlapped,
                                 &numberOfBytesTransferred,
                                 TRUE)) {
            cleanupAndThrowExc();
        }
    }
}

auto PipeConnection::recvBytes(std::optional<int> maxLength,
                               const bool         blocking)
    -> std::optional<nanobind::bytes>
{
    if (!_readable) [[unlikely]]
        throw std::exception("connection is write-only");

    startThread();

    while (true) {
        if (_closed) [[unlikely]]
            throw std::exception("handle is closed");

        // Get RxQueue content and release lock asap
        _RxQueueMutex.lock();
        if (!_RxQueue.empty()) {
            // RxQueue contains a message
            auto rxMessage = std::move(_RxQueue.front());
            _RxQueue.pop();
            if (_RxQueue.empty())
                ResetEvent(_RxQueueEvent);
            _RxQueueMutex.unlock();

            // create python bytes object from vector;
            return nanobind::bytes(&rxMessage->front(), rxMessage->size());
        };
        _RxQueueMutex.unlock();

        // check thread health, if RxQueue is empty
        checkThread();

        // return None, if non-blocking
        if (!blocking)
            return {};

        {
            // wait for event in case of blocking call
            auto nogil   = nanobind::gil_scoped_release();
            auto waitRes = WaitForSingleObject(_RxQueueEvent, 2000);
        }
    }
}

auto PipeConnection::monitorIoCompletion() -> void
{
    ULONG_PTR completionKey{0};
    size_t    bytesReadTotal{0};

    while (!_closed) {
        // wait for completed operation
        DWORD        numberOfBytesTransferred{0};
        LPOVERLAPPED pOv     = nullptr;
        auto         gqcsRes = GetQueuedCompletionStatus(_completionPort,
                                                 &numberOfBytesTransferred,
                                                 &completionKey,
                                                 &pOv,
                                                 INFINITE);
        if (pOv == nullptr) {
            // GetQueuedCompletionStatus failed
            goto threadExit;
        }
        else if (pOv == &_rxOv) {
            // receive operation completed
            GetOverlappedResult(_handle, pOv, &numberOfBytesTransferred, false);
            auto ovRes = GetLastError();
            bytesReadTotal += static_cast<size_t>(numberOfBytesTransferred);
            switch (ovRes) {
                case ERROR_SUCCESS: { // create new vector, which will be saved
                                      // in RxQueue
                    auto rxMessageOut = std::shared_ptr<std::vector<char>>(
                        new std::vector<char>(BUFSIZE));
                    rxMessageOut->swap(_RxBuffer);
                    rxMessageOut->resize(bytesReadTotal);

                    bytesReadTotal = 0; // reset bytesReadTotal

                    // push the new vector to the queue
                    _RxQueueMutex.lock();
                    _RxQueue.push(rxMessageOut);
                    SetEvent(_RxQueueEvent);
                    _RxQueueMutex.unlock();

                    // reset rxOv and start next receive operation
                    std::memset(&_rxOv, 0, sizeof(_rxOv));
                    if (!ReadFile(_handle,
                                  &_RxBuffer.front(),
                                  _RxBuffer.size(),
                                  NULL,
                                  &_rxOv)) {
                        auto errNo = GetLastError();
                        if (errNo != ERROR_IO_PENDING) {
                            goto threadExit;
                        }
                    }
                    break;
                }
                case ERROR_MORE_DATA: {
                    // check how much data of the message is missing
                    DWORD bytesLeftThisMessage;
                    PeekNamedPipe(_handle,
                                  nullptr,
                                  0,
                                  nullptr,
                                  nullptr,
                                  &bytesLeftThisMessage);

                    // check if rxBuffer size is sufficient
                    auto msgSize = bytesReadTotal +
                                   static_cast<size_t>(bytesLeftThisMessage);
                    _RxBuffer.resize(msgSize);

                    // Reset OVERLAPPED and read the rest of the message
                    std::memset(&_rxOv, 0, sizeof(_rxOv));
                    if (!ReadFile(_handle,
                                  &_RxBuffer.at(bytesReadTotal),
                                  bytesLeftThisMessage,
                                  nullptr,
                                  &_rxOv)) {
                        goto threadExit;
                    }
                    break;
                }
                default: {
                    goto threadExit;
                }
            }
        }
        else {
            // send operation completed
            _TxQueueMutex.lock();
            auto pOd = std::move(_TxQueue.front());
            _TxQueue.pop();
            _TxQueueMutex.unlock();

            if (!GetOverlappedResult(_handle,
                                     pOv,
                                     &numberOfBytesTransferred,
                                     false)) {
                goto threadExit;
            }
        }
    }
threadExit:
    _threadErr = GetLastError();
};

inline auto PipeConnection::checkThread() -> void
{
    if (!_started) [[unlikely]]
        startThread();
    if (_threadErr != ERROR_SUCCESS) [[unlikely]]
        cleanupAndThrowExc(_threadErr);
}

auto PipeConnection::cleanupAndThrowExc(DWORD errNo) -> void
{
    close();
    Win32ErrorExit(errNo);
}

OverlappedData::OverlappedData(const char *pBuffer, const size_t len)
{
    overlapped = OVERLAPPED{0};
    vector     = std::vector<char>(pBuffer, pBuffer + len);
};
