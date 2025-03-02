#include <cassert>
#include <iostream>
#include "cufile.h"
#include "gds_backend.h"


nixlGdsEngine::nixlGdsEngine (const nixlGdsInitParams* init_params)
: nixlBackendEngine ((const nixlBackendInitParams *) init_params) {

        CUfileError_t   err;

        err = cuFileDriverOpen();
        if (err.err != CU_FILE_SUCCESS) {
            std::cerr <<" Error initializing GPU Direct Storage driver\n";
            this->initErr = true;
        }
}

nixl_status_t nixlGdsEngine :: registerMem (const nixlStringDesc &mem,
			   const nixl_mem_t &nixl_mem,
			   nixlBackendMD* &out)
{
	return NIXL_SUCCESS;
}

void nixlGdsEngine :: deregisterMem (nixlBackendMD* meta)
{
	return;
}


nixl_status_t nixlGdsEngine :: loadRemoteMD (const nixlStringDesc &input,
					     const nixl_mem_t &nixl_mem,
					     const std::string &remote_agent,
					     nixlBackendMD* &output)
{
	return NIXL_SUCCESS;
}



nixl_xfer_state_t nixlGdsEngine :: postXfer (const nixl_meta_dlist_t &local,
                            const nixl_meta_dlist_t &remote,
                            const nixl_xfer_op_t &operation,
                            const std::string &remote_agent,
                            const std::string &notif_msg,
                            nixlBackendReqH* &handle)
{
	return NIXL_XFER_DONE;
}

nixl_xfer_state_t nixlGdsEngine ::  checkXfer(nixlBackendReqH* handle)
{
	return NIXL_XFER_DONE;
}


void nixlGdsEngine :: releaseReqH(nixlBackendReqH* handle)
{
	return;
}

nixlGdsEngine::~nixlGdsEngine() {
	//close the cuFileDriver
}
