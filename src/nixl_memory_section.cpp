#include <map>
#include "nixl.h"
#include "nixl_descriptors.h"
#include "internal/mem_section.h"
#include "internal/transfer_backend.h"

nixlMemSection:: nixlMemSection (std::string agent_name) {
    this->agentName = agent_name;
    // For easier navigation. If desired we can put the vectors
    // directly as key and remove them from the class
    secMap.insert({DRAM_SEG, &dramMems});
    secMap.insert({VRAM_SEG, &vramMems});
    secMap.insert({BLK_SEG,  &blockMems});
    secMap.insert({FILE_SEG, &fileMems});
}

// Necessary for RemoteSections
int nixlMemSection::addBackendHandler (nixlBackendEngine *backend) {
    // check for error in get_type
    backendMap.insert({backend->getType(), backend});
    return 0;
}

// Find a nixlBasicDesc in the section, if available fills the resp based
// on that, and returns the backend that can use the resp
nixlBackendEngine* nixlMemSection::findQuery (nixlDescList<nixlBasicDesc> query,
                                              nixlDescList<nixlMetaDesc>& resp) {
    std::vector<nixlSegment> *target_list = secMap[query.get_type()];
    // We can have preference over backends, for now just looping over
    for (auto & elm : *target_list)
        if (populate(query, resp, elm.first) == 0)
            return elm.first;
    return nullptr;
}

nixlMemSection::~nixlMemSection () {
    dramMems.clear();
    vramMems.clear();
    blockMems.clear();
    fileMems.clear();
    secMap.clear();
    backendMap.clear();
}

nixlDescList<nixlStringDesc> nixlLocalSection::getStringDesc (nixlSegment input){
    nixlStringDesc element;
    nixlBasicDesc *p = &element;
    nixlDescList<nixlStringDesc> public_meta(input.second.get_type());

    for (int i=0; i<input.second.descCount(); ++i) {
        *p = (nixlBasicDesc) input.second[i];
        element.metadata = input.first->getPublicData(input.second[i].metadata);
        public_meta.addDesc(element);
    }
    return public_meta;
}


int nixlLocalSection::addDescList (nixlDescList<nixlBasicDesc> mem_elems,
                                       nixlBackendEngine *backend) {
    memory_type_t mem_type = mem_elems.get_type();
    std::vector<nixlSegment> *target_list = secMap[mem_type]; // add checks
    int index = -1;

    for (size_t i=0; i<target_list->size(); ++i)
        if ((*target_list)[i].first == backend){
            index = i;
            break;
        }

    int ret;
    nixlMetaDesc out;
    for (auto & elm : mem_elems) {
        // We can add checks for not overlapping previous elements
        // RegisterMem is supposed to add the element to the list
        // If necessary we can get the element and add it here.
        // Explained more in ucx register method.

        // ONLY FILLING metadata NOW
        ret = backend->registerMem(elm, mem_type, out.metadata);
        if (ret<0)
            // better to deregister the previous entries added
            return ret;

        // First time adding this backend for this type of memory
        if (index < 0){
            index = target_list->size();
            nixlDescList<nixlMetaDesc> new_desc(mem_type);
            new_desc.addDesc(out);
            target_list->push_back(std::make_pair(backend, new_desc));
            // Overrides are fine, assuming single instance
            backendMap.insert({backend->getType(),backend});
        } else {
            // If sorting is desired and not using set, we should do it here
            (*target_list)[index].second.addDesc(out);
        }
    }
    return 0;
}

// Per each nixlBasicDesc, the full region that got registered should be deregistered
int nixlLocalSection::removeDescList (nixlDescList<nixlMetaDesc> mem_elements,
                                          nixlBackendEngine *backend) {
    memory_type_t mem_type = mem_elements.get_type();
    std::vector<nixlSegment> *target_list = secMap[mem_type];
    nixlDescList<nixlMetaDesc> * target_descs;
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
        index2 = target_descs->getIndex(elm);
        if (index2<0)
            // Errorful situation, not sure helpful to deregister the rest, best try
            return -1;

        nixlMetaDesc *p = &(*target_descs)[index2];
        // Backend deregister takes care of metadata destruction,
        backend->deregisterMem ((*p).metadata);

        target_descs->remDesc(index2);

        if (target_descs->descCount()==0)
            // No need to remove from backendMap, assuming backends are alive
            target_list->erase(target_list->begin() + index1);
    }

    return 0;
}

// Function that extracts the information for metadata server
std::vector<nixlStringSegment> nixlLocalSection::getPublicData () {
    std::vector<nixlStringSegment> output;
    nixlStringDesc element;

    for (auto &seg : secMap) // Iterate over mem_type vectors
        for (auto &elm : *seg.second) // Iterate over segments
            output.push_back(std::make_pair(elm.first->getType(),
                                            getStringDesc(elm)));

    return output;
}

nixlLocalSection::~nixlLocalSection() {
    for (auto &seg : secMap) // Iterate over mem_type vectors
        for (auto &elm : *seg.second) // Iterate over segments
            removeDescList (elm.second, elm.first);
}

int nixlRemoteSection::loadPublicData (std::vector<nixlStringSegment> input,
                                     std::string remote_agent) {
    int res;
    for (auto &elm : input) {
        backend_type_t backend_type = elm.first;
        memory_type_t mem_type = elm.second.get_type();
        std::vector<nixlSegment> *target_list = secMap[mem_type];
        // Check for error of not being in map
        nixlDescList<nixlMetaDesc> temp (mem_type);
        nixlMetaDesc temp2;
        nixlBasicDesc *p = &temp2;

        for(auto &elm2 : elm.second) {
            (*p) = elm2;
            res =  backendMap[backend_type]->loadRemote(elm2, temp2.metadata, remote_agent);
            temp.addDesc(temp2);

        }
        if (res<0)
            return res;

        target_list->push_back(std::make_pair(backendMap[backend_type], temp));
    }
    return 0;
}

nixlRemoteSection::~nixlRemoteSection() {
}
