#ifndef __GDS_BACKEND_H
#define __GDS_BACKEND_H

#include <nixl.h>
#include <cuda_runtime.h>
#include <unistd.h>
#include <fcntl.h>
#include "cufile.h"

class nixlGdsFile {
private:
   int            fd;
   // What if file is not preallocated
   // -1 means inf size file?
   size_t         size;
   CUfileHandle_t handle;
};

class nixlGdsHandle {
private:
   CUfileHandle_t	handle;
};

class nixlGdsConnection : public nixlBackendConnMD {
   private:
       std::string	mount_path;
       volatile bool	compatible;

   public:
       bool check_gds_compatibility();
       // Other options if required
};


class nixlGdsPrivateMetadata : public nixlBackendMD {
   private:
       nixlGdsFile    fileH;
       std::string    handleStr;

   // serialized handle for sharing
   public:
       nixlGdsPrivateMetadata() : nixlBackendMD(true) {
       }
       ~nixlGdsPrivateMetadata() {
       }
       std::string get() const
       {
	       return handleStr;
       }
};

class nixlGdsPublicMetadata : public nixlBackendMD {

   public:
       nixlGdsPublicMetadata() : nixlBackendMD(false) {}
       ~nixlGdsPublicMetadata() {
       }
};

class nixlGdsEngine : nixlBackendEngine {
   nixlGdsFile	fh;


public:
   nixlGdsEngine(const nixlGdsInitParams* init_params);
   ~nixlGdsEngine();

   // File operations - target is the distributed FS
   // So no requirements to connect to target.
   // Just treat it locally.
   bool		 supportsNotif () const { return false; }
   bool          supportsRemote  () const { return false; }
   bool          supportsLocal   () const { return true; }
   bool          supportsProgTh  () const { return false; }

   // No Public metadata for this backend - let us return empty string here.
   std::string   getPublicData (const nixlBackendMD* meta) const
   {
	   return std::string();
   }

   nixl_status_t connect(const std::string &remote_agent)
   {
	   return NIXL_SUCCESS;
   }

   nixl_status_t disconnect(const std::string &remote_agent)
   {
	   return NIXL_SUCCESS;
   }

   nixl_status_t registerMem(const nixlStringDesc &mem,
                             const nixl_mem_t &nixl_mem,
	                     nixlBackendMD* &out);
   void deregisterMem (nixlBackendMD *meta);
   nixl_status_t loadRemoteMD (const nixlStringDesc &input,
			       const nixl_mem_t &nixl_mem,
			       const std::string &remote_agent,
			       nixlBackendMD* &output);

   nixl_xfer_state_t postXfer (const nixl_meta_dlist_t &local,
			       const nixl_meta_dlist_t &remote,
			       const nixl_xfer_op_t &op,
			       const std::string &remote_agent,
			       const std::string &notif_msg,
			       nixlBackendReqH* &handle);

   nixl_xfer_state_t checkXfer (nixlBackendReqH* handle);
   void releaseReqH(nixlBackendReqH* handle);
};
#endif
