#ifndef __TRANSFER_BACKEND_H_
#define __TRANSFER_BACKEND_H_

#include <string>
#include "nixl_descriptors.h"

// Might be removed to be decided by backend, or changed to high
// level direction or so.
typedef enum {READ, WRITE} transfer_op_t;

// A base class to point to backend initialization data
class BackendInitParams {
    public:
        backend_type_t backend_type;
        backend_type_t get_type () {
            return backend_type;
        }
};

class BackendTransferHandle {
};

// Main goal of BackendMetadata class is to have a common pointer type
// for different backend metadata
// Not sure get/set for serializer/deserializer is necessary
// We can add backend_type and memory_type, but can't see use case
class BackendMetadata {
    bool private_metadata = true;

    public:
        BackendMetadata(bool is_private){
            private_metadata = is_private;
        }

        bool is_private () const { return  private_metadata; }
        bool is_public  () const { return !private_metadata; }

        // Desired serializer instead of std::string
        std::string get() const {
            std::string err = "Not Implemented";
            return err;
        };

        // To be able to populate when receiving data
        int set(std::string) {
            return -1; // Not implemented
        }
};

// Each backend can have different connection requirement
// This class would include the required information to make
// a connection to a remote node. Note that local information
// is passed during the constructor and through BackendInitParams
class BackendConnMD {
  public:
    // And some other details
    std::string   dst_ip_address;
    uint16_t      dst_port;
};

// A pointer required to a metadata object for backends next to each BasicDesc
class MetaDesc : public BasicDesc {
  public:
        // To be able to point to any object
        BackendMetadata *metadata;

        // Main use case is to take the BasicDesc from another object, so just
        // the metadata part is separately copied here, used in DescList
        inline void copy_meta (const MetaDesc& meta) {
            this->metadata = meta.metadata;
        }
};

// String of metadata next to each BasicDesc, used to import/export
class StringDesc : public BasicDesc {
    public:
        std::string metadata;
        inline void copy_meta (const StringDesc& meta){
            this->metadata = meta.metadata;
        }
};

// Base backend engine class, hides away different backend implementaitons
class BackendEngine { // maybe rename to transfer_BackendEngine
    private:
        backend_type_t backend_type;
        BackendInitParams *init_params;

    public:
        BackendEngine (BackendInitParams *init_params) {
            this->backend_type = init_params->get_type();
            this->init_params  = init_params;
        }

        backend_type_t get_type () const { return backend_type; }

        // Can add checks for being public metadata
        std::string get_public_data (BackendMetadata* &meta) const {
            return meta->get();
        }

        virtual ~BackendEngine () {};

        // Register and deregister local memory
        virtual int register_mem (BasicDesc &mem, memory_type_t mem_type,
                                  BackendMetadata* &out) = 0;
        virtual void deregister_mem (BackendMetadata* desc) = 0;

        // If we use an external connection manager, the next 3 methods might change
        // Provide the required connection info for remote nodes
        virtual std::string get_conn_info() const = 0;

        // Deserialize from string the connection info for a remote node
        virtual int load_remote_conn_info (std::string remote_agent,
                                           std::string remote_conn_info) = 0;

        // Make connection to a remote node identified by the name into loaded conn infos
        virtual int make_connection(std::string remote_agent) = 0;

        // Listen for connections from a remote agent
        virtual int listen_for_connection(std::string remote_agent) = 0;

        // Add and remove remtoe metadata
        virtual int load_remote (StringDesc input,
                                 BackendMetadata* &output,
                                 std::string remote_agent) = 0;

        virtual int remove_remote (BackendMetadata* input) = 0;

        // Posting a request, to be updated to return an async handler
        virtual int transfer (DescList<MetaDesc> local,
                              DescList<MetaDesc> remote,
                              transfer_op_t operation,
                              std::string notif_msg,
                              BackendTransferHandle* &handle) = 0;

        // Send the notification message to the target
        int send_notification(BackendTransferHandle* handle);

        //Use a handle to progress backend engine and see if a transfer is completed or not
        virtual int check_transfer(BackendTransferHandle* handle) = 0;

        //Force backend engine worker to progress
        virtual int progress(){
            return 0;
        }
};

#endif
