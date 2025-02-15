#ifndef __MEM_SECTION_H
#define __MEM_SECTION_H

#include <vector>
#include <map>
#include <string>
#include "nixl_descriptors.h"
#include "nixl.h"
#include "internal/transfer_backend.h"

typedef std::pair<mem_type_t, backend_type_t> section_key_t;
typedef std::vector<backend_type_t>           backend_list_t;

class nixlMemSection {
    protected:
        // For flexibility can be merged, and have index per each in the
        // memtoBackendMap, which gets updated per insertion/deletion
        backend_list_t dramBackends;
        backend_list_t vramBackends;
        backend_list_t blockBackends;
        backend_list_t fileBackends;

        std::map<mem_type_t,  backend_list_t*>                memToBackendMap;
        std::map<section_key_t,  nixlDescList<nixlMetaDesc>*> sectionMap;
        // Replica of what Agent has, but tiny in size and helps with modularity
        std::map<backend_type_t, nixlBackendEngine*>          backendToEngineMap;

    public:
        nixlMemSection ();

        int populate (const nixlDescList<nixlBasicDesc> &query,
                      const backend_type_t &backend_type,
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
                               const mem_type_t remote_mem_type,
                               const backend_list_t remote_backends,
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
             const std::map<backend_type_t, nixlBackendEngine*> engine_map);

        int loadRemoteData (nixlSerDes* deserializer);

        ~nixlRemoteSection();
};

#endif
