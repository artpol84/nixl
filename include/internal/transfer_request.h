#ifndef __TRANSFER_REQUEST_H_
#define __TRANSFER_REQUEST_H_


// Has state, a boolean to start the transfer, as well as populated and verified DescLists
class nixlTransferRequest {
    public:
    // private: For now public, later to be improved
        nixlBackendEngine          *engine;
        nixlDescList<nixlMetaDesc> *initiator_descs;
        nixlDescList<nixlMetaDesc> *target_descs;
        nixlBackendTransferHandle  *backend_handle;
        std::string                notif_msg;
        transfer_op_t              backend_op;

        transfer_state_t state;

    inline nixlTransferRequest(){}
    inline ~nixlTransferRequest(){
        delete initiator_descs;
        delete target_descs;
        delete backend_handle;
    }
};

#endif
