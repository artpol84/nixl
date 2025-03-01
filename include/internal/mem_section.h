#ifndef __MEM_SECTION_H
#define __MEM_SECTION_H

#include <vector>
#include <map>
#include <string>
#include <set>
#include "nixl_descriptors.h"
#include "nixl.h"
#include "internal/backend_engine.h"

typedef std::pair<nixl_mem_t, nixl_backend_t> section_key_t;
typedef std::set<nixl_backend_t>              backend_set_t;

class nixlMemSection {
    protected:
        std::map<nixl_mem_t,     backend_set_t>      memToBackendMap;
        std::map<section_key_t,  nixl_meta_dlist_t*> sectionMap;
        // Replica of what Agent has, but tiny in size and helps with modularity
        std::map<nixl_backend_t, nixlBackendEngine*> backendToEngineMap;

    public:
        nixlMemSection ();

        nixl_status_t populate (const nixl_xfer_dlist_t &query,
                                const nixl_backend_t &nixl_backend,
                                nixl_meta_dlist_t &resp) const;

        virtual ~nixlMemSection () = 0; // Making the class abstract
};

class nixlLocalSection : public nixlMemSection {
    private:
        nixl_reg_dlist_t getStringDesc (
                               const nixlBackendEngine* backend,
                               const nixl_meta_dlist_t &d_list) const;
    public:
        nixl_status_t addBackendHandler (nixlBackendEngine* backend);

        nixl_status_t addDescList (const nixl_reg_dlist_t &mem_elms,
                                   nixlBackendEngine* backend,
                                   nixl_meta_dlist_t &remote_self);

        // Each nixlBasicDesc should be same as original registration region
        nixl_status_t remDescList (const nixl_meta_dlist_t &mem_elms,
                                   nixlBackendEngine* backend);

        // Find a nixlBasicDesc in the section, if available fills the resp based
        // on that, and returns the backend pointer that can use the resp
        nixlBackendEngine* findQuery (const nixl_xfer_dlist_t &query,
                                      const nixl_mem_t &remote_nixl_mem,
                                      const backend_set_t &remote_backends,
                                      nixl_meta_dlist_t &resp) const;

        nixl_status_t serialize(nixlSerDes* serializer) const;

        ~nixlLocalSection();
};

class nixlRemoteSection : public nixlMemSection {
    private:
        std::string agentName;

        nixl_status_t addDescList (
                           const nixl_reg_dlist_t &mem_elms,
                           nixlBackendEngine *backend);
    public:
        nixlRemoteSection (const std::string &agent_name,
                           const std::map<nixl_backend_t,
                           nixlBackendEngine*> &engine_map);

        nixl_status_t loadRemoteData (nixlSerDes* deserializer);

        // When adding self as a remote agent for local operations
        nixl_status_t loadLocalData (const nixl_meta_dlist_t& mem_elms,
                                     nixlBackendEngine* backend);
        ~nixlRemoteSection();
};

#endif
