#include "ucx_backend.h"
#include "serdes.h"
#include <cassert>

/****************************************
 * UCX request management
*****************************************/

void nixlUcxEngine::_requestInit(void *request)
{
    /* Initialize request in-place (aka "placement new")*/
    new(request) nixlUcxBckndReq;
}

void nixlUcxEngine::_requestFini(void *request)
{
    /* Finalize request */
    nixlUcxBckndReq *req = (nixlUcxBckndReq*)request;
    req->~nixlUcxBckndReq();
}


/****************************************
 * Progress thread management
*****************************************/

void nixlUcxEngine::progressFunc()
{
    pthr_active = 1;

    while (!pthr_stop) {
        int i;
        for(i = 0; i < no_sync_iters; i++) {
            if( !uw->progress() ){
                /* break if no progress was made*/
                break;
            }
        }
        notifProgress();
        // TODO: once NIXL thread infrastructure is available - move it there!!!
        std::this_thread::yield();
    }

}

void nixlUcxEngine::startProgressThread()
{
    pthr_stop = pthr_active = 0;
    no_sync_iters = 32;

    // Start the thread
    // TODO [Relaxed mem] mem barrier to ensure pthr_x updates are complete
    new (&pthr) std::thread(&nixlUcxEngine::progressFunc, this);

    // Wait for the thread to be started
    while(!pthr_active){
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

void nixlUcxEngine::stopProgressThread()
{
    pthr_stop = 0;
    pthr.join();
}

/****************************************
 * Constructor/Destructor
*****************************************/

nixlUcxEngine::nixlUcxEngine (const nixlUcxInitParams* init_params)
: nixlBackendEngine ((const nixlBackendInitParams*) init_params) {
    std::vector<std::string> devs; /* Empty vector */
    uint64_t n_addr;

    uc = new nixlUcxContext(devs, sizeof(nixlUcxBckndReq),
                           _requestInit, _requestFini, NIXL_UCX_MT_WORKER);
    uw = new nixlUcxWorker(uc);
    uw->epAddr(n_addr, workerSize);
    workerAddr = (void*) n_addr;

    uw->regAmCallback(CONN_CHECK, connectionCheckAmCb, this);
    uw->regAmCallback(DISCONNECT, connectionTermAmCb, this);
    uw->regAmCallback(NOTIF_STR, notifAmCb, this);

    startProgressThread();
}

// Through parent destructor the unregister will be called.
nixlUcxEngine::~nixlUcxEngine () {
    // per registered memory deregisters it, which removes the corresponding metadata too
    // parent destructor takes care of the desc list
    // For remote metadata, they should be removed here
    stopProgressThread();
    delete uw;
    delete uc;
}


/****************************************
 * Connection management
*****************************************/

int nixlUcxEngine::checkConn(const std::string &remote_agent) {
    if(remoteConnMap.find(remote_agent) == remoteConnMap.end()) {
       //not found
       return -1;
   }
   return 0;
}

int nixlUcxEngine::endConn(const std::string &remote_agent)
{
    nixlUcxConnection conn;
    auto search = remoteConnMap.find(remote_agent);

    if(search == remoteConnMap.end()) {
        //TODO: err: remote connection not found
        return -1;
    }

    conn = remoteConnMap[remote_agent];

    uw->disconnect_nb(conn.ep);

    //thread safety?
    remoteConnMap.erase(remote_agent);

    return 0;
}

std::string nixlUcxEngine::getConnInfo() const {
    return nixlSerDes::_bytesToString(workerAddr, workerSize);
}

ucs_status_t
nixlUcxEngine::connectionCheckAmCb(void *arg, const void *header,
                                   size_t header_length, void *data,
                                   size_t length,
                                   const ucp_am_recv_param_t *param)
{
    struct nixl_ucx_am_hdr* hdr = (struct nixl_ucx_am_hdr*) header;

    std::string remote_agent( (char*) data, length);
    nixlUcxEngine* engine = (nixlUcxEngine*) arg;

    if(hdr->op != CONN_CHECK) {
        //is this the best way to ERR?
        return UCS_ERR_INVALID_PARAM;
    }

    //send_am should be forcing EAGER protocol
    if((param->recv_attr & UCP_AM_RECV_ATTR_FLAG_RNDV) != 0) {
        //is this the best way to ERR?
        return UCS_ERR_INVALID_PARAM;
    }

    if(!engine->checkConn(remote_agent)) {
        //TODO: received connect AM from agent we don't recognize
        return UCS_ERR_INVALID_PARAM;
    }

    return UCS_OK;
}

ucs_status_t
nixlUcxEngine::connectionTermAmCb (void *arg, const void *header,
                                   size_t header_length, void *data,
                                   size_t length,
                                   const ucp_am_recv_param_t *param)
{
    struct nixl_ucx_am_hdr* hdr = (struct nixl_ucx_am_hdr*) header;

    std::string remote_agent( (char*) data, length);
    nixlUcxEngine* engine = (nixlUcxEngine*) arg;

    if(hdr->op != DISCONNECT) {
        //is this the best way to ERR?
        return UCS_ERR_INVALID_PARAM;
    }

    //send_am should be forcing EAGER protocol
    if((param->recv_attr & UCP_AM_RECV_ATTR_FLAG_RNDV) != 0) {
        //is this the best way to ERR?
        return UCS_ERR_INVALID_PARAM;
    }

    if(!engine->endConn(remote_agent)) {
        //TODO: received connect AM from agent we don't recognize
        return UCS_ERR_INVALID_PARAM;
    }

    return UCS_OK;
}

int nixlUcxEngine::connect(const std::string &remote_agent) {
    struct nixl_ucx_am_hdr hdr;
    nixlUcxConnection conn;
    uint32_t flags = 0;
    xfer_state_t ret;
    nixlUcxReq req;

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
    
    if(ret == NIXL_XFER_ERR) {
        //TODO: ucx error
        return -1;
    }

    //wait for AM to send
    while(ret == NIXL_XFER_PROC){
        ret = uw->test(req);
    }

    return 0;
}

int nixlUcxEngine::disconnect(const std::string &remote_agent) {

    static struct nixl_ucx_am_hdr hdr;
    nixlUcxConnection conn;
    uint32_t flags = 0;
    xfer_state_t ret;
    nixlUcxReq req;

    auto search = remoteConnMap.find(remote_agent);

    if(search == remoteConnMap.end()) {
        //TODO: err: remote connection not found
        return -1;
    }

    conn = remoteConnMap[remote_agent];

    hdr.op = DISCONNECT;
    //agent names should never be long enough to need RNDV
    flags |= UCP_AM_SEND_FLAG_EAGER;

    ret = uw->sendAm(conn.ep, DISCONNECT, 
                     &hdr, sizeof(struct nixl_ucx_am_hdr), 
                     (void*) localAgent.data(), localAgent.size(),
                     flags, req);

    //don't care
    if(ret == NIXL_XFER_PROC)
        uw->reqRelease(req);

    endConn(remote_agent);

    return 0;
}

int nixlUcxEngine::loadRemoteConnInfo (const std::string &remote_agent,
                                       const std::string &remote_conn_info)
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

/****************************************
 * Memory management
*****************************************/

int nixlUcxEngine::registerMem (const nixlBasicDesc &mem,
                                const mem_type_t &mem_type,
                                nixlBackendMD* &out)
{
    int ret;
    nixlUcxPrivateMetadata *priv = new nixlUcxPrivateMetadata;
    uint64_t rkey_addr;
    size_t rkey_size;

    // TODO: Add mem_type check?
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

void nixlUcxEngine::deregisterMem (nixlBackendMD* meta)
{
    nixlUcxPrivateMetadata *priv = (nixlUcxPrivateMetadata*) meta; //typecast?

    uw->memDereg(priv->mem);
    delete priv;
}


// To be cleaned up
int nixlUcxEngine::loadRemoteMD (const nixlStringDesc &input,
                                 const mem_type_t &mem_type,
                                 const std::string &remote_agent,
                                 nixlBackendMD* &output) {
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

int nixlUcxEngine::removeRemoteMD (nixlBackendMD* input) {

    nixlUcxPublicMetadata *md = (nixlUcxPublicMetadata*) input; //typecast?

    uw->rkeyDestroy(md->rkey);
    delete md;

    return 0;
}

/****************************************
 * Data movement
*****************************************/

int nixlUcxEngine::retHelper(xfer_state_t ret, nixlUcxBckndReq *head, nixlUcxReq &req)
{
    /* if transfer wasn't immediately completed */
    switch(ret) {
        case NIXL_XFER_PROC:
            head->link((nixlUcxBckndReq*)req);
            break;
        case NIXL_XFER_DONE:
            // Nothing to do
            break;
        default:
            // Error. Release all previously initiated ops and exit:
            if (head->next()) {
                releaseReqH(head->next());
            }
            return -1;
    }
    return 0;
}

xfer_state_t nixlUcxEngine::postXfer (const nixlDescList<nixlMetaDesc> &local,
                                      const nixlDescList<nixlMetaDesc> &remote,
                                      const xfer_op_t &op,
                                      const std::string &remote_agent,
                                      const std::string &notif_msg,
                                      nixlBackendReqH* &handle)
{
    size_t lcnt = local.descCount();
    size_t rcnt = remote.descCount();
    size_t i;
    xfer_state_t ret;
    nixlUcxBckndReq dummy, *head = new (&dummy) nixlUcxBckndReq;
    nixlUcxPrivateMetadata *lmd;
    nixlUcxPublicMetadata *rmd;
    nixlUcxReq req;
    

    if (lcnt != rcnt) {
        return NIXL_XFER_ERR;
    }

    for(i = 0; i < lcnt; i++) {
        void *laddr = (void*) local[i].addr;
        size_t lsize = local[i].len;
        void *raddr = (void*) remote[i].addr;
        size_t rsize = remote[i].len;

        lmd = (nixlUcxPrivateMetadata*) local[i].metadata;
        rmd = (nixlUcxPublicMetadata*) remote[i].metadata;

        if (lsize != rsize) {
            // TODO: err output
            return NIXL_XFER_ERR;
        }

        // TODO: remote_agent and msg should be cached in nixlUCxReq or another way
        // TODO: Cases for _FLUSH be added

        switch (op) {
        case NIXL_READ:
        case NIXL_RD_NOTIF:
            ret = uw->read(rmd->conn.ep, (uint64_t) raddr, rmd->rkey, laddr, lmd->mem, lsize, req);
            break;
        case NIXL_WRITE:
        case NIXL_WR_NOTIF:
            ret = uw->write(rmd->conn.ep, laddr, lmd->mem, (uint64_t) raddr, rmd->rkey, lsize, req);
            break;
        default:
            return NIXL_XFER_ERR;
        }

        if (retHelper(ret, head, req)) {
            return ret;
        }
    }

    rmd = (nixlUcxPublicMetadata*) remote[0].metadata;
    ret = uw->flushEp(rmd->conn.ep, req);
    if (retHelper(ret, head, req)) {
        return ret;
    }

    switch (op) {
        case NIXL_RD_NOTIF:
        case NIXL_WR_NOTIF:
            ret = notifSendPriv(remote_agent, notif_msg, req);
            if (retHelper(ret, head, req)) {
                return ret;
            }
            break;
        case NIXL_WRITE:
        case NIXL_READ:
            break;
        default:
            return NIXL_XFER_ERR;
    }

    handle = head->next();
    return (NULL ==  head->next()) ? NIXL_XFER_DONE : NIXL_XFER_PROC;
}

xfer_state_t nixlUcxEngine::checkXfer (nixlBackendReqH* handle)
{
    nixlUcxBckndReq *head = (nixlUcxBckndReq *)handle;
    nixlUcxBckndReq *req = head;
    xfer_state_t out_ret = NIXL_XFER_DONE;

    /* If transfer has returned DONE - no check transfer */
    if (NULL == head) {
        /* Nothing to do */
        return NIXL_XFER_ERR;
    }

    /* Go over all request updating their status */
    while(req) {
        xfer_state_t ret;
        if (!req->is_complete()) {
            ret = uw->test((nixlUcxReq)req);
            switch (ret) {
                case NIXL_XFER_DONE:
                    /* Mark as completed */
                    req->completed();
                    break;
                case NIXL_XFER_ERR:
                    return ret;
                case NIXL_XFER_PROC:
                    out_ret = NIXL_XFER_PROC;
                    break;
                default:
                    /* Any other ret value is unexpected */
                    return NIXL_XFER_ERR;
            }
        }
        req = req->next();
    }

    /* Remove completed requests keeping the first one as
       request representative */
    req = head->unlink();
    while(req) {
        nixlUcxBckndReq *next_req = req->unlink();
        if (req->is_complete()) {
            requestReset(req);
            uw->reqRelease((nixlUcxReq)req);
        } else {
            /* Enqueue back */
            head->link(req);
        }
        req = next_req;
    }

    return out_ret;
}

void nixlUcxEngine::releaseReqH(nixlBackendReqH* handle)
{
    nixlUcxBckndReq *head = (nixlUcxBckndReq *)handle;
    nixlUcxBckndReq *req = head;

    if (head->next() || !head->is_complete()) {
        // TODO: Error log: uncompleted requests found! Cancelling ...
        while(head) {
            bool done = req->is_complete();
            req = head;
            head = req->unlink();
            requestReset(req);
            if (done) {
                uw->reqRelease((nixlUcxReq)req);
            } else {
                uw->reqCancel((nixlUcxReq)req);
            }
        }
    } else {
        /* All requests have been completed.
           Only release the head request */
        uw->reqRelease((nixlUcxReq)head);
    }
}

int nixlUcxEngine::progress() {
    // TODO: add listen for connection handling if necessary
    return uw->progress();
}

/****************************************
 * Notifications
*****************************************/

//agent will provide cached msg
xfer_state_t
nixlUcxEngine::notifSendPriv(const std::string &remote_agent,
                             const std::string &msg, nixlUcxReq &req)
{
    nixlSerDes ser_des;
    std::string *ser_msg;
    nixlUcxConnection conn;
    // TODO - temp fix, need to have an mpool
    static struct nixl_ucx_am_hdr hdr;
    uint32_t flags = 0;
    xfer_state_t ret;

    auto search = remoteConnMap.find(remote_agent);

    if(search == remoteConnMap.end()) {
        //TODO: err: remote connection not found
        return NIXL_XFER_ERR;
    }

    conn = remoteConnMap[remote_agent];

    hdr.op = NOTIF_STR;
    flags |= UCP_AM_SEND_FLAG_EAGER;

    ser_des.addStr("name", localAgent);
    ser_des.addStr("msg", msg);
    // TODO: replace with mpool for performance
    ser_msg = new std::string(ser_des.exportStr());

    ret = uw->sendAm(conn.ep, NOTIF_STR,
                     &hdr, sizeof(struct nixl_ucx_am_hdr),
                     (void*) ser_msg->data(), ser_msg->size(),
                     flags, req);

    if (ret == NIXL_XFER_PROC) {
        nixlUcxBckndReq* nReq = (nixlUcxBckndReq*)req;
        nReq->amBuffer = ser_msg;
    }
    return ret;
}

ucs_status_t
nixlUcxEngine::notifAmCb(void *arg, const void *header,
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

    notif_list_t &l = engine->notifMainList;
    if (engine->isProgressThread()) {
        /* Append to the private list to allow batching */
        l = engine->notifPthrPriv;
    }
    l.push_back(std::make_pair(remote_name, msg));

    return UCS_OK;
}


void nixlUcxEngine::notifCombineHelper(notif_list_t &src, notif_list_t &tgt)
{
    if (!src.size()) {
        // Nothing to do. Exit
        return;
    }

    move(src.begin(), src.end(), back_inserter(tgt));
    src.erase(src.begin(), src.end());
}

void nixlUcxEngine::notifProgressCombineHelper(notif_list_t &src, notif_list_t &tgt)
{
    if (!src.size()) {
        // Nothing to do. Exit
        return;
    }

    notifMtx.lock();

    move(src.begin(), src.end(), back_inserter(tgt));
    src.erase(src.begin(), src.end());

    notifMtx.unlock();
}

void nixlUcxEngine::notifProgress()
{
    notifProgressCombineHelper(notifPthrPriv, notifPthr);
}

int nixlUcxEngine::getNotifs(notif_list_t &notif_list)
{
    if (notif_list.size()!=0)
        return -1;

    notifCombineHelper(notifMainList, notif_list);
    notifProgressCombineHelper(notifPthr, notif_list);
   
    return notif_list.size();
}

int nixlUcxEngine::genNotif(const std::string &remote_agent, const std::string &msg)
{
    xfer_state_t ret;
    nixlUcxReq req;

    ret = notifSendPriv(remote_agent, msg, req);

    switch(ret) {
    case NIXL_XFER_PROC:
        /* do not track the request */
        uw->reqRelease(req);
    case NIXL_XFER_DONE:
        break;
    case NIXL_XFER_ERR:
        // TODO output the error cause
        return -1;
    default:
        /* Should not happen */
        assert(0);
        return -1;        
    }
    return 0;
}