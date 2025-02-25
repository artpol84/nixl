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

nixlBasicDesc::nixlBasicDesc(const std::string &str) {
    if (str.size()==sizeof(nixlBasicDesc))
        str.copy(reinterpret_cast<char*>(this), sizeof(nixlBasicDesc));
    else
        len = 0; // Error indicator
}

bool operator==(const nixlBasicDesc &lhs, const nixlBasicDesc &rhs) {
    return ((lhs.addr  == rhs.addr ) &&
            (lhs.len   == rhs.len  ) &&
            (lhs.devId == rhs.devId));
}

bool operator!=(const nixlBasicDesc &lhs, const nixlBasicDesc &rhs) {
    return !(lhs==rhs);
}

bool nixlBasicDesc::covers (const nixlBasicDesc &query) const {
    if (devId == query.devId) {
        if ((addr <=  query.addr) &&
            (addr + len >= query.addr + query.len))
            return true;
    }
    return false;
}

bool nixlBasicDesc::overlaps (const nixlBasicDesc &query) const {
    if (devId != query.devId)
        return false;
    if ((addr + len <= query.addr) || (query.addr + query.len <= addr))
        return false;
    return true;
}

std::string nixlBasicDesc::serialize() const {
    return std::string(reinterpret_cast<const char*>(this),
                       sizeof(nixlBasicDesc));
}

void nixlBasicDesc::print(const std::string &suffix) const {
    std::cout << "DEBUG: Desc (" << addr << ", " << len
              << ") from devID " << devId << suffix << "\n";
}

nixlStringDesc::nixlStringDesc(const std::string &str) {
    size_t meta_size = str.size() - sizeof(nixlBasicDesc);
    if (meta_size>0) {
        metadata.resize(meta_size);
        str.copy(reinterpret_cast<char*>(this), sizeof(nixlBasicDesc));
        str.copy(reinterpret_cast<char*>(&metadata[0]),
                 meta_size, sizeof(nixlBasicDesc));
    } else { // Error
        len = 0;
        metadata.resize(0);
    }
}

/*** Class nixlDescList implementation ***/

// The template is used to select from nixlBasicDesc/nixlMetaDesc/nixlStringDesc
// There are no virtual functions, so the object is all data, no pointers.

template <class T>
nixlDescList<T>::nixlDescList (nixl_mem_t type, bool unified_addr, bool sorted) {
    static_assert(std::is_base_of<nixlBasicDesc, T>::value);
    this->type        = type;
    this->unifiedAddr = unified_addr;
    this->sorted      = sorted;
}

template <class T>
nixlDescList<T>::nixlDescList(nixlSerDes* deserializer) {
    size_t n_desc;
    std::string str;

    descs.clear();

    str = deserializer->getStr("nixlDList"); // Object type
    if (str.size()==0)
        return;

    // nixlMetaDesc should be internall and not be serialized
    if ((str == "nixlMDList") || (std::is_same<nixlMetaDesc, T>::value))
        return;

    if (deserializer->getBuf("t", &type, sizeof(type)))
        return;
    if (deserializer->getBuf("u", &unifiedAddr, sizeof(unifiedAddr)))
        return;
    if (deserializer->getBuf("s", &sorted, sizeof(sorted)))
        return;
    if (deserializer->getBuf("n", &n_desc, sizeof(n_desc)))
        return;

    if (std::is_same<nixlBasicDesc, T>::value) {
        // Contiguous in memory, so no need for per elm deserialization
        if (str!="nixlBDList")
            return;
        str = deserializer->getStr("");
        if (str.size()!= n_desc * sizeof(nixlBasicDesc))
            return;
        descs.resize(n_desc);
        str.copy(reinterpret_cast<char*>(descs.data()), str.size());

        for (size_t i=0; i<n_desc; ++i)
            if (descs[i].len == 0) { // Error indicator
                descs.clear();
                return;
            }
    } else if(std::is_same<nixlStringDesc, T>::value) {
        if (str!="nixlSDList")
            return;
        for (size_t i=0; i<n_desc; ++i) {
            str = deserializer->getStr("");
            if (str.size()==0) {
                descs.clear();
                return;
            }
            T elm(str);
            if (elm.len == 0) { // Error indicator
                descs.clear();
                return;
            }
            descs.push_back(elm);
        }
    } else {
        return; // Unknown type, error
    }
}

