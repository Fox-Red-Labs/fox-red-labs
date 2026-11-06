/* Fox Red Labs stub: the HSB emulator's Linux data plane only touches CUDA to copy GPU-resident tensors.
   On the SK-TDA4VM there is no CUDA; every tensor we send lives in CPU memory, so these never run. */
#pragma once
typedef int cudaError_t;
enum { cudaSuccess = 0, cudaErrorNoDevice = 100 };
enum cudaMemcpyKind { cudaMemcpyHostToHost = 0, cudaMemcpyHostToDevice = 1, cudaMemcpyDeviceToHost = 2, cudaMemcpyDeviceToDevice = 3, cudaMemcpyDefault = 4 };
static inline cudaError_t cudaMemcpy(void*, const void*, unsigned long, cudaMemcpyKind) { return cudaErrorNoDevice; }
static inline const char* cudaGetErrorString(cudaError_t) { return "no CUDA on this device (Fox Red Labs stub)"; }
