#ifndef _NIXL_METADATA_H
#define _NIXL_METADATA_H

#include <string>
#include <cstdint>
#include "internal/transfer_backend.h"

class SystemTopology {
    // System Topology class TBD
};

// Per node device metadata information
class DeviceMetadata {
    // Information about the node that needs to be sent to ETCD
    // Such as list of devices and their type, assigned IP to ETCD
    // Some topology info might be added as well (Ryan's comment)
    // TBD: Like Topology class - to get system specific information/tuning
    public:
        SystemTopology topology;
        std::string    src_ip_address;
        uint16_t       src_port;
};

// Example backend initialization data for UCX
class nixlUcxInitParams : public BackendInitParams {
    public:
        // TBD: Required parameters to initialize UCX that we need to get from the user
};

#endif
