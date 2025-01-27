#include <nixl.h>
#include "nixl_descriptors.h"
#include "internal/transfer_backend.h"

// No Virtual function in basic_desc, as we want each object to just have the members
bool operator==(const basic_desc& lhs, const basic_desc& rhs) {
    if ((lhs.addr==rhs.addr) &&
        (lhs.len==rhs.len) &&
        (lhs.dev_id==rhs.dev_id))
        return true;
    else
        return false;
}

bool operator!=(const basic_desc& lhs, const basic_desc& rhs) {
    return !(lhs==rhs);
}

basic_desc& basic_desc::operator=(const basic_desc& desc) {
    // Check for self-assignment
    if (this != &desc) {
        addr = desc.addr;
        len = desc.len;
        dev_id = desc.dev_id;
    }
    return *this;
}

basic_desc::basic_desc(const basic_desc& desc){
    if (this != &desc) {
        addr = desc.addr;
        len = desc.len;
        dev_id = desc.dev_id;
    }
}

bool basic_desc::covers (const basic_desc& query) const {
    if (dev_id == query.dev_id) {
        if (((uint64_t) addr <= (uint64_t) query.addr) &&
            ((uint64_t) addr + len >= (uint64_t) query.addr + query.len))
            return true;
    }
    return false;
}

bool basic_desc::overlaps (const basic_desc& query) const {
    // TBD
    return false;
}

// No ~basic_desc() needed, as there are no memory allocations,
// and the addr being pointer is just a memory address value,
// not an actual pointer.


// The template is used to select from basic_desc/meta_desc/string_desc
// This is the backbone of the library, a desc list centered abstraction.
// There are no virtual functions, so the object is all data, no pointers.

template <class T>
desc_list<T>::desc_list (memory_type_t type, bool unified_addr){
    static_assert(std::is_base_of<basic_desc, T>::value);
    this->type = type;
    this->unified_addressing = unified_addr;
}

template <class T>
desc_list<T>::desc_list (const desc_list<T>& t){
    this->type = t.get_type();
    this->unified_addressing = t.is_unified_addressing();
    for (auto & elm : t.descs)
        descs.push_back(elm);
}

template <class T>
int desc_list<T>::add_desc (T desc, bool sorted) {
    descs.push_back(desc);
    return 0;
}

template <class T>
int desc_list<T>::rem_desc (T desc){
    descs.erase(std::remove(descs.begin(), descs.end(), desc), descs.end());
    return 0;
}

template <class T>
int desc_list<T>::populate (desc_list<basic_desc> query, desc_list<T>& resp) {
    // Populate only makes sense when there is extra metadata
    if(std::is_same<basic_desc, T>::value)
        return -1;
    T new_elm;
    basic_desc *p = &new_elm;
    int found = 0;
    for (auto & q : query.descs)
        for (auto & elm : descs)
            if (elm.covers(q)){
                *p = q;
                new_elm.copy_meta(elm);
                resp.add_desc(new_elm);
                found++;
                break;
            }
    if (query.get_desc_count()==found)
        return 0;
    else
        return -1;
}

template <class T>
int desc_list<T>::get_index(basic_desc query) const {
    for (size_t i=0; i<descs.size(); ++i){
        const basic_desc *p = &descs[i];
        if (*p==query)
            return i;
    }
    return -1;
}

template <class T>
bool operator==(const desc_list<T>& lhs, const desc_list<T>& rhs) {
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
template class desc_list<basic_desc>;
template class desc_list<meta_desc>;
template class desc_list<string_desc>;
