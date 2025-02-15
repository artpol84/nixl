#include <map>
#include "nixl.h"
#include "nixl_descriptors.h"
#include "internal/mem_section.h"
#include "internal/transfer_backend.h"

/*** Class nixlMemSection implementation ***/

nixlMemSection:: nixlMemSection () {
    // This map should be exposed if going the plugin path
    memToBackendMap.insert({DRAM_SEG, &dramBackends});
    memToBackendMap.insert({VRAM_SEG, &vramBackends});
    memToBackendMap.insert({BLK_SEG,  &blockBackends});
    memToBackendMap.insert({FILE_SEG, &fileBackends});
}

int nixlMemSection::populate (const nixlDescList<nixlBasicDesc> &query,
                              const backend_type_t &backend_type,
                              nixlDescList<nixlMetaDesc> &resp) const {
    if (query.getType() != resp.getType())
        return -1;
    section_key_t sec_key = std::make_pair(query.getType(), backend_type);
    auto it = sectionMap.find(sec_key);
    if (it==sectionMap.end())
        return -1;
    else
        return it->second->populate(query, resp);
}

// Eventhough it's pure virtual, base class needs a destructor,
// Which can be used for the final clean up, after child classes
// have released the elements pointed to.
nixlMemSection::~nixlMemSection () {
    dramBackends.clear();
    vramBackends.clear();
    blockBackends.clear();
    fileBackends.clear();
    memToBackendMap.clear();
    sectionMap.clear();
    backendToEngineMap.clear();
}

/*** Class nixlLocalSection implementation ***/

nixlDescList<nixlStringDesc> nixlLocalSection::getStringDesc (
                             const nixlBackendEngine* backend,
                             const nixlDescList<nixlMetaDesc> &d_list) const {
    nixlStringDesc element;
    nixlBasicDesc *p = &element;
    nixlDescList<nixlStringDesc> output_desclist(d_list.getType(),
                                                 d_list.isUnifiedAddr(),
                                                 d_list.isSorted());

    for (int i=0; i<d_list.descCount(); ++i) {
        *p = (nixlBasicDesc) d_list[i];
        element.metadata = backend->getPublicData(d_list[i].metadata);
        output_desclist.addDesc(element);
    }
    return output_desclist;
}

int nixlLocalSection::addBackendHandler (nixlBackendEngine* backend) {
    if (backend == nullptr)
        return -1;
    // Agent has already checked for not being the same type of backend
    backendToEngineMap[backend->getType()] = backend;
    return 0;
}

// Calls into backend engine to register the memories in the desc list
int nixlLocalSection::addDescList (const nixlDescList<nixlBasicDesc> &mem_elms,
                                   nixlBackendEngine* backend) {

    if (backend == nullptr)
        return -1;
    // Find the MetaDesc list, or add it to the map
    mem_type_t     mem_type     = mem_elms.getType();
    backend_type_t backend_type = backend->getType();
    section_key_t sec_key = std::make_pair(mem_type, backend_type);
    auto it = sectionMap.find(sec_key);
    if (it==sectionMap.end()) { // New desc list
        sectionMap[sec_key] = new nixlDescList<nixlMetaDesc>(
                                  mem_type, mem_elms.isUnifiedAddr(), true);
        memToBackendMap[mem_type]->push_back(backend_type);
    }
    nixlDescList<nixlMetaDesc> *target = sectionMap[sec_key];

    // Add entries to the target list
    nixlMetaDesc out;
    nixlBasicDesc *p = &out;
    int ret;
    for (int i=0; i<mem_elms.descCount(); ++i) {
        ret = backend->registerMem(mem_elms[i], mem_type, out.metadata);
        if (ret<0) {
            for (int j=0; j<i; ++j) {
                int index = target->getIndex(mem_elms[j]);
                backend->deregisterMem((*target)[index].metadata);
            }
            return ret;
        }
        *p = mem_elms[i]; // Copy the basic desc part
        target->addDesc(out);
    }
    return 0;
}

// Per each nixlBasicDesc, the full region that got registered should be deregistered
int nixlLocalSection::remDescList (const nixlDescList<nixlMetaDesc> &mem_elms,
                                   nixlBackendEngine *backend) {
    if (backend == nullptr)
        return -1;
    mem_type_t     mem_type     = mem_elms.getType();
    backend_type_t backend_type = backend->getType();
    section_key_t sec_key = std::make_pair(mem_type, backend_type);
    auto it = sectionMap.find(sec_key);
    if (it==sectionMap.end())
        return -1;
    nixlDescList<nixlMetaDesc> *target = it->second;

    for (auto & elm : mem_elms) {
        int index = target->getIndex(elm);
        // Errorful situation, not sure helpful to deregister the rest,
        // registering back what was deregistered is not meaningful.
        // Can be secured by going through all the list then deregister
        if (index<0)
            return -1;

        backend->deregisterMem ((*target)[index].metadata);
        target->remDesc(index);
    }

    if (target->descCount()==0){
        delete target;
        sectionMap.erase(sec_key);
        std::vector<backend_type_t> *backend_list = memToBackendMap[mem_type];
        backend_list->erase(std::remove(backend_list->begin(),
             backend_list->end(), backend_type), backend_list->end());
    }

    return 0;
}

