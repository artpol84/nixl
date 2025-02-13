#include "ucx_backend.h"
#include "serdes.h"

static ucs_status_t check_connection (void *arg, const void *header,
                               size_t header_length, void *data,
                               size_t length, 
                               const ucp_am_recv_param_t *param)
{
    struct nixl_ucx_am_hdr* hdr = (struct nixl_ucx_am_hdr*) header;
    
    std::string remote_agent( (char*) data, length);
    nixlUcxEngine* engine = (nixlUcxEngine*) arg;

    //debugging
    //std::cout << " received am to establish connection from " << remote_agent << "\n";

    if(hdr->op != CONN_CHECK) {
        //is this the best way to ERR?
        return UCS_ERR_INVALID_PARAM;
    }

    //send_am should be forcing EAGER protocol
    if((param->recv_attr & UCP_AM_RECV_ATTR_FLAG_RNDV) != 0) {
        //is this the best way to ERR?
        return UCS_ERR_INVALID_PARAM;
    }

    if(engine->updateConnMap(remote_agent) == -1) {
        //is this the best way to ERR?
        return UCS_ERR_INVALID_PARAM;
    }
    
    //debugging
    //std::cout << " finished am to connect to " << remote_agent << "\n";
    
    return UCS_OK;
}

static ucs_status_t get_notif (void *arg, const void *header,
                        size_t header_length, void *data,
                        size_t length, 
                        const ucp_am_recv_param_t *param)
{
    struct nixl_ucx_am_hdr* hdr = (struct nixl_ucx_am_hdr*) header;
    nixlSerDes ser_des;

    std::string ser_str( (char*) data, length);
    nixlUcxEngine* engine = (nixlUcxEngine*) arg;
    std::string remote_name, msg;

    if(hdr->op != NOTIF_STR) {
        //is this the best way to ERR?
        return UCS_ERR_INVALID_PARAM;
    }

    //send_am should be forcing EAGER protocol
    if((param->recv_attr & UCP_AM_RECV_ATTR_FLAG_RNDV) != 0) {
        //is this the best way to ERR?
        return UCS_ERR_INVALID_PARAM;
    }

    ser_des.importStr(ser_str);
    remote_name = ser_des.getStr("name");
    msg = ser_des.getStr("msg");

    if(engine->appendNotif(remote_name, msg) == -1) {
        //is this the best way to ERR?
        return UCS_ERR_INVALID_PARAM;
    }
    
    //debugging
    //std::cout << " finished am to connect to " << remote_agent << "\n";
   
    //TODO: ack notification

    return UCS_OK;
}

nixlUcxEngine::nixlUcxEngine (nixlUcxInitParams* init_params)
: nixlBackendEngine ((nixlBackendInitParams*) init_params) {
    std::vector<std::string> devs; /* Empty vector */
    uint64_t n_addr;

    uw = new nixlUcxWorker(devs);
    uw->epAddr(n_addr, workerSize);
    workerAddr = (void*) n_addr;

    uw->regAmCallback(CONN_CHECK, check_connection, this);
    uw->regAmCallback(NOTIF_STR, get_notif, this);
}

// Through parent destructor the unregister will be called.
nixlUcxEngine::~nixlUcxEngine () {
    // per registered memory deregisters it, which removes the corresponding metadata too
    // parent destructor takes care of the desc list
    // For remote metadata, they should be removed here
    delete uw;
}

/****************************************
 * Helpers
*****************************************/

int nixlUcxEngine::updateConnMap(std::string remote_agent) {
    nixlUcxConnection conn;
    auto search = remoteConnMap.find(remote_agent);

    if(search == remoteConnMap.end()) {
        //TODO: err: remote connection not found
        return -1;
    }

    remoteConnMap[remote_agent].connected = true;

    return 0;
}

int nixlUcxEngine::appendNotif(std::string remote_agent, std::string notif) {
    
    // TODO: [PERF] avoid heap allocation on the data path
    auto new_elm = std::make_pair(remote_agent, notif);

    notifs.push_back(new_elm);

    return 0;
}

/****************************************
 * Connection management
*****************************************/
std::string nixlUcxEngine::getConnInfo() const {
    return nixlSerDes::_bytesToString(workerAddr, workerSize);
}


