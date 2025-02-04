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
class basic_desc {
    public:
        uintptr_t addr;  // Start of buffer
        size_t    len;    // Buffer length
        uint32_t  dev_id; // Device ID

        friend bool operator==(const basic_desc& lhs, const basic_desc& rhs);
        friend bool operator!=(const basic_desc& lhs, const basic_desc& rhs);

        basic_desc& operator=(const basic_desc& desc);
        basic_desc(const basic_desc& desc);
        // no metadata in basic_desc
        void copy_meta (const basic_desc& desc) {};

        bool covers (const basic_desc& query) const;
        bool overlaps (const basic_desc& query) const;

        basic_desc() {};
        ~basic_desc() {};
};

// String of metadata next to each basic_desc, used to import/export
// memory sections to the metadata server.
class string_desc : public basic_desc {
    public:
        std::string metadata;
        inline void copy_meta (const string_desc& meta){
            this->metadata = meta.metadata;
        }
};

// A class for a list of descriptors, where transfer requests are made
// from. It has some additional methods to help with keeping backend
// metadata information as well.
template<class T>
class desc_list {
    private:
        memory_type_t type;
        bool          unified_addressing;

    public:
        std::vector<T> descs;

        desc_list (memory_type_t type, bool unified_addr = true);
        desc_list (const desc_list<T>& t);
        void clear() { descs.clear(); }
        ~desc_list () { descs.clear(); };

        inline memory_type_t get_type() const { return type; };
        inline bool is_unified_addressing() const { return unified_addressing; };
        inline int get_desc_count() const { return descs.size(); };
        inline bool is_empty () const { return (descs.size()==0);}

        int add_desc (T desc, bool sorted=false);
        // Removing descriptor by value instead of index
        int rem_desc (T desc);
        int populate (desc_list<basic_desc> query, desc_list<T>& resp);

        template <class Y>
        friend bool operator==(const desc_list<Y>& lhs, const desc_list<Y>& rhs);
        int get_index(basic_desc query) const;
};
#endif