nixlBackendEngine* nixlLocalSection::findQuery(
                       const nixlDescList<nixlBasicDesc> &query,
                       const mem_type_t remote_mem_type,
                       const backend_list_t remote_backends,
                       nixlDescList<nixlMetaDesc> &resp) const {

    auto it = memToBackendMap.find(query.getType());
    if (it==memToBackendMap.end())
        return nullptr;
    std::vector<backend_type_t> *supported_backends = it->second;

    // Decision making based on supported local backends for this
    // memory type, supported remote backends and remote memory type
    // or here we loop through and find first local match. The more
    // complete option (overkill) is to try all possible scenarios and
    // see which populates on both side are successful and then decide

    for (auto & elm : *supported_backends) {
        // If populate fails, it clears the resp before return
        if (populate(query, elm, resp) == 0)
            return backendToEngineMap.at(elm);
    }
    return nullptr;
}

int nixlLocalSection::serialize(nixlSerDes* serializer) const {
    int ret;
    size_t seg_count = sectionMap.size();
    backend_type_t backend_type;

    ret = serializer->addBuf("nixlSecElms", &seg_count, sizeof(seg_count));
    if (ret) return ret;

    for (auto &seg : sectionMap) {
        backend_type = seg.first.second;
        nixlDescList<nixlStringDesc> s_desc = getStringDesc(
                    backendToEngineMap.at(backend_type), *seg.second);

        ret = serializer->addBuf("bknd", &backend_type, sizeof(backend_type));
        if (ret) return ret;
        ret = s_desc.serialize(serializer);
        if (ret) return ret;
    }

    return 0;
}

nixlLocalSection::~nixlLocalSection() {
    for (auto &seg : sectionMap)
        remDescList(*seg.second, backendToEngineMap[seg.first.second]);
}

/*** Class nixlRemoteSection implementation ***/

nixlRemoteSection::nixlRemoteSection (const std::string &agent_name,
                const std::map<backend_type_t, nixlBackendEngine*> engine_map) {
    this->agentName    = agent_name;
    backendToEngineMap = engine_map;
}

int nixlRemoteSection::addDescList (const nixlDescList<nixlStringDesc>& mem_elms,
                                    nixlBackendEngine* backend) {
    // Less checks than LocalSection, as it's private and called by loadRemoteData
    // In RemoteSection, if we support updates, value for a key gets overwritten
    // Without it, its corrupt data, we keep the last option without raising an error
    mem_type_t     mem_type     = mem_elms.getType();
    backend_type_t backend_type = backend->getType();
    section_key_t sec_key = std::make_pair(mem_type, backend_type);
    sectionMap[sec_key] = new nixlDescList<nixlMetaDesc>(
                              mem_type, mem_elms.isUnifiedAddr(), true);
    memToBackendMap[mem_type]->push_back(backend_type);
    nixlDescList<nixlMetaDesc> *target = sectionMap[sec_key];

    // Add entries to the target list
    nixlMetaDesc out;
    nixlBasicDesc *p = &out;
    int ret;
    for (int i=0; i<mem_elms.descCount(); ++i) {
        ret = backend->loadRemoteMD(mem_elms[i], mem_type, agentName, out.metadata);
        // In case of errors, no need to remove the previous entries
        // Agent will delete the full object.
        if (ret<0)
            return ret;
        *p = mem_elms[i]; // Copy the basic desc part
        target->addDesc(out);
    }
    return 0;
}

int nixlRemoteSection::loadRemoteData (nixlSerDes* deserializer) {
    int ret;
    size_t seg_count;
    backend_type_t backend_type;

    ret = deserializer->getBuf("nixlSecElms", &seg_count, sizeof(seg_count));
    if (ret) return ret;

    for (size_t i=0; i<seg_count; ++i) {
        // In case of errors, no need to remove the previous entries
        // Agent will delete the full object.
        ret = deserializer->getBuf("bknd", &backend_type, sizeof(backend_type));
        if (ret) return ret;
        nixlDescList<nixlStringDesc> s_desc(deserializer);
        if (s_desc.descCount()==0) // can be used for entry removal in future
            return -1;
        if (addDescList(s_desc, backendToEngineMap[backend_type]) < 0)
            return -1;
    }
    return 0;
}

nixlRemoteSection::~nixlRemoteSection() {
    backend_type_t backend_type;
    nixlDescList<nixlMetaDesc> *m_desc;

    for (auto &seg : sectionMap) {
        backend_type = seg.first.second;
        m_desc = seg.second;
        for (auto & elm : *m_desc)
            backendToEngineMap[backend_type]->removeRemoteMD(elm.metadata);
        delete m_desc;
    }
    // nixlMemSection destructor will clean up the rest
}
