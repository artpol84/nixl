#include <map>
#include "nixl.h"
#include "nixl_descriptors.h"
#include "internal/mem_section.h"
#include "internal/transfer_backend.h"

MemSection:: MemSection (std::string sec_id) {
    section_id = sec_id;
    // For easier navigation. If desired we can put the vectors
    // directly as key and remove them from the class
    sec_map.insert({DRAM_SEG, &dram_mems});
    sec_map.insert({VRAM_SEG, &vram_mems});
    sec_map.insert({BLK_SEG,  &block_mems});
    sec_map.insert({FILE_SEG, &file_mems});
}

// Necessary for RemoteSections
int MemSection::add_backend_handler (BackendEngine *backend) {
    // check for error in get_type
    backend_map.insert({backend->get_type(), backend});
    return 0;
}

// Find a BasicDesc in the section, if available fills the resp based
// on that, and returns the backend that can use the resp
BackendEngine* MemSection::find_query (DescList<BasicDesc> query,
                                         DescList<MetaDesc>& resp) {
    std::vector<Segment> *target_list = sec_map[query.get_type()];
    // We can have preference over backends, for now just looping over
    for (auto & elm : *target_list)
        if (populate(query, resp, elm.first) == 0)
            return elm.first;
    return nullptr;
}

MemSection::~MemSection () {
    dram_mems.clear();
    vram_mems.clear();
    block_mems.clear();
    file_mems.clear();
    sec_map.clear();
    backend_map.clear();
}

DescList<StringDesc> LocalSection::get_StringDesc (Segment input){
    StringDesc element;
    BasicDesc *p = &element;
    DescList<StringDesc> public_meta(input.second.get_type());

    for (int i=0; i<input.second.desc_count(); ++i) {
        *p = (BasicDesc) input.second[i];
        element.metadata = input.first->get_public_data(input.second[i].metadata);
        public_meta.add_desc(element);
    }
    return public_meta;
}


int LocalSection::add_DescList (DescList<BasicDesc> mem_elems,
                                  BackendEngine *backend) {
    memory_type_t mem_type = mem_elems.get_type();
    std::vector<Segment> *target_list = sec_map[mem_type]; // add checks
    int index = -1;

    for (size_t i=0; i<target_list->size(); ++i)
        if ((*target_list)[i].first == backend){
            index = i;
            break;
        }

    int ret;
    MetaDesc out;
    for (auto & elm : mem_elems) {
        // We can add checks for not overlapping previous elements
        // Register_mem is supposed to add the element to the list
        // If necessary we can get the element and add it here.
        // Explained more in ucx register method.

        // ONLY FILLING metadata NOW
        ret = backend->register_mem(elm, mem_type, out.metadata);
        if (ret<0)
            // better to deregister the previous entries added
            return ret;

        // First time adding this backend for this type of memory
        if (index < 0){
            index = target_list->size();
            DescList<MetaDesc> new_desc(mem_type);
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

// Per each BasicDesc, the full region that got registered should be deregistered
int LocalSection::remove_DescList (DescList<MetaDesc> mem_elements,
                                     BackendEngine *backend) {
    memory_type_t mem_type = mem_elements.get_type();
    std::vector<Segment> *target_list = sec_map[mem_type];
    DescList<MetaDesc> * target_descs;
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

    for (auto & elm : mem_elements) {
        index2 = target_descs->get_index(elm);
        if (index2<0)
            // Errorful situation, not sure helpful to deregister the rest, best try
            return -1;

        MetaDesc *p = &(*target_descs)[index2];
        // Backend deregister takes care of metadata destruction,
        backend->deregister_mem ((*p).metadata);

        target_descs->rem_desc(index2);

        if (target_descs->desc_count()==0)
            // No need to remove from backend_map, assuming backends are alive
            target_list->erase(target_list->begin() + index1);
    }

    return 0;
}

// Function that extracts the information for metadata server
std::vector<StringSegment> LocalSection::get_public_data () {
    std::vector<StringSegment> output;
    StringDesc element;

    for (auto &seg : sec_map) // Iterate over mem_type vectors
        for (auto &elm : *seg.second) // Iterate over segments
            output.push_back(std::make_pair(elm.first->get_type(),
                                            get_StringDesc(elm)));

    return output;
}

LocalSection::~LocalSection() {
    for (auto &seg : sec_map) // Iterate over mem_type vectors
        for (auto &elm : *seg.second) // Iterate over segments
            remove_DescList (elm.second, elm.first);
}

int RemoteSection::load_public_data (std::vector<StringSegment> input,
                                      std::string remote_agent) {
    int res;
    for (auto &elm : input) {
        backend_type_t backend_type = elm.first;
        memory_type_t mem_type = elm.second.get_type();
        std::vector<Segment> *target_list = sec_map[mem_type];
        // Check for error of not being in map
        DescList<MetaDesc> temp (mem_type);
        MetaDesc temp2;
        BasicDesc *p = &temp2;

        //the last argument here needs to be a string now

        for(auto &elm2 : elm.second) {
            (*p) = elm2;
            res =  backend_map[backend_type]->load_remote(elm2, temp2.metadata, remote_agent);
            temp.add_desc(temp2);

        }
        if (res<0)
            return res;

        target_list->push_back(std::make_pair(backend_map[backend_type], temp));
    }
    return 0;
}

RemoteSection::~RemoteSection() {
}
