#ifndef __UCX_BACKEND_H
#define __UCX_BACKEND_H

#include <vector>
#include <cstring>
#include <iostream>
#include <thread>
#include <mutex>

#include "nixl.h"

#include "utils/ucx/ucx_utils.h"
#include "utils/data_structures/list_elem.h"

typedef enum {CONN_CHECK, NOTIF_STR, DISCONNECT} ucx_cb_op_t;

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

        /* UCX data */
        nixlUcxContext* uc;
        nixlUcxWorker* uw;
        void* workerAddr;
        size_t workerSize;

        /* Progress thread data */
        volatile bool pthr_stop, pthr_active;
        int no_sync_iters;
        std::thread pthr;

        /* Notifications */
        notif_list_t notifMainList;
        std::mutex  notifMtx;
        notif_list_t notifPthrPriv, notifPthr;

        // Map of agent name to saved nixlUcxConnection info
        std::map<std::string, nixlUcxConnection> remoteConnMap;

		class nixlUcxBckndReq : public nixlLinkElem<nixlUcxBckndReq>, public nixlBackendReqH {
            private:
                int _completed;
            public:
                std::string *amBuffer;

                nixlUcxBckndReq() : nixlLinkElem(), nixlBackendReqH() {
                    _completed = 0;
                    amBuffer = NULL;
                }
            
                ~nixlUcxBckndReq() {
                    _completed = 0;
                    if (amBuffer) {
                        delete amBuffer;
                    }
                }
            
                bool is_complete() { return _completed; }
                void completed() { _completed = 1; }
        };

        // Threading infrastructure
        //   TODO: move the thread management one outside of NIXL common infra
        void progressFunc();
        void startProgressThread();
        void stopProgressThread();
        bool isProgressThread(){
            return (std::this_thread::get_id() == pthr.get_id());
        }
    
        // Request management
        static void _requestInit(void *request);
        static void _requestFini(void *request);
        void requestReset(nixlUcxBckndReq *req) {
            _requestInit((void *)req);
        }

        // Connection helper
        static ucs_status_t
        connectionCheckAmCb(void *arg, const void *header,
                            size_t header_length, void *data,
                            size_t length,
                            const ucp_am_recv_param_t *param);

        static ucs_status_t
        connectionTermAmCb(void *arg, const void *header,
                            size_t header_length, void *data,
                            size_t length,
                            const ucp_am_recv_param_t *param);

        // Notifications
        static ucs_status_t notifAmCb(void *arg, const void *header,
                                      size_t header_length, void *data,
                                      size_t length,
                                      const ucp_am_recv_param_t *param);
        xfer_state_t notifSendPriv(const std::string &remote_agent,
                                   const std::string &msg, nixlUcxReq &req);
        void notifProgress();
        void notifCombineHelper(notif_list_t &src, notif_list_t &tgt);
        void notifProgressCombineHelper(notif_list_t &src, notif_list_t &tgt);


        // Data transfer (priv)
        int retHelper(xfer_state_t ret, nixlUcxBckndReq *head, nixlUcxReq &req);
    public:
        nixlUcxEngine(const nixlUcxInitParams* init_params);
        ~nixlUcxEngine();

        bool supportsNotif () const { return true; }

        /* Object management */
        std::string getConnInfo() const;
        int loadRemoteConnInfo (const std::string &remote_agent,
                                const std::string &remote_conn_info);
        int connect(const std::string &remote_agent);
        int disconnect(const std::string &remote_agent);

        int registerMem (const nixlBasicDesc &mem,
                         const mem_type_t &mem_type,
                         nixlBackendMD* &out);
        void deregisterMem (nixlBackendMD* meta);

        int loadRemoteMD (const nixlStringDesc &input,
                          const mem_type_t &mem_type,
                          const std::string &remote_agent,
                          nixlBackendMD* &output);
        int removeRemoteMD (nixlBackendMD* input);

        // Data transfer
        xfer_state_t postXfer (const nixlDescList<nixlMetaDesc> &local,
                               const nixlDescList<nixlMetaDesc> &remote,
                               const xfer_op_t &op,
                               const std::string &remote_agent,
                               const std::string &notif_msg,
                               nixlBackendReqH* &handle);
        xfer_state_t checkXfer (nixlBackendReqH* handle);
        void releaseReqH(nixlBackendReqH* handle);

        int progress();

        int getNotifs(notif_list_t &notif_list);
        int genNotif(const std::string &remote_agent, const std::string &msg);

        //public function for UCX worker to mark connections as connected
        int checkConn(const std::string &remote_agent);
        int endConn(const std::string &remote_agent);
};

#endif
