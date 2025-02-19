#include "nixl.h"
#include "ucx_backend.h"
#include "serdes.h"

nixlAgentData::~nixlAgentData() {
    for (auto & elm: remoteSections)
        delete elm.second;
    remoteSections.clear();

    for (auto & elm: nixlBackendEngines)
        delete elm.second;
    nixlBackendEngines.clear();

}

nixlMetadataH::nixlMetadataH(const std::string& ip_address, uint16_t port){
    this->ipAddress = ip_address;
    this->port      = port;
}

nixlMetadataH::~nixlMetadataH(){
}

nixlAgent::nixlAgent(const std::string &name,
                     const nixlDeviceMD &devs){
   data.name                    = name;
   data.deviceMeta.srcIpAddress = devs.srcIpAddress;
   data.deviceMeta.srcPort      = devs.srcPort;
   // data.deviceMeta.topology      = devs.topology;
}

nixlAgent::~nixlAgent() {
}

nixlBackendEngine* nixlAgent::createBackend(nixlBackendInitParams* params) {
    nixlBackendEngine* backend;
    backend_type_t backend_type = params->getType();

    // Registring same type of backend is not supported, unlikey and prob error
    if (data.nixlBackendEngines.count(backend_type)!=0)
        return nullptr;

    params->localAgent = data.name;
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
        data.connMD[backend_type] = backend->getConnInfo();
    }
    return backend;
}

int nixlAgent::registerMem(const nixlDescList<nixlBasicDesc> &descs,
                           nixlBackendEngine* backend) {
    return (data.memorySection.addDescList(descs, backend));
}

int nixlAgent::deregisterMem(const nixlDescList<nixlBasicDesc> &descs,
                             nixlBackendEngine* backend) {
    // Might not need unified and sorted info
    nixlDescList<nixlMetaDesc> resp(descs.getType(),
                                    descs.isUnifiedAddr(),
                                    descs.isSorted());
    data.memorySection.populate(descs, backend->getType(), resp);
    return (data.memorySection.remDescList(resp, backend));
}

