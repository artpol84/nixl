#include <iostream>
#include <sstream>
#include <string>
#include <cassert>

#include "ucx_mo_backend.h"

using namespace std;

#ifdef USE_VRAM

#include <cuda_runtime.h>
#include <cufile.h>

int gpu_id = 0;

static void checkCudaError(cudaError_t result, const char *message) {
    if (result != cudaSuccess) {
	std::cerr << message << " (Error code: " << result << " - "
                   << cudaGetErrorString(result) << ")" << std::endl;
        exit(EXIT_FAILURE);
    }
}
#endif


static string op2string(nixl_xfer_op_t op)
{
    switch(op) {
        case NIXL_READ:
            return string("READ");
        case NIXL_WRITE:
            return string("WRITE");
            // TODO: update if necessary
        case NIXL_RD_NOTIF:
            return string("READ/NOTIF");
        case NIXL_WR_NOTIF:
            return string("WRITE/NOTIF");
    }
    return string("ERR-OP");
}

std::string memType2Str(nixl_mem_t mem_type)
{
    switch(mem_type) {
    case DRAM_SEG:
        return std::string("DRAM");
    case VRAM_SEG:
        return std::string("VRAM");
    case BLK_SEG:
        return std::string("BLOCK");
    case FILE_SEG:
        return std::string("FILE");
    default:
        std::cout << "Unsupported memory type!" << std::endl;
        assert(0);
    }
}

nixlBackendEngine *createEngine(std::string name, uint32_t ndev, bool p_thread)
{
    nixlBackendEngine     *ucx_mo;
    nixlBackendInitParams init;
    nixl_b_params_t       custom_params;

    custom_params["num_ucx_engines"] = std::to_string(ndev);
    init.enableProgTh = p_thread;
    init.pthrDelay    = 100;
    init.localAgent   = name;
    init.customParams = &custom_params;
    init.type         = "UCX_MO";

    ucx_mo = (nixlBackendEngine*) new nixlUcxMoEngine (&init);
    assert(!ucx_mo->getInitErr());
    if (ucx_mo->getInitErr()) {
        std::cout << "Failed to initialize worker1" << std::endl;
        exit(1);
    }

    return ucx_mo;
}

void releaseEngine(nixlBackendEngine *ucx)
{
    //protected now, should not call
    delete ucx;
}

#ifdef USE_VRAM

static int cudaQueryAddr(void *address, bool &is_dev,
                         CUdevice &dev, CUcontext &ctx)
{
    CUmemorytype mem_type = CU_MEMORYTYPE_HOST;
    uint32_t is_managed = 0;
#define NUM_ATTRS 4
    CUpointer_attribute attr_type[NUM_ATTRS];
    void *attr_data[NUM_ATTRS];
    CUresult result;

    attr_type[0] = CU_POINTER_ATTRIBUTE_MEMORY_TYPE;
    attr_data[0] = &mem_type;
    attr_type[1] = CU_POINTER_ATTRIBUTE_IS_MANAGED;
    attr_data[1] = &is_managed;
    attr_type[2] = CU_POINTER_ATTRIBUTE_DEVICE_ORDINAL;
    attr_data[2] = &dev;
    attr_type[3] = CU_POINTER_ATTRIBUTE_CONTEXT;
    attr_data[3] = &ctx;

    result = cuPointerGetAttributes(4, attr_type, attr_data, (CUdeviceptr)address);

    is_dev = (mem_type == CU_MEMORYTYPE_DEVICE);

    return (CUDA_SUCCESS != result);
}

#endif


