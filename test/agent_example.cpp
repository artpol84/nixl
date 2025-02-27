#include <iostream>
#include <cassert>

#include <sys/time.h>

#include "nixl.h"
#include "ucx_backend.h"

void check_buf(void* buf, size_t len) {

    // Do some checks on the data.
    for(size_t i = 0; i<len; i++){
        assert(((uint8_t*) buf)[i] == 0xbb);
    }
}

void test_side_perf(nixlAgent* A1, nixlAgent* A2, nixlBackendEngine* backend, nixlBackendEngine* backend2){


    int n_mems = 32;
    int descs_per_mem = 64*1024;
    int n_iters = 10;
    nixlDescList<nixlBasicDesc> mem_list1(DRAM_SEG), mem_list2(DRAM_SEG);
    nixlDescList<nixlBasicDesc> src_list(DRAM_SEG), dst_list(DRAM_SEG);
    nixl_status_t status;

    struct timeval start_time, end_time, diff_time;

    nixlXferSideH *src_side[n_iters];
    nixlXferSideH *dst_side[n_iters];

    for(int i = 0; i<n_mems; i++) {
        void* src_buf = malloc(descs_per_mem*8);
        void* dst_buf = malloc(descs_per_mem*8);
        nixlBasicDesc src_desc((uintptr_t) src_buf, descs_per_mem*8, 0);
        nixlBasicDesc dst_desc((uintptr_t) dst_buf, descs_per_mem*8, 0);

        mem_list1.addDesc(src_desc);
        mem_list2.addDesc(dst_desc);

        //std::cout << "mem region " << i << " working \n";

        for(int j = 0; j<descs_per_mem; j++){
            nixlBasicDesc src_desc2((uintptr_t) src_buf + 8*j, 8, 0);
            nixlBasicDesc dst_desc2((uintptr_t) dst_buf + 8*j, 8, 0);

            src_list.addDesc(src_desc2);
            dst_list.addDesc(dst_desc2);
        }
    }

    assert(mem_list1.descCount() == n_mems);
    assert(mem_list2.descCount() == n_mems);

    assert(src_list.descCount() == n_mems*descs_per_mem);
    assert(dst_list.descCount() == n_mems*descs_per_mem);

    status = A1->registerMem(mem_list1, backend);
    assert(status == NIXL_SUCCESS);

    status = A2->registerMem(mem_list2, backend2);
    assert(status == NIXL_SUCCESS);

    std::string meta2 = A2->getLocalMD();
    assert(meta2.size() > 0);

    std::string remote_name = A1->loadRemoteMD(meta2);
    assert(remote_name == "Agent2");

    std::cout << "perf setup done\n";

    gettimeofday(&start_time, NULL);

    for(int i = 0; i<n_iters; i++) {
        status = A1->prepXferSide(dst_list, "Agent2", backend, dst_side[i]);
        assert(status == NIXL_SUCCESS);

        status = A1->prepXferSide(src_list, "", backend, src_side[i]);
        assert(status == NIXL_SUCCESS);
    }

    gettimeofday(&end_time, NULL);

    timersub(&end_time, &start_time, &diff_time);
    std::cout << "prepXferSide, total time for " << n_iters << " iters: "
              << diff_time.tv_sec << "s " << diff_time.tv_usec << "us \n";

    float time_per_iter = ((diff_time.tv_sec * 1000000) + diff_time.tv_usec);
    time_per_iter /=  (n_iters) ;
    std::cout << "time per 2 preps " << time_per_iter << "us\n";

    status = A1->deregisterMem(mem_list1, backend);
    assert(status == NIXL_SUCCESS);
    status = A2->deregisterMem(mem_list2, backend2);
    assert(status == NIXL_SUCCESS);

    for(int i = 0; i<n_iters; i++){
        A1->invalidateXferSide(src_side[i]);
        A1->invalidateXferSide(dst_side[i]);
    }
}

