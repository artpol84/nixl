#ifndef __GDS_BACKEND_H
#define __GDS_BACKEND_H

#include "nixl.h"
#include <cuda_runtime.h>
#include <cufile.h>
#include <unistd>
#include <fcntl.h>

class nixlGdsFile {
private:
   std::string		fname;
   size_t		size;
   CUFileHandle_t	handle;
};

class nixlGdsHandle {
private:
   CuFileHandle_t	handle;
};

class nixlGdsConnection : public nixlBackendConnMD {
   private:
       std::string	mount_path;
       volatile bool	compatible;
   public:
       // Other options if required
};


class nixlGdsPrivateMetadata : public nixlBackendMD {
   private:
	nixlGdsFile    fileH;
	std::string    handleStr;
   public:
       nixlGdsPrivateMetadata() : nixlBackendMD(true) {
       }

       ~nixlGdsPrivateMetadata() {
       }

       std::string get() const {
           return handleStr;
       }
};

class nixlGdsPublicMetadata : public nixlBackendMD {

   public:
       nixlGdsHandle	handle;

       nixlGdsPublicMetadata() : nixlBackendMD(false) {}
       ~nixlGdsPublicMetadata() {
       }
};

class nixlGdsEngine : nixlBackendEngine {
   nixlGdsFile	fh;
public:
   nixlGdsEngine(const nixlGdsInitParams* init_params);
   ~nixlGdsEngine();

   bool		 supportsNotif () const { return true; }
   nixl_status_t registerMem(const nixlBasicDesc &mem,
			     const nixl_mem_t &nixl_mem,
			     nixlBackendMD* &out);
   void deregisterMem (nixlBackendMD *meta);
   nixl_status_t loadRemoteMD (const nixlStringDesc &input,
			       const nixl_mem_t &nixl_mem,
			       const std::string &remote_agent,
			       nixlBackendMD* output);
   nixl_status_t removeRemoteMD (nixlBackendMD* input);

   // Data Transfer
   nixl_xfer_state_t postXfer (const nixlDescList<nixlMetaDesc> &local,
			       const nixlDescList<nixlMetaDesc> &remote,
			       const nixl_xfer_op_t &op,
			       const std::string &remote_agent,
			       const std::string &notif_msg,
			       nixlBackendReqH* &handle);
   nixl_xfer_state_t checkXfer (nixlBackendReqH* handle);
   void releaseReqH(nixlBackendReqH* handle);
};
