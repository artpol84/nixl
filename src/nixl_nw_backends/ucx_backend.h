#include <vector>
#include <cstring>

#include "nixl.h"

#include "utils/ucx/ucx_utils.h"

class ucx_transfer_handle : public backend_transfer_handle {
    private:
	nixl_ucx_req req;

    friend class ucx_engine;
};

class ucx_connection : public backend_conn_meta {
    private:
        std::string remote_agent;
        nixl_ucx_ep ep;

    public:
        // Extra information required for UCX connections

    friend class ucx_engine;
};

// A private metadata has to implement get, and has all the metadata
class ucx_private_metadata : public backend_metadata {
        nixl_ucx_mem mem;
        void *mem_addr;
        size_t mem_addr_size;
    public:
	ucx_private_metadata() : backend_metadata(true) {
	}

        //std::string get() {
		//this is probably not correct
        //    return std::to_string((uint64_t) (&mem));
        //}

    friend class ucx_engine;
};

// A public metadata has to implement put, and only has the remote metadata
class ucx_public_metadata : public backend_metadata {

    public:
        nixl_ucx_rkey rkey;
        ucx_connection conn;

        ucx_public_metadata() : backend_metadata(false) {
	}

        //int set(std::string) {
            // check for string being proper and populate rkey
	//    return 0;
        //}
};

class ucx_engine : backend_engine {
    private:
        ucx_init_params init_params;
        nixl_ucx_worker* uw;
        void *worker_addr;
        size_t worker_size;

        // Map of agent name to saved ucx_connection info
        std::map<std::string, ucx_connection> remote_conn_map;

        // Local private data
	std::vector<ucx_private_metadata> vram_public;
        std::vector<ucx_private_metadata> dram_public;

        // Map of remote uuid to a vector of it's public metadata
        std::map<uuid_t, std::vector<ucx_public_metadata>> vram_map;
        std::map<uuid_t, std::vector<ucx_public_metadata>> dram_map;

        /* TODO: Implement conversion of the arbitrary buffer into a string
           the naive way is to replace each byte with its 2-character hex representation
           We can figure out a better way later */
	std::string _bytes_to_string(void *buf, size_t size) const;
        void *_string_to_bytes(std::string &s, size_t &size);

    public:
	ucx_engine(ucx_init_params* init_params);

	~ucx_engine();

	std::string get_conn_info() const;
	int load_remote_conn_info (std::string remote_agent, std::string remote_conn_info);
	int make_connection(std::string remote_agent);
	int listen_for_connection(std::string remote_agent);

	int register_mem (basic_desc &mem, memory_type_t mem_type, backend_metadata* &out );
	void deregister_mem (backend_metadata* desc);

	int load_remote (string_desc input, backend_metadata* &output, std::string remote_agent);

	int remove_remote (backend_metadata* input);

	//meta_desc instead of basic for local
	int transfer (desc_list<meta_desc> local, desc_list<meta_desc> remote, transfer_op_t op, backend_transfer_handle* &handle);

	int check_transfer (backend_transfer_handle* handle);

	int progress();
};

