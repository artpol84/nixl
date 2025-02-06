#ifndef __MEM_SECTION_H
#define __MEM_SECTION_H

#include <vector>
#include <map>
#include <string>
#include "nixl_descriptors.h"
#include "nixl.h"
#include "internal/transfer_backend.h"

typedef std::pair<BackendEngine*, DescList<MetaDesc>> Segment;
typedef std::pair<backend_type_t, DescList<StringDesc>> StringSegment;
typedef std::pair<backend_type_t, std::string> StringConnMD;

// Merging the DescList to say what backends support each and the corresponding
// metadata gets really messy. Hard to find if a DescList in a submitted request
// is in which backend, while at the end of the day only a single backend will
// process it. So separating the DescLists per backend.

// We don't want to search the descriptors once, and extract the requried metadata
// separately, so merged the calls by asking the backends to populate the desired
// descriptor and see if they succeed.
class MemSection {
    protected:
        // Per each type of memory, pointer to a backend that supports it, and
        // which index in that backend. Not keeping any actual memory in MemSection.
        // When a call to send data to metadata server arrives, we get the required
        // info from each backend and send them together.

        // If we want, based on the memory types in descs we can have priority
        // over which backend we choose, and then ask the backend if it supports
        // a dsec_list.

        std::string agent_name;

        std::vector<Segment> dram_mems;
        std::vector<Segment> vram_mems;
        std::vector<Segment> block_mems;
        std::vector<Segment> file_mems;

        std::map<memory_type_t, std::vector<Segment>*> sec_map;
        std::map<backend_type_t, BackendEngine*> backend_map;

        DescList<MetaDesc>* locate_DescList (memory_type_t mem_type,
                                             BackendEngine *backend) {

                std::vector<Segment> *target_list = sec_map[mem_type];
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

        int populate (DescList<BasicDesc> query, DescList<MetaDesc>& resp,
                                                 BackendEngine *backend) {

                DescList<MetaDesc>* found = locate_DescList(query.get_type(),
                                                            backend);
                if (found == nullptr)
                    return -1;
                else
                    return found->populate(query,resp);
        }

    public:
        MemSection (std::string agent_name);

        // Necessary for RemoteSections
        int add_backend_handler (BackendEngine *backend);

        // Find a BasicDesc in the section, if available fills the resp based
        // on that, and returns the backend that can use the resp
        BackendEngine* find_query (DescList<BasicDesc> query,
                                   DescList<MetaDesc>& resp);
        ~MemSection ();
};

class LocalSection : public MemSection {
    private:
        DescList<StringDesc> get_StringDesc (Segment input);

    public:
        int add_DescList (DescList<BasicDesc> mem_elms,
                          BackendEngine *backend);

        // Per each BasicDesc, the full region that got registered should be deregistered
        int remove_DescList (DescList<MetaDesc> mem_elements,
                             BackendEngine *backend);

        // Function that extracts the information for metadata server
        std::vector<StringSegment> get_public_data ();

        ~LocalSection();
};

class RemoteSection : public MemSection {
    public:
        int load_public_data (std::vector<StringSegment> input,
                              std::string remote_agent);

        ~RemoteSection();
};

#endif
