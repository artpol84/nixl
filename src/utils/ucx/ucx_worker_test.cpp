#include <vector>
#include <string>
#include <cassert>
#include <cstring>
#include <iostream>

#include "ucx_utils.h"

//TODO: meson conditional build for CUDA
//#define USE_VRAM

#ifdef USE_VRAM

#include <cuda_runtime.h>
#include <cufile.h>

int gpu_id = 0;

static void checkCudaError(cudaError_t result, const char *message) {
    if (result != cudaSuccess) {
	std::cerr << message << " (Error code: " << result << " - "
                   << cudaGetErrorString(result) << ")" << std::endl;
        exit(EXIT_FAILURE);
    }
}
#endif


using namespace std;

int main()
{
    vector<string> devs;
    devs.push_back("mlx5_0");
    nixlUcxWorker w[2] = { nixlUcxWorker(devs), nixlUcxWorker(devs) };
    nixlUcxEp ep[2];
    nixlUcxMem mem[2];
    nixlUcxRkey rkey[2];
    nixlUcxReq req;
    uint8_t* buffer[2];
    uint8_t check_buffer[2][128];
    int ret, i;

#ifdef USE_VRAM
    checkCudaError(cudaSetDevice(gpu_id), "Failed to set device");
    checkCudaError(cudaMalloc(&buffer[0], 128), "Failed to allocate CUDA buffer 0");
    checkCudaError(cudaMalloc(&buffer[1], 128), "Failed to allocate CUDA buffer 1");
#else
    buffer[0] = (uint8_t*) calloc(1, 128);
    buffer[1] = (uint8_t*) calloc(1, 128);
#endif

    assert(buffer[0]);
    assert(buffer[1]);
    /* Test control path */
    for(i = 0; i < 2; i++) {
        uint64_t addr;
        size_t size;
        assert(0 == w[i].ep_addr(addr, size));
        assert(0 == w[!i].connect((void*) addr, size, ep[!i]));
        free((void*) addr);
        assert(0 == w[i].mem_reg(buffer[i], 128, mem[i]));
        assert(0 == w[i].mem_addr(mem[i], addr, size));
        assert(0 == w[!i].rkey_import(ep[!i], (void*) addr, size, rkey[!i]));
        free((void*) addr);
    }

    /* Test Write operation */

#ifdef USE_VRAM
    checkCudaError(cudaMemset(buffer[1], 0xbb, 128), "Failed to memset");
    checkCudaError(cudaMemset(buffer[0], 0xda, 128), "Failed to memset");
#else
    memset(buffer[1], 0xbb, 128);
    memset(buffer[0], 0xda, 128);
#endif

    assert(0 == w[0].write(ep[0], buffer[0], mem[0], (uint64_t) buffer[1], rkey[0], 32, req));

    ret = 0;
    while(ret == 0) {
	ret = w[0].test(req);
	w[0].progress();
	w[1].progress();
    }
    assert(ret > 0);

#ifdef USE_VRAM
    checkCudaError(cudaMemcpy(check_buffer[1], buffer[1], 128, cudaMemcpyDeviceToHost), "Failed to memcpy");
#else
    memcpy(check_buffer[1], buffer[1], 128);
#endif

    for(i = 0; i < 32; i++) {
        assert(check_buffer[1][i] == 0xda);
    }
    for(; i < 128; i++) {
        assert(check_buffer[1][i] == 0xbb);
    }

    /* Test Read operation */

#ifdef USE_VRAM
    checkCudaError(cudaMemset(buffer[0], 0xbb, 128), "Failed to memset");
    checkCudaError(cudaMemset(buffer[1], 0xbb, 32), "Failed to memset");
    checkCudaError(cudaMemset(buffer[1] + 32, 0xda, 96), "Failed to memset");
#else
    memset(buffer[0], 0xbb, 128);
    memset(buffer[1], 0xbb, 32);
    memset(buffer[1] + 32, 0xda, 96);
#endif
    assert(0 == w[0].read(ep[0], (uint64_t) buffer[1], rkey[0], buffer[0], mem[0], 128, req));

    ret = 0;
    while( ret == 0) {
	ret = w[0].test(req);
	w[0].progress();
	w[1].progress();
    }
    assert(ret > 0);

#ifdef USE_VRAM
    checkCudaError(cudaMemcpy(check_buffer[0], buffer[0], 128, cudaMemcpyDeviceToHost), "Failed to memcpy");
#else
    memcpy(check_buffer[0], buffer[0], 128);
#endif

    for(i = 0; i < 32; i++) {
        assert(check_buffer[0][i] == 0xbb);
    }
    for(; i < 128; i++) {
        assert(check_buffer[0][i] == 0xda);
    }

    /* Test shutdown */
    for(i = 0; i < 2; i++) {
        w[i].rkey_destroy(rkey[i]);
        w[i].mem_dereg(mem[i]);
        assert(0 == w[i].disconnect(ep[i]));
    }

}