nixl_status_t sideXferTest(nixlAgent* A1, nixlAgent* A2, nixlXferReqH* src_handle, nixlBackendEngine* dst_backend){
    std::cout << "Starting sideXferTest\n";

    nixlBackendEngine* src_backend = A1->getXferBackend(src_handle);

    assert(src_backend);

    std::cout << "Got backend\n";

    test_side_perf(A1, A2, src_backend, dst_backend);

    int n_bufs = 4; //must be even
    size_t len = 1024;
    nixl_status_t status;
    void* src_bufs[n_bufs], *dst_bufs[n_bufs];

    nixlDescList<nixlBasicDesc> src_list(DRAM_SEG), dst_list(DRAM_SEG);
    nixlBasicDesc src_desc[4], dst_desc[4];
    for(int i = 0; i<n_bufs; i++) {

        src_bufs[i] = calloc(1, len);
        std::cout << " src " << i << " " << src_bufs[i] << "\n";
        dst_bufs[i] = calloc(1, len);
        std::cout << " dst " << i << " " << dst_bufs[i] << "\n";

        src_desc[i].len = len;
        src_desc[i].devId = 0;
        src_desc[i].addr = (uintptr_t) src_bufs[i];
        dst_desc[i].len = len;
        dst_desc[i].devId = 0;
        dst_desc[i].addr = (uintptr_t) dst_bufs[i];

        src_list.addDesc(src_desc[i]);
        dst_list.addDesc(dst_desc[i]);
    }

    status = A1->registerMem(src_list, src_backend);
    assert(status == NIXL_SUCCESS);

    status = A2->registerMem(dst_list, dst_backend);
    assert(status == NIXL_SUCCESS);

    std::string meta2 = A2->getLocalMD();
    assert(meta2.size() > 0);

    std::string remote_name = A1->loadRemoteMD(meta2);
    assert(remote_name == "Agent2");

    std::cout << "Ready to prepare side\n";

    nixlXferSideH *src_side, *dst_side;

    status = A1->prepXferSide(src_list, "", src_backend, src_side);
    assert(status == NIXL_SUCCESS);

    status = A1->prepXferSide(dst_list, remote_name, src_backend, dst_side);
    assert(status == NIXL_SUCCESS);

    std::cout << "prep done, starting transfers\n";

    std::vector<int> indices1, indices2;

    for(int i = 0; i<(n_bufs/2); i++) {
        //initial bufs
        memset(src_bufs[i], 0xbb, len);
        indices1.push_back(i);
    }
    for(int i = (n_bufs/2); i<n_bufs; i++)
        indices2.push_back(i);

    nixlXferReqH *req1, *req2, *req3;

    //write first half of src_bufs to dst_bufs
    status = A1->makeXferReq(src_side, indices1, dst_side, indices1, "", NIXL_WRITE, req1);
    assert(status == NIXL_SUCCESS);

    nixl_xfer_state_t xfer_status = A1->postXferReq(req1);

    while(xfer_status != NIXL_XFER_DONE) {
        if(xfer_status != NIXL_XFER_DONE) xfer_status = A1->getXferStatus(req1);
        assert(xfer_status != NIXL_XFER_ERR);
    }

    for(int i = 0; i<(n_bufs/2); i++)
        check_buf(dst_bufs[i], len);

    std::cout << "transfer 1 done\n";

    //read first half of dst_bufs back to second half of src_bufs
    status = A1->makeXferReq(src_side, indices2, dst_side, indices1, "", NIXL_READ, req2);
    assert(status == NIXL_SUCCESS);

    xfer_status = A1->postXferReq(req2);

    while(xfer_status != NIXL_XFER_DONE) {
        if(xfer_status != NIXL_XFER_DONE) xfer_status = A1->getXferStatus(req2);
        assert(xfer_status != NIXL_XFER_ERR);
    }

    for(int i = (n_bufs/2); i<n_bufs; i++)
        check_buf(src_bufs[i], len);

    std::cout << "transfer 2 done\n";

    //write second half of src_bufs to dst_bufs
    status = A1->makeXferReq(src_side, indices2, dst_side, indices2, "", NIXL_WRITE, req3);
    assert(status == NIXL_SUCCESS);

    xfer_status = A1->postXferReq(req3);

    while(xfer_status != NIXL_XFER_DONE) {
        if(xfer_status != NIXL_XFER_DONE) xfer_status = A1->getXferStatus(req3);
        assert(xfer_status != NIXL_XFER_ERR);
    }

    for(int i = (n_bufs/2); i<n_bufs; i++)
        check_buf(dst_bufs[i], len);

    std::cout << "transfer 3 done\n";

    A1->invalidateXferReq(req1);
    A1->invalidateXferReq(req2);
    A1->invalidateXferReq(req3);

    status = A1->deregisterMem(src_list, src_backend);
    assert(status == NIXL_SUCCESS);
    status = A2->deregisterMem(dst_list, dst_backend);
    assert(status == NIXL_SUCCESS);

    A1->invalidateXferSide(src_side);
    A1->invalidateXferSide(dst_side);

    return NIXL_SUCCESS;
}

