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
        nixlUcxWorker* uw;
        void* workerAddr;
        size_t workerSize;

        notif_list_t notifs;

        // Map of agent name to saved nixlUcxConnection info
        std::map<std::string, nixlUcxConnection> remoteConnMap;

    public:
        nixlUcxEngine(const nixlUcxInitParams* init_params);
        ~nixlUcxEngine();

        std::string getConnInfo() const;
        int loadRemoteConnInfo (const std::string &remote_agent,
                                const std::string &remote_conn_info);
        int makeConnection(const std::string &remote_agent);
        int listenForConnection(const std::string &remote_agent);

        int registerMem (const nixlBasicDesc &mem,
                         const mem_type_t &mem_type,
                         nixlBackendMD* &out);
        void deregisterMem (nixlBackendMD* meta);

        int loadRemoteMD (const nixlStringDesc &input,
                          const mem_type_t &mem_type,
                          const std::string &remote_agent,
                          nixlBackendMD* &output);
        int removeRemoteMD (nixlBackendMD* input);

        // MetaDesc instead of basic for local
        xfer_state_t postXfer (const nixlDescList<nixlMetaDesc> &local,
                               const nixlDescList<nixlMetaDesc> &remote,
                               const xfer_op_t &op,
                               const std::string &remote_agent,
                               const std::string &notif_msg,
                               nixlBackendReqH* &handle);
        xfer_state_t checkXfer (nixlBackendReqH* handle);
        void releaseReqH(nixlBackendReqH* handle);

        int progress();

        // TODO: Should become private
        int sendNotif(const std::string &remote_agent, const std::string &msg);
        int getNotifs(notif_list_t &notif_list);

        //public function for UCX worker to mark connections as connected
        int updateConnMap(const std::string &remote_agent);
        int appendNotif(const std::string &remote_agent, const std::string &notif);
};

#endif
