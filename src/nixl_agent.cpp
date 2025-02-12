#include "nixl.h"
#include "ucx_backend.h"
#include "serdes.h"

nixlAgentDataPrivate::~nixlAgentDataPrivate() {
    for (auto & elm: remoteSections)
        delete elm.second;
    remoteSections.clear();

    for (auto & elm: nixlBackendEngines)
        delete elm.second;
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
    nixlBackendEngine* backend;
    backend_type_t backend_type = params->getType();

    // Registring same type of backend is not supported, unlikey and prob error
    if (data.nixlBackendEngines.count(backend_type)!=0)
        return nullptr;

    switch (backend_type) { // For supported backends
        case UCX:
            backend = (nixlBackendEngine*) new nixlUcxEngine(
                                               (nixlUcxInitParams*) params);
            break;

        default: {} // backend stays nullptr
    }

    if (backend!=nullptr) { // Success
        backend_type = backend->getType(); // For safety, should be redundant
        data.nixlBackendEngines[backend_type] = backend;
        data.memorySection.addBackendHandler(backend);
        data.connMd[backend_type] = backend->getConnInfo();
    }
    return backend;
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
    nixlBackendEngine* eng;
    int ret;
    int count = 0;

    if (data.remoteBackends.count(remote_agent)==0)
        return -1;

    // For now making all the possible connections, later might take hints
    for (auto & r_eng: data.remoteBackends[remote_agent]) {
        if(data.nixlBackendEngines.count(r_eng)!=0) {
            eng = data.nixlBackendEngines[r_eng];
            if (direction)
                ret = eng->listenForConnection(remote_agent);
            else
                ret = eng->makeConnection(remote_agent);
            if (ret)
                return ret;
            count++;
        }
    }

    if (count == 0) // No common backend
        return -1;
    return 0;
}

int nixlAgent::createTransferReq(nixlDescList<nixlBasicDesc>& local_descs,
                                 nixlDescList<nixlBasicDesc>& remote_descs,
                                 std::string remote_agent,
                                 std::string notif_msg,
                                 int direction,
                                 nixlTransferRequest* &req_handle) {

    // Check the correspondence between descriptor lists
    if (local_descs.descCount() != remote_descs.descCount())
        return -1;
    for (int i=0; i<local_descs.descCount(); ++i)
        if (local_descs[i].len != remote_descs[i].len)
            return -1;

    int ret;
    if (data.remoteSections.count(remote_agent)==0)
        return -1;

    nixlTransferRequest *handle = new nixlTransferRequest;
    // Might not need unified and sorted info
    handle->initiator_descs = new nixlDescList<nixlMetaDesc> (
                                      local_descs.getType(),
                                      local_descs.isUnifiedAddr(),
                                      local_descs.isSorted());

    handle->engine = data.memorySection.findQuery(local_descs,
                          *handle->initiator_descs, remote_descs.getType(),
                          data.remoteBackends[remote_agent]);

    if (handle->engine==nullptr)
        return -1;

    handle->target_descs = new nixlDescList<nixlMetaDesc>(
                                   remote_descs.getType(),
                                   remote_descs.isUnifiedAddr(),
                                   remote_descs.isSorted());

    // Based on the decided local backend, we check the remote counterpart
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
    // We can't repost while a request is in progress
    if (req->state == NIXL_PROC) {
        req->state = req->engine->checkTransfer(req->backend_handle);
        if (req->state == NIXL_PROC)
            return -1;
    }

    // If state is NIXL_INIT or NIXL_DONE we can repost,
    // backend handle is non_existent or release respectively.
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
    // If the state is done, the backend_handle is released
    if (req->state != NIXL_DONE)
        req->state = req->engine->checkTransfer(req->backend_handle);

    return req->state;
}

std::string nixlAgent::getMetadata () const {
    // data.connMd was populated when the backend was created
    size_t conn_cnt = data.connMd.size();
    backend_type_t backend_type;

    if (conn_cnt == 0) // Error
        return "";

    nixlSerDes sd;
    if (sd.addStr("Agent", data.name)<0)
        return "";

    if (sd.addBuf("Conns", &conn_cnt, sizeof(conn_cnt)))
        return "";

    for (auto &c : data.connMd) {
        backend_type = c.first;
        if (sd.addBuf("t", &backend_type, sizeof(backend_type)))
            return "";
        if (sd.addStr("c", c.second)<0)
            return "";
    }

    if (sd.addStr("", "MemSection")<0)
        return "";

    if (data.memorySection.serialize(&sd)<0)
        return "";

    return sd.exportStr();
}

int nixlAgent::loadMetadata (std::string remote_metadata) {
    int count = 0;
    nixlSerDes sd;
    size_t conn_cnt;
    std::string conn_info;
    backend_type_t backend_type;
    std::vector<backend_type_t> supported_backends;

    if (sd.importStr(remote_metadata)<0)
        return -1;

    std::string remote_agent = sd.getStr("Agent");
    if (remote_agent.size()==0)
        return -1;

    if (sd.getBuf("Conns", &conn_cnt, sizeof(conn_cnt)))
        return -1;

    if (conn_cnt<1)
        return -1;

    for (size_t i=0; i<conn_cnt; ++i) {
        if (sd.getBuf("t", &backend_type, sizeof(backend_type)))
            return -1;
        conn_info = sd.getStr("c");
        if (conn_info.size()==0)
            return -1;

        // Current agent might not support a remote backend
        if (data.nixlBackendEngines.count(backend_type)!=0) {
            if (data.nixlBackendEngines[backend_type]->
                    loadRemoteConnInfo(remote_agent, conn_info)<0)
                return -1; // Error in load
            count++;
            supported_backends.push_back(backend_type);
        }
    }

    // No common backend, no point in loading the rest
    if (count == 0)
        return -1;

    // If there was an issue and we return -1 while some connections
    // are loaded, they will be deleted in backend destructor.
    // the backend connection list for this agent will be empty.

    conn_info = sd.getStr("");
    if (conn_info != "MemSection")
        return -1;

    data.remoteSections[remote_agent] = new nixlRemoteSection(
                        remote_agent, data.nixlBackendEngines);

    if (data.remoteSections[remote_agent]->loadRemoteData(&sd)<0) {
        delete data.remoteSections[remote_agent];
        data.remoteSections.erase(remote_agent);
        return -1;
    }

    data.remoteBackends[remote_agent] = supported_backends;

    return 0;
}

void nixlAgent::invalidateRemoteMetadata(std::string remote_agent) {
    if (data.remoteSections.count(remote_agent)!=0) {
        delete data.remoteSections[remote_agent];
        data.remoteSections.erase(remote_agent);
    }

    if (data.remoteBackends.count(remote_agent)!=0) {
        data.remoteBackends.erase(remote_agent);
    }
}

int nixlAgent::sendMetadata() const {
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
