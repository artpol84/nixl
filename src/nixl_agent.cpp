#include "nixl.h"
#include "ucx_backend.h"
#include "serdes.h"

nixlAgentData::~nixlAgentData() {
    for (auto & elm: remoteSections)
        delete elm.second;

    for (auto & elm: nixlBackendEngines)
        delete elm.second;

    // TODO: deregister memories left in the local section
    // TODO: Delete the transfer requests
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
    nixl_backend_t nixl_backend = params->getType();

    // Registring same type of backend is not supported, unlikey and prob error
    if (data.nixlBackendEngines.count(nixl_backend)!=0)
        return nullptr;

    params->localAgent = data.name;
    // Default not to use progress thread
    params->threading  = false;

    switch (nixl_backend) { // For supported backends
        case UCX:
            backend = (nixlBackendEngine*) new nixlUcxEngine(
                                               (nixlUcxInitParams*) params);
            break;

        default: {} // backend stays nullptr
    }

    if (backend!=nullptr) { // Success
        nixl_backend = backend->getType(); // For safety, should be redundant
        data.nixlBackendEngines[nixl_backend] = backend;
        data.memorySection.addBackendHandler(backend);
        data.connMD[nixl_backend] = backend->getConnInfo();
    }
    return backend;
}

nixl_status_t nixlAgent::registerMem(const nixlDescList<nixlBasicDesc> &descs,
                           nixlBackendEngine* backend) {
    return (data.memorySection.addDescList(descs, backend));
}

nixl_status_t nixlAgent::deregisterMem(const nixlDescList<nixlBasicDesc> &descs,
                             nixlBackendEngine* backend) {
    // Might not need unified and sorted info
    nixlDescList<nixlMetaDesc> resp(descs.getType(),
                                    descs.isUnifiedAddr(),
                                    descs.isSorted());
    data.memorySection.populate(descs, backend->getType(), resp);
    return (data.memorySection.remDescList(resp, backend));
}

nixl_status_t nixlAgent::makeConnection(const std::string &remote_agent) {
    nixlBackendEngine* eng;
    nixl_status_t ret;
    int count = 0;

    if (data.remoteBackends.count(remote_agent)==0)
        return NIXL_ERR_NOT_FOUND;

    // For now making all the possible connections, later might take hints
    for (auto & r_eng: data.remoteBackends[remote_agent]) {
        if (data.nixlBackendEngines.count(r_eng)!=0) {
            eng = data.nixlBackendEngines[r_eng];
            ret = eng->connect(remote_agent);
            if (ret)
                return ret;
            count++;
        }
    }

    if (count == 0) // No common backend
        return NIXL_ERR_BACKEND;
    return NIXL_SUCCESS;
}

nixl_status_t nixlAgent::createXferReq(const nixlDescList<nixlBasicDesc> &local_descs,
                             const nixlDescList<nixlBasicDesc> &remote_descs,
                             const std::string &remote_agent,
                             const std::string &notif_msg,
                             const nixl_xfer_op_t &operation,
                             nixlXferReqH* &req_handle) {

    // Check the correspondence between descriptor lists
    if (local_descs.descCount() != remote_descs.descCount())
        return NIXL_ERR_INVALID_PARAM;
    for (int i=0; i<local_descs.descCount(); ++i)
        if (local_descs[i].len != remote_descs[i].len)
            return NIXL_ERR_INVALID_PARAM;

    if ((notif_msg.size()==0) &&
        ((operation==NIXL_WR_NOTIF) || (operation==NIXL_RD_NOTIF)))
        return NIXL_ERR_INVALID_PARAM;

    nixl_status_t ret;
    if (data.remoteSections.count(remote_agent)==0)
        return NIXL_ERR_NOT_FOUND;
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
        return NIXL_ERR_NOT_FOUND;
    }

    if ((notif_msg.size()!=0) && (!handle->engine->supportsNotif())) {
        delete handle;
        return NIXL_ERR_BACKEND;
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
    handle->backendOp   = operation;
    handle->state       = NIXL_XFER_INIT;

    // TODO: when supporting metadata server, we might get to PRE state

    req_handle = handle;

    // TODO: Add bookkeeping of nixlRequests per target agent
    return NIXL_SUCCESS;
}

void nixlAgent::invalidateXferReq(nixlXferReqH *req) {
    if (req->state != NIXL_XFER_DONE)
        req->engine->releaseReqH(req->backendHandle);
    delete req;
}

