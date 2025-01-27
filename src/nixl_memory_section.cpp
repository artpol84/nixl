#include <map>
#include "nixl.h"
#include "nixl_descriptors.h"
#include "internal/mem_section.h"
#include "internal/transfer_backend.h"

mem_section:: mem_section (uuid_t sec_id) {
    uuid_copy(section_id, sec_id);
    // For easier navigation. If desired we can put the vectors
    // directly as key and remove them from the class
    sec_map.insert({DRAM_SEG, &dram_mems});
    sec_map.insert({VRAM_SEG, &vram_mems});
    sec_map.insert({BLK_SEG,  &block_mems});
    sec_map.insert({FILE_SEG, &file_mems});
}

// Necessary for remote_sections
int mem_section::add_backend_handler (backend_engine *backend) {
    // check for error in get_type
    backend_map.insert({backend->get_type(), backend});
    return 0;
}

// Find a basic_desc in the section, if available fills the resp based
// on that, and returns the backend that can use the resp
backend_engine* mem_section::find_query (desc_list<basic_desc> query,
                                         desc_list<meta_desc>& resp) {
    std::vector<segment> *target_list = sec_map[query.get_type()];
    // We can have preference over backends, for now just looping over
    for (auto & elm : *target_list)
        if (populate(query, resp, elm.first) == 0)
            return elm.first;
    return nullptr;
}

mem_section::~mem_section () {
    dram_mems.clear();
    vram_mems.clear();
    block_mems.clear();
    file_mems.clear();
    sec_map.clear();
    backend_map.clear();
}

desc_list<string_desc> local_section::get_string_desc (segment input){
    string_desc element;
    basic_desc *p = &element;
    desc_list<string_desc> public_meta(input.second.get_type());

    for (int i=0; i<input.second.get_desc_count(); ++i) {
        *p = (basic_desc) input.second.descs[i];
        element.metadata = input.first->get_public_data(input.second.descs[i]);
        public_meta.add_desc(element);
    }
    return public_meta;
}


int local_section::add_desc_list (desc_list<basic_desc> mem_elems,
                                  backend_engine *backend) {
    memory_type_t mem_type = mem_elems.get_type();
    std::vector<segment> *target_list = sec_map[mem_type]; // add checks
    int index = -1;

    for (size_t i=0; i<target_list->size(); ++i)
        if ((*target_list)[i].first == backend){
            index = i;
            break;
        }

    int ret;
    meta_desc out;
    for (auto & elm : mem_elems.descs) {
        // We can add checks for not overlapping previous elements
        // Register_mem is supposed to add the element to the list
        // If necessary we can get the element and add it here.
        // Explained more in ucx register method.
        ret = backend->register_mem(elm, mem_type, out);
        if (ret<0)
            // better to deregister the previous entries added
            return ret;

        // First time adding this backend for this type of memory
        if (index < 0){
            index = target_list->size();
            desc_list<meta_desc> new_desc(mem_type);
            new_desc.add_desc(out);
            target_list->push_back(std::make_pair(backend, new_desc));
            // Overrides are fine, assuming single instance
            backend_map.insert({backend->get_type(),backend});
        } else {
            // If sorting is desired and not using set, we should do it here
            (*target_list)[index].second.add_desc(out);
        }
    }
    return 0;
}

// Per each basic_desc, the full region that got registered should be deregistered
int local_section::remove_desc_list (desc_list<meta_desc> mem_elements,
                                     backend_engine *backend) {
    memory_type_t mem_type = mem_elements.get_type();
    std::vector<segment> *target_list = sec_map[mem_type];
    desc_list<meta_desc> * target_descs;
    int index1 = -1;
    int index2 = -1;

    for (size_t i=0; i<target_list->size(); ++i) {
        if ((*target_list)[i].first == backend){
            index1 = i;
            break;
        }
    }

    if (index1<0) // backend not found
        return -1;

    target_descs = &(*target_list)[index1].second;

    for (auto & elm : mem_elements.descs) {
        index2 = target_descs->get_index(elm);
        if (index2<0)
            // Errorful situation, not sure helpful to deregister the rest, best try
            return -1;

        meta_desc *p = &target_descs->descs[index2];
        // Backend deregister takes care of metadata destruction,
        backend->deregister_mem (*p);

        target_descs->descs.erase(target_descs->descs.begin() + index2);

        if (target_descs->get_desc_count()==0)
            // No need to remove from backend_map, assuming backends are alive
            target_list->erase(target_list->begin() + index1);
    }

    return 0;
}

// Function that extracts the information for metadata server
std::vector<string_segment> local_section::get_public_data () {
    std::vector<string_segment> output;
    string_desc element;

    for (auto &seg : sec_map) // Iterate over mem_type vectors
        for (auto &elm : *seg.second) // Iterate over segments
            output.push_back(std::make_pair(elm.first->get_type(),
                                            get_string_desc(elm)));

    return output;
}

local_section::~local_section() {
    for (auto &seg : sec_map) // Iterate over mem_type vectors
        for (auto &elm : *seg.second) // Iterate over segments
            remove_desc_list (elm.second, elm.first);
}

int remote_section::load_public_data (std::vector<string_segment> input,
                                      uuid_t remote_id) {
    int res;
    for (auto &elm : input) {
        backend_type_t backend_type = elm.first;
        memory_type_t mem_type = elm.second.get_type();
        std::vector<segment> *target_list = sec_map[mem_type];
        // Check for error of not being in map
        desc_list<meta_desc> temp (mem_type);
        res =  backend_map[backend_type]->load_remote(elm.second, temp, remote_id);
        if (res<0)
            return res;

        target_list->push_back(std::make_pair(backend_map[backend_type], temp));
    }
    return 0;
}

remote_section::~remote_section() {
}
