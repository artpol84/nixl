#include "ucx_backend.h"

//not sure if this is the best way to handle init_params
nixlUcxEngine::nixlUcxEngine (nixlUcxInitParams* init_params)
: BackendEngine ((BackendInitParams*) init_params) {
    std::vector<std::string> devs; /* Empty vector */
    uint64_t n_addr;

    uw = new nixlUcxWorker(devs);
    uw->ep_addr(n_addr, worker_size);
    worker_addr = (void*) n_addr;
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

std::string nixlUcxEngine::_bytes_to_string(void *buf, size_t size) const {
    std::string ret_str;

    char temp[16];
    uint8_t* bytes = (uint8_t*) buf;

    for(size_t i = 0; i<size; i++) {
        bytes = (uint8_t*) buf;
        sprintf(temp, "%02x", bytes[i]);
        ret_str.append(temp, 2);
    }
    return ret_str;
}

void * nixlUcxEngine::_string_to_bytes(std::string &s, size_t &size){
    size = s.size()/2;
    uint8_t* ret = (uint8_t*) calloc(1, size);
    char temp[3];
    const uint8_t* in_str = (uint8_t*) s.c_str();

    for(size_t i = 0; i<(s.size()); i+=2) {
        memcpy(temp, in_str + i, 2);
        ret[(i/2)] = (uint8_t) strtoul(temp, NULL, 16);
    }

    return ret;
}

std::string nixlUcxEngine::get_public_data(BackendMetadata* &meta) const {
    return ((nixlUcxPrivateMetadata*) meta)->get(); 
}

/****************************************
 * Connection management
*****************************************/
std::string nixlUcxEngine::get_conn_info() const {
    return _bytes_to_string(worker_addr, worker_size);
}


int nixlUcxEngine::load_remote_conn_info (std::string remote_agent,
                                       std::string remote_conn_info)
{
    size_t size;
    nixlUcxConnection conn;
    int ret;
    void* addr;

    if(remote_conn_map.find(remote_agent) != remote_conn_map.end()) {
        //already connected?
        return -1;
    }

    addr = _string_to_bytes(remote_conn_info, size);
    ret = uw->connect(addr, size, conn.ep);
    if (ret) {
        // TODO: print error
        return -1;
    }

    conn.remote_agent = remote_agent;

    remote_conn_map[remote_agent] = conn;

    free(addr);

    return 0;
}

// Make connection to a remote node accompanied by required info.
int nixlUcxEngine::make_connection(std::string remote_agent) {
    // Nothing to do - lazy connection will be established automatically
    // at first communication
    return 0;
}

int nixlUcxEngine::listen_for_connection(std::string remote_agent) {
    // Nothing to do - UCX implements one-sided
    return 0;
}

int nixlUcxEngine::register_mem (BasicDesc &mem, memory_type_t mem_type,
                              BackendMetadata* &out)
{
    int ret;
    nixlUcxPrivateMetadata *priv = new nixlUcxPrivateMetadata;
    uint64_t mem_addr;
    size_t size;

    ret = uw->mem_reg((void*) mem.addr, mem.len, priv->mem);
    if (ret) {
        // TODO: err out
        return -1;
    }
    ret = uw->mem_addr(priv->mem, mem_addr, size);
    if (ret) {
        // TODO: err out
        return -1;
    }
    priv->rkey_str = _bytes_to_string((void*) mem_addr, size);

    out = (BackendMetadata*) priv; //typecast?

    return 0; // Or errors
}

void nixlUcxEngine::deregister_mem (BackendMetadata* desc)
{
    nixlUcxPrivateMetadata *priv = (nixlUcxPrivateMetadata*) desc; //typecast?

    uw->mem_dereg(priv->mem);
    delete priv;
}


// To be cleaned up
int nixlUcxEngine::load_remote (StringDesc input,
                             BackendMetadata* &output,
                             std::string remote_agent) {
    void *addr;
    size_t size;
    int ret;
    nixlUcxConnection conn;

    nixlUcxPublicMetadata *md = new nixlUcxPublicMetadata;

    auto search = remote_conn_map.find(remote_agent);

    if(search == remote_conn_map.end()) {
        //TODO: err: remote connection not found
        return -1;
    }
    conn = search->second;

    addr = _string_to_bytes(input.metadata, size);

    md->conn = conn;
    ret = uw->rkey_import(conn.ep, addr, size, md->rkey);
    if (ret) {
        // TODO: error out. Should we indicate which desc failed or unroll everything prior
        return -1;
    }
    output = (BackendMetadata*) md;
    
    free(addr);
    return 0;
}

int nixlUcxEngine::remove_remote (BackendMetadata* input) {

    nixlUcxPublicMetadata *md = (nixlUcxPublicMetadata*) input; //typecast?

    uw->rkey_destroy(md->rkey);
    delete md;

    return 0;
}

// using META desc for local list
int nixlUcxEngine::transfer (DescList<MetaDesc> local,
                          DescList<MetaDesc> remote,
                          transfer_op_t op,
                          std::string notif_msg,
                          BackendTransferHandle* &handle)
{
    size_t lcnt = local.desc_count();
    size_t rcnt = remote.desc_count();
    size_t i, ret;

    nixlUcxReq req;

    if (lcnt != rcnt) {
        return -1;
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
            return -1;
        }

        switch (op) {
        case READ:
            ret = uw->read(rmd->conn.ep, (uint64_t) raddr, rmd->rkey, laddr, lmd->mem, lsize, req);
            break;
        case WRITE:
            ret = uw->write(rmd->conn.ep, laddr, lmd->mem, (uint64_t) raddr, rmd->rkey, lsize, req);
            break;
        default:
            return -1;
        }
    }

    handle = (BackendTransferHandle*) req.reqh;

    return ret;
}

int nixlUcxEngine::check_transfer (BackendTransferHandle* handle) {
    nixlUcxReq req;
    req.reqh = (void*) handle;
    return uw->test(req);
}

int nixlUcxEngine::progress() {
    return uw->progress();
}
