#include <iostream>
#include <cassert>

#include "ucx_backend.h"

int main()
{
    int ret1, ret2;

    // Example: assuming two agents running on the same machine,
    // with separate memory regions in DRAM
    std::string agent1("Agent1");
    std::string agent2("Agent2");

    nixlUcxInitParams init1, init2;
    // populate required/desired inits

    // User would ask each of the agents to create a ucx  backend, and the
    // agent returns to them these pointers in the form of transfer_backend *
    BackendEngine *ucx1, *ucx2;
    ucx1 = (BackendEngine*) new nixlUcxEngine (&init1);
    ucx2 = (BackendEngine*) new nixlUcxEngine (&init2);

    // We get the required connection info from UCX to be put on the central
    // location and ask for it for a remote node
    std::string conn_info1 = ucx1->get_conn_info();
    std::string conn_info2 = ucx2->get_conn_info();

    // We assumed we put them to central location and now receiving it on the other process
    ret1 = ucx1->load_remote_conn_info (agent2, conn_info2);
    ret2 = ucx2->load_remote_conn_info (agent1, conn_info1);

    assert(ret1 == 0);
    assert(ret2 == 0);

    // One side gets to listen, one side to initiate. Same string is passed as the last 2 steps
    ret1 = ucx1-> listen_for_connection(agent2);
    ret2 = ucx2-> make_connection(agent1);

    assert(ret1 == 0);
    assert(ret2 == 0);

    // User allocates memories, and passes the corresponding address
    // and length to register with the backend
    BasicDesc buff1, buff2;
    BackendMetadata* local_meta1, *local_meta2;
    size_t len = 256;
    void* addr1 = calloc(1, len);
    void* addr2 = calloc(1, len);

    memset(addr1, 0xbb, len);
    memset(addr2, 0, len);

    buff1.addr   = (uintptr_t) addr1;
    buff1.len    = len;
    buff1.dev_id = 0;
    // sets the metadata field to a pointer to an object inside the ucx_class
    ret1 = ucx1->register_mem(buff1, DRAM_SEG, local_meta1);

    buff2.addr   = (uintptr_t) addr2;
    buff2.len    = len;
    buff2.dev_id = 0;
    ret2 = ucx2->register_mem(buff2, DRAM_SEG, local_meta2);

    assert(ret1 == 0);
    assert(ret2 == 0);

    // Agent keeps a DescList<MetaDesc> for the local descriptors, not seen by the backend

    // Agent asks the backend to output the string per registered region requried for a remote node
    // And makes a DescList<StringDesc> from that.
    StringDesc ucx_dram_info1;
    ucx_dram_info1.addr   = (uintptr_t) addr1;
    ucx_dram_info1.len    = len;
    ucx_dram_info1.dev_id = 0;
    ucx_dram_info1.metadata   = ucx1->get_public_data(local_meta1);

    StringDesc ucx_dram_info2;
    ucx_dram_info2.addr   = (uintptr_t) addr2;
    ucx_dram_info2.len    = len;
    ucx_dram_info2.dev_id = 0;
    ucx_dram_info2.metadata   = ucx2->get_public_data(local_meta2);

    assert(ucx_dram_info1.metadata.size() > 0);
    assert(ucx_dram_info2.metadata.size() > 0);

    // We get the data from the cetnral location and populate the backend, and receive remote_meta
    BackendMetadata* remote_meta1of2, *remote_meta2of1;
    ret1 = ucx1->load_remote (ucx_dram_info2, remote_meta1of2, agent2);
    ret2 = ucx2->load_remote (ucx_dram_info1, remote_meta2of1, agent1);

    assert(ret1 == 0);
    assert(ret2 == 0);

    // User creates a request, size of them should match. Example on Agent 1
    // The agent fills the metadata fields based on local_meta1 and remote_meta1
    size_t req_size = 8;
    size_t dst_offset = 8;
    BackendTransferHandle* handle;

    DescList<MetaDesc> req_src_descs (DRAM_SEG);
    MetaDesc req_src;
    req_src.addr     = (uintptr_t) (((char*) addr1) + 16); //random offset
    req_src.len      = req_size;
    req_src.dev_id   = 0;
    req_src.metadata = local_meta1;
    req_src_descs.add_desc(req_src);

    DescList<MetaDesc> req_dst_descs (DRAM_SEG);
    MetaDesc req_dst;
    req_dst.addr   = (uintptr_t) ((char*) addr2) + dst_offset; //random offset
    req_dst.len    = req_size;
    req_dst.dev_id = 0;
    req_dst.metadata = remote_meta1of2;
    req_dst_descs.add_desc(req_dst);

    std::cout << "Transferring from " << addr1 << " to " << addr2 << "\n";
    // Posting a request, to be updated to return an async handler,
    // or an ID that later can be used to check the status as a new method
    // Also maybe we would remove the WRITE and let the backend class decide the op
    ret1 = ucx1->transfer(req_src_descs, req_dst_descs, WRITE, "", handle);
    assert(ret1 == 0);

    int status = 0;

    while(status == 0) {
        status = ucx1->check_transfer(handle);
        ucx2->progress();
        assert(status != -1);
    }

    // Do some checks on the data.
    for(size_t i = dst_offset; i<req_size; i++){
        assert( ((uint8_t*) addr2)[i] == 0xbb);    
    }

    std::cout << "Transfer verified\n";

    // At the end we deregister the memories, by agent knowing all the registered regions
    ucx1->deregister_mem(local_meta1);
    ucx2->deregister_mem(local_meta2);

    // As well as all the remote notes, asking to remove them one by one
    // need to provide list of descs
    ucx1->remove_remote (remote_meta1of2);
    ucx2->remove_remote (remote_meta2of1);

    // Agent will remove the local and remote sections that kept these descriptors,
    // And destructors are called outmatically at the end
}
