#ifndef __TRANSFER_BACKEND_H_
#define __TRANSFER_BACKEND_H_

#include <mutex>
#include <string>
#include "nixl_descriptors.h"
#include "nixl_types.h"

// Might be removed to be decided by backend, or changed to high
// level direction or so.
typedef std::vector<std::pair<std::string, std::string>> notif_list_t;

// A base class to point to backend initialization data

// User doesn't know about fields such as local_agent but can access it
// after the backend is initialized by agent. If we needed to make it private
// from the user, we should make nixlBackendEngine/nixlAgent friend classes.
class nixlBackendInitParams {
    public:
        std::string localAgent;
        bool        threading;

        virtual nixl_backend_t getType() const = 0;

        virtual ~nixlBackendInitParams() = default;
};

class nixlBackendReqH {
public:
    nixlBackendReqH() { }
    ~nixlBackendReqH() { }
};

// Main goal of BackendMetadata class is to have a common pointer type
// for different backend metadata
// Not sure get/set for serializer/deserializer is necessary
// We can add nixl_backend and memory_type, but can't see use case
class nixlBackendMD {
    bool privateMetadata = true;

    public:
        nixlBackendMD(bool isPrivate){
            privateMetadata = isPrivate;
        }

        virtual ~nixlBackendMD(){
        }

        bool isPrivate () const { return  privateMetadata; }
        bool isPublic  () const { return !privateMetadata; }

        // Desired serializer instead of std::string
        virtual std::string get() const {
            std::string err = "Not Implemented";
            return err;
        }

        // To be able to populate when receiving data
        int set(const std::string &str) {
            return -1; // Not implemented
        }
};

// Each backend can have different connection requirement
// This class would include the required information to make
// a connection to a remote node. Note that local information
// is passed during the constructor and through BackendInitParams
class nixlBackendConnMD {
  public:
    // And some other details
    std::string dstIpAddress;
    uint16_t    dstPort;
};

// A pointer required to a metadata object for backends next to each BasicDesc
class nixlMetaDesc : public nixlBasicDesc {
  public:
        // To be able to point to any object
        nixlBackendMD* metadata;

        // Reuse parent constructor without the metadata
        using nixlBasicDesc::nixlBasicDesc;

        // No serializer or deserializer, using parent not to expose the metadata

        inline friend bool operator==(const nixlMetaDesc &lhs, const nixlMetaDesc &rhs) {
            return (((nixlBasicDesc)lhs == (nixlBasicDesc)rhs) &&
                          (lhs.metadata == rhs.metadata));
        }

        // Main use case is to take the BasicDesc from another object, so just
        // the metadata part is separately copied here, used in DescList
        inline void copyMeta (const nixlMetaDesc &meta) {
            this->metadata = meta.metadata;
        }

        inline void print(const std::string &suffix) const {
            nixlBasicDesc::print(", Backend ptr val: " +
                                 std::to_string((uintptr_t)metadata) + suffix);
        }
};

// String of metadata next to each BasicDesc, used to import/export
class nixlStringDesc : public nixlBasicDesc {
    public:
        std::string metadata;

        // Reuse parent constructor without the metadata
        using nixlBasicDesc::nixlBasicDesc;

        nixlStringDesc(const std::string &str); // Deserializer

        inline friend bool operator==(const nixlStringDesc &lhs, const nixlStringDesc &rhs){
            return (((nixlBasicDesc)lhs == (nixlBasicDesc)rhs) &&
                          (lhs.metadata == rhs.metadata));
        }

        inline std::string serialize() const {
            return nixlBasicDesc::serialize() + metadata;
        }

        inline void copyMeta (const nixlStringDesc &meta){
            this->metadata = meta.metadata;
        }

        inline void print(const std::string &suffix) const {
            nixlBasicDesc::print(", Metadata: " + metadata + suffix);
        }
};

// Base backend engine class for different backend implementaitons
class nixlBackendEngine {
    private:
        nixl_backend_t backendType;

    protected:
        std::string    localAgent;

    public: // public to be removed
        nixlBackendEngine (const nixlBackendInitParams* init_params) {
            this->backendType = init_params->getType();
            this->localAgent  = init_params->localAgent;
        }

        // Can add checks for being public metadata
        std::string getPublicData (const nixlBackendMD* meta) const {
            return meta->get();
        }

        // Determines if a backend supports sending notifications. Related methods are not
        // pure virtual, and return errors, as parent shouldn't call if supportsNotif is false.
        virtual bool supportsNotif () const = 0;

        virtual ~nixlBackendEngine () = default;

        // Register and deregister local memory
        virtual nixl_status_t registerMem (const nixlBasicDesc &mem,
                                           const nixl_mem_t &nixl_mem,
                                           nixlBackendMD* &out) = 0;
        virtual void deregisterMem (nixlBackendMD* meta) = 0;

        // If we use an external connection manager, the next 3 methods might change
        // Provide the required connection info for remote nodes
        virtual std::string getConnInfo() const = 0;

        // Deserialize from string the connection info for a remote node
        // The generated data should be deleted in nixlBackendEngine destructor
        virtual nixl_status_t loadRemoteConnInfo (const std::string &remote_agent,
                                                  const std::string &remote_conn_info) = 0;

        // Make connection to a remote node identified by the name into loaded conn infos
        // Child might just return 0, if making proactive connections are not necessary.
        virtual nixl_status_t connect(const std::string &remote_agent) = 0;
        virtual nixl_status_t disconnect(const std::string &remote_agent) = 0;

        // Add and remove remtoe metadata
        virtual nixl_status_t loadRemoteMD (const nixlStringDesc &input,
                                            const nixl_mem_t &nixl_mem,
                                            const std::string &remote_agent,
                                            nixlBackendMD* &output) = 0;

        virtual nixl_status_t removeRemoteMD (nixlBackendMD* input) = 0;

        // Posting a request, which returns populates the async handle.
        // Returns the status of transfer, among NIXL_XFER_PROC/DONE/ERR.
        // Empty notif_msg means no notification, or can be ignored if supportsNotif is false.
        virtual nixl_xfer_state_t postXfer (const nixlDescList<nixlMetaDesc> &local,
                                            const nixlDescList<nixlMetaDesc> &remote,
                                            const nixl_xfer_op_t &operation,
                                            const std::string &remote_agent,
                                            const std::string &notif_msg,
                                            nixlBackendReqH* &handle) = 0;

        // Use a handle to progress backend engine and see if a transfer is completed or not
        virtual nixl_xfer_state_t checkXfer(nixlBackendReqH* handle) = 0;

        //Backend aborts the transfer if necessary, and destructs the relevant objects
        virtual void releaseReqH(nixlBackendReqH* handle) = 0;

        // Populate an empty received notif list. Elements are released within backend then.
        virtual int getNotifs(notif_list_t &notif_list) { return NIXL_ERR_BACKEND; }

        // Generates a standalone notification, not bound to a transfer.
        // Used for extra sync or ctrl msgs.
        virtual nixl_status_t genNotif(const std::string &remote_agent, const std::string &msg) {
            return NIXL_ERR_BACKEND;
        }

        // Force backend engine worker to progress.
        // TODO: remove
        virtual int progress() { return 0; }

    // public:
        nixl_backend_t getType () const { return backendType; }

    friend class nixlAgent;
    friend class memSection;
    friend class nixlXferReqH;
    friend class nixlAgentData;
    friend class nixlLocalSection;
    friend class nixlRemoteSection;
};

#endif
