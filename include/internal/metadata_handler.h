#include <uuid.h>
#include "mem_section.h"

// This class talks to the metadata server.
// It also caches the data received, both for sections and connection info

// Note that string_segment and string_conn_md add string serialization/deserialization
// If that becomes an issue, we can use std::variant to make enum style classes of all
// public_metadata for backend, and for connection info, if there is no pointers in it,
// just send it as it is. This would change the other parts of the library too. This helps
// the initial modular attemp, as it's unlikely for it to be on non-amortized datapath.
class metadata_handler {
    private:
        std::string ip_address;
        uint16_t    port;

        std::string local_agent;
        uuid_t      local_segment;

    public:
        metadata_handler(std::string& ip_address, uint16_t port,
                         std::string local_agent, uuid_t local_segment);
        ~metadata_handler();

        /** Sync the local section with the metadata server */
        int sync_local_section_cache();
        // Invalidating the information in the metadata server
        int remove_local_section_metadata();
        // For partial updates. It will override the info for a specific memory type and backend
        // Can add remove individual elements, but instead we can write them with empty desc_list
        int put_section_metadata(uuid_t section_id,
                                 std::vector<string_segment>& md);
        // Get a remote section from the metadata server
        std::vector<string_segment> get_section_metadata(uuid_t section_id);

        // Similar functioanlities, but for connection information
        int sync_local_connection_metadata();
        int remove_local_connection_metadata();
        int put_connection_metadata(std::vector<string_conn_md>& conn_md);
        std::vector<string_conn_md> get_connection_metadata(std::string remote_agent);

        // For now we're not including any local device information in the metadata server
        // If necessary that can be added, per agent name, but conn_metadata seems sufficient
};
