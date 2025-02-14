#include <iostream>
#include <cassert>

#include "nixl.h"
#include "ucx_backend.h"

int main()
{
    int ret1, ret2;

    // Example: assuming two agents running on the same machine,
    // with separate memory regions in DRAM
    std::string agent1("Agent1");
    std::string agent2("Agent2");

    nixlDeviceMD devices;
    nixlUcxInitParams init1, init2;
    // populate required/desired inits

    nixlAgent A1("Agent1", devices);
    nixlAgent A2("Agent2", devices);

    nixlBackendEngine* ucx1 = A1.createBackend(&init1);
    nixlBackendEngine* ucx2 = A2.createBackend(&init2);

    // // One side gets to listen, one side to initiate. Same string is passed as the last 2 steps
    // ret1 = A1->makeConnection("Agent2", 0);
    // ret2 = A2->makeConnection("Agent1", 1);

    // assert(ret1 == 0);
    // assert(ret2 == 0);

    // User allocates memories, and passes the corresponding address
    // and length to register with the backend
    nixlBasicDesc buff1, buff2;
    nixlDescList<nixlBasicDesc> dlist1(DRAM_SEG), dlist2(DRAM_SEG);
    size_t len = 256;
    void* addr1 = calloc(1, len);
    void* addr2 = calloc(1, len);

    memset(addr1, 0xbb, len);
    memset(addr2, 0, len);

    buff1.addr   = (uintptr_t) addr1;
    buff1.len    = len;
    buff1.devId = 0;
    dlist1.addDesc(buff1);

    buff2.addr   = (uintptr_t) addr2;
    buff2.len    = len;
    buff2.devId = 0;
    dlist2.addDesc(buff2);

    // dlist1.print();
    // dlist2.print();

    // sets the metadata field to a pointer to an object inside the ucx_class
    ret1 = A1.registerMem(dlist1, ucx1);
    ret2 = A2.registerMem(dlist2, ucx2);

    assert(ret1 == 0);
    assert(ret2 == 0);

    std::string meta1 = A1.getMetadata();
    std::string meta2 = A2.getMetadata();

    std::cout << "Agent1's Metadata: " << meta1 << "\n";
    std::cout << "Agent2's Metadata: " << meta2 << "\n";

    ret1 = A1.loadMetadata (meta2);
    ret1 = A2.loadMetadata (meta1);

    size_t req_size = 8;
    size_t dst_offset = 8;

    nixlDescList<nixlBasicDesc> req_src_descs (DRAM_SEG);
    nixlBasicDesc req_src;
    req_src.addr     = (uintptr_t) (((char*) addr1) + 16); //random offset
    req_src.len      = req_size;
    req_src.devId   = 0;
    req_src_descs.addDesc(req_src);

    nixlDescList<nixlBasicDesc> req_dst_descs (DRAM_SEG);
    nixlBasicDesc req_dst;
    req_dst.addr   = (uintptr_t) ((char*) addr2) + dst_offset; //random offset
    req_dst.len    = req_size;
    req_dst.devId = 0;
    req_dst_descs.addDesc(req_dst);

    std::cout << "Transfer request from " << addr1 << " to " << addr2 << "\n";
    // Posting a request, to be updated to return an async handler,
    // or an ID that later can be used to check the status as a new method
    // Also maybe we would remove the WRITE and let the backend class decide the op
    nixlXferReqH* req_handle;
    ret1 = A1.createTransferReq(req_src_descs, req_dst_descs, "Agent2", "", 1, req_handle);
    assert(ret1 == 0);

    ret1 = A1.postRequest(req_handle);

    std::cout << "Transfer was posted\n";

    int status = 0;

    while(status != NIXL_XFER_DONE) {
        status = A1.getStatus(req_handle);
        assert(status != NIXL_XFER_ERR);
    }

    // Do some checks on the data.
    for(size_t i = dst_offset; i<req_size; i++){
        assert( ((uint8_t*) addr2)[i] == 0xbb);
    }

    std::cout << "Transfer verified\n";

    A1.invalidateRequest(req_handle);
    ret1 = A1.deregisterMem(dlist1, ucx1);
    ret2 = A2.deregisterMem(dlist2, ucx2);

    A1.invalidateRemoteMetadata("Agent2");
    A2.invalidateRemoteMetadata("Agent1");
}
