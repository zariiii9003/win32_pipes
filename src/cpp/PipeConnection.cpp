/* SPDX-FileCopyrightText: 2024-present Artur Drogunow <artur.drogunow@zf.com>
#
# SPDX-License-Identifier: MIT */

#include "./PipeConnection.h"
#include "./util.h"
#include <nanobind/nanobind.h>

PipeConnection::PipeConnection(const size_t handle,
                               const bool   readable,
                               const bool   writable)
    : _handle{reinterpret_cast<const HANDLE>(handle)},
      _readable{readable},
      _writable{writable}
{
    if (!readable && !writable)
        throw nanobind::value_error(
            "at least one of `readable` and `writable` must be True");

    _completionPort = CreateIoCompletionPort(_handle, nullptr, 0, 0);
    if (_completionPort == NULL)
        CleanupAndThrow(
            0,
            "PipeConnection::PipeConnection.CreateIoCompletionPort");

    _thread = std::thread(&PipeConnection::MonitorIoCompletion, this);
};

PipeConnection::~PipeConnection() { Close(); }
auto PipeConnection::Close() -> void
{
    if (!_closed) {
        _closed = true;

        // close handles
        if (!CloseHandle(_handle))
            Win32ErrorExit(0, "PipeConnection::Close.CloseHandle.1");
        if (!CloseHandle(_completionPort))
            Win32ErrorExit(0, "PipeConnection::Close.CloseHandle.2");

        // wait until thread stops
        if (_thread.joinable())
            _thread.join();
    }
}

auto PipeConnection::GetHandle() const -> size_t
{
    return reinterpret_cast<size_t>(_handle);
}

auto PipeConnection::GetReadable() const -> bool { return _readable; }

auto PipeConnection::GetWritable() const -> bool { return _writable; }

auto PipeConnection::GetClosed() -> bool { return _closed; }

auto PipeConnection::SendBytes(const nanobind::bytes       buffer,
                               const size_t                offset,
                               const std::optional<size_t> size) -> void
{
    if (_closed)
        throw std::exception("handle is closed");
    if (!_writable)
        throw std::exception("connection is read-only");

    CheckThread();

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
    auto pOdRaw = new OverlappedData(buffer.c_str() + offset, _size);
    {
        auto             pOdUnique = std::unique_ptr<OverlappedData>(pOdRaw);
        std::scoped_lock lock(_TxQueueMutex);
        _TxQueue.push(std::move(pOdUnique));
    }
    if (!WriteFile(_handle,
                   &pOdRaw->vector.front(),
                   pOdRaw->vector.size(),
                   NULL,
                   &pOdRaw->overlapped)) {
        auto errNo = GetLastError();
        switch (errNo) {
            case ERROR_SUCCESS:
            case ERROR_IO_PENDING:
            case ERROR_IO_INCOMPLETE:
                break;
            default:
                CleanupAndThrow(errNo, "PipeConnection::SendBytes.WriteFile");
        }
    }
}

auto PipeConnection::RecvBytes() -> std::optional<nanobind::bytes>
{
    if (_closed)
        throw std::exception("handle is closed");
    if (!_readable)
        throw std::exception("connection is write-only");

    CheckThread();

    // Get RxQueue content and release lock asap. Return if empty.
    std::unique_ptr<std::vector<char>> rxMessage;
    {
        std::scoped_lock lock(_RxQueueMutex);
        if (_RxQueue.empty())
            return {};
        rxMessage = std::move(_RxQueue.front());
        _RxQueue.pop();
    }

    // create python bytes object from vector
    auto nbbytes = new nanobind::bytes(&rxMessage->front(), rxMessage->size());
    return std::move(*nbbytes);
}