int nixlUcxEngine::loadRemoteConnInfo (std::string remote_agent,
                                       std::string remote_conn_info)
{
    size_t size = remote_conn_info.size();
    nixlUcxConnection conn;
    int ret;
    //TODO: eventually std::byte?
    char* addr = new char[size];

    if(remoteConnMap.find(remote_agent) != remoteConnMap.end()) {
        //already connected?
        return -1;
    }

    nixlSerDes::_stringToBytes((void*) addr, remote_conn_info, size);
    ret = uw->connect(addr, size, conn.ep);
    if (ret) {
        // TODO: print error
        return -1;
    }

    conn.remoteAgent = remote_agent;
    conn.connected = false;

    remoteConnMap[remote_agent] = conn;

    return 0;
}

int nixlUcxEngine::makeConnection(std::string remote_agent) {
    struct nixl_ucx_am_hdr hdr;
    nixlUcxConnection conn;
    uint32_t flags = 0;
    int ret = 0;
    nixlUcxReq req;
    volatile bool done = false;

    auto search = remoteConnMap.find(remote_agent);

    if(search == remoteConnMap.end()) {
        //TODO: err: remote connection not found
        return -1;
    }

    conn = remoteConnMap[remote_agent];

    hdr.op = CONN_CHECK;
    //agent names should never be long enough to need RNDV
    flags |= UCP_AM_SEND_FLAG_EAGER;

    ret = uw->sendAm(conn.ep, CONN_CHECK, 
                     &hdr, sizeof(struct nixl_ucx_am_hdr), 
                     (void*) localAgent.data(), localAgent.size(),
                     flags, req);
    
    if(ret != 0) {
        //TODO: error
        return -1;
    }

    //wait for AM to send
    while(ret == 0){
        ret = uw->test(req);
    }

    //wait for remote agent to complete handshake
    while(!done){ 
        uw->progress();
        done = remoteConnMap[remote_agent].connected;
    }
    
    return 0;
}

int nixlUcxEngine::listenForConnection(std::string remote_agent) {
    
    nixlUcxConnection conn;
    struct nixl_ucx_am_hdr hdr;
    uint32_t flags = 0;
    nixlUcxReq req;
    int ret = 0;
    volatile bool done = false;
 
    auto search = remoteConnMap.find(remote_agent);
    if(search == remoteConnMap.end()) {
        //TODO: err: remote connection not found
        return -1;
    }

    conn = remoteConnMap[remote_agent];   

    while(!done){
        uw->progress();
        done = remoteConnMap[remote_agent].connected;   
    }

    hdr.op = CONN_CHECK;
    //agent names should never be long enough to need RNDV
    flags |= UCP_AM_SEND_FLAG_EAGER;

    ret = uw->sendAm(conn.ep, CONN_CHECK, 
                     &hdr, sizeof(struct nixl_ucx_am_hdr), 
                     (void*) localAgent.data(), localAgent.size(),
                     flags, req);
    
    if(ret != 0) {
        //TODO: error
        return -1;
    }

    //wait for AM to send
    while(ret == 0){
        ret = uw->test(req);
    }

    //wait for remote agent to complete handshake
    while(!done){ 
        uw->progress();
        done = remoteConnMap[remote_agent].connected;
    }
    
    return 0;
}

/****************************************
 * Memory management
*****************************************/
int nixlUcxEngine::registerMem (const nixlBasicDesc &mem,
                                memory_type_t mem_type,
                                nixlBackendMD* &out)
{
    int ret;
    nixlUcxPrivateMetadata *priv = new nixlUcxPrivateMetadata;
    uint64_t rkey_addr;
    size_t rkey_size;

    ret = uw->memReg((void*) mem.addr, mem.len, priv->mem);
    if (ret) {
        // TODO: err out
        return -1;
    }
    ret = uw->packRkey(priv->mem, rkey_addr, rkey_size);
    if (ret) {
        // TODO: err out
        return -1;
    }
    priv->rkeyStr = nixlSerDes::_bytesToString((void*) rkey_addr, rkey_size);

    out = (nixlBackendMD*) priv; //typecast?

    return 0; // Or errors
}

void nixlUcxEngine::deregisterMem (nixlBackendMD* desc)
{
    nixlUcxPrivateMetadata *priv = (nixlUcxPrivateMetadata*) desc; //typecast?

    uw->memDereg(priv->mem);
    delete priv;
}


