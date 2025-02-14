#ifndef __TRANSFER_BACKEND_H_
#define __TRANSFER_BACKEND_H_

#include <mutex>
#include <string>
#include "nixl_descriptors.h"

// Might be removed to be decided by backend, or changed to high
// level direction or so.
typedef enum {NIXL_RD, NIXL_WR, NIXL_RD_W_NOTIF, NIXL_WR_W_NOTIF} transfer_op_t;
typedef enum {NIXL_XFER_INIT, NIXL_XFER_PROC,
              NIXL_XFER_DONE, NIXL_XFER_ERR} transfer_state_t;
typedef std::vector<std::pair<std::string, std::string>> notif_list_t;

// A base class to point to backend initialization data

// User doesn't know about fields such as local_agent but can access it
// after the backend is initialized by agent. If we needed to make it private
// from the user, we should make nixlBackendEngine/nixlAgent friend classes.
class nixlBackendInitParams {
    public:
        std::string localAgent;

        virtual backend_type_t getType () = 0;
        virtual ~nixlBackendInitParams() = default;
};

class nixlBackendReqH {
};

// Main goal of BackendMetadata class is to have a common pointer type
// for different backend metadata
// Not sure get/set for serializer/deserializer is necessary
// We can add backend_type and memory_type, but can't see use case
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
        int set(std::string) {
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
    std::string   dstIpAddress;
    uint16_t      dstPort;
};

// A pointer required to a metadata object for backends next to each BasicDesc
class nixlMetaDesc : public nixlBasicDesc {
  public:
        // To be able to point to any object
        nixlBackendMD *metadata;

        // Reuse parent constructor without the metadata
        using nixlBasicDesc::nixlBasicDesc;

        // No serializer or deserializer, using parent not to expose the metadata

        inline friend bool operator==(const nixlMetaDesc& lhs, const nixlMetaDesc& rhs) {
            return (((nixlBasicDesc)lhs == (nixlBasicDesc)rhs) &&
                          (lhs.metadata == rhs.metadata));
        }

        // Main use case is to take the BasicDesc from another object, so just
        // the metadata part is separately copied here, used in DescList
        inline void copyMeta (const nixlMetaDesc& meta) {
            this->metadata = meta.metadata;
        }

        inline void print(const std::string suffix) const {
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

        nixlStringDesc(const std::string& str); // Deserializer

        inline friend bool operator==(const nixlStringDesc& lhs, const nixlStringDesc& rhs){
            return (((nixlBasicDesc)lhs == (nixlBasicDesc)rhs) &&
                          (lhs.metadata == rhs.metadata));
        }

        inline std::string serialize() const {
            return nixlBasicDesc::serialize() + metadata;
        }

        inline void copyMeta (const nixlStringDesc& meta){
            this->metadata = meta.metadata;
        }

        inline void print(const std::string suffix) const {
            nixlBasicDesc::print(", Metadata: " + metadata + suffix);
        }
};

// Base backend engine class, hides away different backend implementaitons
class nixlBackendEngine { // maybe rename to transfer_BackendEngine
    protected:
        backend_type_t        backendType;
        nixlBackendInitParams *initParams;
        std::string           localAgent;

    public:
        nixlBackendEngine (nixlBackendInitParams *init_params) {
            this->backendType = init_params->getType();
            this->localAgent  = init_params->localAgent;
            this->initParams  = init_params;
        }

        backend_type_t getType () const { return backendType; }

        // Can add checks for being public metadata
        std::string getPublicData (const nixlBackendMD* meta) const {
            return meta->get();
        }

        virtual ~nixlBackendEngine () {};

        // Register and deregister local memory
        virtual int registerMem (const nixlBasicDesc &mem, memory_type_t mem_type,
                                 nixlBackendMD* &out) = 0;
        virtual void deregisterMem (nixlBackendMD* meta) = 0;

        // If we use an external connection manager, the next 3 methods might change
        // Provide the required connection info for remote nodes
        virtual std::string getConnInfo() const = 0;

        // Deserialize from string the connection info for a remote node
        // The generated data should be deleted in nixlBackendEngine destructor
        virtual int loadRemoteConnInfo (std::string remote_agent,
                                        std::string remote_conn_info) = 0;

        // Make connection to a remote node identified by the name into loaded conn infos
        virtual int makeConnection(std::string remote_agent) = 0;

        // Listen for connections from a remote agent
        virtual int listenForConnection(std::string remote_agent) = 0;

        // Add and remove remtoe metadata
        virtual int loadRemote (nixlStringDesc input,
                                nixlBackendMD* &output,
                                std::string remote_agent) = 0;

        virtual int removeRemote (nixlBackendMD* input) = 0;

        // Posting a request, to be updated to return an async handler
        virtual int transfer (nixlDescList<nixlMetaDesc> local,
                              nixlDescList<nixlMetaDesc> remote,
                              transfer_op_t operation,
                              std::string remote_agent,
                              std::string notif_msg,
                              nixlBackendReqH* &handle) = 0;

        // Populate received notifications list. Elements are released within backend then.
        virtual int getNotifications(notif_list_t &notif_list) = 0;

        // Use a handle to progress backend engine and see if a transfer is completed or not
        virtual transfer_state_t checkTransfer(nixlBackendReqH* handle) = 0;

        virtual void releaseReqH(nixlBackendReqH* handle) = 0;

        // Force backend engine worker to progress
        virtual int progress() { return 0; }
};

#endif
