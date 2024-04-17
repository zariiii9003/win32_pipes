/* SPDX-FileCopyrightText: 2024-present Artur Drogunow <artur.drogunow@zf.com>
#
# SPDX-License-Identifier: MIT */

#include <nanobind/nanobind.h>
#include <nanobind/stl/optional.h>
#include <nanobind/stl/tuple.h>

#include "./Pipe.h"
#include "./PipeConnection.h"
#include "./util.h"

#define STRINGIFY(x) #x
#define MACRO_STRINGIFY(x) STRINGIFY(x)

using namespace nanobind::literals;

NB_MODULE(_ext, m)
{
    nanobind::class_<PipeConnection>(m, "PipeConnection")
        .def(nanobind::init<const size_t, const bool, const bool>(),
             "handle"_a,
             "readable"_a = true,
             "writable"_a = true)
        .def("close", &PipeConnection::Close)
        .def("fileno", &PipeConnection::GetHandle)
        .def("send_bytes",
             &PipeConnection::SendBytes,
             "buffer"_a,
             "offset"_a = 0,
             "size"_a   = nanobind::none())
        .def("recv_bytes", &PipeConnection::RecvBytes)
        .def_prop_ro("closed", &PipeConnection::GetClosed)
        .def_prop_ro("readable", &PipeConnection::GetReadable)
        .def_prop_ro("writable", &PipeConnection::GetWritable)
        .def("__enter__", [](PipeConnection &pc) { return &pc; })
        .def(
            "__exit__",
            [](PipeConnection &pc,
               nanobind::handle,
               nanobind::handle,
               nanobind::handle) { pc.Close(); },
            "exc_type"_a.none(),
            "exc_value"_a.none(),
            "traceback"_a.none());

    m.def("Pipe", &Pipe, "duplex"_a = true);

#ifdef VERSION_INFO
    m.attr("__version__") = MACRO_STRINGIFY(VERSION_INFO);
#else
    m.attr("__version__") = "dev";
#endif
}
