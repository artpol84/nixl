#include <map>
#include "nixl.h"
#include "nixl_descriptors.h"
#include "internal/mem_section.h"
#include "internal/transfer_backend.h"

/*** Class nixlMemSection implementation ***/

// For full polymorphic, we can make it dynamic allocation.
nixlMemSection::nixlMemSection () {
    memToBackendMap[DRAM_SEG] = std::set<nixl_backend_t>();
    memToBackendMap[VRAM_SEG] = std::set<nixl_backend_t>();
    memToBackendMap[BLK_SEG]  = std::set<nixl_backend_t>();
    memToBackendMap[FILE_SEG] = std::set<nixl_backend_t>();
}

// It's pure virtual, but base also class needs a destructor due to its memebrs.
nixlMemSection::~nixlMemSection () {}

int nixlMemSection::populate (const nixlDescList<nixlBasicDesc> &query,
                              const nixl_backend_t &nixl_backend,
                              nixlDescList<nixlMetaDesc> &resp) const {
    if (query.getType() != resp.getType())
        return -1;
    section_key_t sec_key = std::make_pair(query.getType(), nixl_backend);
    auto it = sectionMap.find(sec_key);
    if (it==sectionMap.end())
        return -1;
    else
        return it->second->populate(query, resp);
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
    nixl_mem_t     nixl_mem     = mem_elms.getType();
    nixl_backend_t nixl_backend = backend->getType();
    section_key_t  sec_key      = std::make_pair(nixl_mem, nixl_backend);

    auto it = sectionMap.find(sec_key);
    if (it==sectionMap.end()) { // New desc list
        sectionMap[sec_key] = new nixlDescList<nixlMetaDesc>(
                                  nixl_mem, mem_elms.isUnifiedAddr(), true);
        memToBackendMap[nixl_mem].insert(nixl_backend);
    }
    nixlDescList<nixlMetaDesc> *target = sectionMap[sec_key];

    // Add entries to the target list
    nixlMetaDesc out;
    nixlBasicDesc *p = &out;
    int ret;
    for (int i=0; i<mem_elms.descCount(); ++i) {
        // TODO: check if fully covered, continue. If partially covered
        //       split and get new metadata for the new part
        ret = backend->registerMem(mem_elms[i], nixl_mem, out.metadata);
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
    nixl_mem_t     nixl_mem     = mem_elms.getType();
    nixl_backend_t nixl_backend = backend->getType();
    section_key_t sec_key = std::make_pair(nixl_mem, nixl_backend);
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
        memToBackendMap[nixl_mem].erase(nixl_backend);
    }

    return 0;
}

nixlBackendEngine* nixlLocalSection::findQuery(
                       const nixlDescList<nixlBasicDesc> &query,
                       const nixl_mem_t remote_nixl_mem,
                       const backend_set_t remote_backends,
                       nixlDescList<nixlMetaDesc> &resp) const {

    auto it = memToBackendMap.find(query.getType());
    if (it==memToBackendMap.end())
        return nullptr;

    // Decision making based on supported local backends for this
    // memory type, supported remote backends and remote memory type
    // or here we loop through and find first local match. The more
    // complete option (overkill) is to try all possible scenarios and
    // see which populates on both side are successful and then decide

    for (auto & elm : it->second) {
        // If populate fails, it clears the resp before return
        if (populate(query, elm, resp) == 0)
            return backendToEngineMap.at(elm);
    }
    return nullptr;
}

int nixlLocalSection::serialize(nixlSerDes* serializer) const {
    int ret;
    size_t seg_count = sectionMap.size();
    nixl_backend_t nixl_backend;

    ret = serializer->addBuf("nixlSecElms", &seg_count, sizeof(seg_count));
    if (ret) return ret;

    for (auto &seg : sectionMap) {
        nixl_backend = seg.first.second;
        nixlDescList<nixlStringDesc> s_desc = getStringDesc(
                    backendToEngineMap.at(nixl_backend), *seg.second);

        ret = serializer->addBuf("bknd", &nixl_backend, sizeof(nixl_backend));
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
                const std::map<nixl_backend_t, nixlBackendEngine*> engine_map) {
    this->agentName    = agent_name;
    backendToEngineMap = engine_map;
}

int nixlRemoteSection::addDescList (const nixlDescList<nixlStringDesc>& mem_elms,
                                    nixlBackendEngine* backend) {
    // Less checks than LocalSection, as it's private and called by loadRemoteData
    // In RemoteSection, if we support updates, value for a key gets overwritten
    // Without it, its corrupt data, we keep the last option without raising an error
    nixl_mem_t     nixl_mem     = mem_elms.getType();
    nixl_backend_t nixl_backend = backend->getType();
    section_key_t sec_key = std::make_pair(nixl_mem, nixl_backend);
    if (sectionMap.count(sec_key) == 0)
        sectionMap[sec_key] = new nixlDescList<nixlMetaDesc>(
                                  nixl_mem, mem_elms.isUnifiedAddr(), true);
    memToBackendMap[nixl_mem].insert(nixl_backend); // Fine to overwrite, it's a set
    nixlDescList<nixlMetaDesc> *target = sectionMap[sec_key];

    // Add entries to the target list
    nixlMetaDesc out;
    nixlBasicDesc *p = &out;
    int ret;
    for (int i=0; i<mem_elms.descCount(); ++i) {
        // TODO: remote might change the metadata, have to keep stringDesc to compare
        if (target->getIndex((const nixlBasicDesc) mem_elms[i]) < 0) {
            ret = backend->loadRemoteMD(mem_elms[i], nixl_mem, agentName, out.metadata);
            // In case of errors, no need to remove the previous entries
            // Agent will delete the full object.
            if (ret<0)
                return ret;
            *p = mem_elms[i]; // Copy the basic desc part
            target->addDesc(out);
        }
    }
    return 0;
}

int nixlRemoteSection::loadRemoteData (nixlSerDes* deserializer) {
    int ret;
    size_t seg_count;
    nixl_backend_t nixl_backend;

    ret = deserializer->getBuf("nixlSecElms", &seg_count, sizeof(seg_count));
    if (ret) return ret;

    for (size_t i=0; i<seg_count; ++i) {
        // In case of errors, no need to remove the previous entries
        // Agent will delete the full object.
        ret = deserializer->getBuf("bknd", &nixl_backend, sizeof(nixl_backend));
        if (ret) return ret;
        nixlDescList<nixlStringDesc> s_desc(deserializer);
        if (s_desc.descCount()==0) // can be used for entry removal in future
            return -1;
        if (addDescList(s_desc, backendToEngineMap[nixl_backend]) < 0)
            return -1;
    }
    return 0;
}

nixlRemoteSection::~nixlRemoteSection() {
    nixl_backend_t nixl_backend;
    nixlDescList<nixlMetaDesc> *m_desc;

    for (auto &seg : sectionMap) {
        nixl_backend = seg.first.second;
        m_desc = seg.second;
        for (auto & elm : *m_desc)
            backendToEngineMap[nixl_backend]->removeRemoteMD(elm.metadata);
        delete m_desc;
    }
    // nixlMemSection destructor will clean up the rest
}