void allocateBuffer(nixl_mem_t mem_type, int dev_id, size_t len, void* &addr)
{
    switch(mem_type) {
    case DRAM_SEG:
        //addr = calloc(1, len);
        posix_memalign(&addr, 4096, len);
        break;
#ifdef USE_VRAM
    case VRAM_SEG:{
        bool is_dev;
        CUdevice dev;
        CUcontext ctx;

        checkCudaError(cudaSetDevice(dev_id), "Failed to set device");
        checkCudaError(cudaMalloc(&addr, len), "Failed to allocate CUDA buffer 0");
        cudaQueryAddr(addr, is_dev, dev, ctx);
        std::cout << "CUDA addr: " << std::hex << addr << " dev=" << std::dec << dev 
            << " ctx=" << std::hex << ctx << std::dec << std::endl;
        break;
    }
#endif
    default:
        std::cout << "Unsupported memory type!" << std::endl;
        assert(0);
    }
    assert(addr);
}

void releaseBuffer(nixl_mem_t mem_type, int dev_id, void* &addr)
{
    switch(mem_type) {
    case DRAM_SEG:
        free(addr);
        break;
#ifdef USE_VRAM
    case VRAM_SEG:
        checkCudaError(cudaSetDevice(dev_id), "Failed to set device");
        checkCudaError(cudaFree(addr), "Failed to allocate CUDA buffer 0");
        break;
#endif
    default:
        std::cout << "Unsupported memory type!" << std::endl;
        assert(0);
    }
}

void doMemset(nixl_mem_t mem_type, int dev_id, void *addr, char byte, size_t len)
{
    switch(mem_type) {
    case DRAM_SEG:
        memset(addr, byte, len);
        break;
#ifdef USE_VRAM
    case VRAM_SEG:
        checkCudaError(cudaSetDevice(dev_id), "Failed to set device");
        checkCudaError(cudaMemset(addr, byte, len), "Failed to memset");
        break;
#endif
    default:
        std::cout << "Unsupported memory type!" << std::endl;
        assert(0);
    }
}

void *getValidationPtr(nixl_mem_t mem_type, void *addr, size_t len)
{
    switch(mem_type) {
    case DRAM_SEG:
        return addr;
        break;
#ifdef USE_VRAM
    case VRAM_SEG: {
        void *ptr = calloc(len, 1);
        checkCudaError(cudaMemcpy(ptr, addr, len, cudaMemcpyDeviceToHost), "Failed to memcpy");
        return ptr;
    }
#endif
    default:
        std::cout << "Unsupported memory type!" << std::endl;
        assert(0);
    }
}

void *releaseValidationPtr(nixl_mem_t mem_type, void *addr)
{
    switch(mem_type) {
    case DRAM_SEG:
        break;
#ifdef USE_VRAM
    case VRAM_SEG:
        free(addr);
        break;
#endif
    default:
        std::cout << "Unsupported memory type!" << std::endl;
        assert(0);
    }
    return NULL;
}

void createLocalDescs(nixlBackendEngine *ucx, nixl_meta_dlist_t &descs,
                      int dev_cnt,
                      int desc_cnt, size_t desc_size)
{

    for(int i = 0; i < desc_cnt; i++) {
        nixlBasicDesc desc;
        nixlMetaDesc desc_m;
        nixlBlobDesc desc_s;
        void *addr;

        desc.len = desc_size;
        desc.devId = i % dev_cnt;

        allocateBuffer(descs.getType(), desc.devId, desc.len, addr);
        desc.addr = (uintptr_t)addr;
        *((nixlBasicDesc*)&desc_s) = desc;
        *((nixlBasicDesc*)&desc_m) = desc;
        int ret = ucx->registerMem(desc_s, descs.getType(), desc_m.metadataP);
        assert(ret == NIXL_SUCCESS);
        descs.addDesc(desc_m);
    }
}


void destroyLocalDescs(nixlBackendEngine *ucx, nixl_meta_dlist_t &descs)
{
    for(int i = 0; i < descs.descCount(); i++) {
        auto dev_id = descs[i].devId;
        void *addr = (void*)descs[i].addr;
        ucx->deregisterMem(descs[i].metadataP);
        releaseBuffer(descs.getType(), dev_id, addr);
    }

    while(descs.descCount()) {
        descs.remDesc(0);
    }
}

