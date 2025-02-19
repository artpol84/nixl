/** NVIDIA Inference Xfer Library */
#ifndef _NIXL_H
#define _NIXL_H

#include "nixl_descriptors.h"
#include "nixl_params.h"
#include "internal/transfer_backend.h"
#include "internal/metadata_handler.h"
#include "internal/transfer_request.h"

typedef std::map<std::string, std::vector<std::string>> notif_map_t;

// Main transfer object
class nixlAgent {
    private:
        nixlAgentData data;

    public:

        /*** Initialization and Regsitering Methods ***/

        // Populates agent name and device metadata
        nixlAgent (const std::string &name, const nixlDeviceMD &devs);
        ~nixlAgent ();

        // Instantiate BackendEngine objects, based on corresponding params
        nixlBackendEngine* createBackend (nixlBackendInitParams* params);
        // Register with the backend and populate memory_section
        int registerMem (const nixlDescList<nixlBasicDesc> &descs,
                         nixlBackendEngine* backend);
        // Deregister and remove from memory section
        int deregisterMem (const nixlDescList<nixlBasicDesc> &descs,
                           nixlBackendEngine* backend);

        // Make connection proactively, instead of at transfer time
        int makeConnection (const std::string &remote_agent, int direction);


        /*** Transfer Request Handling ***/

        // populates the transfer request. Empty notif_msg means no notification
        int createXferReq (const nixlDescList<nixlBasicDesc> &local_descs,
                           const nixlDescList<nixlBasicDesc> &remote_descs,
                           const std::string &remote_agent,
                           const std::string &notif_msg,
                           const xfer_op_t &operation,
                           nixlXferReqH* &req_handle);

        // Submit a transfer request, which populates the req async handler.
        xfer_state_t postXferReq (nixlXferReqH* req);

        // Check the status of transfer requests
        xfer_state_t getXferStatus (nixlXferReqH* req);

        // Invalidate transfer request if we no longer need it.
        // Will abort a running transfer.
        void invalidateXferReq (nixlXferReqH* req);


        /*** Notification Handling ***/

        // Add entries to the passed received notifications list (can be
        // non-empty), and return number of added entries, or -1 if there was
        // an error. Elements are released within the Agent after this call.
        int getNewNotifs (notif_map_t &notif_map);

        // Generate a notification, not bound to a transfer, e.g., for control.
        // Can be used after the remote metadata is exchanged. Will be received
        // in notif list. Nixl will choose a backend if null is passed.
        int genNotif (const std::string &remote_agent,
                      const std::string &msg,
                      nixlBackendEngine* backend = nullptr);


        /*** Metadata handling through side channel ***/

        // Get nixl_metadata for this agent. Empty string means error.
        // The std::string used for serialized MD can have \0 values.
        std::string getLocalMD () const;

        // Load other agent's metadata and unpack it internally.
        // Returns the found agent name in metadata, or "" in case of error.
        std::string loadRemoteMD (const std::string &remote_metadata);

        // Invalidate the remote section information cached locally
        int invalidateRemoteMD (const std::string &remote_agent);


        /*** Metadata handling through central kv service, or for p2p test ***/

        // Send the local metadata to kv service to store it
        int sendLocalMD () const;

        // Request for a remote Agent's metadata, used for proactive prefetch
        int fetchRemoteMD (const std::string &remote_agent);

        // Sends messages to kv service to invalidate this Agent's metadata
        int invalidateLocalMD () const;
};

#endif
