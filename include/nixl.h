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
        // Populates agent name, metadata_id and device metadata
				// (maybe metadata_id and agent_name can be merged)
        TransferAgent(const std::string &name, std::string md_id,
                      const DeviceMetadata &devs);
        ~TransferAgent();

        // Instantiate BackendEngine objects, based on corresponding parameters
        BackendEngine *create_backend(BackendInitParams *);
        // Register with the backend and populate memory_section
        int register_mem(const DescList<BasicDesc>& descs, BackendEngine *backend);
        // Deregister and remove from memory section
        int deregister_mem(const DescList<BasicDesc>& descs, BackendEngine *backend);

        // Make connection to a remote agent
        int make_connection(std::string remote_agent);

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

        /** Metadata Handling */

        // Invalidate the remote section information cached locally upon receiving
        // invalidation request.
        void invalidate_remote_metadata(std::string remote_md_id);

        // Sends messages to ETCD to invalidate. Also to agents with active
        // connections. Runtime directs the message to the corresponding process.
        void invalidate_local_metadata();

        // Prepares the data to be sent to etcd
        NIXLMetadata get_metadata ();

        // Populates a Remote metadata, can be used for prefetch.
        int fetch_metadata (std::string &remote_md_id);

        // If metadata was received through other channels, it can be loaded into the library
        int load_metadata (NIXLMetadata remote_metadata);

        // Send the local metadata to remote process/service
        // Primarily for testing or connecting to remote service to store metadata
        int send_metadata(std::string &remote_md_id);

};

#endif