void createRemoteDescs(nixlBackendEngine *src_ucx,
                       std::string agent,
                       nixl_meta_dlist_t &src_descs,
                       nixlBackendEngine *dst_ucx,
                       nixl_meta_dlist_t &dst_descs)
{
    for(int i = 0; i < src_descs.descCount(); i++) {
        nixlBlobDesc desc_s;
        nixlMetaDesc desc_m;
        nixl_status_t status;

        *((nixlBasicDesc*)&desc_s) = (nixlBasicDesc)src_descs[i];
        *((nixlBasicDesc*)&desc_m) = (nixlBasicDesc)src_descs[i];
        status = src_ucx->getPublicData(src_descs[i].metadataP, desc_s.metaInfo);
        assert(NIXL_SUCCESS == status);
        status = dst_ucx->loadRemoteMD (desc_s, src_descs.getType(), 
                                        agent, desc_m.metadataP);
        assert(status == NIXL_SUCCESS);        
        dst_descs.addDesc(desc_m);
    }
}

void destroyRemoteDescs(nixlBackendEngine *dst_ucx,
                        nixl_meta_dlist_t &dst_descs)
{
    nixl_status_t status;
    for(int i = 0; i < dst_descs.descCount(); i++) {
        status = dst_ucx->unloadMD (dst_descs[i].metadataP);
        assert(status == NIXL_SUCCESS);
    }

    while(dst_descs.descCount()) {
        dst_descs.remDesc(0);
    }
}

void performTransfer(nixlBackendEngine *ucx1, nixlBackendEngine *ucx2,
                     nixl_meta_dlist_t &req_src_descs,
                     nixl_meta_dlist_t &req_dst_descs,
                     nixl_xfer_op_t op, bool progress)
{
    nixl_status_t status;
    nixlBackendReqH* handle;
    void *chkptr1, *chkptr2;

    std::string remote_agent ("Agent2");

    if(ucx1 == ucx2) remote_agent = "Agent1";

    std::string test_str("test");
    std::cout << "\t" << op2string(op) << "\n";
    nixl_opt_b_args_t opt_args;
    opt_args.notifMsg = test_str;

    // Posting a request, to be updated to return an async handler,
    // or an ID that later can be used to check the status as a new method
    // Also maybe we would remove the WRITE and let the backend class decide the op
    status = ucx1->postXfer(op, req_src_descs, req_dst_descs, remote_agent, handle, &opt_args);
    assert(status == NIXL_SUCCESS || status == NIXL_IN_PROG);


    if (status == NIXL_SUCCESS) {
        cout << "\t\tWARNING: Tansfer request completed immmediately - no testing non-inline path" << endl;
    } else {
        cout << "\t\tNOTE: Testing non-inline Transfer path!" << endl;

        while(status == NIXL_IN_PROG) {
            status = ucx1->checkXfer(handle);
            if(progress){
                ucx2->progress();
            }
            assert( (NIXL_SUCCESS == status) || (NIXL_IN_PROG == status) );
        }
        ucx1->releaseReqH(handle);
    }

    switch (op) {
        case NIXL_RD_NOTIF:
        case NIXL_WR_NOTIF: {
            /* Test notification path */
            notif_list_t target_notifs;

            cout << "\t\tChecking notification flow: " << flush;

            while(!target_notifs.size()){
                status = ucx2->getNotifs(target_notifs);
                assert(NIXL_SUCCESS == status);
                if(progress){
                    ucx1->progress();
                }
            }

            assert(target_notifs.size() == 1);
            assert(target_notifs.front().first == "Agent1");
            assert(target_notifs.front().second == test_str);

            cout << "OK" << endl;
            break;
        }
        default:
            break;
    }

    cout << "\t\tData verification: " << flush;

    assert(req_src_descs.descCount() == req_dst_descs.descCount());
    for(int i = 0; i < req_src_descs.descCount(); i++) {
        auto sdesc = req_src_descs[i];
        auto ddesc = req_dst_descs[i];
        assert(sdesc.len == ddesc.len);
        size_t len = ddesc.len;
        chkptr1 = getValidationPtr(req_src_descs.getType(), (void*)sdesc.addr, len);
        chkptr2 = getValidationPtr(req_dst_descs.getType(), (void*)ddesc.addr, len);

        // Perform correctness check.
        for(size_t i = 0; i < len; i++){
            assert( ((uint8_t*) chkptr1)[i] == ((uint8_t*) chkptr2)[i]);    
        }

        releaseValidationPtr(req_src_descs.getType(), chkptr1);
        releaseValidationPtr(req_dst_descs.getType(), chkptr2);
    }
    cout << "OK" << endl;
}

