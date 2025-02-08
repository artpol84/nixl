#include <map>
#include "nixl.h"
#include "nixl_descriptors.h"
#include "internal/mem_section.h"
#include "internal/transfer_backend.h"

// Might become handy for other methods, so separated it
nixlDescList<nixlMetaDesc>* nixlMemSection::locateDescList (
                                  const memory_type_t mem_type,
                                  const nixlBackendEngine *backend) {

    if (secMap.count(mem_type)==0)
        return nullptr;

    std::vector<nixlSegment>* target_list = secMap[mem_type];
    int index = -1;

    for (size_t i=0; i<target_list->size(); ++i)
        if ((*target_list)[i].first == backend){
            index = i;
            break;
    }

    if (index < 0)
        return nullptr;

    return &((*target_list)[index].second);
}

int nixlMemSection::populate (const nixlDescList<nixlBasicDesc> query,
                              nixlDescList<nixlMetaDesc>& resp,
                              const nixlBackendEngine *backend) {

    nixlDescList<nixlMetaDesc>* found = locateDescList(query.getType(), backend);
    if (found == nullptr)
        return -1;
    else
        return found->populate(query, resp);
}

nixlMemSection:: nixlMemSection () {
    // For easier navigation. If desired we can put the vectors
    // directly as key and remove them from the class
    // This map should be exposed if going the plugin path
    secMap.insert({DRAM_SEG, &dramMems});
    secMap.insert({VRAM_SEG, &vramMems});
    secMap.insert({BLK_SEG,  &blockMems});
    secMap.insert({FILE_SEG, &fileMems});
}

// Necessary for RemoteSections
int nixlMemSection::addBackendHandler (nixlBackendEngine *backend) {
    if (backend == nullptr)
        return -1;
    backendMap.insert({backend->getType(), backend});
    return 0;
}

// Find a nixlBasicDesc in the section, if available fills the resp based
// on that, and returns the backend that can use the resp
nixlBackendEngine* nixlMemSection::findQuery (const nixlDescList<nixlBasicDesc>& query,
                                              nixlDescList<nixlMetaDesc>& resp) {
    std::vector<nixlSegment> *target_list = secMap[query.getType()];
    // We can have preference over backends, for now just looping over
    for (auto & elm : *target_list)
        // If populate fails, it clears the resp before return
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

nixlDescList<nixlStringDesc> nixlLocalSection::getStringDesc (const nixlSegment &input) const{
    nixlStringDesc element;
    nixlBasicDesc *p = &element;
    nixlDescList<nixlStringDesc> output_desclist(input.second.getType(),
                                                 input.second.isUnifiedAddr(),
                                                 input.second.isSorted());

    for (int i=0; i<input.second.descCount(); ++i) {
        *p = (nixlBasicDesc) input.second[i];
        element.metadata = input.first->getPublicData(input.second[i].metadata);
        output_desclist.addDesc(element);
    }
    return output_desclist;
}

// TBD, refactor code by using locateDescList
int nixlLocalSection::addDescList (const nixlDescList<nixlBasicDesc>& mem_elems,
                                   nixlBackendEngine *backend) {
    memory_type_t mem_type = mem_elems.getType();
    if (secMap.count(mem_type)==0)
        return -1;
    std::vector<nixlSegment> *target_list = secMap[mem_type];
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

// TBD, refactor code by using locateDescList
// Per each nixlBasicDesc, the full region that got registered should be deregistered
int nixlLocalSection::remDescList (const nixlDescList<nixlMetaDesc>& mem_elements,
                                   nixlBackendEngine *backend) {
    memory_type_t mem_type = mem_elements.getType();
    if (secMap.count(mem_type)==0)
        return -1;
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

        const nixlMetaDesc *p = &(*target_descs)[index2];
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
std::vector<nixlStringSegment> nixlLocalSection::getPublicData() const {
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
            remDescList (elm.second, elm.first);
}

int nixlRemoteSection::loadPublicData (const std::vector<nixlStringSegment> input,
                                       const std::string remote_agent) {
    int res;
    for (auto &elm : input) {
        backend_type_t backend_type = elm.first;
        memory_type_t mem_type = elm.second.getType();
        if (secMap.count(mem_type)==0)
            return -1;
        std::vector<nixlSegment> *target_list = secMap[mem_type];
        nixlDescList<nixlMetaDesc> temp (mem_type, elm.second.isUnifiedAddr(),
                                         elm.second.isSorted());
        nixlMetaDesc temp2;
        nixlBasicDesc *p = &temp2;

        for(auto &elm2 : elm.second) {
            (*p) = elm2;
            res =  backendMap[backend_type]->loadRemote(elm2, temp2.metadata, remote_agent);
            if (res<0){
                temp.clear(); // Not fully removing the older values, just the problematic one
                return res;
            }
            temp.addDesc(temp2);
        }

        target_list->push_back(std::make_pair(backendMap[backend_type], temp));
    }
    return 0;
}

nixlRemoteSection::~nixlRemoteSection() {
}
