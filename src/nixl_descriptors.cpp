#include <algorithm>
#include <iostream>
#include <functional>
#include "nixl.h"
#include "nixl_descriptors.h"
#include "internal/transfer_backend.h"

/*** Class nixlBasicDesc implementation ***/

// No Virtual function in nixlBasicDesc class or its children, as we want
// each object to just have the members during serialization.

nixlBasicDesc::nixlBasicDesc(uintptr_t addr, size_t len, uint32_t dev_id) {
    this->addr  = addr;
    this->len   = len;
    this->devId = dev_id;
}

nixlBasicDesc::nixlBasicDesc(const nixlBasicDesc& desc) {
    if (this != &desc) {
        addr  = desc.addr;
        len   = desc.len;
        devId = desc.devId;
    }
}

nixlBasicDesc& nixlBasicDesc::operator=(const nixlBasicDesc& desc) {
    if (this != &desc) {
        addr  = desc.addr;
        len   = desc.len;
        devId = desc.devId;
    }
    return *this;
}

bool operator==(const nixlBasicDesc& lhs, const nixlBasicDesc& rhs) {
    if ((lhs.addr  == rhs.addr ) &&
        (lhs.len   == rhs.len  ) &&
        (lhs.devId == rhs.devId))
        return true;
    else
        return false;
}

bool operator!=(const nixlBasicDesc& lhs, const nixlBasicDesc& rhs) {
    return !(lhs==rhs);
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
    if (devId != query.devId)
        return false;
    if ((addr + len < query.addr) || (query.addr + query.len < addr))
        return false;
    return true;
}

void nixlBasicDesc::printDesc(const std::string suffix) const {
    std::cout << "DEBUG: Desc (" << addr << ", " << len
              << ") from devID " << devId << suffix << "\n";
}

/*** Class nixlDescList implementation ***/

// The template is used to select from nixlBasicDesc/nixlMetaDesc/nixlStringDesc
// There are no virtual functions, so the object is all data, no pointers.

template <class T>
nixlDescList<T>::nixlDescList (memory_type_t type, bool unified_addr, bool sorted) {
    static_assert(std::is_base_of<nixlBasicDesc, T>::value);
    this->type        = type;
    this->unifiedAddr = unified_addr;
    this->sorted      = sorted;
}

template <class T>
nixlDescList<T>::nixlDescList (const nixlDescList<T>& d_list) {
    if (this != &d_list) {
        this->type = d_list.getType();
        this->unifiedAddr = d_list.isUnifiedAddr();
        this->sorted = d_list.isSorted();
        for (auto & elm : d_list.descs)
            descs.push_back(elm);
    }
}

template <class T>
nixlDescList<T>& nixlDescList<T>::operator=(const nixlDescList<T>& d_list) {
    if (this != &d_list) {
        this->type = d_list.getType();
        this->unifiedAddr = d_list.isUnifiedAddr();
        this->sorted = d_list.isSorted();
        for (auto & elm : d_list.descs)
            descs.push_back(elm);
    }
    return *this;
}

// Internal function used for sorting the Vector and logarithmic search
bool descAddrCompare (const nixlBasicDesc& a, const nixlBasicDesc& b, bool unifiedAddr) {
    if (unifiedAddr) { // Ignore devId
        return (a.addr < b.addr);
    } else {
        if (a.devId < b.devId)
            return true;
        else if (a.devId == b.devId)
            return (a.addr < b.addr);
        else
            return false;
    }
}

#define desc_comparator_f [&](const nixlBasicDesc& a, const nixlBasicDesc& b) {\
                              return descAddrCompare(a, b, unifiedAddr); }

