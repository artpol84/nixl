#ifndef __GDS_UTILS_H
#define __GDS_UTILS_H

#include <fcntl.h>
#include <unistd.h>
#include <nixl.h>

#include "cufile.h"

class gdsFileHandle {
private:
   int            fd;
   // -1 means inf size file?
   size_t         size;
   CUfileHandle_t cu_fhandle;
};


class gdsIOBatch {
   private:
	int		           max_reqs;

	CUfileBatchHandle_t batch_handle;
   CUfileIOEvents_t    *io_batch_events;
	CUfileIOParams_t    *io_batch_params;
	CUfileError_t       init_err;
	nixl_status_t	     current_status;
	int		           entrties_completed;
   int                 batch_size;

   public:
	gdsIOBatch(int size);
   ~gdsIOBatch();

	void		           addToBatch(CUfileHandle_t fh,  void *buffer,
			         	             size_t size, size_t file_offset,
				                      size_t ptr_offset, CUfileOpcode_t type);
	int                 submitBatch(int flags);
	nixl_status_t       checkStatus();
};


class gdsRegUtil {
      std::map<int, gdsFileHandle> gds_file_map;

    public:
      nixl_status_t registerFileHandle(int fd, size_t size, std::string location,
                                       gdsFileHandle& handle);
      nixl_status_t registerBufHandle(void *ptr, size_t size, int flags);
      void deregisterFileHandle(gdsFileHandle& handle);
      nixl_status_t deregisterBufHandle(void *ptr);
      int openGdsDriver();
      void closeGdsDrive();
};

#endif