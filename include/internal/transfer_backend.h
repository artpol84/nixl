#ifndef __TRANSFER_BACKEND_H_
#define __TRANSFER_BACKEND_H_

#include <string>
#include "nixl_descriptors.h"

// Might be removed to be decided by backend, or changed to high
// level direction or so.
typedef enum {READ, WRITE} transfer_op_t;

// A base class to point to backend initialization data
class nixlBackendInitParams {
    public:
        virtual backend_type_t getType () = 0;
        virtual ~nixlBackendInitParams() = default;
};

class nixlBackendTransferHandle {
};

// Main goal of BackendMetadata class is to have a common pointer type
// for different backend metadata
// Not sure get/set for serializer/deserializer is necessary
// We can add backend_type and memory_type, but can't see use case
class nixlBackendMetadata {
    bool privateMetadata = true;

    public:
        nixlBackendMetadata(bool isPrivate){
            privateMetadata = isPrivate;
        }

        virtual ~nixlBackendMetadata(){
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
        nixlBackendMetadata *metadata;

        // Reuse parent constructor without the metadata
        using nixlBasicDesc::nixlBasicDesc;

        // No serializer or deserializer, using parent not to expose the metadata

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
    private:
        backend_type_t backendType;
        nixlBackendInitParams *initParams;

    public:
        nixlBackendEngine (nixlBackendInitParams *initParams) {
            this->backendType = initParams->getType();
            this->initParams  = initParams;
        }

        backend_type_t getType () const { return backendType; }

        // Can add checks for being public metadata
        std::string getPublicData (const nixlBackendMetadata* meta) const {
            return meta->get();
        }

        virtual ~nixlBackendEngine () {};

        // Register and deregister local memory
        virtual int registerMem (const nixlBasicDesc &mem, memory_type_t mem_type,
                                 nixlBackendMetadata* &out) = 0;
        virtual void deregisterMem (nixlBackendMetadata* desc) = 0;

        // If we use an external connection manager, the next 3 methods might change
        // Provide the required connection info for remote nodes
        virtual std::string getConnInfo() const = 0;

        // Deserialize from string the connection info for a remote node
        virtual int loadRemoteConnInfo (std::string remote_agent,
                                        std::string remote_conn_info) = 0;

        // Make connection to a remote node identified by the name into loaded conn infos
        virtual int makeConnection(std::string remote_agent) = 0;

        // Listen for connections from a remote agent
        virtual int listenForConnection(std::string remote_agent) = 0;

        // Add and remove remtoe metadata
        virtual int loadRemote (nixlStringDesc input,
                                nixlBackendMetadata* &output,
                                std::string remote_agent) = 0;

        virtual int removeRemote (nixlBackendMetadata* input) = 0;

        // Posting a request, to be updated to return an async handler
        virtual int transfer (nixlDescList<nixlMetaDesc> local,
                              nixlDescList<nixlMetaDesc> remote,
                              transfer_op_t operation,
                              std::string notif_msg,
                              nixlBackendTransferHandle* &handle) = 0;

        // Send the notification message to the target
        int sendNotification(nixlBackendTransferHandle* handle);

        //Use a handle to progress backend engine and see if a transfer is completed or not
        virtual int checkTransfer(nixlBackendTransferHandle* handle) = 0;

        //Force backend engine worker to progress
        virtual int progress(){
            return 0;
        }
};

#endif
