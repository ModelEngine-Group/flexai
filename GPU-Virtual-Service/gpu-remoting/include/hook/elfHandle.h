#ifndef ELF_HANDLE_H
#define ELF_HANDLE_H

#include <libelf.h>
#include <gelf.h>
#include <dlfcn.h>

#include "../define.h"
#include "fatBinary.h"

int InitElf2(void);

int GetFatbinInfo(FatHeader_t *fatbin, std::vector<KernelInfo_t*> *kernel_list, uint8_t** fatbin_mem, size_t* fatbin_size);

int GetParameterInfo(std::vector<KernelInfo_t*> *kernel_list, void* memory, size_t memsize);

KernelInfo_t* GetKernelInfoByKernelName(std::vector<KernelInfo_t*> *kernel_list, const char *kernelname);

#endif 


