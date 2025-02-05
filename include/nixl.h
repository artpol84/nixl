/** Inference Transfer Library */
#ifndef _NIXL_H
#define _NIXL_H

#include "nixl_descriptors.h"
#include "nixl_params.h"
#include "internal/transfer_backend.h"
#include "internal/metadata_handler.h"
#include "internal/transfer_request.h"

// Main transfer object
class TransferAgent {
    private:
        AgentDataPrivate data;

    public:

        /*** Initialization and Regsitering Methods ***/

        // Populates agent name, metadata_id and device metadata
        TransferAgent(const std::string &name, std::string md_id,
                      const DeviceMetadata &devs);
        ~TransferAgent();

        // Instantiate BackendEngine objects, based on corresponding parameters
        BackendEngine *create_backend(BackendInitParams *);
        // Register with the backend and populate memory_section
        int register_mem(const DescList<BasicDesc>& descs, BackendEngine *backend);
        // Deregister and remove from memory section
        int deregister_mem(const DescList<BasicDesc>& descs, BackendEngine *backend);

        // Make connection to a remote agent proactively, instead of at transfer time
        int make_connection(std::string remote_agent);


        /*** Transfer Request Handling ***/

        // populates the transfer request.
        TransferRequest *create_transfer_req (DescList<BasicDesc>& local_desc,
                                              DescList<BasicDesc>& remote_desc,
                                              std::string remote_md_id,
                                              std::string notif_msg,
                                              int direction);

        // Invalidate transfer request if we no longer need it.
        void invalidate_request(TransferRequest *req);

        // Submit transfer requests
        // The async handler is the TransferRequest object
        int post_request(TransferRequest *req);

        // Send the notification message to the target
        int send_notification(TransferRequest *req);

        // Check the status of transfer requests
        transfer_state_t get_status (TransferRequest *req);


        /*** Metadata handling through side channel ***/

        // Get nixl_metadata for this agent
        NIXLMetadata get_metadata ();

        // Load other agent's metadata and unpack it internally
        int load_metadata (NIXLMetadata remote_metadata);

        // Invalidate the remote section information cached locally
        void invalidate_remote_metadata(std::string remote_md_id);


        /*** Metadata handling through central kv service, or single peer for test ***/

        // Send the local metadata to kv service to store it
        int send_metadata(std::string &remote_md_id);

        // Request for a remote Agent's metadata, can be used for proactive prefetch
        int fetch_metadata (std::string &remote_md_id);

        // Sends messages to kv service to invalidate this Agent's metadata
        void invalidate_local_metadata();
};

#endif
