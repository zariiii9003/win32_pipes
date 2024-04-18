/* SPDX-FileCopyrightText: 2024-present Artur Drogunow <artur.drogunow@zf.com>
#
# SPDX-License-Identifier: MIT */

#include <nanobind/nanobind.h>
#include <nanobind/stl/optional.h>
#include <nanobind/stl/tuple.h>

#include "./Pipe.h"
#include "./PipeClient.h"
#include "./PipeConnection.h"
#include "./PipeListener.h"
#include "./util.h"

#define STRINGIFY(x) #x

using namespace nanobind::literals;

NB_MODULE(_ext, m)
{
    nanobind::class_<PipeConnection>(m, "PipeConnection")
        .def(nanobind::init<const size_t, const bool, const bool>(),
             "handle"_a,
             "readable"_a = true,
             "writable"_a = true)
        .def("start_thread", &PipeConnection::startThread)
        .def("close", &PipeConnection::close)
        .def("fileno", &PipeConnection::getHandle)
        .def("send_bytes",
             &PipeConnection::sendBytes,
             "buffer"_a,
             "offset"_a = 0,
             "size"_a   = nanobind::none())
        .def("recv_bytes", &PipeConnection::recvBytes)
        .def_prop_ro("closed", &PipeConnection::getClosed)
        .def_prop_ro("readable", &PipeConnection::getReadable)
        .def_prop_ro("writable", &PipeConnection::getWritable)
        .def("__enter__",
             [](PipeConnection &pc) {
                 pc.startThread();
                 return &pc;
             })
        .def(
            "__exit__",
            [](PipeConnection &pc,
               nanobind::handle,
               nanobind::handle,
               nanobind::handle) { pc.close(); },
            "exc_type"_a.none(),
            "exc_value"_a.none(),
            "traceback"_a.none());

    m.def("Pipe", &pipe, "duplex"_a = true, "start_thread"_a = false);

    nanobind::class_<PipeListener>(m, "PipeListener")
        .def(nanobind::init<std::string, std::optional<size_t>>(),
             "address"_a,
             "backlog"_a = nanobind::none())
        .def("accept", &PipeListener::accept)
        .def("close", &PipeListener::close)
        .def_prop_ro("address", &PipeListener::getAddress)
        .def_prop_ro("last_accepted",
                     [](PipeListener &pl) { return nanobind::none(); })
        .def("__enter__", [](PipeListener &pl) { return &pl; })
        .def(
            "__exit__",
            [](PipeListener &pl,
               nanobind::handle,
               nanobind::handle,
               nanobind::handle) { pl.close(); },
            "exc_type"_a.none(),
            "exc_value"_a.none(),
            "traceback"_a.none());
    m.def("PipeClient", &pipeClient, "address"_a);

#ifdef VERSION_INFO
    m.attr("__version__") = STRINGIFY(VERSION_INFO);
#else
    m.attr("__version__") = "dev";
#endif
}