nixl_xfer_state_t nixlAgent::postXferReq(nixlXferReqH *req) {
    if (req==nullptr)
        return NIXL_XFER_ERR;
    // TODO: add NIXL_XFER_PRE handling after metadata server is added

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

nixl_xfer_state_t nixlAgent::getXferStatus (nixlXferReqH *req) {
    // TODO: add NIXL_XFER_PRE handling after metadata server is added

    // If the state is done, no need to recheck.
    if (req->state != NIXL_XFER_DONE)
        req->state = req->engine->checkXfer(req->backendHandle);

    return req->state;
}

nixl_status_t nixlAgent::genNotif(const std::string &remote_agent,
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
    return NIXL_ERR_NOT_FOUND;
}

int nixlAgent::getNotifs(nixl_notifs_t &notif_map) {
    notif_list_t backend_list;
    int ret, bad_ret=0, tot=0;
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
    nixl_backend_t nixl_backend;

    if (conn_cnt == 0) // Error
        return "";

    nixlSerDes sd;
    if (sd.addStr("Agent", data.name)<0)
        return "";

    if (sd.addBuf("Conns", &conn_cnt, sizeof(conn_cnt)))
        return "";

    for (auto &c : data.connMD) {
        nixl_backend = c.first;
        if (sd.addBuf("t", &nixl_backend, sizeof(nixl_backend)))
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

std::string nixlAgent::loadRemoteMD (const std::string &remote_metadata) {
    int count = 0;
    nixlSerDes sd;
    size_t conn_cnt;
    std::string conn_info;
    nixl_backend_t nixl_backend;

    if (sd.importStr(remote_metadata)<0)
        return "";

    std::string remote_agent = sd.getStr("Agent");
    if (remote_agent.size()==0)
        return "";

    if (sd.getBuf("Conns", &conn_cnt, sizeof(conn_cnt)))
        return "";

    if (conn_cnt<1)
        return "";

    for (size_t i=0; i<conn_cnt; ++i) {
        if (sd.getBuf("t", &nixl_backend, sizeof(nixl_backend)))
            return "";
        conn_info = sd.getStr("c");
        if (conn_info.size()==0) // Fine if doing marginal updates
            return "";

        // Current agent might not support a remote backend
        if (data.nixlBackendEngines.count(nixl_backend)!=0) {

            // No need to reload the same conn info, maybe TODO to improve
            if (data.remoteBackends.count(remote_agent)!=0)
                if (data.remoteBackends[remote_agent].count(nixl_backend)!=0) {
                    count++;
                    continue;
                }

            if (data.nixlBackendEngines[nixl_backend]->
                    loadRemoteConnInfo(remote_agent, conn_info)<0)
                return ""; // Error in load
            count++;
            data.remoteBackends[remote_agent].insert(nixl_backend);
        }
    }

    // No common backend, no point in loading the rest, unexpected
    if (count == 0)
        return "";

    // If there was an issue and we return -1 while some connections
    // are loaded, they will be deleted in backend destructor.
    // the backend connection list for this agent will be empty.

    // It's just a check, not introducing section_info
    conn_info = sd.getStr("");
    if (conn_info != "MemSection")
        return "";

    if (data.remoteSections.count(remote_agent) == 0)
        data.remoteSections[remote_agent] = new nixlRemoteSection(
                            remote_agent, data.nixlBackendEngines);

    if (data.remoteSections[remote_agent]->loadRemoteData(&sd)<0) {
        delete data.remoteSections[remote_agent];
        data.remoteSections.erase(remote_agent);
        return "";
    }

    return remote_agent;
}

nixl_status_t nixlAgent::invalidateRemoteMD(const std::string &remote_agent) {
    nixl_status_t ret = NIXL_ERR_NOT_FOUND;
    if (data.remoteSections.count(remote_agent)!=0) {
        delete data.remoteSections[remote_agent];
        data.remoteSections.erase(remote_agent);
        ret = NIXL_SUCCESS;
    }

    if (data.remoteBackends.count(remote_agent)!=0) {
        for (auto & elm: data.remoteBackends[remote_agent])
            data.nixlBackendEngines[elm]->disconnect(remote_agent);
        data.remoteBackends.erase(remote_agent);
        ret = NIXL_SUCCESS;
    }

    // TODO: Put the transfers belonging to this remote nixl_status_to ERR state
    return ret;
}

nixl_status_t nixlAgent::sendLocalMD() const {
    // TODO
    return NIXL_SUCCESS;
}

nixl_status_t nixlAgent::fetchRemoteMD (const std::string &remote_agent) {
    // TODO
    return NIXL_SUCCESS;
}

nixl_status_t nixlAgent::invalidateLocalMD() const {
    // TODO
    return NIXL_SUCCESS;
}
