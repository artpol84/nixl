#ifndef __METADATA_HANDLER_H_
#define __METADATA_HANDLER_H_

#include "mem_section.h"

// This class talks to the metadata server.
class nixlMetadataHandler {
    private:
        // Maybe the connection information should go to Agent,
        // to add p2p support
        std::string   ipAddress;
        uint16_t      port;

    public:
        // Creates the connection to the metadata server
        nixlMetadataHandler() {}
        nixlMetadataHandler(std::string& ip_address, uint16_t port);
        ~nixlMetadataHandler();

        /** Sync the local section with the metadata server */
        int sendLocalMetadata(std::string& local_md);

        // Get a remote section from the metadata server
        std::string getRemoteMd(std::string remote_agent);

        // Invalidating the information in the metadata server
        int removeLocalMetadata(std::string local_agent);
};

class nixlAgentDataPrivate {
    public:
        std::string                                name;
        // Device specfic metadata such as topology/others
        nixlDeviceMetadata                         deviceMeta;
        // Handles to different registered backend engines
        std::vector<nixlBackendEngine *>           nixlBackendEngines;
        // Memory section objects for local and list of cached remote objects
        nixlLocalSection                           memorySection;
        // Handler for metadata server access
        nixlMetadataHandler                        mdHandler;
        // Remote section(s) for Transfer Agent stored locally.
        std::map<std::string, nixlRemoteSection *> remoteSections;
        // local conn strings after serialization
        std::map<backend_type_t, std::string>      connMd;

        // // Transfer connection class handles
        // // Discovery and connection information of different nodes
        nixlAgentDataPrivate() {}
        ~nixlAgentDataPrivate();
};

#endif
