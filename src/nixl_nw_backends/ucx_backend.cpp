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
    using namespace nixlTime;
    pthrActive = 1;

    while (!pthrStop) {
        int i;
        for(i = 0; i < noSyncIters; i++) {
            uw->progress();
        }
        notifProgress();
        // TODO: once NIXL thread infrastructure is available - move it there!!!

        /* Wait for predefined number of */
        us_t start = getUs();
        while( (start + pthrDelay) > getUs()) {
            std::this_thread::yield();
        }
    }
}

void nixlUcxEngine::startProgressThread(nixlTime::us_t delay)
{
    pthrStop = pthrActive = 0;
    noSyncIters = 32;

    pthrDelay = delay;

    // Start the thread
    // TODO [Relaxed mem] mem barrier to ensure pthr_x updates are complete
    new (&pthr) std::thread(&nixlUcxEngine::progressFunc, this);

    // Wait for the thread to be started
    while(!pthrActive){
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

void nixlUcxEngine::stopProgressThread()
{
    pthrStop = 1;
    pthr.join();
}

/****************************************
 * Constructor/Destructor
*****************************************/

nixlUcxEngine::nixlUcxEngine (const nixlUcxInitParams* init_params)
: nixlBackendEngine ((const nixlBackendInitParams*) init_params) {
    std::vector<std::string> devs; /* Empty vector */
    uint64_t                 n_addr;

    if (init_params->enableProgTh) {
        if (!nixlUcxContext::mtLevelIsSupproted(NIXL_UCX_MT_WORKER)) {
            this->initErr = true;
            return;
        }
    }

    uc = new nixlUcxContext(init_params->devices, sizeof(nixlUcxBckndReq),
                           _requestInit, _requestFini, NIXL_UCX_MT_WORKER);
    uw = new nixlUcxWorker(uc);
    uw->epAddr(n_addr, workerSize);
    workerAddr = (void*) n_addr;

    uw->regAmCallback(CONN_CHECK, connectionCheckAmCb, this);
    uw->regAmCallback(DISCONNECT, connectionTermAmCb, this);
    uw->regAmCallback(NOTIF_STR, notifAmCb, this);

    if (init_params->enableProgTh) {
        pthrOn = true;
        startProgressThread(init_params->pthrDelay);
    } else {
        pthrOn = false;
    }
    // TODO: check if UCX does not support threading but it was asked to set initErr.
    //       and destruct any allocated resources, or use the flag in destructor.
}

// Through parent destructor the unregister will be called.
nixlUcxEngine::~nixlUcxEngine () {
    // per registered memory deregisters it, which removes the corresponding metadata too
    // parent destructor takes care of the desc list
    // For remote metadata, they should be removed here
    if (this->initErr) {
        // Nothing to do
        return;
    }

    if (pthrOn)
        stopProgressThread();
    delete uw;
    delete uc;
}

/****************************************
 * Connection management
*****************************************/

nixl_status_t nixlUcxEngine::checkConn(const std::string &remote_agent) {
     if(remoteConnMap.find(remote_agent) == remoteConnMap.end()) {
        return NIXL_ERR_NOT_FOUND;
    }
    return NIXL_SUCCESS;
}

nixl_status_t nixlUcxEngine::endConn(const std::string &remote_agent) {

    auto search = remoteConnMap.find(remote_agent);

    if(search == remoteConnMap.end()) {
        return NIXL_ERR_NOT_FOUND;
    }

    nixlUcxConnection &conn = remoteConnMap[remote_agent];

    if(uw->disconnect_nb(conn.ep) < 0) {
        return NIXL_ERR_BACKEND;
    }

    //thread safety?
    remoteConnMap.erase(remote_agent);

    return NIXL_SUCCESS;
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

    if(engine->checkConn(remote_agent)) {
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

    if(NIXL_SUCCESS != engine->endConn(remote_agent)) {
        //TODO: received connect AM from agent we don't recognize
        return UCS_ERR_INVALID_PARAM;
    }

    return UCS_OK;
}

nixl_status_t nixlUcxEngine::connect(const std::string &remote_agent) {
    struct nixl_ucx_am_hdr hdr;
    uint32_t flags = 0;
    nixl_xfer_state_t ret;
    nixlUcxReq req;

    auto search = remoteConnMap.find(remote_agent);

    if(search == remoteConnMap.end()) {
        return NIXL_ERR_NOT_FOUND;
    }

    nixlUcxConnection &conn = remoteConnMap[remote_agent];

    hdr.op = CONN_CHECK;
    //agent names should never be long enough to need RNDV
    flags |= UCP_AM_SEND_FLAG_EAGER;

    ret = uw->sendAm(conn.ep, CONN_CHECK,
                     &hdr, sizeof(struct nixl_ucx_am_hdr),
                     (void*) localAgent.data(), localAgent.size(),
                     flags, req);

    if(ret == NIXL_XFER_ERR) {
        return NIXL_ERR_BACKEND;
    }

    //wait for AM to send
    while(ret == NIXL_XFER_PROC){
        ret = uw->test(req);
    }

    return NIXL_SUCCESS;
}

nixl_status_t nixlUcxEngine::disconnect(const std::string &remote_agent) {

    static struct nixl_ucx_am_hdr hdr;
    uint32_t flags = 0;
    nixl_xfer_state_t ret;
    nixlUcxReq req;

    auto search = remoteConnMap.find(remote_agent);

    if(search == remoteConnMap.end()) {
        return NIXL_ERR_NOT_FOUND;
    }

    nixlUcxConnection &conn = remoteConnMap[remote_agent];

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

    return NIXL_SUCCESS;
}

nixl_status_t nixlUcxEngine::loadRemoteConnInfo (const std::string &remote_agent,
                                                 const std::string &remote_conn_info)
{
    size_t size = remote_conn_info.size();
    nixlUcxConnection conn;
    int ret;
    //TODO: eventually std::byte?
    char* addr = new char[size];

    if(remoteConnMap.find(remote_agent) != remoteConnMap.end()) {
        return NIXL_ERR_INVALID_PARAM;
    }

    nixlSerDes::_stringToBytes((void*) addr, remote_conn_info, size);
    ret = uw->connect(addr, size, conn.ep);
    if (ret) {
        return NIXL_ERR_BACKEND;
    }

    conn.remoteAgent = remote_agent;
    conn.connected = false;

    remoteConnMap[remote_agent] = conn;

    return NIXL_SUCCESS;
}

/****************************************
 * Memory management
*****************************************/
nixl_status_t nixlUcxEngine::registerMem (const nixlStringDesc &mem,
                                          const nixl_mem_t &nixl_mem,
                                          nixlBackendMD* &out)
{
    int ret;
    nixlUcxPrivateMetadata *priv = new nixlUcxPrivateMetadata;
    uint64_t rkey_addr;
    size_t rkey_size;

    // TODO: Add nixl_mem check?
    ret = uw->memReg((void*) mem.addr, mem.len, priv->mem);
    if (ret) {
        return NIXL_ERR_BACKEND;
    }
    ret = uw->packRkey(priv->mem, rkey_addr, rkey_size);
    if (ret) {
        return NIXL_ERR_BACKEND;
    }
    priv->rkeyStr = nixlSerDes::_bytesToString((void*) rkey_addr, rkey_size);

    out = (nixlBackendMD*) priv; //typecast?

    return NIXL_SUCCESS; // Or errors
}

void nixlUcxEngine::deregisterMem (nixlBackendMD* meta)
{
    nixlUcxPrivateMetadata *priv = (nixlUcxPrivateMetadata*) meta; //typecast?

    uw->memDereg(priv->mem);
    delete priv;
}

std::string nixlUcxEngine::getPublicData (const nixlBackendMD* meta) const {
    const nixlUcxPrivateMetadata *priv = (nixlUcxPrivateMetadata*) meta;
    return priv->get();
}

nixl_status_t nixlUcxEngine::loadLocalMD (nixlBackendMD* input,
                                          nixlBackendMD* &output) {
    nixlUcxConnection conn;
    nixlUcxPrivateMetadata* input_md = (nixlUcxPrivateMetadata*) input;
    nixlUcxPublicMetadata *md = new nixlUcxPublicMetadata;

    //look up our own name
    auto search = remoteConnMap.find(localAgent);

    if(search == remoteConnMap.end()) {
        //TODO: something wrong, local connection should have been established
        return NIXL_ERR_NOT_FOUND;
    }
    conn = (nixlUcxConnection) search->second;

    //directly copy underlying conn struct
    md->conn = conn;

    size_t size = input_md->rkeyStr.size();
    char *addr = new char[size];
    nixlSerDes::_stringToBytes(addr, input_md->rkeyStr, size);
    
    int ret = uw->rkeyImport(conn.ep, addr, size, md->rkey);
    if (ret) {
        // TODO: error out. Should we indicate which desc failed or unroll everything prior
        return NIXL_ERR_BACKEND;
    }

    output = (nixlBackendMD*) md;

    return NIXL_SUCCESS;
}

// To be cleaned up
nixl_status_t nixlUcxEngine::loadRemoteMD (const nixlStringDesc &input,
                                           const nixl_mem_t &nixl_mem,
                                           const std::string &remote_agent,
                                           nixlBackendMD* &output) {
    size_t size = input.metaInfo.size();
    char *addr = new char[size];
    int ret;
    nixlUcxConnection conn;

    nixlUcxPublicMetadata *md = new nixlUcxPublicMetadata;

    auto search = remoteConnMap.find(remote_agent);

    if(search == remoteConnMap.end()) {
        //TODO: err: remote connection not found
        return NIXL_ERR_NOT_FOUND;
    }
    conn = (nixlUcxConnection) search->second;

    nixlSerDes::_stringToBytes(addr, input.metaInfo, size);

    md->conn = conn;
    ret = uw->rkeyImport(conn.ep, addr, size, md->rkey);
    if (ret) {
        // TODO: error out. Should we indicate which desc failed or unroll everything prior
        return NIXL_ERR_BACKEND;
    }
    output = (nixlBackendMD*) md;

    return NIXL_SUCCESS;
}

nixl_status_t nixlUcxEngine::removeRemoteMD (nixlBackendMD* input) {

    nixlUcxPublicMetadata *md = (nixlUcxPublicMetadata*) input; //typecast?

    uw->rkeyDestroy(md->rkey);
    delete md;

    return NIXL_SUCCESS;
}

/****************************************
 * Data movement
*****************************************/

nixl_status_t nixlUcxEngine::retHelper(nixl_xfer_state_t ret, nixlUcxBckndReq *head, nixlUcxReq &req)
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
            return NIXL_ERR_BACKEND;
    }
    return NIXL_SUCCESS;
}

nixl_xfer_state_t nixlUcxEngine::postXfer (const nixl_meta_dlist_t &local,
                                           const nixl_meta_dlist_t &remote,
                                           const nixl_xfer_op_t &op,
                                           const std::string &remote_agent,
                                           const std::string &notif_msg,
                                           nixlBackendReqH* &handle)
{
    size_t lcnt = local.descCount();
    size_t rcnt = remote.descCount();
    size_t i;
    nixl_xfer_state_t ret;
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

        lmd = (nixlUcxPrivateMetadata*) local[i].metadataP;
        rmd = (nixlUcxPublicMetadata*) remote[i].metadataP;

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

    rmd = (nixlUcxPublicMetadata*) remote[0].metadataP;
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

nixl_xfer_state_t nixlUcxEngine::checkXfer (nixlBackendReqH* handle)
{
    nixlUcxBckndReq *head = (nixlUcxBckndReq *)handle;
    nixlUcxBckndReq *req = head;
    nixl_xfer_state_t out_ret = NIXL_XFER_DONE;

    /* If transfer has returned DONE - no check transfer */
    if (NULL == head) {
        /* Nothing to do */
        return NIXL_XFER_ERR;
    }

    /* Go over all request updating their status */
    while(req) {
        nixl_xfer_state_t ret;
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

    //this case should not happen
    //if (head == NULL) return;

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
nixl_xfer_state_t nixlUcxEngine::notifSendPriv(const std::string &remote_agent,
                                               const std::string &msg, nixlUcxReq &req)
{
    nixlSerDes ser_des;
    std::string *ser_msg;
    nixlUcxConnection conn;
    // TODO - temp fix, need to have an mpool
    static struct nixl_ucx_am_hdr hdr;
    uint32_t flags = 0;
    nixl_xfer_state_t ret;

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

    if (engine->isProgressThread()) {
        /* Append to the private list to allow batching */
        engine->notifPthrPriv.push_back(std::make_pair(remote_name, msg));
    } else {
        engine->notifMainList.push_back(std::make_pair(remote_name, msg));
    }

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
    notifMtx.lock();

    if (src.size()) {
        move(src.begin(), src.end(), back_inserter(tgt));
        src.erase(src.begin(), src.end());
    }

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

    if(!pthrOn) while(progress());

    notifCombineHelper(notifMainList, notif_list);
    notifProgressCombineHelper(notifPthr, notif_list);

    return notif_list.size();
}

nixl_status_t nixlUcxEngine::genNotif(const std::string &remote_agent, const std::string &msg)
{
    nixl_xfer_state_t ret;
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
        return NIXL_ERR_BACKEND;
    default:
        /* Should not happen */
        assert(0);
        return NIXL_ERR_BAD;
    }
    return NIXL_SUCCESS;
}
