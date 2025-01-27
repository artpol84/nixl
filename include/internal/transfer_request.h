typedef enum {INIT, PROC, DONE, ERR} transfer_state_t;

// Has state, a boolean to start the transfer, as well as populated and verified desc_lists
class TransferRequest {
    private:
        backend_engine* engine;
        desc_list<meta_desc> src; // updated by calling populate in backend engine
        desc_list<meta_desc> dst; // updated by calling populate in corresponding remote_section
        // Other metadata such as pointer to remote_section or device ID and so
    public:
        transfer_state_t state;
};
