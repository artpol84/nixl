#ifndef __TRANSFER_REQUEST_H_
#define __TRANSFER_REQUEST_H_

// Contains pointers to corresponding backend engine and its handler, and populated
// and verified DescLists, and other state and metadata needed for a NIXL transfer
class nixlXferReqH {
    public:
        nixlBackendEngine          *engine;
        nixlBackendReqH            *backendHandle;

        nixlDescList<nixlMetaDesc> *initiatorDescs;
        nixlDescList<nixlMetaDesc> *targetDescs;

        std::string                remoteAgent;
        std::string                notifMsg;

        transfer_op_t              backendOp;
        transfer_state_t           state;

        inline nixlXferReqH() {}
        inline ~nixlXferReqH() {
            delete initiatorDescs;
            delete targetDescs;
            engine->releaseReqH(backendHandle);
        }
};

#endif
