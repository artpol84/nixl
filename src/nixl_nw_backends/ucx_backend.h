#ifndef __UCX_BACKEND_H
#define __UCX_BACKEND_H

#include <vector>
#include <cstring>
#include <iostream>

#include "nixl.h"

#include "utils/ucx/ucx_utils.h"
#include "utils/data_structures/list_elem.h"

typedef enum {CONN_CHECK, NOTIF_STR} ucx_cb_op_t;

struct nixl_ucx_am_hdr {
    ucx_cb_op_t op;
};

class nixlUcxTransferHandle : public nixlBackendReqH {
    private:
        nixlUcxReq req;

    friend class nixlUcxEngine;
};

class nixlUcxConnection : public nixlBackendConnMD {
    private:
        std::string remoteAgent;
        nixlUcxEp ep;
        volatile bool connected;

    public:
        // Extra information required for UCX connections

    friend class nixlUcxEngine;
};

// A private metadata has to implement get, and has all the metadata
class nixlUcxPrivateMetadata : public nixlBackendMD {
    private:
        nixlUcxMem mem;
        std::string rkeyStr;

    public:
        nixlUcxPrivateMetadata() : nixlBackendMD(true) {
        }

        ~nixlUcxPrivateMetadata(){
        }

        std::string get() const {
            return rkeyStr;
        }

    friend class nixlUcxEngine;
};

// A public metadata has to implement put, and only has the remote metadata
class nixlUcxPublicMetadata : public nixlBackendMD {

    public:
        nixlUcxRkey rkey;
        nixlUcxConnection conn;

        nixlUcxPublicMetadata() : nixlBackendMD(false) {}

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

        notif_list_t notifs;

        // Map of agent name to saved nixlUcxConnection info
        std::map<std::string, nixlUcxConnection> remoteConnMap;

    public:
        nixlUcxEngine(nixlUcxInitParams* initParams);

        ~nixlUcxEngine();

        std::string getConnInfo() const;
        int loadRemoteConnInfo (std::string remote_agent, std::string remote_conn_info);
        int makeConnection(std::string remote_agent);
        int listenForConnection(std::string remote_agent);

        int registerMem (const nixlBasicDesc &mem, memory_type_t mem_type, nixlBackendMD* &out);
        void deregisterMem (nixlBackendMD* desc);

        int loadRemote (nixlStringDesc input, nixlBackendMD* &output, std::string remote_agent);

        int removeRemote (nixlBackendMD* input);

        // MetaDesc instead of basic for local
        transfer_state_t transfer (nixlDescList<nixlMetaDesc> local,
                                    nixlDescList<nixlMetaDesc> remote,
                                    transfer_op_t op,
                                    std::string remote_agent,
                                    std::string notif_msg,
                                    nixlBackendReqH* &handle);

        transfer_state_t checkTransfer (nixlBackendReqH* handle);

        void releaseReqH(nixlBackendReqH* handle);

        int progress();

        // TODO: Should become private
        int sendNotif(std::string remote_agent, std::string msg);

        int getNotifs(notif_list_t &notif_list);

        //public function for UCX worker to mark connections as connected
        int updateConnMap(std::string remote_agent);
        int appendNotif(std::string remote_agent, std::string notif);
};

#endif
