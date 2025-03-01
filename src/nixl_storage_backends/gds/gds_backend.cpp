#include "gds_backend.h"
#include <cassert>

nixlGdsEngine::nixlGdsEngine (const nixlGdsInitParams* init_params)
: nixlBackendEngine ((const nixlBackendInitParams *) init_params) {
 
        CufileError_t   err;

        err = cuFileDriverOpen();
        id (err.err != CU_FILE_SUCCESS) {
            std::cerr <<" Error initializing GPU Direct Storage driver "
                      << cuFileGetErrorString(err)
                      << std::cout;
            this->initErr = true;
        }
        return NIXL_SUCCESS;       
}
