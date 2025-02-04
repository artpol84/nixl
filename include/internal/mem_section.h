#ifndef __MEM_SECTION_H
#define __MEM_SECTION_H

#include <vector>
#include <map>
#include <string>
#include "nixl_descriptors.h"
#include "nixl.h"
#include "internal/transfer_backend.h"

typedef std::pair<backend_engine*, desc_list<meta_desc>  > segment;
typedef std::pair<backend_type_t,  desc_list<string_desc>> string_segment;
typedef std::pair<backend_type_t,  std::string>            string_conn_md;

// Merging the desc_list to say what backends support each and the corresponding
// metadata gets really messy. Hard to find if a desc_list in a submitted request
// is in which backend, while at the end of the day only a single backend will
// process it. So separating the desc_lists per backend.

// We don't want to search the descriptors once, and extract the requried metadata
// separately, so merged the calls by asking the backends to populate the desired
// descriptor and see if they succeed.
class mem_section {
    protected:
        // Per each type of memory, pointer to a backend that supports it, and
        // which index in that backend. Not keeping any actual memory in mem_section.
        // When a call to send data to metadata server arrives, we get the required
        // info from each backend and send them together.

        // If we want, based on the memory types in descs we can have priority
        // over which backend we choose, and then ask the backend if it supports
        // a dsec_list.

        std::string section_id;

        std::vector<segment> dram_mems;
        std::vector<segment> vram_mems;
        std::vector<segment> block_mems;
        std::vector<segment> file_mems;

        std::map<memory_type_t,  std::vector<segment>*> sec_map;
        std::map<backend_type_t, backend_engine*>       backend_map;

        desc_list<meta_desc>* locate_desc_list (memory_type_t mem_type,
                                                backend_engine *backend)
        {
                std::vector<segment> *target_list = sec_map[mem_type];
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

        int populate (desc_list<basic_desc> query, desc_list<meta_desc>& resp,
                                                   backend_engine *backend) {
                desc_list<meta_desc>* found = locate_desc_list(query.get_type(),
                                                               backend);
                if (found == nullptr)
                    return -1;
                else
                return found->populate(query,resp);
        }

    public:
        mem_section (std::string sec_id);

        // Necessary for remote_sections
        int add_backend_handler (backend_engine *backend);

        // Find a basic_desc in the section, if available fills the resp based
        // on that, and returns the backend that can use the resp
        backend_engine* find_query (desc_list<basic_desc> query,
                                    desc_list<meta_desc>& resp);
        ~mem_section ();
};

class local_section : public mem_section {
    private:
        desc_list<string_desc> get_string_desc (segment input);

    public:
        int add_desc_list (desc_list<basic_desc> mem_elms,
                           backend_engine *backend);

        // Per each basic_desc, the full region that got registered should be deregistered
        int remove_desc_list (desc_list<meta_desc> mem_elements,
                              backend_engine *backend);

        // Function that extracts the information for metadata server
        std::vector<string_segment> get_public_data ();

        ~local_section();
};

class remote_section : public mem_section {
    public:
        int load_public_data (std::vector<string_segment> input,
                              std::string remote_agent);

        ~remote_section();
};

#endif
