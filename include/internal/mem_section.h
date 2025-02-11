#ifndef __MEM_SECTION_H
#define __MEM_SECTION_H

#include <vector>
#include <map>
#include <string>
#include "nixl_descriptors.h"
#include "nixl.h"
#include "internal/transfer_backend.h"

typedef std::pair<memory_type_t, backend_type_t> section_key_t;

class nixlMemSection {
    protected:
        // For flexibility can be merged, and have index per each in the
        // memtoBackendMap, which gets updated per insertion/deletion
        std::vector<backend_type_t> dramBackends;
        std::vector<backend_type_t> vramBackends;
        std::vector<backend_type_t> blockBackends;
        std::vector<backend_type_t> fileBackends;

        std::map<memory_type_t,  std::vector<backend_type_t>*> memToBackendMap;
        std::map<section_key_t,  nixlDescList<nixlMetaDesc>*>  sectionMap;
        std::map<backend_type_t, nixlBackendEngine*>           backendToEngineMap;

    public:
        nixlMemSection ();

        // used internally to populate the resp with given query and backend
        int populate (const nixlDescList<nixlBasicDesc>& query,
                      nixlDescList<nixlMetaDesc>& resp,
                      const backend_type_t& backend_type);

        // Find a nixlBasicDesc in the section, if available fills the resp based
        // on that, and returns the backend pointer that can use the resp
        // Might need information from target node to help with the decision
        // Should become const
        nixlBackendEngine* findQuery (const nixlDescList<nixlBasicDesc>& query,
                                      nixlDescList<nixlMetaDesc>& resp);


        virtual ~nixlMemSection () = 0; // Making the class abstract
};

class nixlLocalSection : public nixlMemSection {
    private:
        nixlDescList<nixlStringDesc> getStringDesc (
                                     const nixlBackendEngine *backend,
                                     const nixlDescList<nixlMetaDesc>& d_list) const;
    public:
        int addBackendHandler (nixlBackendEngine *backend);

        int addDescList (const nixlDescList<nixlBasicDesc>& mem_elms,
                         nixlBackendEngine *backend);

        // Each nixlBasicDesc should be same as original registration region
        int remDescList (const nixlDescList<nixlMetaDesc>& mem_elms,
                         nixlBackendEngine *backend);

        int serialize(nixlSerDes* serializer); // should be const;

        inline std::map<backend_type_t, nixlBackendEngine*> getEngineMap() {
            return backendToEngineMap;
        }

        ~nixlLocalSection();
};

class nixlRemoteSection : public nixlMemSection {
    private:
        std::string agent_name;

        // Used for loadRemoteData
        int addDescList (const nixlDescList<nixlStringDesc>& mem_elms,
                         nixlBackendEngine *backend);
    public:
        nixlRemoteSection (std::string& agent_name,
             std::map<backend_type_t, nixlBackendEngine*> engine_map);

        // Should become constructor? And have a separate update call?
        int loadRemoteData (nixlSerDes* deserializer);

        ~nixlRemoteSection();
};

#endif
