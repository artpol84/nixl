#include "nixl.h"
#include "ucx_backend.h"
#include "serdes.h"

nixlAgentDataPrivate::~nixlAgentDataPrivate() {
    for (auto & elm: remoteSections)
        delete elm.second;
    remoteSections.clear();

    for (auto & elm: nixlBackendEngines)
        delete elm;
    nixlBackendEngines.clear();

}

nixlMetadataHandler::nixlMetadataHandler(std::string& ip_address, uint16_t port){
    this->ipAddress = ip_address;
    this->port      = port;
}

nixlMetadataHandler::~nixlMetadataHandler(){
}

nixlAgent::nixlAgent(const std::string &name,
                     const nixlDeviceMetadata &devs){
   data.name                    = name;
   data.deviceMeta.srcIpAddress = devs.srcIpAddress;
   data.deviceMeta.srcPort      = devs.srcPort;
   // data.deviceMeta.topology      = devs.topology;
}

nixlAgent::~nixlAgent() {
}

nixlBackendEngine* nixlAgent::createBackend(nixlBackendInitParams *params) {
    if (params->getType()==UCX) {
        nixlBackendEngine* ucx = (nixlBackendEngine*) new nixlUcxEngine(
                                 (nixlUcxInitParams*) params);
        data.nixlBackendEngines.push_back(ucx);
        data.memorySection.addBackendHandler(ucx);
        data.connMd[UCX] = ucx->getConnInfo();
        return ucx;
    }
    return nullptr;
}

int nixlAgent::registerMem(const nixlDescList<nixlBasicDesc>& descs,
                           nixlBackendEngine *backend) {
    return (data.memorySection.addDescList(descs, backend));
}

int nixlAgent::deregisterMem(const nixlDescList<nixlBasicDesc>& descs,
                             nixlBackendEngine *backend) {
    // Might not need unified and sorted info
    nixlDescList<nixlMetaDesc> resp(descs.getType(),
                                    descs.isUnifiedAddr(),
                                    descs.isSorted());
    data.memorySection.populate(descs, resp, backend->getType());
    return (data.memorySection.remDescList(resp, backend));
}

int nixlAgent::makeConnection(std::string remote_agent, int direction) {
    if (data.nixlBackendEngines.size()==0)
        return -1;

    // TBD: Support for more backends and determine which need to be connected
    if (direction)
        return (data.nixlBackendEngines[0]->listenForConnection(remote_agent));
    else
        return (data.nixlBackendEngines[0]->makeConnection(remote_agent));
}

int nixlAgent::createTransferReq(nixlDescList<nixlBasicDesc>& local_descs,
                                 nixlDescList<nixlBasicDesc>& remote_descs,
                                 std::string remote_agent,
                                 std::string notif_msg,
                                 int direction,
                                 nixlTransferRequest* &req_handle) {

    int ret;
    if (data.remoteSections.count(remote_agent)==0)
        return -1;

    nixlTransferRequest *handle = new nixlTransferRequest;
    // Might not need unified and sorted info
    handle->initiator_descs = new nixlDescList<nixlMetaDesc> (
                                      local_descs.getType(),
                                      local_descs.isUnifiedAddr(),
                                      local_descs.isSorted());

    handle->engine = data.memorySection.findQuery (local_descs,
                                                   *handle->initiator_descs);
    if (handle->engine==nullptr)
        return -1;

    handle->target_descs = new nixlDescList<nixlMetaDesc> (
                                   remote_descs.getType(),
                                   remote_descs.isUnifiedAddr(),
                                   remote_descs.isSorted());

    ret = data.remoteSections[remote_agent]->populate(remote_descs,
               *handle->target_descs, handle->engine->getType());
    if (ret<0)
        return ret;

    handle->notif_msg = notif_msg;
    // Based on notif_msg we can set WRITE_W_NOTIF
    handle->backend_op = direction ? WRITE : READ;
    handle->state = NIXL_INIT;

    req_handle = handle;

    // Not bookkeeping transferRequests, assuming user releases all
    return 0;
}

void nixlAgent::invalidateRequest(nixlTransferRequest *req) {
    delete req;
}

int nixlAgent::postRequest(nixlTransferRequest *req) {
    if (req==nullptr)
        return -1;
    return (req->engine->transfer (*req->initiator_descs,
                                   *req->target_descs,
                                   req->backend_op,
                                   req->notif_msg,
                                   req->backend_handle));
}

int nixlAgent::sendNotification(nixlTransferRequest *req) {
    // TBD
    return 0;
}

transfer_state_t nixlAgent::getStatus (nixlTransferRequest *req) {
    req->state =  req->engine->checkTransfer(req->backend_handle);

    return req->state;
}

std::string nixlAgent::getMetadata () {
    // data.connMd was populated when the backend was created
    // For now just single conn of UCX
    auto it = data.connMd.find(UCX);
    if (it==data.connMd.end())
        return "";

    // TBD supporting more than one entry
    nixlSerDes sd;
    if (sd.addStr("Agent", data.name)<0)
        return "";
    if (sd.addStr("Conn", it->second)<0)
        return "";
    if (data.memorySection.serialize(&sd)<0)
        return "";

    // Maybe should copy to separate string
    return sd.exportStr();
}

int nixlAgent::loadMetadata (std::string remote_metadata) {
    nixlSerDes sd;

    if (sd.importStr(remote_metadata)<0)
        return -1;

    std::string remote_agent = sd.getStr("Agent");
    if (remote_agent.size()==0)
        return -1;

    std::string conn_info = sd.getStr("Conn");
    if (conn_info.size()==0)
        return -1;

    // TBD to determine the corresponding backend
    if (data.nixlBackendEngines[0]->loadRemoteConnInfo(
                                    remote_agent, conn_info)<0)
        return -1;

    data.remoteSections[remote_agent] = new nixlRemoteSection(
        remote_agent, data.memorySection.getEngineMap());

    if (data.remoteSections[remote_agent]->loadRemoteData(&sd)<0){
        delete data.remoteSections[remote_agent];
        data.remoteSections.erase(remote_agent);
        return -1;
    }

    return 0;
}

void nixlAgent::invalidateRemoteMetadata(std::string remote_agent) {
    if (data.remoteSections.count(remote_agent)!=0)
        delete data.remoteSections[remote_agent];
}

int nixlAgent::sendMetadata() {
    // TBD
    return 0;
}

int nixlAgent::fetchMetadata (std::string &remote_agent) {
    // TBD
    return 0;
}

void nixlAgent::invalidateLocalMetadata() {
    //TBD
}

