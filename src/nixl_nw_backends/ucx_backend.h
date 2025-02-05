#include <vector>
#include <cstring>

#include "nixl.h"

#include "utils/ucx/ucx_utils.h"

class nixlUcxTransferHandle : public BackendTransferHandle {
    private:
        nixlUcxReq req;

    friend class nixlUcxEngine;
};

class nixlUcxConnection : public BackendConnMD {
    private:
        std::string remote_agent;
        nixlUcxEp ep;

    public:
        // Extra information required for UCX connections

    friend class nixlUcxEngine;
};

// A private metadata has to implement get, and has all the metadata
class nixlUcxPrivateMetadata : public BackendMetadata {
    private:
        nixlUcxMem mem;
        std::string rkey_str;

    public:
        nixlUcxPrivateMetadata() : BackendMetadata(true) {
        }

        std::string get() {
            return rkey_str;
        }

    friend class nixlUcxEngine;
};

// A public metadata has to implement put, and only has the remote metadata
class nixlUcxPublicMetadata : public BackendMetadata {

    public:
        nixlUcxRkey rkey;
        nixlUcxConnection conn;

        nixlUcxPublicMetadata() : BackendMetadata(false) {}

        // int set(std::string) {
        //     // check for string being proper and populate rkey
        //     return 0;
        // }
};

class nixlUcxEngine : BackendEngine {
    private:
        nixlUcxInitParams init_params;
        nixlUcxWorker* uw;
        void *worker_addr;
        size_t worker_size;

        // Map of agent name to saved nixlUcxConnection info
        std::map<std::string, nixlUcxConnection> remote_conn_map;

        // Local private data
        //std::vector<nixlUcxPrivateMetadata> vram_public;
        //std::vector<nixlUcxPrivateMetadata> dram_public;

        // Map of remote string to a vector of it's public metadata
        //std::map<std::string, std::vector<nixlUcxPublicMetadata>> vram_map;
        //std::map<std::string, std::vector<nixlUcxPublicMetadata>> dram_map;

        /* TODO: Implement conversion of the arbitrary buffer into a string
           the naive way is to replace each byte with its 2-character hex representation
           We can figure out a better way later */
        std::string _bytes_to_string(void *buf, size_t size) const;
            void *_string_to_bytes(std::string &s, size_t &size);

    public:
        nixlUcxEngine(nixlUcxInitParams* init_params);

        ~nixlUcxEngine();

        std::string get_public_data(BackendMetadata* &meta) const;

        std::string get_conn_info() const;
        int load_remote_conn_info (std::string remote_agent, std::string remote_conn_info);
        int make_connection(std::string remote_agent);
        int listen_for_connection(std::string remote_agent);

        int register_mem (BasicDesc &mem, memory_type_t mem_type, BackendMetadata* &out );
        void deregister_mem (BackendMetadata* desc);

        int load_remote (StringDesc input, BackendMetadata* &output, std::string remote_agent);

        int remove_remote (BackendMetadata* input);

        //MetaDesc instead of basic for local
        int transfer (DescList<MetaDesc> local, DescList<MetaDesc> remote,
                      transfer_op_t op, std::string notif_msg, BackendTransferHandle* &handle);

        int check_transfer (BackendTransferHandle* handle);

        int progress();
};

