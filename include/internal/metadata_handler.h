#ifndef __METADATA_HANDLER_H_
#define __METADATA_HANDLER_H_

#include "mem_section.h"

// This class talks to the metadata server.
class nixlMetadataH {
    private:
        // Maybe the connection information should go to Agent,
        // to add p2p support
        std::string   ipAddress;
        uint16_t      port;

    public:
        // Creates the connection to the metadata server
        nixlMetadataH() {}
        nixlMetadataH(const std::string &ip_address, uint16_t port);
        ~nixlMetadataH();

        /** Sync the local section with the metadata server */
        int sendLocalMetadata(const std::string &local_md);

        // Get a remote section from the metadata server
        std::string getRemoteMd(const std::string &remote_agent);

        // Invalidating the information in the metadata server
        int removeLocalMetadata(const std::string &local_agent);
};

class nixlAgentData {
    public:
        std::string                                  name;
        nixlDeviceMD                                 deviceMeta;

        std::map<backend_type_t, nixlBackendEngine*> nixlBackendEngines;
        std::map<backend_type_t, std::string>        connMD; // Local info

        nixlLocalSection                             memorySection;
        nixlMetadataH                                mdHandler;

        std::map<std::string, nixlRemoteSection*>    remoteSections;
        std::map<std::string, backend_set_t>         remoteBackends;

        nixlAgentData() {}
        ~nixlAgentData();
};

#endif
