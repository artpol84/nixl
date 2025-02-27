#ifndef __TRANSFER_REQUEST_H_
#define __TRANSFER_REQUEST_H_

// Contains pointers to corresponding backend engine and its handler, and populated
// and verified DescLists, and other state and metadata needed for a NIXL transfer
class nixlXferReqH {
    private:
        nixlBackendEngine*          engine;
        nixlBackendReqH*            backendHandle;

        nixlDescList<nixlMetaDesc>* initiatorDescs;
        nixlDescList<nixlMetaDesc>* targetDescs;

        std::string                 remoteAgent;
        std::string                 notifMsg;

        nixl_xfer_op_t              backendOp;
        nixl_xfer_state_t           state;

    public:
        inline nixlXferReqH() {}
        inline ~nixlXferReqH() {
            delete initiatorDescs;
            delete targetDescs;
            if(backendHandle != NULL) engine->releaseReqH(backendHandle);
        }

    friend class nixlAgent;
};

class nixlXferSideH {
    private:
        nixlDescList<nixlMetaDesc>* descs;

        nixlBackendEngine*          engine;
        std::string                 remoteAgent;
        bool                        isLocal;

    public:
        inline nixlXferSideH() {}
        inline ~nixlXferSideH() {
            delete descs;
        }

    friend class nixlAgent;
};

#endif