void test_inter_agent_transfer(bool p_thread, 
                nixlBackendEngine *ucx1, nixl_mem_t src_mem_type, int src_dev_cnt, 
                nixlBackendEngine *ucx2, nixl_mem_t dst_mem_type, int dst_dev_cnt)
{
    int iter = 10;
    nixl_status_t status;

    std::cout << std::endl << std::endl;
    std::cout << "****************************************************" << std::endl;
    std::cout << "    Inter-agent memory transfer test P-Thr=" <<          
                        (p_thread ? "ON" : "OFF") << std::endl;
    std::cout << "         (" << memType2Str(src_mem_type) << " -> " 
                << memType2Str(dst_mem_type) << ")" << std::endl;
    std::cout << "****************************************************" << std::endl;
    std::cout << std::endl << std::endl;

    // Example: assuming two agents running on the same machine,
    // with separate memory regions in DRAM
    std::string agent1("Agent1");
    std::string agent2("Agent2");

    // We get the required connection info from UCX to be put on the central
    // location and ask for it for a remote node
    std::string conn_info1;
    status = ucx1->getConnInfo(conn_info1);
    assert(NIXL_SUCCESS == status);

    std::string conn_info2;
    status = ucx2->getConnInfo(conn_info2);
    assert(NIXL_SUCCESS == status);

    // We assumed we put them to central location and now receiving it on the other process
    status = ucx1->loadRemoteConnInfo (agent2, conn_info2);
    assert(NIXL_SUCCESS == status);
    
    // TODO: Causes race condition - investigate conn management implementation
    // ret = ucx2->loadRemoteConnInfo (agent1, conn_info1);

    std::cout << "Synchronous handshake complete\n";

    // Number of transfer descriptors
    int desc_cnt = 16; 
    // Size of a single descriptor 
    size_t desc_size = 32 * 1024 * 1024; 
    nixl_meta_dlist_t ucx1_src_descs (src_mem_type);
    nixl_meta_dlist_t ucx2_src_descs (dst_mem_type);
    nixl_meta_dlist_t ucx1_dst_descs (dst_mem_type);

    createLocalDescs(ucx1, ucx1_src_descs, src_dev_cnt, 
                     desc_cnt, desc_size);
    createLocalDescs(ucx2, ucx2_src_descs, dst_dev_cnt, 
                     desc_cnt, desc_size);
    createRemoteDescs(ucx2, agent2, ucx2_src_descs, 
                      ucx1, ucx1_dst_descs);


    nixl_xfer_op_t ops[] = { NIXL_READ, NIXL_WRITE, NIXL_RD_NOTIF, NIXL_WR_NOTIF };

    for (size_t i = 0; i < sizeof(ops)/sizeof(ops[i]); i++) {
        cout << endl << op2string(ops[i]) << " test (" << iter << ") iterations" <<endl;
        for(int k = 0; k < iter; k++ ) {
            /* Init data */
            for(int i = 0; i < ucx1_src_descs.descCount(); i++) {
                auto desc = ucx1_src_descs[i];
                doMemset(src_mem_type, desc.devId, (void*)desc.addr, 0xda, desc.len);
            }
            for(int i = 0; i < ucx2_src_descs.descCount(); i++) {
                auto desc = ucx2_src_descs[i];
                doMemset(dst_mem_type, desc.devId, (void*)desc.addr, 0xff, desc.len);
            }

            /* Test */
            performTransfer(ucx1, ucx2, ucx1_src_descs, ucx1_dst_descs,
                            ops[i], !p_thread);
        }
    }

    cout << endl << "Test genNotif operation" << endl;

    for(int k = 0; k < iter; k++) {
        std::string test_str("test");
        std::string tgt_agent("Agent2");
        notif_list_t target_notifs;

        cout << "\t gnNotif to Agent2" <<endl;

        ucx1->genNotif(tgt_agent, test_str);

        cout << "\t\tChecking notification flow: " << flush;

        while(target_notifs.size() == 0){
            status = ucx2->getNotifs(target_notifs);
            assert(NIXL_SUCCESS == status);
            if (!p_thread) {
                /* progress UCX1 as well */
                ucx1->progress();
            }
        }

        assert(target_notifs.size() == 1);
        assert(target_notifs.front().first == "Agent1");
        assert(target_notifs.front().second == test_str);

        cout << "OK" << endl;
    }

    // As well as all the remote notes, asking to remove them one by one
    // need to provide list of descs
    destroyRemoteDescs(ucx1, ucx1_dst_descs);

    destroyLocalDescs(ucx1, ucx1_src_descs);
    destroyLocalDescs(ucx2, ucx2_src_descs);

    // Test one-sided disconnect (initiator only)
    ucx1->disconnect(agent2);

    // TODO: Causes race condition - investigate conn management implementation
    //ucx2->disconnect(agent1);
}

