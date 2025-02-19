#ifndef _NIXL_DESCRIPTORS_H
#define _NIXL_DESCRIPTORS_H

#include <cstddef>
#include <cstdint>
#include <vector>
#include <string>
#include <algorithm>
#include "serdes.h"

typedef enum {UCX, GPUDIRECTIO, NVMe, NVMeoF} backend_type_t;
typedef enum {DRAM_SEG, VRAM_SEG, BLK_SEG, FILE_SEG} mem_type_t;

typedef enum {NIXL_XFER_INIT, NIXL_XFER_PROC,
              NIXL_XFER_DONE, NIXL_XFER_ERR} xfer_state_t;
typedef enum {NIXL_READ,  NIXL_RD_FLUSH, NIXL_RD_NOTIF,
              NIXL_WRITE, NIXL_WR_FLUSH, NIXL_WR_NOTIF} xfer_op_t;

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
        mem_type_t     type;
        bool           unifiedAddr;
        bool           sorted;
        std::vector<T> descs;

    public:
        nixlDescList(mem_type_t type, bool unifiedAddr=true, bool sorted=false);
        nixlDescList(nixlSerDes* deserializer);
        nixlDescList(const nixlDescList<T> &d_list) = default;
        nixlDescList& operator=(const nixlDescList<T> &d_list) = default;
        ~nixlDescList () = default;

        inline mem_type_t getType() const { return type; }
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
        int addDesc(const T &desc);
        int remDesc(int index);
        int remDesc(const T &desc);
        int populate(const nixlDescList<nixlBasicDesc> &query, nixlDescList<T> &resp) const;
        void clear() { descs.clear(); }

        int getIndex(const nixlBasicDesc &query) const;
        int serialize(nixlSerDes* serializer) const;
        void print() const;
};

#endif
