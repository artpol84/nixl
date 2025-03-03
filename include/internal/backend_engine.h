#ifndef __TRANSFER_BACKEND_H_
#define __TRANSFER_BACKEND_H_

#include <mutex>
#include <string>
#include "nixl_descriptors.h"
#include "nixl_types.h"
#include "utils/sys/nixl_time.h"

// Might be removed to be decided by backend, or changed to high
// level direction or so.
typedef std::vector<std::pair<std::string, std::string>> notif_list_t;

// A base class to point to backend initialization data

// User doesn't know about fields such as local_agent but can access it
// after the backend is initialized by agent. If we needed to make it private
// from the user, we should make nixlBackendEngine/nixlAgent friend classes.
class nixlBackendInitParams {
    public:
        std::string    localAgent;
        bool           enableProgTh;
        nixlTime::us_t pthrDelay;

        virtual nixl_backend_t getType() const = 0;

        virtual ~nixlBackendInitParams() = default;
};

// Pure virtual class to have a common pointer type
class nixlBackendReqH {
public:
    nixlBackendReqH() { }
    ~nixlBackendReqH() { }
};

// Pure virtual class to have a common pointer type for different backendMD.
class nixlBackendMD {
    protected:
        bool isPrivateMD;

    public:
        nixlBackendMD(bool isPrivate){
            isPrivateMD = isPrivate;
        }

        virtual ~nixlBackendMD(){
        }
};

// Each backend can have different connection requirement
// This class would include the required information to make
// a connection to a remote node. Note that local information
// is passed during the constructor and through BackendInitParams
class nixlBackendConnMD {
  public:
    // And some other details
    std::string dstIpAddress;
    uint16_t    dstPort;
};

// A pointer required to a metadata object for backends next to each BasicDesc
class nixlMetaDesc : public nixlBasicDesc {
  public:
        // To be able to point to any object
        nixlBackendMD* metadataP;

        // Reuse parent constructor without the metadata pointer
        using nixlBasicDesc::nixlBasicDesc;

        nixlMetaDesc() : nixlBasicDesc() { metadataP = nullptr; }

        // No serializer or deserializer, using parent not to expose the metadata

        inline friend bool operator==(const nixlMetaDesc &lhs, const nixlMetaDesc &rhs) {
            return (((nixlBasicDesc)lhs == (nixlBasicDesc)rhs) &&
                          (lhs.metadataP == rhs.metadataP));
        }

        // Main use case is to take the BasicDesc from another object, so just
        // the metadata part is separately copied here, used in DescList
        inline void copyMeta (const nixlMetaDesc &meta) {
            this->metadataP = meta.metadataP;
        }

        inline void print(const std::string &suffix) const {
            nixlBasicDesc::print(", Backend ptr val: " +
                                 std::to_string((uintptr_t)metadataP) + suffix);
        }
};

typedef nixlDescList<nixlMetaDesc> nixl_meta_dlist_t;

// Base backend engine class for different backend implementaitons
class nixlBackendEngine {
    private:
        nixl_backend_t backendType;

    public:
        nixlBackendEngine (const nixlBackendInitParams* init_params) {
            this->backendType = init_params->getType();
            this->localAgent  = init_params->localAgent;
            this->initErr     = false;
        }
        
        virtual ~nixlBackendEngine () = default;

        bool getInitErr() { return initErr; }

        // The support function determine which methods are necessary by the child backend, and
        // if they're called by mistake, they will return error if not implemented by backend.

        // Determines if a backend supports remote operations
        virtual bool supportsRemote () const = 0;

        // Determines if a backend supports local operations
        virtual bool supportsLocal () const = 0;

        // Determines if a backend supports sending notifications. Related methods are not
        // pure virtual, and return errors, as parent shouldn't call if supportsNotif is false.
        virtual bool supportsNotif () const = 0;

        // Determines if a backend supports progress thread.
        virtual bool supportsProgTh () const = 0;

        nixl_backend_t getType () const { return backendType; }

    protected:
        std::string    localAgent;
        bool           initErr;

        // *** Pure virtual methods that need to be implemented by any backend *** //

        // Register and deregister local memory
        virtual nixl_status_t registerMem (const nixlStringDesc &mem,
                                           const nixl_mem_t &nixl_mem,
                                           nixlBackendMD* &out) = 0;
        virtual void deregisterMem (nixlBackendMD* meta) = 0;

        // Make connection to a remote node identified by the name into loaded conn infos
        // Child might just return 0, if making proactive connections are not necessary.
        // An agent might need to connect to itself for local operations.
        virtual nixl_status_t connect(const std::string &remote_agent) = 0;
        virtual nixl_status_t disconnect(const std::string &remote_agent) = 0;

        // Remove loaded local or remtoe metadata for target
        virtual nixl_status_t unloadMD (nixlBackendMD* input) = 0;

        // Posting a request, which returns populates the async handle.
        virtual nixl_xfer_state_t postXfer (const nixl_meta_dlist_t &local,
                                            const nixl_meta_dlist_t &remote,
                                            const nixl_xfer_op_t &operation,
                                            const std::string &remote_agent,
                                            const std::string &notif_msg,
                                            nixlBackendReqH* &handle) = 0;

        // Use a handle to progress backend engine and see if a transfer is completed or not
        virtual nixl_xfer_state_t checkXfer(nixlBackendReqH* handle) = 0;

