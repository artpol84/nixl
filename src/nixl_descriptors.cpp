#include <nixl.h>
#include "nixl_descriptors.h"
#include "internal/transfer_backend.h"

// No Virtual function in nixlBasicDesc, as we want each object to just have the members
bool operator==(const nixlBasicDesc& lhs, const nixlBasicDesc& rhs) {
    if ((lhs.addr==rhs.addr) &&
        (lhs.len==rhs.len) &&
        (lhs.devId==rhs.devId))
        return true;
    else
        return false;
}

bool operator!=(const nixlBasicDesc& lhs, const nixlBasicDesc& rhs) {
    return !(lhs==rhs);
}

nixlBasicDesc& nixlBasicDesc::operator=(const nixlBasicDesc& desc) {
    // Check for self-assignment
    if (this != &desc) {
        addr = desc.addr;
        len = desc.len;
        devId = desc.devId;
    }
    return *this;
}

nixlBasicDesc::nixlBasicDesc(const nixlBasicDesc& desc){
    if (this != &desc) {
        addr = desc.addr;
        len = desc.len;
        devId = desc.devId;
    }
}

bool nixlBasicDesc::covers (const nixlBasicDesc& query) const {
    if (devId == query.devId) {
        if ((addr <=  query.addr) &&
            (addr + len >= query.addr + query.len))
            return true;
    }
    return false;
}

bool nixlBasicDesc::overlaps (const nixlBasicDesc& query) const {
    // TBD
    return false;
}

// No ~nixlBasicDesc() needed, as there are no memory allocations,
// and the addr being pointer is just a memory address value,
// not an actual pointer.


// The template is used to select from nixlBasicDesc/nixlMetaDesc/nixlStringDesc
// This is the backbone of the library, a desc list centered abstraction.
// There are no virtual functions, so the object is all data, no pointers.

template <class T>
nixlDescList<T>::nixlDescList (memory_type_t type, bool unified_addr){
    static_assert(std::is_base_of<nixlBasicDesc, T>::value);
    this->type = type;
    this->unifiedAddressing = unified_addr;
}

template <class T>
nixlDescList<T>::nixlDescList (const nixlDescList<T>& t){
    this->type = t.get_type();
    this->unifiedAddressing = t.isUnifiedAddressing();
    for (auto & elm : t.descs)
        descs.push_back(elm);
}

template <class T>
int nixlDescList<T>::addDesc (T desc, bool sorted) {
    descs.push_back(desc);
    return 0;
}

template <class T>
int nixlDescList<T>::remDesc (int index){
    if ((size_t) index >= descs.size())
        return -1;
    descs.erase(descs.begin() + index);
    return 0;
}

template <class T>
int nixlDescList<T>::remDesc (T desc){
    // Add check for existence
    descs.erase(std::remove(descs.begin(), descs.end(), desc), descs.end());
    return 0;
}

template <class T>
int nixlDescList<T>::populate (nixlDescList<nixlBasicDesc> query, nixlDescList<T>& resp) {
    // Populate only makes sense when there is extra metadata
    if(std::is_same<nixlBasicDesc, T>::value)
        return -1;
    T new_elm;
    nixlBasicDesc *p = &new_elm;
    int found = 0;
    for (auto & q : query)
        for (auto & elm : descs)
            if (elm.covers(q)){
                *p = q;
                new_elm.copyMeta(elm);
                resp.addDesc(new_elm);
                found++;
                break;
            }
    if (query.descCount()==found)
        return 0;
    else
        return -1;
}

template <class T>
int nixlDescList<T>::getIndex(nixlBasicDesc query) const {
    for (size_t i=0; i<descs.size(); ++i){
        const nixlBasicDesc *p = &descs[i];
        if (*p==query)
            return i;
    }
    return -1;
}

template <class T>
bool operator==(const nixlDescList<T>& lhs, const nixlDescList<T>& rhs) {
    if (get_type(lhs)!=get_type(rhs))
        return false;
    if (lhs.descs.size() != rhs.descs.size())
        return false;
    for (int i=0; i<lhs.descs.size(); ++i)
        if (lhs.descs[i]!=rhs.descs[i])
            return false;
    // Not checking metadata, just the addresses. If necessary child
    // class should override with those checks.
    return true;
}

// Since we implement a template class from header files, this is necessary
template class nixlDescList<nixlBasicDesc>;
template class nixlDescList<nixlMetaDesc>;
template class nixlDescList<nixlStringDesc>;