// Internal function used for sorting the Vector and logarithmic search
bool descAddrCompare (const nixlBasicDesc &a, const nixlBasicDesc &b,
                      bool unifiedAddr) {
    if (unifiedAddr) // Ignore devId
        return (a.addr < b.addr);
    if (a.devId < b.devId)
        return true;
    if (a.devId == b.devId)
        return (a.addr < b.addr);
    return false;
}

#define desc_comparator_f [&](const nixlBasicDesc &a, const nixlBasicDesc &b) {\
                              return descAddrCompare(a, b, unifiedAddr); }

// User might want to create a transfer where the descriptors are
// not in an accending order, so vector is used for descs instead of map,
// and during insertion we guarantee that.
template <class T>
nixl_status_t nixlDescList<T>::addDesc (const T &desc, const bool overlap_check) {
    if (desc.len == 0) // Error indicator
        return NIXL_ERR_INVALID_PARAM;

    if (!overlap_check) {
        descs.push_back(desc);
        return NIXL_SUCCESS;
    }

    if (!sorted) {
        for (auto & elm : descs) {
            // No overlap is allowed among descs of a list
            if (elm.overlaps(desc))
                return NIXL_ERR_INVALID_PARAM;
        }
        descs.push_back(desc);
    } else {
        // Since vector is kept soted, we can use upper_bound
        auto itr = std::upper_bound(descs.begin(), descs.end(),
                                    desc, desc_comparator_f);
        if (itr == descs.end())
            descs.push_back(desc);
        else if ((*itr).overlaps(desc))
            return NIXL_ERR_INVALID_PARAM;
        else
            descs.insert(itr, desc);
    }
    return NIXL_SUCCESS;
}

template <class T>
nixl_status_t nixlDescList<T>::remDesc (int index){
    if (((size_t) index >= descs.size()) || (index < 0))
        return NIXL_ERR_INVALID_PARAM;
    descs.erase(descs.begin() + index);
    return NIXL_SUCCESS;
}

template <class T>
nixl_status_t nixlDescList<T>::populate (const nixlDescList<nixlBasicDesc> &query,
                                         nixlDescList<T> &resp) const {
    // Populate only makes sense when there is extra metadata
    if(std::is_same<nixlBasicDesc, T>::value)
        return NIXL_ERR_INVALID_PARAM;

    if ((type != query.getType()) || (type != resp.getType()))
        return NIXL_ERR_INVALID_PARAM;

    if ((unifiedAddr != query.isUnifiedAddr()) ||
        (unifiedAddr != resp.isUnifiedAddr()))
        return NIXL_ERR_INVALID_PARAM;

    T new_elm;
    nixlBasicDesc *p = &new_elm;
    int count = 0, last_found = 0;
    bool found, q_sorted = query.isSorted();

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

        if (query.descCount()==count) {
            return NIXL_SUCCESS;
        } else {
            resp.clear();
            return NIXL_ERR_BAD;
        }
    } else {
        for (auto & q : query) {
            found = false;
            auto itr = std::lower_bound(descs.begin() + last_found,
                                        descs.end(), q, desc_comparator_f);

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
                if (q_sorted) // only check rest of the list
                    last_found = itr - descs.begin();
            } else {
                resp.clear();
                return NIXL_ERR_BAD;
            }
        }
        return NIXL_SUCCESS;
    }
}

