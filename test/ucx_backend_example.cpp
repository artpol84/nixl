#include <iostream>
#include <cassert>

#include "ucx_backend.h"

using namespace std;

int main()
{
    int ret1, ret2;
    xfer_state_t ret3;

    // Example: assuming two agents running on the same machine,
    // with separate memory regions in DRAM
    std::string agent1("Agent1");
    std::string agent2("Agent2");

    nixlUcxInitParams init1, init2;
    // populate required/desired inits

    // User would ask each of the agents to create a ucx  backend, and the
    // agent returns to them these pointers in the form of transfer_backend *
    nixlBackendEngine *ucx1, *ucx2;

    init1.localAgent = agent1;
    init2.localAgent = agent2;

    ucx1 = (nixlBackendEngine*) new nixlUcxEngine (&init1);
    ucx2 = (nixlBackendEngine*) new nixlUcxEngine (&init2);

    // We get the required connection info from UCX to be put on the central
    // location and ask for it for a remote node
    std::string conn_info1 = ucx1->getConnInfo();
    std::string conn_info2 = ucx2->getConnInfo();

    // We assumed we put them to central location and now receiving it on the other process
    ret1 = ucx1->loadRemoteConnInfo (agent2, conn_info2);
    ret2 = ucx2->loadRemoteConnInfo (agent1, conn_info1);

    assert(ret1 == 0);
    assert(ret2 == 0);

    //This won't work single threaded
    //ret1 = ucx1-> listenForConnection(agent2);
    //ret2 = ucx2-> makeConnection(agent1);

    assert(ret1 == 0);
    assert(ret2 == 0);

    std::cout << "Synchronous handshake complete\n";

    // User allocates memories, and passes the corresponding address
    // and length to register with the backend
    nixlBasicDesc buff1, buff2;
    nixlBackendMD* local_meta1, *local_meta2;
    // Number of transfer descriptors
    int desc_cnt = 16; 
    // Size of a single descriptor 
    // UCX SHMem is using 16MB bounce buffers, 
    // use large size to ensure that request is non-inline
    size_t desc_size = 32 * 1024 * 1024; 
    size_t len = desc_cnt * desc_size;
    void* addr1 = calloc(1, len);
    void* addr2 = calloc(1, len);

    memset(addr1, 0xbb, len);
    memset(addr2, 0, len);

    buff1.addr   = (uintptr_t) addr1;
    buff1.len    = len;
    buff1.devId = 0;
    // sets the metadata field to a pointer to an object inside the ucx_class
    ret1 = ucx1->registerMem(buff1, DRAM_SEG, local_meta1);

    buff2.addr   = (uintptr_t) addr2;
    buff2.len    = len;
    buff2.devId = 0;
    ret2 = ucx2->registerMem(buff2, DRAM_SEG, local_meta2);

    assert(ret1 == 0);
    assert(ret2 == 0);

    // Agent keeps a DescList<MetaDesc> for the local descriptors, not seen by the backend

    // Agent asks the backend to output the string per registered region requried for a remote node
    // And makes a DescList<StringDesc> from that.
    nixlStringDesc ucx_dram_info1;
    ucx_dram_info1.addr     = (uintptr_t) addr1;
    ucx_dram_info1.len      = len;
    ucx_dram_info1.devId    = 0;
    ucx_dram_info1.metadata = ucx1->getPublicData(local_meta1);

    nixlStringDesc ucx_dram_info2;
    ucx_dram_info2.addr     = (uintptr_t) addr2;
    ucx_dram_info2.len      = len;
    ucx_dram_info2.devId    = 0;
    ucx_dram_info2.metadata = ucx2->getPublicData(local_meta2);

    assert(ucx_dram_info1.metadata.size() > 0);
    assert(ucx_dram_info2.metadata.size() > 0);

    // We get the data from the cetnral location and populate the backend, and receive remote_meta
    nixlBackendMD* remote_meta1of2, *remote_meta2of1;
    ret1 = ucx1->loadRemoteMD (ucx_dram_info2, DRAM_SEG, agent2, remote_meta1of2);
    ret2 = ucx2->loadRemoteMD (ucx_dram_info1, DRAM_SEG, agent1, remote_meta2of1);

    assert(ret1 == 0);
    assert(ret2 == 0);

    // User creates a request, size of them should match. Example on Agent 1
    // The agent fills the metadata fields based on local_meta1 and remote_meta1
    nixlBackendReqH* handle;

    nixlDescList<nixlMetaDesc> req_src_descs (DRAM_SEG);
    for(int i = 0; i < desc_cnt; i++) {
        nixlMetaDesc req_src;
        req_src.addr     = (uintptr_t) (((char*) addr1) + i * desc_size); //random offset
        req_src.len      = desc_size;
        req_src.devId   = 0;
        req_src.metadata = local_meta1;
        req_src_descs.addDesc(req_src);
    }

    nixlDescList<nixlMetaDesc> req_dst_descs (DRAM_SEG);
    for(int i = 0; i < desc_cnt; i++) {
        nixlMetaDesc req_dst;
        req_dst.addr   = (uintptr_t) ((char*) addr2 + i * desc_size); //random offset
        req_dst.len    = desc_size;
        req_dst.devId = 0;
        req_dst.metadata = remote_meta1of2;
        req_dst_descs.addDesc(req_dst);
    }

    std::string test_str("test");
    std::cout << "Transferring from " << addr1 << " to " << addr2 << "\n";

    // Posting a request, to be updated to return an async handler,
    // or an ID that later can be used to check the status as a new method
    // Also maybe we would remove the WRITE and let the backend class decide the op
    ret3 = ucx1->postXfer(req_src_descs, req_dst_descs, NIXL_WRITE, "Agent2", test_str, handle);
    assert( ret3 == NIXL_XFER_DONE || ret3 == NIXL_XFER_PROC);

    ucx1->progress();
    ucx2->progress();

    if (ret3 == NIXL_XFER_DONE) {
        cout << "WARNING: Tansfer request completed immmediately - no testing non-inline path" << endl;
    } else {
        cout << "NOTE: Testing non-inline Transfer path!" << endl;

        while(ret3 == NIXL_XFER_PROC) {
            ret3 = ucx1->checkXfer(handle);
            ucx2->progress();
            assert( ret3 == NIXL_XFER_DONE || ret3 == NIXL_XFER_PROC);
        }
        ucx1->releaseReqH(handle);
    }

#if 0
    // status = ucx1->sendNotif(agent2, test_str);
    // assert(status != -1);
#endif

    // Do some checks on the data.
    for(size_t i = 0; i < len; i++){
        assert( ((uint8_t*) addr2)[i] == 0xbb);    
    }

    std::cout << "Transfer verified\n";

#if 0
    // TODO: Fix notifications
    notif_list_t target_notifs;

    ret2 = 0;

    while(ret2 == 0){
        ucx2->progress();
        ret2 = ucx2->getNotifs(target_notifs);
    }

    assert(ret2 == 1);

    assert(target_notifs.front().first == agent1);
    assert(target_notifs.front().second == test_str);
#endif

    // At the end we deregister the memories, by agent knowing all the registered regions
    ucx1->deregisterMem(local_meta1);
    ucx2->deregisterMem(local_meta2);

    // As well as all the remote notes, asking to remove them one by one
    // need to provide list of descs
    ucx1->removeRemoteMD (remote_meta1of2);
    ucx2->removeRemoteMD (remote_meta2of1);

    // Agent will remove the local and remote sections that kept these descriptors,
    // And destructors are called outmatically at the end
}
