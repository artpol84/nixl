/** Inference Transfer Library */
#ifndef _NIXL_H
#define _NIXL_H

#include <uuid.h>
#include "nixl_descriptors.h"
#include "nixl_metadata.h"
#include "internal/transfer_backend.h"
#include "internal/mem_section.h"
#include "internal/metadata_handler.h"
#include "internal/transfer_request.h"

// Main transfer object
class TransferAgent {
    private:
        std::string                     name;
        // Device specfic metadata such as topology/others
        device_metadata                 device_meta;
        // Handles to different registered backend engines
        std::vector<backend_engine>     backend_engines;
        // Memory section objects for local and list of cached remote objects
        local_section                   *memory_section;
        std::vector<remote_section>     remote_sections;
        // Handler for metadata server access
        metadata_handler                md_handle;
        // Transfer connection class handles
        // Discovery and connection information of different nodes
        std::vector<backend_conn_meta>  conn_handle;

    public:
        // Populates agent name, section_id and device metadata
        TransferAgent(const std::string &name, uuid_t section_id,
                      const device_metadata &devs);
        ~TransferAgent();

        // Instantiate backend_engine objects, based on corresponding parameters
        backend_engine *create_backend(backend_init_params *);
        // Register with the backend and populate memory_section
        int register_mem(const desc_list<basic_desc>& descs, backend_engine *backend);
        // Deregister and remove from memory section
        int deregister_mem(const desc_list<basic_desc>& descs, backend_engine *backend);

        // Make connection to a remote agent
        int make_connection(std::string remote_agent);

        // Invalidate the remote section information cached locally upon receiving
        // invalidation request.
        void invalidate_remote_sec_cache(uuid_t section_id);

        // Sends messages to ETCD to invalidate. Also to agents with active
        // connections. Runtime directs the message to the corresponding process.
        void invalidate_local_sec_metadata();

        // Populates a remote_sections object, can be used for prefetch.
        int fetch_metadata (uuid_t &remote_sec_id);

        // Prepares the data to be sent to etcd
        std::vector<string_segment> get_public_data ();

        // If not already cached, gets remote metadata from etcd. Goes over the elements and
        // populates the transfer request.
        TransferRequest *create_transfer_req (desc_list<basic_desc>& local_desc,
                                              desc_list<basic_desc>& remote_desc,
                                              uuid_t remote_sec_id,
                                              int direction);

        // Invalidate transfer request if we no longer need it.
        void invalidate_request(TransferRequest *req);

        // Submit transfer requests
        // The async handler is the TransferRequest object
        int post_request(TransferRequest *req);

        // Check the status of transfer requests
        transfer_state_t get_status (TransferRequest *req);
};

#endif
