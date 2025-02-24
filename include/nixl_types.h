#ifndef _NIXL_TYPES_H
#define _NIXL_TYPES_H
#include <vector>
#include <string>
#include <map>

typedef std::map<std::string, std::vector<std::string>> nixl_notifs_t;

typedef enum {UCX, GPUDIRECTIO, NVMe, NVMeoF} nixl_backend_t;

typedef enum {DRAM_SEG, VRAM_SEG, BLK_SEG, FILE_SEG} nixl_mem_t;

typedef enum {NIXL_XFER_PRE, NIXL_XFER_INIT, NIXL_XFER_PROC,
                  NIXL_XFER_DONE, NIXL_XFER_ERR} nixl_state_t;

typedef enum {NIXL_READ,  NIXL_RD_FLUSH, NIXL_RD_NOTIF,
                  NIXL_WRITE, NIXL_WR_FLUSH, NIXL_WR_NOTIF} nixl_op_t;

typedef enum {
    NIXL_ERR_INVALID_PARAM = -1,
    NIXL_ERR_BACKEND = -2,
    NIXL_ERR_NOT_IMPLEMENTED = -3
} nixl_err_t;

#endif