int nixlAgent::makeConnection(const std::string &remote_agent, int direction) {
    nixlBackendEngine* eng;
    int ret;
    int count = 0;

    if (data.remoteBackends.count(remote_agent)==0)
        return -1;

    // For now making all the possible connections, later might take hints
    for (auto & r_eng: data.remoteBackends[remote_agent]) {
        if (data.nixlBackendEngines.count(r_eng)!=0) {
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

int nixlAgent::createXferReq(const nixlDescList<nixlBasicDesc> &local_descs,
                             const nixlDescList<nixlBasicDesc> &remote_descs,
                             const std::string &remote_agent,
                             const std::string &notif_msg,
                             int direction,
                             nixlXferReqH* &req_handle) {

    // Check the correspondence between descriptor lists
    if (local_descs.descCount() != remote_descs.descCount())
        return -1;
    for (int i=0; i<local_descs.descCount(); ++i)
        if (local_descs[i].len != remote_descs[i].len)
            return -1;

    int ret;
    if (data.remoteSections.count(remote_agent)==0)
        return -1;
    // TODO: when central KV is supported, add a call to fetchRemoteMD

    // TODO [Perf]: Avoid heap allocation on the datapath, maybe use a mem pool
    // TODO [Perf]: merge descriptors if source and remote device are the same,
    //              and also back to back in memory (unlikly case).

    nixlXferReqH *handle = new nixlXferReqH;
    // Might not need unified and sorted info
    handle->initiatorDescs = new nixlDescList<nixlMetaDesc> (
                                      local_descs.getType(),
                                      local_descs.isUnifiedAddr(),
                                      local_descs.isSorted());

    handle->engine = data.memorySection.findQuery(local_descs,
                          remote_descs.getType(),
                          data.remoteBackends[remote_agent],
                          *handle->initiatorDescs);

    if (handle->engine==nullptr) {
        delete handle;
        return -1;
    }

    if ((notif_msg.size()!=0) && (!handle->engine->supportsNotif())) {
        delete handle;
        return -1;
    }

    handle->targetDescs = new nixlDescList<nixlMetaDesc> (
                                   remote_descs.getType(),
                                   remote_descs.isUnifiedAddr(),
                                   remote_descs.isSorted());

    // Based on the decided local backend, we check the remote counterpart
    ret = data.remoteSections[remote_agent]->populate(remote_descs,
               handle->engine->getType(), *handle->targetDescs);
    if (ret<0) {
        delete handle;
        return ret;
    }

    handle->remoteAgent = remote_agent;
    handle->notifMsg    = notif_msg;

    if (notif_msg.size()==0)
        handle->backendOp = direction ? NIXL_READ : NIXL_WRITE;
    else
        handle->backendOp = direction ? NIXL_RD_NOTIF : NIXL_WR_NOTIF;

    handle->state = NIXL_XFER_INIT;

    req_handle    = handle;

    // Not bookkeeping xferRequests, assuming user releases all
    return 0;
}

void nixlAgent::invalidateXferReq(nixlXferReqH *req) {
    if (req->state != NIXL_XFER_DONE)
        req->engine->releaseReqH(req->backendHandle);
    delete req;
}

xfer_state_t nixlAgent::postXferReq(nixlXferReqH *req) {
    if (req==nullptr)
        return NIXL_XFER_ERR;
    // We can't repost while a request is in progress
    if (req->state == NIXL_XFER_PROC) {
        req->state = req->engine->checkXfer(req->backendHandle);
        if (req->state == NIXL_XFER_PROC)
            return NIXL_XFER_ERR;
    }

    // If state is NIXL_XFER_INIT or NIXL_XFER_DONE we can repost,
    return (req->engine->postXfer (*req->initiatorDescs,
                                   *req->targetDescs,
                                   req->backendOp,
                                   req->remoteAgent,
                                   req->notifMsg,
                                   req->backendHandle));
}

xfer_state_t nixlAgent::getXferStatus (nixlXferReqH *req) {
    // If the state is done, no need to recheck.
    if (req->state != NIXL_XFER_DONE)
        req->state = req->engine->checkXfer(req->backendHandle);

    return req->state;
}

int nixlAgent::genNotif(const std::string &remote_agent,
                        const std::string &msg,
                        nixlBackendEngine* backend) {
    if (backend!=nullptr)
        return backend->genNotif(remote_agent, msg);

    // TODO: add logic to choose between backends if multiple support it
    for (auto & eng: data.nixlBackendEngines) {
        if (eng.second->supportsNotif()) {
            if (data.remoteBackends[remote_agent].count(
                                    eng.second->getType()) != 0)
                return eng.second->genNotif(remote_agent, msg);
        }
    }
    return -1;
}

int nixlAgent::getNewNotifs(notif_map_t &notif_map) {
    notif_list_t backend_list;
    int ret, bad_ret, tot=0;
    bool any_backend = false;

    // Doing best effort, if any backend errors out we return
    // error but proceed with the rest. We can add metadata about
    // the backend to the msg, but user could put it themselves.
    for (auto & eng: data.nixlBackendEngines) {
        if (eng.second->supportsNotif()) {
            any_backend = true;
            backend_list.clear();
            ret = eng.second->getNotifs(backend_list);
            if (ret<0)
                bad_ret=ret;

            if (backend_list.size()==0)
                continue;

            for (auto & elm: backend_list) {
                if (notif_map.count(elm.first)==0)
                    notif_map[elm.first] = std::vector<std::string>();

                notif_map[elm.first].push_back(elm.second);
                tot++;
            }
        }
    }

    if (bad_ret)
        return bad_ret;
    else if (!any_backend)
        return -1;
    else
        return tot;
}

std::string nixlAgent::getLocalMD () const {
    // data.connMD was populated when the backend was created
    size_t conn_cnt = data.connMD.size();
    backend_type_t backend_type;

    if (conn_cnt == 0) // Error
        return "";

    nixlSerDes sd;
    if (sd.addStr("Agent", data.name)<0)
        return "";

    if (sd.addBuf("Conns", &conn_cnt, sizeof(conn_cnt)))
        return "";

    for (auto &c : data.connMD) {
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

int nixlAgent::loadRemoteMD (const std::string &remote_metadata) {
    int count = 0;
    nixlSerDes sd;
    size_t conn_cnt;
    std::string conn_info;
    backend_type_t backend_type;
    backend_set_t supported_backends;

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
            supported_backends.insert(backend_type);
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

void nixlAgent::invalidateRemoteMD(const std::string &remote_agent) {
    if (data.remoteSections.count(remote_agent)!=0) {
        delete data.remoteSections[remote_agent];
        data.remoteSections.erase(remote_agent);
    }

    if (data.remoteBackends.count(remote_agent)!=0) {
        data.remoteBackends.erase(remote_agent);
    }
}

int nixlAgent::sendLocalMD() const {
    // TBD
    return 0;
}

int nixlAgent::fetchRemoteMD (const std::string &remote_agent) {
    // TBD
    return 0;
}

void nixlAgent::invalidateLocalMD() const {
    //TBD
}
