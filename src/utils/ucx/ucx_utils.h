#include <ucp/api/ucp.h>
#include "nixl.h"

class nixlUcxEp {
private:
    ucp_ep_h  eph;

public:
    friend class nixlUcxWorker;
};

class nixlUcxMem {
private:
    void *base;
    size_t size;
    ucp_mem_h memh;
public:
    friend class nixlUcxWorker;
};

class nixlUcxRkey {
private:
    ucp_rkey_h rkeyh;

public:

    friend class nixlUcxWorker;
};

class nixlUcxReq : public BackendTransferHandle {
private:
    int complete;
    void* reqh;

public:
    nixlUcxReq() {
        complete = 0;
    }

    friend class nixlUcxWorker;
};

class nixlUcxWorker {
private:
    /* Local UCX stuff */
    ucp_context_h ctx;
    ucp_worker_h worker;

public:
    nixlUcxWorker(std::vector<std::string> devices);
    //{
        // Create UCX worker spanning provided devices
        // Have a special conf when we want UCX to detect devices
        // automatically
    //}

    ~nixlUcxWorker();

    /* Connection */
    int ep_addr(uint64_t &addr, size_t &size);
    int connect(void* addr, size_t size, nixlUcxEp &ep);
    int disconnect(nixlUcxEp &ep);

    /* Memory management */
    int mem_reg(void *addr, size_t size, nixlUcxMem &mem);
    size_t mem_addr(nixlUcxMem &mem, uint64_t &addr, size_t size);
    void mem_dereg(nixlUcxMem &mem);

    /* Rkey */
    int rkey_import(nixlUcxEp &ep, void* addr, size_t size, nixlUcxRkey &rkey);
    void rkey_destroy(nixlUcxRkey &rkey);

    /* Active message handling */
    int reg_am_callback(unsigned msg_id, ucp_am_recv_callback_t cb, void* arg);
    int send_am(nixlUcxEp &ep, unsigned msg_id, void* hdr, size_t hdr_len, void* buffer, size_t len, uint32_t flags, nixlUcxReq &req);
    int get_rndv_data(void* data_desc, void* buffer, size_t len, const ucp_request_param_t *param, nixlUcxReq &req);

    /* Data access */
    int progress();
    int read(nixlUcxEp &ep,
            uint64_t raddr, nixlUcxRkey &rk,
            void *laddr, nixlUcxMem &mem,
            size_t size, nixlUcxReq &req);
    int write(nixlUcxEp &ep,
            void *laddr, nixlUcxMem &mem,
            uint64_t raddr, nixlUcxRkey &rk,
            size_t size, nixlUcxReq &req);
    int test(nixlUcxReq &req);

};

