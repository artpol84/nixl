#ifndef _NIXL_METADATA_H
#define _NIXL_METADATA_H

#include <string>
#include <cstdint>
#include "internal/transfer_backend.h"

class nixlSysTopology {
    // System Topology class TBD
};

// Per node device metadata information
class nixlDeviceMD {
    // Information about the node that needs to be sent to ETCD
    // Such as list of devices and their type, assigned IP to ETCD
    // Some topology info might be added as well (Ryan's comment)
    // TBD: Like Topology class - to get system specific information/tuning
    public:
        nixlSysTopology topology;
        std::string     srcIpAddress;
        uint16_t        srcPort;
};

// Example backend initialization data for UCX
class nixlUcxInitParams : public nixlBackendInitParams {
    public:
        backend_type_t getType () { return UCX; }
        // TBD: Required parameters to initialize UCX that we need to get from the user
};

#endif
