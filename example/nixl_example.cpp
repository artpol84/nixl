#include "nixl.h"

int main(){
    device_metadata devices;
    // Fill devices
    ucx_init_params ucx_params;
    // Fill ucx_params

    TransferAgent agent("007", &devices);

    backend_engine *ucx;

    ucx = agent.create_backend(&ucx_params);

    if (ucx==nullptr)
        return -1;

    basic_desc temp;
    desc_list<basic_desc> dram_for_ucx(DRAM);
    for (int i=0; i<10; ++i){
        temp.addr = 100*i;
        temp.len = 10;
        temp.dev = 0; // Single unified DRAM
        dram_for_ucx.add_desc(temp);
    }
    desc_list<basic_desc> vram_for_ucx(VRAM);
    for (int i=0; i<10; ++i){
        temp.addr = 10000+100*i;
        temp.len = 10;
        temp.dev = i;
        vram_for_ucx.add_desc(temp);
    }

    agent.register_mem(dram_for_ucx, ucx);
    agent.register_mem(vram_for_ucx, ucx);

    desc_list<basic_desc> req_src(DRAM);
    for (int i=2; i<8; ++i){
        temp.addr = 100*i;
        temp.len = 4;
        temp.dev = 0;
        req_src.add_desc(temp);
    }

    desc_list<basic_desc> req_dst(VRAM);
    for (int i=2; i<8; ++i){
        temp.addr = 50000+100*i; // Remote node address
        temp.len = 4; // Aggregate lengths of req_src/dst should match
        temp.dev = i;
        req_src.add_desc(temp);
    }

    TransferRequest req*;
    req = agent.create_transfer_req (req_src, req_dst, "Queen");

    if (req.state==ERR)
        return -1;

    // metadata server parts might need to be added here

    // deregisters
}
