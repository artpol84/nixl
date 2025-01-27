#ifndef _NIXL_METADATA_H
#define _NIXL_METADATA_H

#include <string>
#include <cstdint>

class SystemTopology {
    // System Topology class TBD
};

// Per node device metadata information
class device_metadata {
    // Information about the node that needs to be sent to ETCD
    // Such as list of devices and their type, assigned IP to ETCD
    // Some topology info might be added as well (Ryan's comment)
    // TBD: Like Topology class - to get system specific information/tuning
    public:
        SystemTopology topology;
        std::string    src_ip_address;
        uint16_t       src_port;
};

#endif
