#ifndef __METADATA_HANDLER_H_
#define __METADATA_HANDLER_H_

#include "mem_section.h"

class NIXLMetadata {
    public:
        std::string                agent_name;
        std::vector<StringSegment> sec_md;
        std::vector<StringConnMD>  conn_md;
};

// This class talks to the metadata server.
class MetadataHandler {
    private:
        // Maybe the connection information should go to Agent,
        // to add p2p support
        std::string   ip_address;
        uint16_t      port;

    public:
        // Creates the connection to the metadata server
        MetadataHandler(std::string& ip_address, uint16_t port);
        ~MetadataHandler();

        /** Sync the local section with the metadata server */
        int send_local_metadata(NIXLMetadata& local_md);

        // Get a remote section from the metadata server
        NIXLMetadata get_remote_md(std::string remote_agent);

        // Invalidating the information in the metadata server
        int remove_local_metadata(std::string local_agent);
};

class AgentDataPrivate {
    public:
        std::string                    name;
        // Device specfic metadata such as topology/others
        DeviceMetadata                 device_meta;
        // Handles to different registered backend engines
        std::vector<BackendEngine *>   BackendEngines;
        // Memory section objects for local and list of cached remote objects
        LocalSection                   *memory_section;
        // Handler for metadata server access
        MetadataHandler                md_handler;
        // Remote section(s) for Transfer Agent stored locally.
        std::vector<RemoteSection *>   RemoteSections;

        NIXLMetadata  local_metadata;

        // // Transfer connection class handles
        // // Discovery and connection information of different nodes
};

#endif