template <class T>
int nixlDescList<T>::getIndex(const nixlBasicDesc &query) const {
    if (!sorted) {
        auto itr = std::find(descs.begin(), descs.end(), query);
        if (itr == descs.end())
            return -1; // not found
        return itr - descs.begin();
    } else {
        auto itr = std::lower_bound(descs.begin(), descs.end(),
                                    query, desc_comparator_f);
        if (itr == descs.end())
            return -1; // not found
        // As desired, becomes nixlBasicDesc on both sides
        if (*itr == query)
            return itr - descs.begin();
    }
    return -1;
}

template <class T>
nixl_status_t nixlDescList<T>::serialize(nixlSerDes* serializer) const {

    nixl_status_t ret;
    size_t n_desc = descs.size();

    // nixlMetaDesc should be internall and not be serialized
    if(std::is_same<nixlMetaDesc, T>::value)
        return NIXL_ERR_INVALID_PARAM;

    if (std::is_same<nixlBasicDesc, T>::value)
        ret = serializer->addStr("nixlDList", "nixlBDList");
    else if (std::is_same<nixlStringDesc, T>::value)
        ret = serializer->addStr("nixlDList", "nixlSDList");
    else
        return NIXL_ERR_INVALID_PARAM;

    if (ret) return ret;

    ret = serializer->addBuf("t", &type, sizeof(type));
    if (ret) return ret;

    ret = serializer->addBuf("u", &unifiedAddr, sizeof(unifiedAddr));
    if (ret) return ret;

    ret = serializer->addBuf("s", &sorted, sizeof(sorted));
    if (ret) return ret;

    ret = serializer->addBuf("n", &(n_desc), sizeof(n_desc));
    if (ret) return ret;

    if (n_desc==0)
        return NIXL_SUCCESS; // Unusual, but supporting it

    if (std::is_same<nixlBasicDesc, T>::value) {
        // Contiguous in memory, so no need for per elm serialization
        ret = serializer->addStr("", std::string(
                                 reinterpret_cast<const char*>(descs.data()),
                                 n_desc * sizeof(nixlBasicDesc)));
        if (ret) return ret;
    } else { // already checked it can be only nixlStringDesc
        for(auto & elm : descs) {
            ret = serializer->addStr("", elm.serialize());
            if(ret) return ret;
        }
    }

    return NIXL_SUCCESS;
}

template <class T>
void nixlDescList<T>::print() const {
    std::cout << "DEBUG: DescList of mem type " << type
              << (unifiedAddr ? " with" : " without") << " unified addressing and "
              << (sorted ? "sorted" : "unsorted") << "\n";
    for (auto & elm : descs) {
        std::cout << "    ";
        elm.print("");
    }
}

template <class T>
bool operator==(const nixlDescList<T> &lhs, const nixlDescList<T> &rhs) {
    if ((lhs.getType()       != rhs.getType())       ||
        (lhs.descCount()     != rhs.descCount())     ||
        (lhs.isUnifiedAddr() != rhs.isUnifiedAddr()) ||
        (lhs.isSorted()      != rhs.isSorted()))
        return false;

    for (size_t i=0; i<lhs.descs.size(); ++i)
        if (lhs.descs[i] != rhs.descs[i])
            return false;
    return true;
}

// Since we implement a template class declared in a header files, this is necessary
template class nixlDescList<nixlBasicDesc>;
template class nixlDescList<nixlMetaDesc>;
template class nixlDescList<nixlStringDesc>;

template bool operator==<nixlBasicDesc> (const nixlDescList<nixlBasicDesc> &lhs,
                                         const nixlDescList<nixlBasicDesc> &rhs);
template bool operator==<nixlMetaDesc>  (const nixlDescList<nixlMetaDesc> &lhs,
                                         const nixlDescList<nixlMetaDesc> &rhs);
template bool operator==<nixlStringDesc>(const nixlDescList<nixlStringDesc> &lhs,
                                         const nixlDescList<nixlStringDesc> &rhs);