// To be cleaned up
int nixlUcxEngine::loadRemote (nixlStringDesc input,
                               nixlBackendMD* &output,
                               std::string remote_agent) {
    size_t size = input.metadata.size();
    char *addr = new char[size];
    int ret;
    nixlUcxConnection conn;

    nixlUcxPublicMetadata *md = new nixlUcxPublicMetadata;

    auto search = remoteConnMap.find(remote_agent);

    if(search == remoteConnMap.end()) {
        //TODO: err: remote connection not found
        return -1;
    }
    conn = (nixlUcxConnection) search->second;

    nixlSerDes::_stringToBytes(addr, input.metadata, size);

    md->conn = conn;
    ret = uw->rkeyImport(conn.ep, addr, size, md->rkey);
    if (ret) {
        // TODO: error out. Should we indicate which desc failed or unroll everything prior
        return -1;
    }
    output = (nixlBackendMD*) md;

    return 0;
}

int nixlUcxEngine::removeRemote (nixlBackendMD* input) {

    nixlUcxPublicMetadata *md = (nixlUcxPublicMetadata*) input; //typecast?

    uw->rkeyDestroy(md->rkey);
    delete md;

    return 0;
}

/****************************************
 * Data movement
*****************************************/
transfer_state_t nixlUcxEngine::transfer (nixlDescList<nixlMetaDesc> local,
                                          nixlDescList<nixlMetaDesc> remote,
                                          transfer_op_t op,
                                          std::string remote_agent,
                                          std::string notif_msg,
                                          nixlBackendReqH* &handle)
{
    size_t lcnt = local.descCount();
    size_t rcnt = remote.descCount();
    size_t i, ret;

    nixlUcxReq req;

    if (lcnt != rcnt) {
        return NIXL_XFER_ERR;
    }

    for(i = 0; i < lcnt; i++) {
        void *laddr = (void*) local[i].addr;
        size_t lsize = local[i].len;
        void *raddr = (void*) remote[i].addr;
        size_t rsize = remote[i].len;

        nixlUcxPrivateMetadata *lmd = (nixlUcxPrivateMetadata*) local[i].metadata;
        nixlUcxPublicMetadata *rmd = (nixlUcxPublicMetadata*) remote[i].metadata; //typecast?

        if (lsize != rsize) {
            // TODO: err output
            return NIXL_XFER_ERR;
        }

        // TODO: remote_agent and msg should be cached in nixlUCxReq or another way
        // TODO: Cases for _NOTIF be added

        switch (op) {
        case NIXL_READ:
            ret = uw->read(rmd->conn.ep, (uint64_t) raddr, rmd->rkey, laddr, lmd->mem, lsize, req);
            break;
        case NIXL_WRITE:
            ret = uw->write(rmd->conn.ep, laddr, lmd->mem, (uint64_t) raddr, rmd->rkey, lsize, req);
            break;
        default:
            return NIXL_XFER_ERR;
        }
    }

    handle = (nixlBackendReqH*) req;
    // TODO: update if it's already DONE
    if (ret)
        return NIXL_XFER_ERR;
    else
        return NIXL_XFER_PROC;
}

transfer_state_t nixlUcxEngine::checkTransfer (nixlBackendReqH* handle) {
    nixlUcxReq req;

    req = (nixlUcxReq*) handle;
    return uw->test(req);
}

int nixlUcxEngine::progress() {
    return uw->progress();
}

/****************************************
 * Notifications
*****************************************/

//agent will provide cached msg
int nixlUcxEngine::sendNotif(std::string remote_agent, std::string msg)
{
    nixlSerDes ser_des;
    int status = 0;
    std::string ser_msg;
    nixlUcxConnection conn;
    nixlUcxReq req;
    struct nixl_ucx_am_hdr hdr;
    uint32_t flags = 0;

    auto search = remoteConnMap.find(remote_agent);

    if(search == remoteConnMap.end()) {
        //TODO: err: remote connection not found
        return -1;
    }

    conn = remoteConnMap[remote_agent];

    hdr.op = NOTIF_STR;
    flags |= UCP_AM_SEND_FLAG_EAGER;
    
    ser_des.addStr("name", localAgent);
    ser_des.addStr("msg", msg);
    ser_msg = ser_des.exportStr();
    
    status = uw->sendAm(conn.ep, NOTIF_STR, 
                        &hdr, sizeof(struct nixl_ucx_am_hdr), 
                        (void*) ser_msg.data(), ser_msg.size(), 
                        flags, req);

    if(status) {
        //TODO: err
        return status;
    }

    while(status == 0) status = uw->test(req);

    return status;
}

int nixlUcxEngine::getNotifs(notif_list_t &notif_list)
{
    int n_notifs;

    notif_list = notifs;
    n_notifs = notifs.size();
    notifs.clear();
   
    return n_notifs;
}