        //Backend aborts the transfer if necessary, and destructs the relevant objects
        virtual void releaseReqH(nixlBackendReqH* handle) = 0;


        // *** Needs to be implemented if supportsRemote() is true *** //

        // Gets serialized form of public metadata
        virtual std::string getPublicData (const nixlBackendMD* meta) const { return ""; };

        // Provide the required connection info for remote nodes, should be non-empty
        virtual std::string getConnInfo() const { return ""; }

        // Deserialize from string the connection info for a remote node, if supported
        // The generated data should be deleted in nixlBackendEngine destructor
        virtual nixl_status_t loadRemoteConnInfo (const std::string &remote_agent,
                                                  const std::string &remote_conn_info) {
            return NIXL_ERR_BACKEND;
        }

        // Load remtoe metadata, if supported.
        virtual nixl_status_t loadRemoteMD (const nixlStringDesc &input,
                                            const nixl_mem_t &nixl_mem,
                                            const std::string &remote_agent,
                                            nixlBackendMD* &output) {
            return NIXL_ERR_BACKEND;
        }


        // *** Needs to be implemented if supportsLocal() is true *** //

        // Provide the target metadata necessary for local operations, if supported
        virtual nixl_status_t loadLocalMD (nixlBackendMD* input,
                                           nixlBackendMD* &output) {
            return NIXL_ERR_BACKEND;
        }


        // *** Needs to be implemented if supportsNotif() is true *** //

        // Populate an empty received notif list. Elements are released within backend then.
        virtual int getNotifs(notif_list_t &notif_list) { return NIXL_ERR_BACKEND; }

        // Generates a standalone notification, not bound to a transfer.
        virtual nixl_status_t genNotif(const std::string &remote_agent, const std::string &msg) {
            return NIXL_ERR_BACKEND;
        }


        // *** Needs to be implemented if supportsProgTh() is true *** //

        // Force backend engine worker to progress.
        virtual int progress() { return 0; }


    friend class nixlAgent;
    friend class memSection;
    friend class nixlXferReqH;
    friend class nixlAgentData;
    friend class nixlLocalSection;
    friend class nixlRemoteSection;
    friend class nixlBackendTester;
};

class nixlBackendTester {

    private:
        nixlBackendEngine *engine;

    public:
        nixlBackendTester(nixlBackendEngine* &new_engine) {
            engine = new_engine;
        }
        ~nixlBackendTester() {
        }

        nixl_backend_t getType () { 
            return engine->getType(); 
        }

        bool supportsLocal () { 
            return engine->supportsLocal();
        }
        bool supportsRemote () { 
            return engine->supportsRemote();
        }
        bool supportsNotif () { 
            return engine->supportsNotif();
        }
        bool supportsProgTh () { 
            return engine->supportsProgTh();
        }
        nixl_status_t registerMem (const nixlStringDesc &mem,
                                   const nixl_mem_t &nixl_mem,
                                   nixlBackendMD* &out) { 
            return engine->registerMem(mem, nixl_mem, out);
        }
        void deregisterMem (nixlBackendMD* meta) {
            engine->deregisterMem(meta);
        }
        std::string getPublicData (const nixlBackendMD* meta) {
            return engine->getPublicData(meta);
        }
        std::string getConnInfo() {
            return engine->getConnInfo();
        }
        nixl_status_t loadRemoteConnInfo (const std::string &remote_agent,
                                          const std::string &remote_conn_info){
            return engine->loadRemoteConnInfo(remote_agent, remote_conn_info);
        }
        nixl_status_t connect(const std::string &remote_agent){
            return engine->connect(remote_agent);
        }
        nixl_status_t disconnect(const std::string &remote_agent){
            return engine->disconnect(remote_agent);
        }
        nixl_status_t unloadMD (nixlBackendMD* input){
            return engine->unloadMD(input);
        }
        nixl_status_t loadRemoteMD (const nixlStringDesc &input,
                                    const nixl_mem_t &nixl_mem,
                                    const std::string &remote_agent,
                                    nixlBackendMD* &output){
            return engine->loadRemoteMD(input, nixl_mem, remote_agent, output);
        }
        nixl_status_t loadLocalMD (nixlBackendMD* input,
                                   nixlBackendMD* &output) {
            return engine->loadLocalMD(input, output);
        }
        nixl_xfer_state_t postXfer (const nixlDescList<nixlMetaDesc> &local,
                                    const nixlDescList<nixlMetaDesc> &remote,
                                    const nixl_xfer_op_t &operation,
                                    const std::string &remote_agent,
                                    const std::string &notif_msg,
                                    nixlBackendReqH* &handle){
            return engine->postXfer(local, remote, operation, remote_agent, notif_msg, handle);
        }
        nixl_xfer_state_t checkXfer(nixlBackendReqH* handle){
            return engine->checkXfer(handle);
        }
        void releaseReqH(nixlBackendReqH* handle){
            engine->releaseReqH(handle);
        }
        int getNotifs(notif_list_t &notif_list) { 
            return engine->getNotifs(notif_list);
        }
        nixl_status_t genNotif(const std::string &remote_agent, const std::string &msg) {
            return engine->genNotif(remote_agent, msg);
        }
        int progress() { 
            return engine->progress(); 
        }
};

#endif
