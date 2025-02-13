#ifndef __UCX_UTILS_H
#define __UCX_UTILS_H

extern "C"
{
#include <ucp/api/ucp.h>
}

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

typedef void * nixlUcxReq;

class nixlUcxWorker {
private:
    /* Local UCX stuff */
    ucp_context_h ctx;
    ucp_worker_h worker;

public:

    typedef void req_cb_t(void *request);
    nixlUcxWorker(std::vector<std::string> devices, size_t req_size, req_cb_t init_cb, req_cb_t fini_cb);
    //{
        // Create UCX worker spanning provided devices
        // Have a special conf when we want UCX to detect devices
        // automatically
    //}

    ~nixlUcxWorker();

    /* Connection */
    int epAddr(uint64_t &addr, size_t &size);
    int connect(void* addr, size_t size, nixlUcxEp &ep);
    int disconnect(nixlUcxEp &ep);

    /* Memory management */
    int memReg(void *addr, size_t size, nixlUcxMem &mem);
    size_t packRkey(nixlUcxMem &mem, uint64_t &addr, size_t &size);
    void memDereg(nixlUcxMem &mem);

    /* Rkey */
    int rkeyImport(nixlUcxEp &ep, void* addr, size_t size, nixlUcxRkey &rkey);
    void rkeyDestroy(nixlUcxRkey &rkey);

    /* Active message handling */
    int regAmCallback(unsigned msg_id, ucp_am_recv_callback_t cb, void* arg);
    int sendAm(nixlUcxEp &ep, unsigned msg_id, void* hdr, size_t hdr_len, void* buffer, size_t len, uint32_t flags, nixlUcxReq &req);
    int getRndvData(void* data_desc, void* buffer, size_t len, const ucp_request_param_t *param, nixlUcxReq &req);

    /* Data access */
    int progress();
    transfer_state_t read(nixlUcxEp &ep,
                            uint64_t raddr, nixlUcxRkey &rk,
                            void *laddr, nixlUcxMem &mem,
                            size_t size, nixlUcxReq &req);
    transfer_state_t write(nixlUcxEp &ep,
                            void *laddr, nixlUcxMem &mem,
                            uint64_t raddr, nixlUcxRkey &rk,
                            size_t size, nixlUcxReq &req);
    transfer_state_t test(nixlUcxReq &req);

};

#endif

