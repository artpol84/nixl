#include "nixl.h"
// A private metadata has to implement get, and has all the metadata
class ucx_dram_private_metadata : public backend_metadata {
    public:
        uint32_t lkey;
        void*    registration_handle; // example, if want to keep some handle
        uint32_t rkey;

        ucx_dram_private_metadata
        : backend_metadata(true) {}

        std::string get() {
            return std::to_string(rkey);
        }
};

// A public metadata has to implement put, and only has the remote metadata
class ucx_dram_public_metadata : public backend_metadata {
    public:

        ucx_dram_public_metadata
        : backend_metadata(false) {}

        int set(std::string) {
            // check for string being proper and populate rkey
        };
};

class ucx_vram_private_metadata : public backend_metadata {
    public:
        uint32_t lkey;
        void*    registration_handle; // example, if want to keep some handle
        uint32_t rkey;

        ucx_vram_private_metadata
        : backend_metadata(true) {}

        std::string get() {
            return std::to_string(rkey);
        }
};

class ucx_vram_public_metadata : public backend_metadata {
    public:
        uint32_t rkey;

        ucx_vram_public_metadata
        : backend_metadata(false) {}

        int set(std::string){
            // check for string being proper and populate rkey
        };
};

class ucx_connection : public backend_conn_meta {
    public:
        // Extra information required for UCX connections
};

class ucx_engine : public_backend {
    private:
        ucx_init_params init_params;
        ucx_connection  conn_info;

        // Map of agent name to saved ucx_connection info
        std::map<std::string, ucx_connection> remote_conn_info;

        // Local private data
        std::vector<ucx_vram_private_metadata> vram_public;
        std::vector<ucx_vram_private_metadata> dram_public;

        // Map of remote uuid to a vector of it's public metadata
        std::map<uuid_t, std::vector<ucx_vram_public_metadata>> vram_public;
        std::map<uuid_t, std::vector<ucx_vram_public_metadata>> dram_public;
};

int ucx_engine::register_mem (basic_desc &mem, memory_type_t mem_type,
                              meta_desc &out) {
    basic_desc *p = &out;
    *p = mem;
    // Register the element with UCX, based on mem_type decide
    // which metadata to instantiate. Then fill the metadata of out
    return 0; // Or errors
}

void ucx_engine::deregister_mem (meta_desc desc) {
    // based on the memory address and private metadata deregister.
    // And remove the corresponding metadata object
}

ucx_engine::ucx_engine (ucx_init_params init_params)
: backend_engine (UCX, &init_params) {
    // The initialization process and populates conn_info too
    // If init params are not useful, we can remove the member from the base class
}

// Through parent destructor the unregister will be called.
ucx_engine::~ucx_engine () {
    // per registered memory deregisters it, which removes the corresponding metadata too
    // parent destructor takes care of the desc list
    // For remote metadata, they should be removed here
}

// To be extended to return an async handler
int ucx_engine::transfer (desc_list<basic_desc> local,
                          desc_list<meta_desc> remote,
                          transfer_op_t operation) {
    // Finds corresponding metadata in its registered_desc_lists which has the
    // More complete form of desc_list<local_desc>.
    // Does final checks for the transfer and initiates the transfer
}

// They might change if we use an external connection manager.
std::string ucx_engine::get_conn_info() {
    // return string form of conn_info;
}

int ucx_engine::load_remote_conn_info (std::string remote_agent,
                                       std::string remote_conn_info) {
    // From string form of remote_conn_info populate the remote_conn_info
}

// Make connection to a remote node accompanied by required info.
int ucx_engine::make_connection(std::string remote_agent) {
    // use the string to find the entry in remote_conn_info to initiate a connection
}

int ucx_engine::listen_for_connection(std::string remote_agent) {
    // use the string to find the entry in remote_conn_info and listen to it
}

// To be cleaned up
int ucx_engine::load_remote (desc_list<string_desc>& input,
                             desc_list<meta_desc>& output,
                             uuid_t remote_id) {
    meta_desc element;
    basic_desc *p = &element;
    if (input.get_type()==DRAM) {
        ucx_dram_public_metadata new_metadata;
        for (auto & desc : input.descs){
            *p = (basic_desc) desc; // typecasting should be fixed
            new_metadata.set(desc.metadata);
            remote_dram_meta.push_back(new_metadata);
            element.meta_data = (backend_metadata *) &remote_dram_meta.back();
            output.add_desc(element);
        }
    } else if (input.get_type()==VRAM) {
        ucx_vram_public_metadata new_metadata;
        for (auto & desc : input.descs){
            *p = (basic_desc) desc;
            new_metadata.set(desc.metadata);
            remote_vram_meta.push_back(new_metadata);
            element.meta_data = (backend_metadata *) &remote_vram_meta.back();
            output.add_desc(element);
        }
    } else {
        return -1;
    }
}

int ucx_engine::remove_remote (desc_list<meta_desc>& input,
                               uuid_t remote_id) {
    for (auto & desc : input.descs){
        // remove the elements from remote_dram/vram_meta
        // Since they're added per input string list, there
        // should be only 1 instance of each. Not needing
        // unique_ptr for now.
    }
}
