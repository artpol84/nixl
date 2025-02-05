#include <nixl.h>
#include "nixl_descriptors.h"
#include "internal/transfer_backend.h"

// No Virtual function in BasicDesc, as we want each object to just have the members
bool operator==(const BasicDesc& lhs, const BasicDesc& rhs) {
    if ((lhs.addr==rhs.addr) &&
        (lhs.len==rhs.len) &&
        (lhs.dev_id==rhs.dev_id))
        return true;
    else
        return false;
}

bool operator!=(const BasicDesc& lhs, const BasicDesc& rhs) {
    return !(lhs==rhs);
}

BasicDesc& BasicDesc::operator=(const BasicDesc& desc) {
    // Check for self-assignment
    if (this != &desc) {
        addr = desc.addr;
        len = desc.len;
        dev_id = desc.dev_id;
    }
    return *this;
}

BasicDesc::BasicDesc(const BasicDesc& desc){
    if (this != &desc) {
        addr = desc.addr;
        len = desc.len;
        dev_id = desc.dev_id;
    }
}

bool BasicDesc::covers (const BasicDesc& query) const {
    if (dev_id == query.dev_id) {
        if ((addr <=  query.addr) &&
            (addr + len >= query.addr + query.len))
            return true;
    }
    return false;
}

bool BasicDesc::overlaps (const BasicDesc& query) const {
    // TBD
    return false;
}

// No ~BasicDesc() needed, as there are no memory allocations,
// and the addr being pointer is just a memory address value,
// not an actual pointer.


// The template is used to select from BasicDesc/MetaDesc/StringDesc
// This is the backbone of the library, a desc list centered abstraction.
// There are no virtual functions, so the object is all data, no pointers.

template <class T>
DescList<T>::DescList (memory_type_t type, bool unified_addr){
    static_assert(std::is_base_of<BasicDesc, T>::value);
    this->type = type;
    this->unified_addressing = unified_addr;
}

template <class T>
DescList<T>::DescList (const DescList<T>& t){
    this->type = t.get_type();
    this->unified_addressing = t.is_unified_addressing();
    for (auto & elm : t.descs)
        descs.push_back(elm);
}

template <class T>
int DescList<T>::add_desc (T desc, bool sorted) {
    descs.push_back(desc);
    return 0;
}

template <class T>
int DescList<T>::rem_desc (int index){
    if ((size_t) index >= descs.size())
        return -1;
    descs.erase(descs.begin() + index);
    return 0;
}

template <class T>
int DescList<T>::rem_desc (T desc){
    // Add check for existence
    descs.erase(std::remove(descs.begin(), descs.end(), desc), descs.end());
    return 0;
}

template <class T>
int DescList<T>::populate (DescList<BasicDesc> query, DescList<T>& resp) {
    // Populate only makes sense when there is extra metadata
    if(std::is_same<BasicDesc, T>::value)
        return -1;
    T new_elm;
    BasicDesc *p = &new_elm;
    int found = 0;
    for (auto & q : query)
        for (auto & elm : descs)
            if (elm.covers(q)){
                *p = q;
                new_elm.copy_meta(elm);
                resp.add_desc(new_elm);
                found++;
                break;
            }
    if (query.desc_count()==found)
        return 0;
    else
        return -1;
}

template <class T>
int DescList<T>::get_index(BasicDesc query) const {
    for (size_t i=0; i<descs.size(); ++i){
        const BasicDesc *p = &descs[i];
        if (*p==query)
            return i;
    }
    return -1;
}

template <class T>
bool operator==(const DescList<T>& lhs, const DescList<T>& rhs) {
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
template class DescList<BasicDesc>;
template class DescList<MetaDesc>;
template class DescList<StringDesc>;
