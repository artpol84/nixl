#ifndef _NIXL_DESCRIPTORS_H
#define _NIXL_DESCRIPTORS_H

#include <cstddef>
#include <cstdint>
#include <vector>
#include <string>
#include <algorithm>
#include "serdes.h"
#include "nixl_types.h"

// A basic descriptor class, contiguous in memory, with some supporting methods
class nixlBasicDesc {
    public:
        uintptr_t addr;  // Start of buffer
        size_t    len;   // Buffer length
        uint32_t  devId; // Device ID

        nixlBasicDesc() {};
        nixlBasicDesc(uintptr_t addr, size_t len, uint32_t dev_id);
        nixlBasicDesc(const std::string &str); // deserializer
        nixlBasicDesc(const nixlBasicDesc &desc) = default;
        nixlBasicDesc& operator=(const nixlBasicDesc &desc) = default;
        ~nixlBasicDesc() = default;

        friend bool operator==(const nixlBasicDesc &lhs, const nixlBasicDesc &rhs);
        friend bool operator!=(const nixlBasicDesc &lhs, const nixlBasicDesc &rhs);
        bool covers (const nixlBasicDesc &query) const;
        bool overlaps (const nixlBasicDesc &query) const;

        void copyMeta (const nixlBasicDesc &desc) {}; // No metadata in BasicDesc
        std::string serialize() const;
        void print(const std::string &suffix) const; // For debugging
};

// A class for a list of descriptors, where transfer requests are made from.
// It has some additional methods to help with creation and population.
template<class T>
class nixlDescList {
    private:
        nixl_mem_t     type;
        bool           unifiedAddr;
        bool           sorted;
        std::vector<T> descs;

    public:
        nixlDescList(nixl_mem_t type, bool unifiedAddr=true, bool sorted=false);
        nixlDescList(nixlSerDes* deserializer);
        nixlDescList(const nixlDescList<T> &d_list) = default;
        nixlDescList& operator=(const nixlDescList<T> &d_list) = default;
        ~nixlDescList () = default;

        inline nixl_mem_t getType() const { return type; }
        inline bool isUnifiedAddr() const { return unifiedAddr; }
        inline int descCount() const { return descs.size(); }
        inline bool isEmpty() const { return (descs.size()==0); }
        inline bool isSorted() const { return sorted; }

        inline const T& operator[](int index) const { return descs[index]; }
        inline typename std::vector<T>::const_iterator begin() const { return descs.begin(); }
        inline typename std::vector<T>::const_iterator end() const { return descs.end(); }
        template <class Y> friend bool operator==(const nixlDescList<Y> &lhs,
                                                  const nixlDescList<Y> &rhs);

        // addDesc is the only method to add new individual entries, checks for no overlap
        nixl_status_t addDesc(const T &desc, const bool overlap_check=true);
        nixl_status_t remDesc(int index);
        nixl_status_t populate(const nixlDescList<nixlBasicDesc> &query, nixlDescList<T> &resp) const;
        void clear() { descs.clear(); }

        int getIndex(const nixlBasicDesc &query) const;
        nixl_status_t serialize(nixlSerDes* serializer) const;
        void print() const;
};

#endif
