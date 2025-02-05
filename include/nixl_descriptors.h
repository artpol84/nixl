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
class BasicDesc {
    public:
        uintptr_t addr;  // Start of buffer
        size_t    len;    // Buffer length
        uint32_t  dev_id; // Device ID

        friend bool operator==(const BasicDesc& lhs, const BasicDesc& rhs);
        friend bool operator!=(const BasicDesc& lhs, const BasicDesc& rhs);

        BasicDesc& operator=(const BasicDesc& desc);
        BasicDesc(const BasicDesc& desc);
        // no metadata in BasicDesc
        void copy_meta (const BasicDesc& desc) {};

        bool covers (const BasicDesc& query) const;
        bool overlaps (const BasicDesc& query) const;

        BasicDesc() {};
        ~BasicDesc() {};
};

// String of metadata next to each BasicDesc, used to import/export
// memory sections to the metadata server.
class StringDesc : public BasicDesc {
    public:
        std::string metadata;
        inline void copy_meta (const StringDesc& meta){
            this->metadata = meta.metadata;
        }
};

// A class for a list of descriptors, where transfer requests are made
// from. It has some additional methods to help with keeping backend
// metadata information as well.
template<class T>
class DescList {
    private:
        memory_type_t  type;
        bool           unified_addressing;
        std::vector<T> descs;

    public:
        DescList (memory_type_t type, bool unified_addr = true);
        DescList (const DescList<T>& t);
        void clear() { descs.clear(); }
        ~DescList () { descs.clear(); };

        inline memory_type_t get_type() const { return type; }
        inline bool is_unified_addressing() const { return unified_addressing; }
        inline int desc_count() const { return descs.size(); }
        inline bool is_empty() const { return (descs.size()==0);}

        inline T& operator[](int index) { return descs[index]; }
        inline const T& operator[](int index) const { return descs[index]; }
        inline typename std::vector<T>::iterator begin() { return descs.begin(); }
        inline typename std::vector<T>::const_iterator begin() const { return descs.begin(); }
        inline typename std::vector<T>::iterator end() { return descs.end(); }
        inline typename std::vector<T>::const_iterator end() const { return descs.end(); }

        int add_desc (T desc, bool sorted=false);
        // Removing descriptor by index or value
        int rem_desc (int index);
        int rem_desc (T desc);
        int populate (DescList<BasicDesc> query, DescList<T>& resp);

        template <class Y>
        friend bool operator==(const DescList<Y>& lhs, const DescList<Y>& rhs);
        int get_index(BasicDesc query) const;
};
#endif
