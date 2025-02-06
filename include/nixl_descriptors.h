#ifndef _NIXL_DESCRIPTORS_H
#define _NIXL_DESCRIPTORS_H

#include <cstddef>
#include <cstdint>
#include <vector>
#include <string>
#include <algorithm>

typedef enum {UCX, GPUDIRECTIO, NVMe, NVMeoF} backend_type_t;
typedef enum {DRAM_SEG, VRAM_SEG, BLK_SEG, FILE_SEG} memory_type_t;

// A basic descriptor class, with basic operators and math checks
class nixlBasicDesc {
    public:
        uintptr_t addr;  // Start of buffer
        size_t    len;    // Buffer length
        uint32_t  devId; // Device ID

        friend bool operator==(const nixlBasicDesc& lhs, const nixlBasicDesc& rhs);
        friend bool operator!=(const nixlBasicDesc& lhs, const nixlBasicDesc& rhs);

        nixlBasicDesc& operator=(const nixlBasicDesc& desc);
        nixlBasicDesc(const nixlBasicDesc& desc);
        // no metadata in BasicDesc
        void copyMeta (const nixlBasicDesc& desc) {};

        bool covers (const nixlBasicDesc& query) const;
        bool overlaps (const nixlBasicDesc& query) const;

        nixlBasicDesc() {};
        ~nixlBasicDesc() {};
};

// A class for a list of descriptors, where transfer requests are made from.
// It has some additional methods to help with creation and population.
template<class T>
class nixlDescList {
    private:
        memory_type_t  type;
        bool           unifiedAddressing;
        std::vector<T> descs;

    public:
        nixlDescList (memory_type_t type, bool unifiedAddr = true);
        nixlDescList (const nixlDescList<T>& t);
        void clear() { descs.clear(); }
        ~nixlDescList () { descs.clear(); };

        inline memory_type_t get_type() const { return type; }
        inline bool isUnifiedAddressing() const { return unifiedAddressing; }
        inline int descCount() const { return descs.size(); }
        inline bool isEmpty() const { return (descs.size()==0);}

        inline T& operator[](int index) { return descs[index]; }
        inline const T& operator[](int index) const { return descs[index]; }
        inline typename std::vector<T>::iterator begin() { return descs.begin(); }
        inline typename std::vector<T>::const_iterator begin() const { return descs.begin(); }
        inline typename std::vector<T>::iterator end() { return descs.end(); }
        inline typename std::vector<T>::const_iterator end() const { return descs.end(); }

        int addDesc (T desc, bool sorted=false);
        // Removing descriptor by index or value
        int remDesc (int index);
        int remDesc (T desc);
        int populate (nixlDescList<nixlBasicDesc> query, nixlDescList<T>& resp);

        template <class Y>
        friend bool operator==(const nixlDescList<Y>& lhs, const nixlDescList<Y>& rhs);
        int getIndex(nixlBasicDesc query) const;
};

#endif
