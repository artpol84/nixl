#include <vector>
#include <cstring>

#include "nixl.h"

#include "utils/ucx/ucx_utils.h"

class nixlUcxTransferHandle : public nixlBackendTransferHandle {
    private:
        nixlUcxReq req;

    friend class nixlUcxEngine;
};

class nixlUcxConnection : public nixlBackendConnMD {
    private:
        std::string remoteAgent;
        nixlUcxEp ep;

    public:
        // Extra information required for UCX connections

    friend class nixlUcxEngine;
};

// A private metadata has to implement get, and has all the metadata
class nixlUcxPrivateMetadata : public nixlBackendMetadata {
    private:
        nixlUcxMem mem;
        std::string rkeyStr;

    public:
        nixlUcxPrivateMetadata() : nixlBackendMetadata(true) {
        }

        ~nixlUcxPrivateMetadata(){
        }

        std::string get() const {
            return rkeyStr;
        }

    friend class nixlUcxEngine;
};

// A public metadata has to implement put, and only has the remote metadata
class nixlUcxPublicMetadata : public nixlBackendMetadata {

    public:
        nixlUcxRkey rkey;
        nixlUcxConnection conn;

        nixlUcxPublicMetadata() : nixlBackendMetadata(false) {}

        ~nixlUcxPublicMetadata(){
        }

        // int set(std::string) {
        //     // check for string being proper and populate rkey
        //     return 0;
        // }
};

class nixlUcxEngine : nixlBackendEngine {
    private:
        nixlUcxInitParams initParams;
        nixlUcxWorker* uw;
        void *workerAddr;
        size_t workerSize;

        // Map of agent name to saved nixlUcxConnection info
        std::map<std::string, nixlUcxConnection> remoteConnMap;

        // Local private data
        //std::vector<nixlUcxPrivateMetadata> vram_public;
        //std::vector<nixlUcxPrivateMetadata> dram_public;

        // Map of remote string to a vector of it's public metadata
        //std::map<std::string, std::vector<nixlUcxPublicMetadata>> vram_map;
        //std::map<std::string, std::vector<nixlUcxPublicMetadata>> dram_map;

        /* TODO: Implement conversion of the arbitrary buffer into a string
           the naive way is to replace each byte with its 2-character hex representation
           We can figure out a better way later */
        std::string _bytesToString(void *buf, size_t size) const;
        void *_stringToBytes(std::string &s, size_t &size);

    public:
        nixlUcxEngine(nixlUcxInitParams* initParams);

        ~nixlUcxEngine();

        std::string getConnInfo() const;
        int loadRemoteConnInfo (std::string remote_agent, std::string remote_conn_info);
        int makeConnection(std::string remote_agent);
        int listenForConnection(std::string remote_agent);

        int registerMem (nixlBasicDesc &mem, memory_type_t mem_type, nixlBackendMetadata* &out );
        void deregisterMem (nixlBackendMetadata* desc);

        int loadRemote (nixlStringDesc input, nixlBackendMetadata* &output, std::string remote_agent);

        int removeRemote (nixlBackendMetadata* input);

        //MetaDesc instead of basic for local
        int transfer (nixlDescList<nixlMetaDesc> local, nixlDescList<nixlMetaDesc> remote, transfer_op_t op, std::string notif_msg, nixlBackendTransferHandle* &handle);

        int checkTransfer (nixlBackendTransferHandle* handle);

        int progress();
};

