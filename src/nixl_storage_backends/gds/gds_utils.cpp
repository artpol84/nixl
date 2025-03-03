#include <iostream>
#include "gds_util.h"

nixl_status_t gdsUtil::registerFileHandle(int fd, std::string location,
					     gdsFileHandle& gds_handle)
{
	CUfileError_t  status;
	CUfileDescr_t  descr;
	CUfileHandle_t handle;

	descr.handle.fd = fd;
	descr.type = CU_FILE_HANDLE_TYPE_OPAQUE_FD;

	status = cuFileHandleRegister(&handle, &descr);
	if (status.err != CU_FILE_SUCCESS) {
            std::cerr << "file register error:"
		      << std::endl;
	    return NIXL_ERR_BACKEND;
	}

	gds_handle.cu_fhandle = handle;
	gds_handle.fd = fd;
	gds_handle.location = location;
	gds_file_map[fd] = gds_handle;
	return NIXL_SUCCESS;
}

nixl_status_t gdsUtil::registerBufHandle(void *ptr, size_t size, int flags)
{
	CUfileError_t  status;

	status = cuFileBufRegister(ptr, size, flags);
	if (status.err != CU_FILE_SUCCESS) {
	    ret = -1;
	    std::cerr << "Buffer registration failed\n";
	    return NIXL_ERR_BACKEND;
	}
	return NIXL_SUCCESS;
}

int gdsUtil::openGdsDriver()
{
	CUfileError_t   err;

	err = cuFileDriverOpen();
        if (err.err != CU_FILE_SUCCESS) {
            std::cerr <<" Error initializing GPU Direct Storage driver\n";
	    return -1;
	}
	return  0;
}

void gdsUtil::closeGdsDriver()
{
	cuFileDriverClose();
}

void gdsUtil::deregisterFileHandle(gdsFileHandle& handle)
{
	gds_file_map.erase(handle.fd);
	cuFileHandleDeregister(handle);
}

nixl_status_t gdsUtil::deregisterBufHandle(void *ptr)
{
	CUfileError_t  status;

	status = cuFileBufDeregister(ptr);
	if (status.err != CU_FILE_SUCCESS) {
		std::cerr <<"Error De-Registering Buffer\n";
		return NIXL_ERR_BACKEND;
	}
}

gdsIOBatch::gdsIOBatch(int size)
{
	max_reqs 		= size;
	io_batch_events		= new CUfileIOEvents_t[size];
	io_batch_params 	= new CUfileIOParams_t[size];
	current_status		= NIXL_XFER_INIT;
	entries_completed	= 0;
	batch_size		= 0;

	if (io_batch_events == nullptr ||
	    io_batch_params == nullptr) {
		return -1;
	}
	init_err = cuFileBatchIOSetUp(&batch_handle, size);
	if (init_err.err != 0) {
		std::cerr << "Error in creating the batch\n";
	}
}

gdsIOBatch::~gdsIOBatch()
{
	if (current_status != NIXL_XFER_PROC &&
	    current_status != NIXL_XFER_ERR) {
		delete io_batch_events;
		delete io_batch_params;
	} else {
		cerr<<"Attempting to delete a batch before completion\n";
	}
}

int gdsIOBatch::addToBatch(CUfileHandle_t fh, void *buffer, size_t size,
			   size_t file_offset, size_t ptr_offset, CUfileOpcode_t type)
{
	CUfileIOParams_t    *params	= nullptr;
	
	if (batch_size >= max_reqs)
		return -1;
	
	params				= &io_batch_params[batch_size];
	params->mode 			= CUFILE_BATCH;
	params->fh   		    	= fh;
	params->u.batch.devPtr_base 	= buffer;
	params->u.batch.file_offset 	= file_offset;
	params->u.batch.devPtr_offset 	= ptr_offset;
	params->u.batch.size          	= size;
	params->opcode		      	= CUFILE_READ;
	params->cookie		      	= params;
	batch_size++;
	return 0;
}

int gdsIOBatch::submitBatch(int flags)
{
	CUfileError_t	err;

	err = cuFileBatchIOSubmit(batch_handle, batch_size, io_batch_params, flags);
	if (err.err != 0) {
		std::cerr << "Error in setting up Batch\n" << std::endl;
	}
}

nixl_status_t gdsIOBatch::checkStatus()
{
	CUfileError_t	errBatch;
	int 		nr = max_reqs;

	errBatch = cuFileBatchIOGetStatus(batch_handle, 0, &nr, io_batch_events, NULL);
	if (errBatch.err != 0) {
		std::cerr << "Error in IO Batch Get Status" << std::endl;
		current_status = NIXL_XFER_ERR;
	}
	entries_completed += nr;
	if (nr < max_reqs)
		current_status = NIXL_XFER_PROC;
	else if (nr > max_reqs)
		current_status = NIXL_XFER_ERR;
	else
		current_status = NIXL_XFER_DONE;
	
	return 0;
}