// User might want to create a Transfer where the descriptors are
// not in an accending order, so vector is used for descs instead of map,
// and during insertion we guarantee that.
template <class T>
int nixlDescList<T>::addDesc (const T& desc) {
    if (!sorted) {
        for (auto & elm : descs)
            // No overlap is allowed among descs of a list
            if (elm.overlaps(desc))
                return -1;
        descs.push_back(desc);
    } else {
        // Since vector is kept soted, we can use upper_bound
        auto itr = std::upper_bound(descs.begin(), descs.end(), desc, desc_comparator_f);
        if (itr == descs.end())
            descs.push_back(desc);
        else if ((*itr).overlaps(desc))
            return -1;
        else
            descs.insert(itr, desc);
    }
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
int nixlDescList<T>::remDesc (const T& desc) {
    int index = getIndex(desc);
    if (index < 0)
        return -1;

    descs.erase(descs.begin() + index);
    return 0;
}

template <class T>
int nixlDescList<T>::populate (const nixlDescList<nixlBasicDesc>& query,
                               nixlDescList<T>& resp) const {
    // Populate only makes sense when there is extra metadata
    if(std::is_same<nixlBasicDesc, T>::value)
        return -1;

    if ((type != query.getType()) || (type != resp.getType()))
        return -1;

    if ((unifiedAddr != query.isUnifiedAddr()) || (unifiedAddr != resp.isUnifiedAddr()))
        return -1;

    T new_elm;
    nixlBasicDesc *p = &new_elm;
    int count = 0;
    bool found;

    if (!sorted) {
        for (auto & q : query)
            for (auto & elm : descs)
                if (elm.covers(q)){
                    *p = q;
                    new_elm.copyMeta(elm);
                    resp.addDesc(new_elm);
                    count++;
                    break;
                }
    } else {
        // if (query.isSorted()) // "There can be an optimization. TBD

        for (auto & q : query) {
            found = false;
            auto itr = std::lower_bound(descs.begin(), descs.end(), q, desc_comparator_f);

            // Same start address case
            if (itr != descs.end()){
                if ((*itr).covers(q)) {
                    found = true;
                }
            }

            // query starts starts later, try previous entry
            if ((!found) && (itr != descs.begin())){
                itr = std::prev(itr , 1);
                if ((*itr).covers(q)) {
                    found = true;
                }
            }

            if (found) {
                *p = q;
                new_elm.copyMeta(*itr);
                resp.addDesc(new_elm);
                count++; // redundant in sorted mode, double checking
            } else {
                resp.clear();
                return -1;
            }
        }
    }

    if (query.descCount()==count)
        return 0;

    resp.clear();
    return -1;
}

template <class T>
int nixlDescList<T>::getIndex(nixlBasicDesc query) const {
    if (!sorted) {
        auto itr = std::find(descs.begin(), descs.end(), query);
        if (itr == descs.end())
            return -1; // not found
        return itr - descs.begin();
    } else {
        auto itr = std::lower_bound(descs.begin(), descs.end(), query, desc_comparator_f);
        if (itr == descs.end())
            return -1; // not found
        if (*itr == query)
            return itr - descs.begin();
    }
    return -1;
}

template <class T>
void nixlDescList<T>::printDescList() const {
    std::cout << "DEBUG: DescList of mem type " << type
              << (unifiedAddr ? " with" : " without") << " unified addressing and "
              << (sorted ? "sorted" : "unsorted") << "\n";
    for (auto & elm : descs) {
        std::cout << "    ";
        elm.printDesc("");
    }
}

template <class T>
bool operator==(const nixlDescList<T>& lhs, const nixlDescList<T>& rhs) {
    if ((lhs.getType()       != rhs.getType())      ||
        (lhs.descCount()     != rhs.descCount())     ||
        (lhs.isUnifiedAddr() != rhs.isUnifiedAddr()) ||
        (lhs.isSorted()      != rhs.isSorted()))
        return false;

    for (int i=0; i<lhs.descs.size(); ++i)
        if (lhs.descs[i] != rhs.descs[i])
            return false;
    return true;
}

// Since we implement a template class declared in a header files, this is necessary
template class nixlDescList<nixlBasicDesc>;
template class nixlDescList<nixlMetaDesc>;
template class nixlDescList<nixlStringDesc>;
