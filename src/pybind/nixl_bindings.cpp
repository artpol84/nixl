#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include <iostream>

#include "nixl.h"

namespace py = pybind11;

//JUST FOR TESTING
uintptr_t malloc_passthru(int size) {
    return (uintptr_t) malloc(size);
}

//JUST FOR TESTING
void free_passthru(uintptr_t buf) {
    free((void*) buf);
}

//JUST FOR TESTING
void ba_buf(uintptr_t addr, int size) {
    uint8_t* buf = (uint8_t*) addr;
    for(int i = 0; i<size; i++) buf[i] = 0xba;
}

//JUST FOR TESTING
void verify_transfer(uintptr_t addr1, uintptr_t addr2, int size) {
    uint8_t* buf1 = (uint8_t*) addr1;
    uint8_t* buf2 = (uint8_t*) addr2;

    for(int i = 0; i<size; i++) assert(buf1[i] == buf2[i]);
}

PYBIND11_MODULE(nixl_bindings, m) {

    m.doc() = "pybind11 NIXL plugin: Implements NIXL desciptors and lists, soon Agent API as well";

    //cast types
    py::enum_<backend_type_t>(m, "backend_type")
        .value("UCX", UCX)
        .value("GPUDIRECTIO", GPUDIRECTIO)
        .value("NVMe", NVMe)
        .value("NVMeoF", NVMeoF)
        .export_values();

    py::enum_<mem_type_t>(m, "mem_type")
        .value("DRAM_SEG", DRAM_SEG)
        .value("VRAM_SEG", VRAM_SEG)
        .value("BLK_SEG", BLK_SEG)
        .value("FILE_SEG", FILE_SEG)
        .export_values();

    py::enum_<xfer_state_t>(m, "xfer_state")
        .value("NIXL_XFER_INIT", NIXL_XFER_INIT)
        .value("NIXL_XFER_PROC", NIXL_XFER_PROC)
        .value("NIXL_XFER_DONE", NIXL_XFER_DONE)
        .value("NIXL_XFER_ERR", NIXL_XFER_ERR)
        .export_values();

    //JUST FOR TESTING
    m.def("malloc_passthru", &malloc_passthru);
    m.def("free_passthru", &free_passthru);
    m.def("ba_buf", &ba_buf);
    m.def("verify_transfer", &verify_transfer);

    //TODO: decide which functions need to be publically accessible
    //can overload operators if wanted
    py::class_<nixlBasicDesc>(m, "nixlBasicDesc")
        .def(py::init<uintptr_t, size_t, uint32_t>())
        .def_readwrite("m_addr", &nixlBasicDesc::addr)
        .def_readwrite("m_len", &nixlBasicDesc::len)
        .def_readwrite("m_devId", &nixlBasicDesc::devId)
        .def("covers", &nixlBasicDesc::covers)
        .def("overlaps", &nixlBasicDesc::overlaps)
        .def("print", &nixlBasicDesc::print);

    //TODO: decide which functions need to be publically accessible
    py::class_<nixlDescList<nixlBasicDesc>>(m, "nixlDescList")
        .def(py::init<mem_type_t, bool, bool>())
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

    py::class_<nixlUcxInitParams>(m, "nixlUcxInitParams")
        //implicit constructor
        .def(py::init<>())
        .def_readwrite("m_localAgent", &nixlUcxInitParams::localAgent)
        .def("getType()", &nixlUcxInitParams::getType);
    //inherited nixlUcxInitParams class does not need parent to be bound

    py::class_<nixlDeviceMD>(m, "nixlDeviceMD")
        //implicit constructor
        .def(py::init<>())
        .def_readwrite("m_srcIpAddress", &nixlDeviceMD::srcIpAddress)
        .def_readwrite("m_srcPort", &nixlDeviceMD::srcPort);

    //note: pybind will automatically convert notif_map to python types:
    //so, a Dictionary of string: List<string>
    py::class_<nixlAgent>(m, "nixlAgent")
        .def(py::init<std::string, nixlDeviceMD>())
        //need to convert to pointer
        .def("createBackend", [](nixlAgent &agent, nixlUcxInitParams initParams) -> void* {    
                nixlBackendEngine* ret = agent.createBackend(&initParams);
                std::cout << "created backend " << ret << " with type " << ret->getType() << "\n";
                return (void*) ret; })
        .def("registerMem", [](nixlAgent &agent, nixlDescList<nixlBasicDesc> descs, void* ptr) -> int {    
                nixlBackendEngine* engine = (nixlBackendEngine*) ptr;
                std::cout << "registering mem on " << engine << " with type " << engine->getType() <<"\n";
                assert(engine->getType() == UCX);
                return agent.registerMem(descs, engine); })
        .def("deregisterMem", [](nixlAgent &agent, nixlDescList<nixlBasicDesc> descs, void* backend) -> int {    return agent.deregisterMem(descs, (nixlBackendEngine*) backend); })
        //.def("deregisterMem", &nixlAgent::deregisterMem)
        .def("makeConnection", &nixlAgent::makeConnection)
        //note: slight API change, python cannot receive values by passing refs, so handle must be returned
        .def("createXferReq", [](nixlAgent &agent, 
                                 const nixlDescList<nixlBasicDesc> &local_descs,
                                 const nixlDescList<nixlBasicDesc> &remote_descs,
                                 const std::string &remote_agent,
                                 const std::string &notif_msg, 
                                 int direction) -> uintptr_t {
                    nixlXferReqH* handle;
                    int ret = agent.createXferReq(local_descs, remote_descs, remote_agent, notif_msg, direction, handle);
                    if(ret == -1) return 0;
                    else return (uintptr_t) handle;
                })
        .def("invalidateXferReq", [](nixlAgent &agent, uintptr_t reqh) -> void {
                    agent.invalidateXferReq((nixlXferReqH*) reqh);
                })
        .def("postXferReq", [](nixlAgent &agent, uintptr_t reqh) -> xfer_state_t {
                    return agent.postXferReq((nixlXferReqH*) reqh);
                })
        .def("getXferStatus", [](nixlAgent &agent, uintptr_t reqh) -> xfer_state_t { 
                    return agent.getXferStatus((nixlXferReqH*) reqh);
                })
        .def("addNewNotifs", &nixlAgent::addNewNotifs)
        .def("getLocalMD", [](nixlAgent &agent) {
                    //python can only interpret text strings
                    return py::bytes(agent.getLocalMD());
                })
        .def("loadRemoteMD", &nixlAgent::loadRemoteMD)
        .def("invalidateRemoteMD", &nixlAgent::invalidateRemoteMD)
        .def("sendLocalMD", &nixlAgent::sendLocalMD)
        //pybind did not like this function
        //.def("fetchRemoteMD", [](std::string name) -> int { return 0; })
        .def("fetchRemoteMD", &nixlAgent::fetchRemoteMD)
        .def("invalidateLocalMD", &nixlAgent::invalidateLocalMD);
}