int main()
{
    bool thread_on[] = {false , true};
#define THREAD_ON_SIZE (sizeof(thread_on) / sizeof(thread_on[0]))
    nixlBackendEngine *ucx[THREAD_ON_SIZE][2] = { 0 };

#define NUM_WORKERS 8

int ndevices = NUM_WORKERS;
#ifdef USE_VRAM
    int n_vram_dev;
    cudaGetDeviceCount(&n_vram_dev);
    std::cout << "Detected " << n_vram_dev << " CUDA devices" << std::endl;
#endif


    // Allocate UCX engines
    for(size_t i = 0; i < THREAD_ON_SIZE; i++) {
        for(int j = 0; j < 2; j++) {
            std::stringstream s;
            s << "Agent" << (j + 1);
            ucx[i][j] = createEngine(s.str(), ndevices, thread_on[i]);
        }
    }

    for(size_t i = 0; i < THREAD_ON_SIZE; i++) {
        test_inter_agent_transfer(thread_on[i], 
                                ucx[i][0], DRAM_SEG, ndevices,
                                ucx[i][1], DRAM_SEG, ndevices);
#ifdef USE_VRAM
        if (n_vram_dev) {
            test_inter_agent_transfer(thread_on[i], 
                                    ucx[i][0], VRAM_SEG, n_vram_dev,
                                    ucx[i][1], VRAM_SEG, n_vram_dev);
            test_inter_agent_transfer(thread_on[i], 
                                    ucx[i][0], VRAM_SEG, n_vram_dev,
                                    ucx[i][1], VRAM_SEG, n_vram_dev);
            test_inter_agent_transfer(thread_on[i], 
                                    ucx[i][0], VRAM_SEG, n_vram_dev,
                                    ucx[i][1], DRAM_SEG, n_vram_dev);
            test_inter_agent_transfer(thread_on[i], 
                                    ucx[i][0], DRAM_SEG, n_vram_dev,
                                    ucx[i][1], VRAM_SEG, n_vram_dev);
                        
        }
#endif
    }

    // Allocate UCX engines
    for(int i = 0; i < 2; i++) {
        for(int j = 0; j < 2; j++) {
            releaseEngine(ucx[i][j]);
        }
    }
}
