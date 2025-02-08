#ifndef _NIXL_DESCRIPTORS_H
#define _NIXL_DESCRIPTORS_H

#include <cstddef>
#include <cstdint>
#include <vector>
#include <string>
#include <algorithm>
#include "serdes.h"

typedef enum {UCX, GPUDIRECTIO, NVMe, NVMeoF} backend_type_t;
typedef enum {DRAM_SEG, VRAM_SEG, BLK_SEG, FILE_SEG} memory_type_t;

// A basic descriptor class, with basic operators and math checks
class nixlBasicDesc {
    public:
        uintptr_t addr;  // Start of buffer
        size_t    len;    // Buffer length
        uint32_t  devId; // Device ID

        nixlBasicDesc() {};
        nixlBasicDesc(uintptr_t addr, size_t len, uint32_t dev_id);
        nixlBasicDesc(const nixlBasicDesc& desc);
        nixlBasicDesc& operator=(const nixlBasicDesc& desc);
        ~nixlBasicDesc() {};

        friend bool operator==(const nixlBasicDesc& lhs, const nixlBasicDesc& rhs);
        friend bool operator!=(const nixlBasicDesc& lhs, const nixlBasicDesc& rhs);
        bool covers (const nixlBasicDesc& query) const;
        bool overlaps (const nixlBasicDesc& query) const;

        void copyMeta (const nixlBasicDesc& desc) {}; // No metadata in BasicDesc
        void printDesc(const std::string suffix) const; // For debugging

        int serialize(nixlSerDes* serializor);
        int deserialize(nixlSerDes* deserializor);
};

// A class for a list of descriptors, where transfer requests are made from.
// It has some additional methods to help with creation and population.
template<class T>
class nixlDescList {
    private:
        memory_type_t  type;
        bool           unifiedAddr;
        bool           sorted;
        std::vector<T> descs;

    public:
        nixlDescList (memory_type_t type, bool unifiedAddr=true, bool sorted=false);
        nixlDescList(nixlSerDes* deserializor);
        nixlDescList (const nixlDescList<T>& d_list);
        nixlDescList& operator=(const nixlDescList<T>& d_list);
        ~nixlDescList () { descs.clear(); };

        inline memory_type_t getType() const { return type; }
        inline bool isUnifiedAddr() const { return unifiedAddr; }
        inline int descCount() const { return descs.size(); }
        inline bool isEmpty() const { return (descs.size()==0); }
        inline bool isSorted() const { return sorted; }

        inline const T& operator[](int index) const { return descs[index]; }
        inline typename std::vector<T>::const_iterator begin() const { return descs.begin(); }
        inline typename std::vector<T>::const_iterator end() const { return descs.end(); }

        // addDesc is the only method to add new individual entries, checks for no overlap
        int addDesc (const T& desc);
        int remDesc (int index);
        int remDesc (const T& desc);
        int populate (const nixlDescList<nixlBasicDesc>& query, nixlDescList<T>& resp) const;
        void clear() { descs.clear(); }

        int getIndex(nixlBasicDesc query) const;
        int serialize(nixlSerDes* serializor); // Is const
        void printDescList() const;
        template <class Y> friend bool operator==(const nixlDescList<Y>& lhs,
                                                  const nixlDescList<Y>& rhs);
};

#endif
