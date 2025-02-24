#ifndef _NIXL_PARAMS_H
#define _NIXL_PARAMS_H

#include <string>
#include <cstdint>
#include "internal/transfer_backend.h"
#include "nixl_types.h"

// class nixlSysTopology {
//     // TBD if needed
// };

// Per Agent device metadata information. Also assigned IP for the
// main process, or additional information required for central KV
// service if used.
class nixlDeviceMD {
    public:
        // nixlSysTopology topology;
        std::string     srcIpAddress;
        uint16_t        srcPort;
};

// Example backend initialization data for UCX.
// For now UCX autodetects devices, later might add hints.
class nixlUcxInitParams : public nixlBackendInitParams {
    public:
        inline nixl_backend_t getType() const { return UCX; }
};

#endif
