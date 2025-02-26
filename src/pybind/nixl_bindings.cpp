#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/operators.h>

#include <iostream>

#include "nixl.h"

namespace py = pybind11;

PYBIND11_MODULE(nixl_bindings, m) {

    //TODO: each nixl class and/or function can be documented in place
    m.doc() = "pybind11 NIXL plugin: Implements NIXL desciptors and lists, soon Agent API as well";

    //cast types
    py::enum_<nixl_backend_t>(m, "nixl_backend_t")
        .value("UCX", UCX)
        .value("GPUDIRECTIO", GPUDIRECTIO)
        .value("NVMe", NVMe)
        .value("NVMeoF", NVMeoF)
        .export_values();

    py::enum_<nixl_mem_t>(m, "nixl_mem_t")
        .value("DRAM_SEG", DRAM_SEG)
        .value("VRAM_SEG", VRAM_SEG)
        .value("BLK_SEG", BLK_SEG)
        .value("FILE_SEG", FILE_SEG)
        .export_values();

    py::enum_<nixl_xfer_state_t>(m, "nixl_xfer_state_t")
        .value("NIXL_XFER_INIT", NIXL_XFER_INIT)
        .value("NIXL_XFER_PROC", NIXL_XFER_PROC)
        .value("NIXL_XFER_DONE", NIXL_XFER_DONE)
        .value("NIXL_XFER_ERR", NIXL_XFER_ERR)
        .export_values();

    py::enum_<nixl_xfer_op_t>(m, "nixl_xfer_op_t")
        .value("NIXL_READ", NIXL_READ)
        .value("NIXL_RD_FLUSH", NIXL_RD_FLUSH)
        .value("NIXL_RD_NOTIF", NIXL_RD_NOTIF)
        .value("NIXL_WRITE", NIXL_WRITE)
        .value("NIXL_WR_FLUSH", NIXL_WR_FLUSH)
        .value("NIXL_WR_NOTIF", NIXL_WR_NOTIF)
        .export_values();

    py::enum_<nixl_status_t>(m, "nixl_status_t")
        .value("NIXL_SUCCESS", NIXL_SUCCESS)
        .value("NIXL_ERR_INVALID_PARAM", NIXL_ERR_INVALID_PARAM)
        .value("NIXL_ERR_BACKEND", NIXL_ERR_BACKEND)
        .value("NIXL_ERR_NOT_FOUND", NIXL_ERR_NOT_FOUND)
        .value("NIXL_ERR_NYI", NIXL_ERR_NYI)
        .value("NIXL_ERR_BAD", NIXL_ERR_BAD)
        .export_values();

    py::class_<nixlBasicDesc>(m, "nixlBasicDesc")
        .def(py::init<uintptr_t, size_t, uint32_t>())
        .def_readwrite("m_addr", &nixlBasicDesc::addr)
        .def_readwrite("m_len", &nixlBasicDesc::len)
        .def_readwrite("m_devId", &nixlBasicDesc::devId)
        //this is how operators are bound in pybind11/operators.h:
        .def(py::self == py::self)
        .def(py::self != py::self)
        .def("covers", &nixlBasicDesc::covers)
        .def("overlaps", &nixlBasicDesc::overlaps)
        .def("print", &nixlBasicDesc::print);

    py::class_<nixlDescList<nixlBasicDesc>>(m, "nixlDescList")
        .def(py::init<nixl_mem_t, bool, bool>())
        .def("getType", &nixlDescList<nixlBasicDesc>::getType)
        .def("isUnifiedAddr", &nixlDescList<nixlBasicDesc>::isUnifiedAddr)
        .def("descCount", &nixlDescList<nixlBasicDesc>::descCount)
        .def("isEmpty", &nixlDescList<nixlBasicDesc>::isEmpty)
        .def("isSorted", &nixlDescList<nixlBasicDesc>::isSorted)
        .def("__getitem__", &nixlDescList<nixlBasicDesc>::operator[])
        .def(py::self == py::self)
        .def("addDesc", &nixlDescList<nixlBasicDesc>::addDesc, py::arg("desc"), py::arg("overlap_check") = true)
        .def("remDesc", &nixlDescList<nixlBasicDesc>::remDesc)
        .def("clear", &nixlDescList<nixlBasicDesc>::clear)
        .def("print", &nixlDescList<nixlBasicDesc>::print)
        .def(py::pickle(
            [](const nixlDescList<nixlBasicDesc>& self) { // __getstate__
                nixlSerDes serdes;
                nixl_status_t ret = self.serialize(&serdes);
                assert(ret == NIXL_SUCCESS);
                return py::bytes(serdes.exportStr());
            },
            [](py::bytes serdes_str) { // __setstate__
                nixlSerDes serdes;
                serdes.importStr(std::string(serdes_str));
                nixlDescList<nixlBasicDesc> newObj =
                    nixlDescList<nixlBasicDesc>(&serdes);
                return newObj;
            }
        ));

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
        .def("createBackend", [](nixlAgent &agent, nixlUcxInitParams initParams) -> uintptr_t {
                    return (uintptr_t) agent.createBackend(&initParams);
            })
        .def("registerMem", [](nixlAgent &agent, nixlDescList<nixlBasicDesc> descs, uintptr_t backend) -> nixl_status_t {
                    return agent.registerMem(descs, (nixlBackendEngine*) backend);
                })
        .def("deregisterMem", [](nixlAgent &agent, nixlDescList<nixlBasicDesc> descs, uintptr_t backend) -> nixl_status_t {
                    return agent.deregisterMem(descs, (nixlBackendEngine*) backend);
                })
        .def("makeConnection", &nixlAgent::makeConnection)
        //note: slight API change, python cannot receive values by passing refs, so handle must be returned
        .def("createXferReq", [](nixlAgent &agent,
                                 const nixlDescList<nixlBasicDesc> &local_descs,
                                 const nixlDescList<nixlBasicDesc> &remote_descs,
                                 const std::string &remote_agent,
                                 const std::string &notif_msg,
                                 const nixl_xfer_op_t &operation) -> uintptr_t {
                    nixlXferReqH* handle;
                    nixl_status_t ret = agent.createXferReq(local_descs, remote_descs, remote_agent, notif_msg, operation, handle);
                    if (ret != NIXL_SUCCESS) return (uintptr_t) nullptr;
                    else return (uintptr_t) handle;
                })
        .def("getXferBackend", [](nixlAgent &agent, uintptr_t reqh) -> uintptr_t {
                    return (uintptr_t) agent.getXferBackend((nixlXferReqH*) reqh);
            })
        .def("prepXferSide", [](nixlAgent &agent,
                                const nixlDescList<nixlBasicDesc> &descs,
                                const std::string &remote_agent,
                                uintptr_t backend) -> uintptr_t {
                    nixlXferSideH* handle;
                    nixl_status_t ret = agent.prepXferSide(descs, remote_agent, (nixlBackendEngine*) backend, handle);
                    if (ret != NIXL_SUCCESS) return (uintptr_t) nullptr;
                    else return (uintptr_t) handle;
                })
        .def("makeXferReq", [](nixlAgent &agent,
                               uintptr_t local_side,
                               const std::vector<int> &local_indices,
                               uintptr_t remote_side,
                               const std::vector<int> &remote_indices,
                               const std::string &notif_msg,
                               const nixl_xfer_op_t &operation,
                               const bool no_checks) -> uintptr_t {
                    nixlXferReqH* handle;
                    nixl_status_t ret = agent.makeXferReq((nixlXferSideH*) local_side, local_indices,
                                                          (nixlXferSideH*) remote_side, remote_indices,
                                                          notif_msg, operation, handle, no_checks);
                    if (ret != NIXL_SUCCESS) return (uintptr_t) nullptr;
                    else return (uintptr_t) handle;
                },             py::arg("local_side"),
                               py::arg("local_indices"),
                               py::arg("remote_side"),
                               py::arg("remote_indices"),
                               py::arg("notif_msg"),
                               py::arg("operation"),
                               py::arg("no_checks") = false )
        .def("invalidateXferReq", [](nixlAgent &agent, uintptr_t reqh) -> void {
                    agent.invalidateXferReq((nixlXferReqH*) reqh);
                })
        .def("invalidateXferSide", [](nixlAgent &agent, uintptr_t handle) -> void {
                    agent.invalidateXferSide((nixlXferSideH*) handle);
                })
        .def("postXferReq", [](nixlAgent &agent, uintptr_t reqh) -> nixl_xfer_state_t {
                    return agent.postXferReq((nixlXferReqH*) reqh);
                })
        .def("getXferStatus", [](nixlAgent &agent, uintptr_t reqh) -> nixl_xfer_state_t {
                    return agent.getXferStatus((nixlXferReqH*) reqh);
                })
        .def("getNotifs", [](nixlAgent &agent, nixl_notifs_t notif_map) -> nixl_notifs_t {
                    int n_new  = agent.getNotifs(notif_map);
                    if (n_new == 0) return notif_map;

                    nixl_notifs_t ret_map;
                    for (const auto& pair : notif_map) {
                        std::vector<std::string> agent_notifs;

                        for(const auto& str : pair.second)  {
                            agent_notifs.push_back(py::bytes(str));
                        }

                        ret_map[pair.first] = agent_notifs;
                    }
                    return ret_map;
                })
        .def("genNotif", &nixlAgent::genNotif)
        .def("getLocalMD", [](nixlAgent &agent) {
                    //python can only interpret text strings
                    return py::bytes(agent.getLocalMD());
                })
        .def("loadRemoteMD", &nixlAgent::loadRemoteMD)
        .def("invalidateRemoteMD", &nixlAgent::invalidateRemoteMD);
}