auto PipeConnection::MonitorIoCompletion() -> void
{
    ULONG_PTR completionKey{0};
    size_t    bytesReadTotal{0};

    if (_readable) {
        // initialize read buffer
        _RxBuffer.resize(static_cast<size_t>(BUFSIZE));
        // start first read operation
        if (!ReadFile(_handle,
                      &_RxBuffer.front(),
                      _RxBuffer.size(),
                      nullptr,
                      &rxOv)) {
            auto errNo = GetLastError();
            if (errNo != ERROR_IO_PENDING) {
                _threadErrContext =
                    "PipeConnection::MonitorIoCompletion.ReadFile";
                goto threadExit;
            }
        }
    }

    while (!_closed) {
        // wait for completed operation
        DWORD        numberOfBytesTransferred{0};
        LPOVERLAPPED pOv     = nullptr;
        auto         gqcsRes = GetQueuedCompletionStatus(_completionPort,
                                                 &numberOfBytesTransferred,
                                                 &completionKey,
                                                 &pOv,
                                                 INFINITE);
        if (!gqcsRes) {
            DWORD errNo = GetLastError();
            switch (errNo) {
                case ERROR_SUCCESS:
                case ERROR_MORE_DATA:
                    break;
                default:
                    _threadErrContext =
                        "MonitorIoCompletion.GetQueuedCompletionStatus";
                    goto threadExit;
            }
        };

        if (pOv == &rxOv) {
            // receive operation completed
            auto ovRes = GetOverlappedResult(_handle,
                                             pOv,
                                             &numberOfBytesTransferred,
                                             false);
            bytesReadTotal += static_cast<size_t>(numberOfBytesTransferred);

            if (!ovRes) {
                if (GetLastError() == ERROR_MORE_DATA) {
                    // check how much data of the message is missing
                    DWORD bytesLeftThisMessage;
                    PeekNamedPipe(_handle,
                                  nullptr,
                                  0,
                                  nullptr,
                                  nullptr,
                                  &bytesLeftThisMessage);

                    // check if rxBuffer size is sufficient
                    size_t msgSize = bytesReadTotal +
                                     static_cast<size_t>(bytesLeftThisMessage);
                    if (msgSize > _RxBuffer.size())
                        _RxBuffer.resize(msgSize);

                    // Reset OVERLAPPED and read the rest of the message
                    rxOv = OVERLAPPED{0};
                    if (!ReadFile(_handle,
                                  &_RxBuffer.at(bytesReadTotal),
                                  bytesLeftThisMessage,
                                  nullptr,
                                  &rxOv)) {
                        _threadErrContext =
                            "PipeConnection::MonitorIoCompletion.ReadFile";
                        goto threadExit;
                    }
                    continue;
                }
                else {
                    _threadErrContext = "PipeConnection::MonitorIoCompletion."
                                        "GetOverlappedResult";
                    goto threadExit;
                }
            }

            // create new vector, which will be saved in RxQueue
            auto rxMessageOut = std::unique_ptr<std::vector<char>>(
                new std::vector<char>(BUFSIZE));
            rxMessageOut->swap(_RxBuffer);
            rxMessageOut->resize(bytesReadTotal);
            bytesReadTotal = 0; // reset bytesReadTotal

            // push the new vector to the queue
            std::scoped_lock lock(_RxQueueMutex);
            _RxQueue.push(std::move(rxMessageOut));

            // reset rxOv and start next receive operation
            rxOv = OVERLAPPED{0};
            if (!ReadFile(_handle,
                          &_RxBuffer.front(),
                          _RxBuffer.size(),
                          NULL,
                          &rxOv)) {
                auto errNo = GetLastError();
                if (errNo != ERROR_IO_PENDING) {
                    _threadErrContext =
                        "PipeConnection::MonitorIoCompletion.ReadFile";
                    goto threadExit;
                }
            }
        }
        else {
            // send operation completed
            std::scoped_lock lock(_TxQueueMutex);
            if (!_TxQueue.empty() && (&_TxQueue.front()->overlapped == pOv)) {
                auto pOd = std::move(_TxQueue.front());
                _TxQueue.pop();
            }
        }
    }
threadExit:
    _threadErr = GetLastError();
};

inline auto PipeConnection::CheckThread() -> void
{
    if (_threadErr != ERROR_SUCCESS)
        if (_threadErrContext.has_value()) {
            CleanupAndThrow(_threadErr, _threadErrContext.value());
        }
        else {
            CleanupAndThrow(_threadErr);
        }
}

auto PipeConnection::CleanupAndThrow(DWORD                      errNo,
                                     std::optional<std::string> context) -> void
{
    Close();
    Win32ErrorExit(errNo, context);
}

OverlappedData::OverlappedData(const char *pBuffer, const size_t len)
{
    overlapped = OVERLAPPED{0};
    vector     = std::vector<char>(pBuffer, pBuffer + len);
};
