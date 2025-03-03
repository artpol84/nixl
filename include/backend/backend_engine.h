#ifndef __BACKEND_ENGINE_H_
#define __BACKEND_ENGINE_H_

#include <mutex>
#include <string>
#include "nixl_descriptors.h"
#include "nixl_types.h"
#include "utils/sys/nixl_time.h"

#include "backend_aux.h"

// Base backend engine class for different backend implementaitons
class nixlBackendEngine {
    private:
        nixl_backend_t backendType;

    public:
        nixl_backend_t getType () const { return backendType; }
        // Determines if a backend supports remote operations
        virtual bool supportsRemote () const = 0;

        // Determines if a backend supports local operations
        virtual bool supportsLocal () const = 0;

        // Determines if a backend supports sending notifications. Related methods are not
        // pure virtual, and return errors, as parent shouldn't call if supportsNotif is false.
        virtual bool supportsNotif () const = 0;

        // Determines if a backend supports progress thread.
        virtual bool supportsProgTh () const = 0;

        // The support function determine which methods are necessary by the child backend, and
        // if they're called by mistake, they will return error if not implemented by backend.

    protected:
        std::string    localAgent;
        bool           initErr;

        nixlBackendEngine (const nixlBackendInitParams* init_params) {
            this->backendType = init_params->getType();
            this->localAgent  = init_params->localAgent;
            this->initErr     = false;
        }
        
        virtual ~nixlBackendEngine () = default;

        bool getInitErr() { return initErr; }

        // *** Pure virtual methods that need to be implemented by any backend *** //

        // Register and deregister local memory
        virtual nixl_status_t registerMem (const nixlStringDesc &mem,
                                           const nixl_mem_t &nixl_mem,
                                           nixlBackendMD* &out) = 0;
        virtual void deregisterMem (nixlBackendMD* meta) = 0;

        // Make connection to a remote node identified by the name into loaded conn infos
        // Child might just return 0, if making proactive connections are not necessary.
        // An agent might need to connect to itself for local operations.
        virtual nixl_status_t connect(const std::string &remote_agent) = 0;
        virtual nixl_status_t disconnect(const std::string &remote_agent) = 0;

        // Remove loaded local or remtoe metadata for target
        virtual nixl_status_t unloadMD (nixlBackendMD* input) = 0;

        // Posting a request, which returns populates the async handle.
        virtual nixl_xfer_state_t postXfer (const nixl_meta_dlist_t &local,
                                            const nixl_meta_dlist_t &remote,
                                            const nixl_xfer_op_t &operation,
                                            const std::string &remote_agent,
                                            const std::string &notif_msg,
                                            nixlBackendReqH* &handle) = 0;

        // Use a handle to progress backend engine and see if a transfer is completed or not
        virtual nixl_xfer_state_t checkXfer(nixlBackendReqH* handle) = 0;

        //Backend aborts the transfer if necessary, and destructs the relevant objects
        virtual void releaseReqH(nixlBackendReqH* handle) = 0;


        // *** Needs to be implemented if supportsRemote() is true *** //

        // Gets serialized form of public metadata
        virtual std::string getPublicData (const nixlBackendMD* meta) const { return ""; };

        // Provide the required connection info for remote nodes, should be non-empty
        virtual std::string getConnInfo() const { return ""; }

        // Deserialize from string the connection info for a remote node, if supported
        // The generated data should be deleted in nixlBackendEngine destructor
        virtual nixl_status_t loadRemoteConnInfo (const std::string &remote_agent,
                                                  const std::string &remote_conn_info) {
            return NIXL_ERR_BACKEND;
        }

        // Load remtoe metadata, if supported.
        virtual nixl_status_t loadRemoteMD (const nixlStringDesc &input,
                                            const nixl_mem_t &nixl_mem,
                                            const std::string &remote_agent,
                                            nixlBackendMD* &output) {
            return NIXL_ERR_BACKEND;
        }


        // *** Needs to be implemented if supportsLocal() is true *** //

        // Provide the target metadata necessary for local operations, if supported
        virtual nixl_status_t loadLocalMD (nixlBackendMD* input,
                                           nixlBackendMD* &output) {
            return NIXL_ERR_BACKEND;
        }


        // *** Needs to be implemented if supportsNotif() is true *** //

        // Populate an empty received notif list. Elements are released within backend then.
        virtual int getNotifs(notif_list_t &notif_list) { return NIXL_ERR_BACKEND; }

        // Generates a standalone notification, not bound to a transfer.
        virtual nixl_status_t genNotif(const std::string &remote_agent, const std::string &msg) {
            return NIXL_ERR_BACKEND;
        }


        // *** Needs to be implemented if supportsProgTh() is true *** //

        // Force backend engine worker to progress.
        virtual int progress() { return 0; }

    friend class nixlAgent;
    friend class memSection;
    friend class nixlXferReqH;
    friend class nixlAgentData;
    friend class nixlLocalSection;
    friend class nixlRemoteSection;
    friend class nixlBackendTester;
};
#endif
