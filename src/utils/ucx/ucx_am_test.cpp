#include <vector>
#include <string>
#include <cassert>
#include <cstring>
#include <iostream>

#include "ucx_utils.h"

using namespace std;

struct sample_header {
    uint32_t test;
};

ucs_status_t check_buffer (void *arg, const void *header,
   		                   size_t header_length, void *data,
				           size_t length, 
				           const ucp_am_recv_param_t *param)
{
    struct sample_header* hdr = (struct sample_header*) header;
    //TODO: is data 8 byte aligned?
    uint64_t recv_data = *((uint64_t*) data);

    if(hdr->test != 0xcee) 
    {
	    return UCS_ERR_INVALID_PARAM;
    }

    assert(length == 8);
    assert(recv_data == 0xdeaddeaddeadbeef);

    std::cout << "check_buffer passed\n";

    return UCS_OK;
}

ucs_status_t rndv_test (void *arg, const void *header,
   		                   size_t header_length, void *data,
				           size_t length, 
				           const ucp_am_recv_param_t *param)
{

    struct sample_header* hdr = (struct sample_header*) header;
    void* recv_buffer = calloc(1, length);
    nixlUcxWorker* am_worker = (nixlUcxWorker*) arg;
    ucp_request_param_t recv_param = {0};
    uint64_t check_data;
    nixlUcxReq req;
    int ret = 0;

    if(hdr->test != 0xcee) 
    {
	    return UCS_ERR_INVALID_PARAM;
    }

    std::cout << "rndv_test started\n";
    
    assert(param->recv_attr & UCP_AM_RECV_ATTR_FLAG_RNDV);
   
    ret = am_worker->get_rndv_data(data, recv_buffer, length, &recv_param, req);
    assert(ret == 0);

    while(ret == 0){
	    ret = am_worker->test(req);
    }

    check_data = ((uint64_t*) recv_buffer)[0];
    assert(check_data == 0xdeaddeaddeadbeef);

    std::cout << "rndv_test passed\n";
    
    return UCS_OK;
}

int main()
{
    vector<string> devs;
    devs.push_back("mlx5_0");
    nixlUcxWorker w[2] = { nixlUcxWorker(devs), nixlUcxWorker(devs) };
    nixlUcxEp ep[2];
    nixlUcxReq req;
    uint64_t buffer;
    int ret, i;

    unsigned check_cb_id = 1, rndv_cb_id = 2;

    void* big_buffer = calloc(1, 8192);
    struct sample_header hdr = {0};

    buffer = 0xdeaddeaddeadbeef;
    ((uint64_t*) big_buffer)[0] = 0xdeaddeaddeadbeef;
    hdr.test = 0xcee;

    /* Test control path */
    for(i = 0; i < 2; i++) {
        uint64_t addr;
        size_t size;
        assert(0 == w[i].ep_addr(addr, size));
        assert(0 == w[!i].connect((void*) addr, size, ep[!i]));
	
	//no need for mem_reg with active messages
	//assert(0 == w[i].mem_reg(buffer[i], 128, mem[i]));
        //assert(0 == w[i].mem_addr(mem[i], addr, size));
        //assert(0 == w[!i].rkey_import(ep[!i], (void*) addr, size, rkey[!i]));
        free((void*) addr);
    }

    /* Register active message callbacks */
    ret = w[0].reg_am_callback(check_cb_id, check_buffer, NULL);
    assert(ret == 0);

    w[0].progress();
    w[1].progress();

    ret = w[0].reg_am_callback(rndv_cb_id, rndv_test, &(w[0]));
    assert(ret == 0);
    
    w[0].progress();

    /* Test first callback */
    ret = w[1].send_am(ep[1], check_cb_id, &hdr, sizeof(struct sample_header), (void*) &buffer, sizeof(buffer), 0, req);
    assert(ret == 0);

    while(ret == 0){
	    ret = w[1].test(req);
	    w[0].progress();
    }

    std::cout << "first active message complete\n";

    /* Test second callback */
    uint32_t flags = 0;
    flags |= UCP_AM_SEND_FLAG_RNDV;

    ret = w[1].send_am(ep[1], rndv_cb_id, &hdr, sizeof(struct sample_header), big_buffer, 8192, flags, req);
    assert(ret == 0);

    while(ret == 0){
	    ret = w[1].test(req);
	    w[0].progress();
    }

    std::cout << "second active message complete\n";

    //make sure callbacks are complete
    w[0].progress();

    /* Test shutdown */
    for(i = 0; i < 2; i++) {
        assert(0 == w[i].disconnect(ep[i]));
    }

}
