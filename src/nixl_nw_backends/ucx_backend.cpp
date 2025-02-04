#include "ucx_backend.h"

//not sure if this is the best way to handle init_params
ucx_engine::ucx_engine (ucx_init_params* init_params)
: backend_engine ((backend_init_params*) init_params) {
    std::vector<std::string> devs; /* Empty vector */
    uint64_t n_addr;

    uw = new nixl_ucx_worker(devs);
    uw->ep_addr(n_addr, worker_size);
    worker_addr = (void*) n_addr;
}

// Through parent destructor the unregister will be called.
ucx_engine::~ucx_engine () {
    // per registered memory deregisters it, which removes the corresponding metadata too
    // parent destructor takes care of the desc list
    // For remote metadata, they should be removed here
    delete uw;
}

/****************************************
 * Helpers
*****************************************/

std::string ucx_engine::_bytes_to_string(void *buf, size_t size) const {
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

void * ucx_engine::_string_to_bytes(std::string &s, size_t &size){
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

/****************************************
 * Connection management
*****************************************/
std::string ucx_engine::get_conn_info() const {
    return _bytes_to_string(worker_addr, worker_size);
}


int ucx_engine::load_remote_conn_info (std::string remote_agent,
                                       std::string remote_conn_info)
{
    size_t size;
    ucx_connection conn;
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

    return 0;
}

// Make connection to a remote node accompanied by required info.
int ucx_engine::make_connection(std::string remote_agent) {
    // Nothing to do - lazy connection will be established automatically
    // at first communication
    return 0;
}

int ucx_engine::listen_for_connection(std::string remote_agent) {
    // Nothing to do - UCX implements one-sided
    return 0;
}

int ucx_engine::register_mem (basic_desc &mem, memory_type_t mem_type,
                              backend_metadata* &out)
{
    int ret;
    ucx_private_metadata *priv = new ucx_private_metadata;

    ret = uw->mem_reg((void*) mem.addr, mem.len, priv->mem);
    if (ret) {
        // TODO: err out
        return -1;
    }
    ret = uw->mem_addr(priv->mem, (uint64_t&) priv->mem_addr, priv->mem_addr_size);
    if (ret) {
        // TODO: err out
        return -1;
    }

    out = (backend_metadata*) priv; //typecast?

    return 0; // Or errors
}

void ucx_engine::deregister_mem (backend_metadata* desc)
{
    ucx_private_metadata *priv = (ucx_private_metadata*) desc; //typecast?

    uw->mem_dereg(priv->mem);
    delete priv;
}


// To be cleaned up
int ucx_engine::load_remote (string_desc input,
                             backend_metadata* &output,
                             std::string remote_agent) {
    void *addr = (void*) input.addr;
    size_t size = input.len;
    int ret;
    ucx_connection conn;

    ucx_public_metadata *md = new ucx_public_metadata;

    auto search = remote_conn_map.find(remote_agent);

    if(search == remote_conn_map.end()) {
	//TODO: err: remote connection not found
	return -1;
    }
    conn = search->second;

    md->conn = conn;
    ret = uw->rkey_import(conn.ep, addr, size, md->rkey);
    if (ret) {
        // TODO: error out. Should we indicate which desc failed or unroll everything prior
        return -1;
    }
    output = (backend_metadata*) md;

    return 0;
}

int ucx_engine::remove_remote (backend_metadata* input) {

    ucx_public_metadata *md = (ucx_public_metadata*) input; //typecast?

    uw->rkey_destroy(md->rkey);
    delete md;

    return 0;
}

// using META desc for local list
int ucx_engine::transfer (desc_list<meta_desc> local,
                          desc_list<meta_desc> remote,
                          transfer_op_t op,
			  backend_transfer_handle* &handle)
{
    size_t lcnt = local.descs.size();
    size_t rcnt = remote.descs.size();
    size_t i, ret;

    nixl_ucx_req req;

    if (lcnt != rcnt) {
        return -1;
    }

    for(i = 0; i < lcnt; i++) {
        void *laddr = (void*) local.descs[i].addr;
        size_t lsize = local.descs[i].len;
        void *raddr = (void*) remote.descs[i].addr;
        size_t rsize = remote.descs[i].len;

        ucx_private_metadata *lmd = (ucx_private_metadata*) local.descs[i].metadata;
        ucx_public_metadata *rmd = (ucx_public_metadata*) remote.descs[i].metadata; //typecast?

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

    handle = (backend_transfer_handle*) &req;

    return ret;
}

int ucx_engine::check_transfer (backend_transfer_handle* handle) {
    nixl_ucx_req req = *((nixl_ucx_req*) handle);
    return uw->test(req);
}

int ucx_engine::progress() {
    return uw->progress();
}