int main()
{
    nixl_status_t ret1, ret2;
    std::string ret_s1, ret_s2;

    // Example: assuming two agents running on the same machine,
    // with separate memory regions in DRAM
    std::string agent1("Agent1");
    std::string agent2("Agent2");

    nixlAgentConfig cfg(true);
    nixlUcxInitParams init1, init2;
    // populate required/desired inits

    nixlAgent A1("Agent1", cfg);
    nixlAgent A2("Agent2", cfg);

    nixlBackendEngine* ucx1 = A1.createBackend(&init1);
    nixlBackendEngine* ucx2 = A2.createBackend(&init2);

    // // One side gets to listen, one side to initiate. Same string is passed as the last 2 steps
    // ret1 = A1->makeConnection("Agent2", 0);
    // ret2 = A2->makeConnection("Agent1", 1);

    // assert(ret1 == NIXL_SUCCESS);
    // assert(ret2 == NIXL_SUCCESS);

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

    assert(ret1 == NIXL_SUCCESS);
    assert(ret2 == NIXL_SUCCESS);

    std::string meta1 = A1.getLocalMD();
    std::string meta2 = A2.getLocalMD();

    std::cout << "Agent1's Metadata: " << meta1 << "\n";
    std::cout << "Agent2's Metadata: " << meta2 << "\n";

    ret_s1 = A1.loadRemoteMD (meta2);
    ret_s2 = A2.loadRemoteMD (meta1);

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
    nixlXferReqH* req_handle;

    ret1 = A1.createXferReq(req_src_descs, req_dst_descs, agent2, "notification", NIXL_WR_NOTIF, req_handle);
    assert(ret1 == NIXL_SUCCESS);

    nixl_xfer_state_t status = A1.postXferReq(req_handle);

    std::cout << "Transfer was posted\n";

    nixl_notifs_t notif_map;
    int n_notifs = 0;

    while(status != NIXL_XFER_DONE || n_notifs == 0) {
        if(status != NIXL_XFER_DONE) status = A1.getXferStatus(req_handle);
        if(n_notifs == 0) n_notifs = A2.getNotifs(notif_map);
        assert(status != NIXL_XFER_ERR);
        assert(n_notifs >= 0);
    }

    std::vector<std::string> agent1_notifs = notif_map[agent1];
    assert(agent1_notifs.size() == 1);
    assert(agent1_notifs.front() == "notification");

    std::cout << "Transfer verified\n";

    std::cout << "performing sideXferTest with backends " << ucx1 << " " << ucx2 << "\n";
    ret1 = sideXferTest(&A1, &A2, req_handle, ucx2);
    assert(ret1 == NIXL_SUCCESS);

    A1.invalidateXferReq(req_handle);
    ret1 = A1.deregisterMem(dlist1, ucx1);
    ret2 = A2.deregisterMem(dlist2, ucx2);

    //only initiator should call invalidate
    A1.invalidateRemoteMD("Agent2");
    //A2.invalidateRemoteMD("Agent1");

    std::cout << "Test done\n";
}
