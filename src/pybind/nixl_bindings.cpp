#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include "nixl.h"

namespace py = pybind11;

PYBIND11_MODULE(nixl_bindings, m) {

    m.doc() = "pybind11 NIXL plugin: Implements NIXL desciptors and lists, soon Agent API as well";

    //cast types
    py::enum_<backend_type_t>(m, "backend_type")
        .value("UCX", UCX)
        .value("GPUDIRECTIO", GPUDIRECTIO)
        .value("NVMe", NVMe)
        .value("NVMeoF", NVMeoF)
        .export_values();

    py::enum_<memory_type_t>(m, "memory_type")
        .value("DRAM_SEG", DRAM_SEG)
        .value("VRAM_SEG", VRAM_SEG)
        .value("BLK_SEG", BLK_SEG)
        .value("FILE_SEG", FILE_SEG)
        .export_values();

    //TODO: decide which functions need to be publically accessible
    //can overload operators if wanted
    py::class_<nixlBasicDesc>(m, "nixlBasicDesc")
        .def(py::init<uintptr_t, size_t, uint32_t>())
        .def("covers", &nixlBasicDesc::covers)
        .def("overlaps", &nixlBasicDesc::overlaps)
        .def("print", &nixlBasicDesc::print);

    //TODO: decide which functions need to be publically accessible
    py::class_<nixlDescList<nixlBasicDesc>>(m, "nixlDescList")
        .def(py::init<memory_type_t, bool, bool>())
        .def("getType", &nixlDescList<nixlBasicDesc>::getType)
        .def("isUnifiedAddr", &nixlDescList<nixlBasicDesc>::isUnifiedAddr)
        .def("descCount", &nixlDescList<nixlBasicDesc>::descCount)
        .def("isEmpty", &nixlDescList<nixlBasicDesc>::isEmpty)
        .def("isSorted", &nixlDescList<nixlBasicDesc>::isSorted)
        .def("addDesc", &nixlDescList<nixlBasicDesc>::addDesc)
        //TODO: figure out pybind11 overloaded function
        //.def("remDesc", &nixlDescList<nixlBasicDesc>::remDesc)
        .def("clear", &nixlDescList<nixlBasicDesc>::clear)
        .def("print", &nixlDescList<nixlBasicDesc>::print);
}
