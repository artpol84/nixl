#ifndef __MEM_SECTION_H
#define __MEM_SECTION_H

#include <vector>
#include <map>
#include <string>
#include "nixl_descriptors.h"
#include "nixl.h"
#include "internal/transfer_backend.h"

typedef std::pair<nixlBackendEngine*, nixlDescList<nixlMetaDesc>> nixlSegment;
typedef std::pair<backend_type_t, nixlDescList<nixlStringDesc>> nixlStringSegment;
typedef std::pair<backend_type_t, std::string> nixlStringConnMD;

// Merging the nixlDescList to say what backends support each and the corresponding
// metadata gets really messy. Hard to find if a nixlDescList in a submitted request
// is in which backend, while at the end of the day only a single backend will
// process it. So separating the nixlDescLists per backend.

// We don't want to search the descriptors once, and extract the requried metadata
// separately, so merged the calls by asking the backends to populate the desired
// descriptor and see if they succeed.
class nixlMemSection {
    protected:
        // Per each type of memory, pointer to a backend that supports it, and
        // which index in that backend. Not keeping any actual memory in MemSection.
        // When a call to send data to metadata server arrives, we get the required
        // info from each backend and send them together.

        // If we want, based on the memory types in descs we can have priority
        // over which backend we choose, and then ask the backend if it supports
        // a dsec_list.

        std::string agentName;

        std::vector<nixlSegment> dramMems;
        std::vector<nixlSegment> vramMems;
        std::vector<nixlSegment> blockMems;
        std::vector<nixlSegment> fileMems;

        std::map<memory_type_t, std::vector<nixlSegment>*> secMap;
        std::map<backend_type_t, nixlBackendEngine*> backendMap;

        nixlDescList<nixlMetaDesc>* locateDescList (memory_type_t mem_type,
                                                    nixlBackendEngine *backend) {

                std::vector<nixlSegment> *target_list = secMap[mem_type];
                int index = 0;

                for (size_t i=0; i<target_list->size(); ++i)
                    if ((*target_list)[i].first == backend){
                        index = i;
                        break;
                }

                if (index < 0)
                    return nullptr;

                return &((*target_list)[index].second);
        }

        int populate (nixlDescList<nixlBasicDesc> query, 
                      nixlDescList<nixlMetaDesc>& resp,
                      nixlBackendEngine *backend) {

                nixlDescList<nixlMetaDesc>* found = locateDescList(query.get_type(), backend);
                if (found == nullptr)
                    return -1;
                else
                    return found->populate(query,resp);
        }

    public:
        nixlMemSection (std::string agent_name);

        // Necessary for RemoteSections
        int addBackendHandler (nixlBackendEngine *backend);

        // Find a nixlBasicDesc in the section, if available fills the resp based
        // on that, and returns the backend that can use the resp
        nixlBackendEngine* findQuery (nixlDescList<nixlBasicDesc> query,
                                      nixlDescList<nixlMetaDesc>& resp);
        ~nixlMemSection ();
};

class nixlLocalSection : public nixlMemSection {
    private:
        nixlDescList<nixlStringDesc> getStringDesc (nixlSegment input);

    public:
        int addDescList (nixlDescList<nixlBasicDesc> mem_elms,
                         nixlBackendEngine *backend);

        // Per each nixlBasicDesc, the full region that got registered should be deregistered
        int removeDescList (nixlDescList<nixlMetaDesc> mem_elements,
                            nixlBackendEngine *backend);

        // Function that extracts the information for metadata server
        std::vector<nixlStringSegment> getPublicData ();

        ~nixlLocalSection();
};

class nixlRemoteSection : public nixlMemSection {
    public:
        int loadPublicData (std::vector<nixlStringSegment> input,
                            std::string remote_agent);

        ~nixlRemoteSection();
};

#endif
