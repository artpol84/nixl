/*
 * SPDX-FileCopyrightText: Copyright (c) 2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include <iostream>
#include <cassert>

#include <sys/time.h>

#include "nixl.h"
#include "ucx_backend.h"

std::string agent1("Agent001");
std::string agent2("Agent002");

void check_buf(void* buf, size_t len) {

    // Do some checks on the data.
    for(size_t i = 0; i<len; i++){
        assert(((uint8_t*) buf)[i] == 0xbb);
    }
}

bool equal_buf (void* buf1, void* buf2, size_t len) {

    // Do some checks on the data.
    for (size_t i = 0; i<len; i++)
        if (((uint8_t*) buf1)[i] != ((uint8_t*) buf2)[i])
            return false;
    return true;
}

void test_side_perf(nixlAgent* A1, nixlAgent* A2, nixlBackendH* backend, nixlBackendH* backend2){


    int n_mems = 32;
    int descs_per_mem = 64*1024;
    int n_iters = 10;
    nixl_reg_dlist_t mem_list1(DRAM_SEG), mem_list2(DRAM_SEG);
    nixl_xfer_dlist_t src_list(DRAM_SEG), dst_list(DRAM_SEG);
    nixl_status_t status;

    struct timeval start_time, end_time, diff_time;

    nixlXferSideH *src_side[n_iters];
    nixlXferSideH *dst_side[n_iters];

    void* src_buf = malloc(n_mems*descs_per_mem*8);
    void* dst_buf = malloc(n_mems*descs_per_mem*8);

    for(int i = 0; i<n_mems; i++) {
        nixlStringDesc src_desc((uintptr_t) src_buf + i*descs_per_mem*8, descs_per_mem*8, 0);
        nixlStringDesc dst_desc((uintptr_t) dst_buf + i*descs_per_mem*8, descs_per_mem*8, 0);

        mem_list1.addDesc(src_desc);
        mem_list2.addDesc(dst_desc);

        //std::cout << "mem region " << i << " working \n";

        for(int j = 0; j<descs_per_mem; j++){
            nixlBasicDesc src_desc2((uintptr_t) src_buf + i*descs_per_mem*8 + 8*j, 8, 0);
            nixlBasicDesc dst_desc2((uintptr_t) dst_buf + i*descs_per_mem*8 + 8*j, 8, 0);

            src_list.addDesc(src_desc2);
            dst_list.addDesc(dst_desc2);
        }
    }

    assert (src_list.verifySorted() == true);
    assert (dst_list.verifySorted() == true);

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
    assert(remote_name == agent2);

    std::cout << "perf setup done\n";

    gettimeofday(&start_time, NULL);

    for(int i = 0; i<n_iters; i++) {
        status = A1->prepXferSide(dst_list, agent2, backend, dst_side[i]);
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

    //test makeXfer optimization

    std::vector<int> indices;
    nixlXferReqH* reqh1, *reqh2;

    for(int i = 0; i<(n_mems*descs_per_mem); i++)
        indices.push_back(i);

    //should print n_mems number of final descriptors
    status = A1->makeXferReq(src_side[0], indices, dst_side[0], indices, "test", NIXL_WRITE, reqh1);
    assert(status == NIXL_SUCCESS);

    indices.clear();
    for(int i = 0; i<(n_mems*descs_per_mem); i+=2)
        indices.push_back(i);

    //should print (n_mems*descs_per_mem/2) number of final descriptors
    status = A1->makeXferReq(src_side[0], indices, dst_side[0], indices, "test", NIXL_WRITE, reqh2);
    assert(status == NIXL_SUCCESS);

    A1->invalidateXferReq(reqh1);
    A1->invalidateXferReq(reqh2);

    status = A1->deregisterMem(mem_list1, backend);
    assert(status == NIXL_SUCCESS);
    status = A2->deregisterMem(mem_list2, backend2);
    assert(status == NIXL_SUCCESS);

    for(int i = 0; i<n_iters; i++){
        A1->invalidateXferSide(src_side[i]);
        A1->invalidateXferSide(dst_side[i]);
    }

    free(src_buf);
    free(dst_buf);
}

nixl_status_t sideXferTest(nixlAgent* A1, nixlAgent* A2, nixlXferReqH* src_handle, nixlBackendH* dst_backend){
    std::cout << "Starting sideXferTest\n";

    nixlBackendH* src_backend = A1->getXferBackend(src_handle);

    assert(src_backend);

    std::cout << "Got backend\n";

    test_side_perf(A1, A2, src_backend, dst_backend);

    int n_bufs = 4; //must be even
    size_t len = 1024;
    nixl_status_t status;
    void* src_bufs[n_bufs], *dst_bufs[n_bufs];

    nixl_reg_dlist_t mem_list1(DRAM_SEG), mem_list2(DRAM_SEG);
    nixl_xfer_dlist_t src_list(DRAM_SEG), dst_list(DRAM_SEG);
    nixlStringDesc src_desc[4], dst_desc[4];
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

        mem_list1.addDesc(src_desc[i]);
        mem_list2.addDesc(dst_desc[i]);
    }

    src_list = mem_list1.trim();
    dst_list = mem_list2.trim();

    status = A1->registerMem(mem_list1, src_backend);
    assert(status == NIXL_SUCCESS);

    status = A2->registerMem(mem_list2, dst_backend);
    assert(status == NIXL_SUCCESS);

    std::string meta2 = A2->getLocalMD();
    assert(meta2.size() > 0);

    std::string remote_name = A1->loadRemoteMD(meta2);
    assert(remote_name == agent2);

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

    nixl_status_t xfer_status = A1->postXferReq(req1);

    while(xfer_status != NIXL_SUCCESS) {
        if(xfer_status != NIXL_SUCCESS) xfer_status = A1->getXferStatus(req1);
        assert(xfer_status >= 0);
    }

    for(int i = 0; i<(n_bufs/2); i++)
        check_buf(dst_bufs[i], len);

    std::cout << "transfer 1 done\n";

    //read first half of dst_bufs back to second half of src_bufs
    status = A1->makeXferReq(src_side, indices2, dst_side, indices1, "", NIXL_READ, req2);
    assert(status == NIXL_SUCCESS);

    xfer_status = A1->postXferReq(req2);

    while(xfer_status != NIXL_SUCCESS) {
        if(xfer_status != NIXL_SUCCESS) xfer_status = A1->getXferStatus(req2);
        assert(xfer_status >= 0);
    }

    for(int i = (n_bufs/2); i<n_bufs; i++)
        check_buf(src_bufs[i], len);

    std::cout << "transfer 2 done\n";

    //write second half of src_bufs to dst_bufs
    status = A1->makeXferReq(src_side, indices2, dst_side, indices2, "", NIXL_WRITE, req3);
    assert(status == NIXL_SUCCESS);

    xfer_status = A1->postXferReq(req3);

    while(xfer_status != NIXL_SUCCESS) {
        if(xfer_status != NIXL_SUCCESS) xfer_status = A1->getXferStatus(req3);
        assert(xfer_status >= 0);
    }

    for(int i = (n_bufs/2); i<n_bufs; i++)
        check_buf(dst_bufs[i], len);

    std::cout << "transfer 3 done\n";

    A1->invalidateXferReq(req1);
    A1->invalidateXferReq(req2);
    A1->invalidateXferReq(req3);

    status = A1->deregisterMem(mem_list1, src_backend);
    assert(status == NIXL_SUCCESS);
    status = A2->deregisterMem(mem_list2, dst_backend);
    assert(status == NIXL_SUCCESS);

    A1->invalidateXferSide(src_side);
    A1->invalidateXferSide(dst_side);

    for(int i = 0; i<n_bufs; i++) {
        free(src_bufs[i]);
        free(dst_bufs[i]);
    }

    return NIXL_SUCCESS;
}

void printParams(const nixl_b_params_t& params) {
    if (params.empty()) {
        std::cout << "Parameters: (empty)" << std::endl;
        return;
    }

    std::cout << "Parameters:" << std::endl;
    for (const auto& pair : params) {
        std::cout << "  " << pair.first << " = " << pair.second << std::endl;
    }
}

int main()
{
    nixl_status_t ret1, ret2;
    std::string ret_s1, ret_s2;

    // Example: assuming two agents running on the same machine,
    // with separate memory regions in DRAM

    nixlAgentConfig cfg(true);
    nixl_b_params_t init1, init2;

    // populate required/desired inits
    nixlAgent A1(agent1, cfg);
    nixlAgent A2(agent2, cfg);

    init1 = A1.getBackendOptions("UCX");
    init2 = A2.getBackendOptions("UCX");

    printParams(init1);
    printParams(init2);

    nixlBackendH* ucx1 = A1.createBackend("UCX", init1);
    nixlBackendH* ucx2 = A2.createBackend("UCX", init2);

    // // One side gets to listen, one side to initiate. Same string is passed as the last 2 steps
    // ret1 = A1->makeConnection(agent2, 0);
    // ret2 = A2->makeConnection(agent1, 1);

    // assert(ret1 == NIXL_SUCCESS);
    // assert(ret2 == NIXL_SUCCESS);

    // User allocates memories, and passes the corresponding address
    // and length to register with the backend
    nixlStringDesc buff1, buff2, buff3;
    nixl_reg_dlist_t dlist1(DRAM_SEG), dlist2(DRAM_SEG);
    size_t len = 256;
    void* addr1 = calloc(1, len);
    void* addr2 = calloc(1, len);
    void* addr3 = calloc(1, len);

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

    buff3.addr   = (uintptr_t) addr3;
    buff3.len    = len;
    buff3.devId = 0;
    dlist1.addDesc(buff3);

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

    nixl_xfer_dlist_t req_src_descs (DRAM_SEG);
    nixlBasicDesc req_src;
    req_src.addr     = (uintptr_t) (((char*) addr1) + 16); //random offset
    req_src.len      = req_size;
    req_src.devId   = 0;
    req_src_descs.addDesc(req_src);

    nixl_xfer_dlist_t req_dst_descs (DRAM_SEG);
    nixlBasicDesc req_dst;
    req_dst.addr   = (uintptr_t) ((char*) addr2) + dst_offset; //random offset
    req_dst.len    = req_size;
    req_dst.devId = 0;
    req_dst_descs.addDesc(req_dst);

    nixl_xfer_dlist_t req_ldst_descs (DRAM_SEG);
    nixlBasicDesc req_ldst;
    req_ldst.addr   = (uintptr_t) ((char*) addr3) + dst_offset; //random offset
    req_ldst.len    = req_size;
    req_ldst.devId = 0;
    req_ldst_descs.addDesc(req_ldst);

    std::cout << "Transfer request from " << addr1 << " to " << addr2 << "\n";
    nixlXferReqH *req_handle, *req_handle2;

    ret1 = A1.createXferReq(req_src_descs, req_dst_descs, agent2, "notification", NIXL_WR_NOTIF, req_handle);
    assert(ret1 == NIXL_SUCCESS);

    nixl_status_t status = A1.postXferReq(req_handle);

    std::cout << "Transfer was posted\n";

    nixl_notifs_t notif_map;
    int n_notifs = 0;

    while(status != NIXL_SUCCESS || n_notifs == 0) {
        if(status != NIXL_SUCCESS) status = A1.getXferStatus(req_handle);
        if(n_notifs == 0) n_notifs = A2.getNotifs(notif_map);
        assert(status >= 0);
        assert(n_notifs >= 0);
    }

    std::vector<std::string> agent1_notifs = notif_map[agent1];
    assert(agent1_notifs.size() == 1);
    assert(agent1_notifs.front() == "notification");
    notif_map[agent1].clear();
    n_notifs = 0;

    std::cout << "Transfer verified\n";

    std::cout << "performing sideXferTest with backends " << ucx1 << " " << ucx2 << "\n";
    ret1 = sideXferTest(&A1, &A2, req_handle, ucx2);
    assert(ret1 == NIXL_SUCCESS);

    std::cout << "Performing local test\n";
    ret2 = A1.createXferReq(req_src_descs, req_ldst_descs, agent1, "local_notif", NIXL_WR_NOTIF, req_handle2);
    assert(ret2 == NIXL_SUCCESS);

    status = A1.postXferReq(req_handle2);
    std::cout << "Local transfer was posted\n";

    while(status != NIXL_SUCCESS || n_notifs == 0) {
        if(status != NIXL_SUCCESS) status = A1.getXferStatus(req_handle2);
        if(n_notifs == 0) n_notifs = A1.getNotifs(notif_map);
        assert(status >= 0);
        assert(n_notifs >= 0);
    }

    agent1_notifs = notif_map[agent1];
    assert(agent1_notifs.size() == 1);
    assert(agent1_notifs.front() == "local_notif");
    assert(equal_buf((void*) req_src.addr, (void*) req_ldst.addr, req_size) == true);

    A1.invalidateXferReq(req_handle);
    A1.invalidateXferReq(req_handle2);
    ret1 = A1.deregisterMem(dlist1, ucx1);
    ret2 = A2.deregisterMem(dlist2, ucx2);

    //only initiator should call invalidate
    A1.invalidateRemoteMD(agent2);
    //A2.invalidateRemoteMD(agent1);

    free(addr1);
    free(addr2);
    free(addr3);

    std::cout << "Test done\n";
}
