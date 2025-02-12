/** Inference Transfer Library */
#ifndef _NIXL_H
#define _NIXL_H

#include "nixl_descriptors.h"
#include "nixl_params.h"
#include "internal/transfer_backend.h"
#include "internal/metadata_handler.h"
#include "internal/transfer_request.h"

// Main transfer object
class nixlAgent {
    private:
        nixlAgentDataPrivate data;

    public:

        /*** Initialization and Regsitering Methods ***/

        // Populates agent name and device metadata
        nixlAgent (const std::string &name, const nixlDeviceMetadata &devs);
        ~nixlAgent ();

        // Instantiate BackendEngine objects, based on corresponding params
        nixlBackendEngine* createBackend (nixlBackendInitParams *params);
        // Register with the backend and populate memory_section
        int registerMem (const nixlDescList<nixlBasicDesc>& descs,
                         nixlBackendEngine *backend);
        // Deregister and remove from memory section
        int deregisterMem (const nixlDescList<nixlBasicDesc>& descs,
                           nixlBackendEngine *backend);

        // Make connectionproactively, instead of at transfer time
        int makeConnection (std::string remote_agent, int direction);


        /*** Transfer Request Handling ***/

        // populates the transfer request.
        int createTransferReq (nixlDescList<nixlBasicDesc>& local_descs,
                               nixlDescList<nixlBasicDesc>& remote_descs,
                               std::string remote_agent,
                               std::string notif_msg,
                               int direction,
                               nixlTransferRequest* &req_handle);

        // Invalidate transfer request if we no longer need it.
        void invalidateRequest (nixlTransferRequest *req);

        // Submit transfer requests
        // The async handler is the TransferRequest object
        int postRequest (nixlTransferRequest *req);

        // Send the notification message to the target
        int sendNotification (nixlTransferRequest *req);

        // Check the status of transfer requests
        transfer_state_t getStatus (nixlTransferRequest *req);


        /*** Metadata handling through side channel ***/

        // Get nixl_metadata for this agent
        std::string getMetadata () const;

        // Load other agent's metadata and unpack it internally
        int loadMetadata (std::string remote_metadata);

        // Invalidate the remote section information cached locally
        void invalidateRemoteMetadata (std::string remote_agent);


        /*** Metadata handling through central kv service, or for p2p test ***/

        // Send the local metadata to kv service to store it
        int sendMetadata () const;

        // Request for a remote Agent's metadata, used for proactive prefetch
        int fetchMetadata (std::string &remote_agent);

        // Sends messages to kv service to invalidate this Agent's metadata
        void invalidateLocalMetadata ();
};

#endif
