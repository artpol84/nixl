#ifndef __METADATA_HANDLER_H_
#define __METADATA_HANDLER_H_

#include "mem_section.h"

class NIXLMetadata {
    public:
        std::string                 agent_name;
        std::string                 sec_md_id;
        std::vector<StringSegment> sec_md;
        std::vector<StringConnMD> conn_md;
};

// This class talks to the metadata server.
// It also caches the data received, both for sections and connection info

// Note that StringSegment and StringConnMD add string serialization/deserialization
// If that becomes an issue, we can use std::variant to make enum style classes of all
// public_metadata for backend, and for connection info, if there is no pointers in it,
// just send it as it is. This would change the other parts of the library too. This helps
// the initial modular attemp, as it's unlikely for it to be on non-amortized datapath.
class MetadataHandler {
    private:
        std::string   ip_address;
        uint16_t      port;

        std::string   local_agent;
        std::string   local_md_id;

        NIXLMetadata  local_metadata;

    public:
        MetadataHandler(std::string& ip_address, uint16_t port,
                        std::string local_agent, std::string local_md_id);
        ~MetadataHandler();

        /** Sync the local section with the metadata server */
        int send_local_metadata(std::string remote_md_id);

        // Invalidating the information in the metadata server
        int remove_local_metadata();

        // Get a remote section from the metadata server
        NIXLMetadata get_remote_md(std::string remote_md_id);
};

class AgentDataPrivate {
    public:
        std::string                     name;
        // Device specfic metadata such as topology/others
        DeviceMetadata                 device_meta;
        // Handles to different registered backend engines
        std::vector<BackendEngine *>   BackendEngines;
        // Memory section objects for local and list of cached remote objects
        LocalSection                   *memory_section;
        // Handler for metadata server access
        MetadataHandler                md_handler;
        // Remte section(s) for Transfer Agent stored locally.
        std::vector<RemoteSection *>   RemoteSections;

        // // Transfer connection class handles
        // // Discovery and connection information of different nodes
};

#endif
