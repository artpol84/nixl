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

        // Make connectionproactively, instead of at transfer time
        int makeConnection (const std::string &remote_agent, int direction);


        /*** Transfer Request Handling ***/

        // populates the transfer request. Empty notif_msg means no notif
        int createXferReq (const nixlDescList<nixlBasicDesc> &local_descs,
                           const nixlDescList<nixlBasicDesc> &remote_descs,
                           const std::string &remote_agent,
                           const std::string &notif_msg,
                           int direction,
                           nixlXferReqH* &req_handle);

        // Invalidate transfer request if we no longer need it.
        void invalidateXferReq (nixlXferReqH* req);

        // Submit a transfer request, which populates the req async handler.
        // Returns the status of transfer, among NIXL_XFER_PROC/DONE/ERR.
        xfer_state_t postXferReq (nixlXferReqH* req);

        // Check the status of transfer requests
        xfer_state_t getXferStatus (nixlXferReqH* req);

        // Add entries to the passed received notifications list, and
        // return number of added entries, or -1 if there were an error.
        // Elements are released within the Agent after this call.
        int addNewNotifs(notif_map_t &notif_map);

        /*** Metadata handling through side channel ***/

        // Get nixl_metadata for this agent
        std::string getLocalMD () const;

        // Load other agent's metadata and unpack it internally
        int loadRemoteMD (const std::string &remote_metadata);

        // Invalidate the remote section information cached locally
        void invalidateRemoteMD (const std::string &remote_agent);


        /*** Metadata handling through central kv service, or for p2p test ***/

        // Send the local metadata to kv service to store it
        int sendLocalMD () const;

        // Request for a remote Agent's metadata, used for proactive prefetch
        int fetchRemoteMD (const std::string &remote_agent);

        // Sends messages to kv service to invalidate this Agent's metadata
        void invalidateLocalMD () const;
};

#endif
