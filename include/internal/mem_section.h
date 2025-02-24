#ifndef __MEM_SECTION_H
#define __MEM_SECTION_H

#include <vector>
#include <map>
#include <string>
#include <set>
#include "nixl_descriptors.h"
#include "nixl.h"
#include "internal/transfer_backend.h"

typedef std::pair<nixl_mem_t, nixl_backend_t> section_key_t;
typedef std::set<nixl_backend_t>              backend_set_t;

class nixlMemSection {
    protected:
        std::map<nixl_mem_t,     backend_set_t>               memToBackendMap;
        std::map<section_key_t,  nixlDescList<nixlMetaDesc>*> sectionMap;
        // Replica of what Agent has, but tiny in size and helps with modularity
        std::map<nixl_backend_t, nixlBackendEngine*>          backendToEngineMap;

    public:
        nixlMemSection ();

        int populate (const nixlDescList<nixlBasicDesc> &query,
                      const nixl_backend_t &nixl_backend,
                      nixlDescList<nixlMetaDesc> &resp) const;

        virtual ~nixlMemSection () = 0; // Making the class abstract
};

class nixlLocalSection : public nixlMemSection {
    private:
        nixlDescList<nixlStringDesc> getStringDesc (
                               const nixlBackendEngine* backend,
                               const nixlDescList<nixlMetaDesc> &d_list) const;
    public:
        int addBackendHandler (nixlBackendEngine* backend);

        int addDescList (const nixlDescList<nixlBasicDesc> &mem_elms,
                         nixlBackendEngine* backend);

        // Each nixlBasicDesc should be same as original registration region
        int remDescList (const nixlDescList<nixlMetaDesc> &mem_elms,
                         nixlBackendEngine* backend);

        // Find a nixlBasicDesc in the section, if available fills the resp based
        // on that, and returns the backend pointer that can use the resp
        nixlBackendEngine* findQuery (const nixlDescList<nixlBasicDesc> &query,
                               const nixl_mem_t remote_nixl_mem,
                               const backend_set_t remote_backends,
                               nixlDescList<nixlMetaDesc> &resp) const;

        int serialize(nixlSerDes* serializer) const;

        ~nixlLocalSection();
};

class nixlRemoteSection : public nixlMemSection {
    private:
        std::string agentName;

        int addDescList (const nixlDescList<nixlStringDesc> &mem_elms,
                         nixlBackendEngine *backend);
    public:
        nixlRemoteSection (const std::string &agent_name,
             const std::map<nixl_backend_t, nixlBackendEngine*> engine_map);

        int loadRemoteData (nixlSerDes* deserializer);

        ~nixlRemoteSection();
};

#endif
