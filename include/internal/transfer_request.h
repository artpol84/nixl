#ifndef __TRANSFER_REQUEST_H_
#define __TRANSFER_REQUEST_H_

typedef enum {INIT, PROC, DONE, ERR} transfer_state_t;

// Has state, a boolean to start the transfer, as well as populated and verified DescLists
class TransferRequest {
    private:
        BackendEngine* engine;
        DescList<MetaDesc> src; // updated by calling populate in backend engine
        DescList<MetaDesc> dst; // updated by calling populate in corresponding RemoteSection
        // Other metadata such as pointer to RemoteSection or device ID and so
    public:
        transfer_state_t state;
};

#endif
