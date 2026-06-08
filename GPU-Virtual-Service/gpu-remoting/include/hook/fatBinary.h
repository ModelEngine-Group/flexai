#ifndef FAT_BINARY_ELF_H
#define FAT_BINARY_ELF_H

#include "../chunkStructure.h"
#include "../constVar.h"
#include <stdint.h>

typedef struct __attribute__((__packed__)) {
    uint32_t magic;
    uint32_t version;
    uint64_t text;      // points to first text section
    uint64_t data;      // points to outside of the file
    uint64_t unknown;
    uint64_t text2;     // points to second text section
    uint64_t zero;
} FatHeader_t;

#define CRICKET_ELF_NV_INFO_PREFIX ".nv.info"
#define CRICKET_ELF_NV_SHARED_PREFIX ".nv.shared."
#define CRICKET_ELF_NV_TEXT_PREFIX ".nv.text."
#define CRICKET_ELF_TEXT_PREFIX ".text."

#define CRICKET_ELF_FATBIN ".nv_fatbin"
#define CRICKET_ELF_REGFUN "_ZL24__sti____cudaRegisterAllv"

#define FATBIN_STRUCT_MAGIC 0x466243b1
#define FATBIN_TEXT_MAGIC   0xBA55ED50

typedef struct  __attribute__((__packed__)) {
    uint32_t magic;
    uint16_t version;
    uint16_t header_size;
    uint64_t size;
} FatElfHeader_t;

typedef struct  __attribute__((__packed__)) {
    uint16_t kind;
    uint16_t unknown1;
    uint32_t header_size;
    uint64_t size;
    uint32_t compressed_size;       // Size of compressed data
    uint32_t unknown2;              // Address size for PTX?
    uint16_t minor;
    uint16_t major;
    uint32_t arch;
    uint32_t obj_name_offset;
    uint32_t obj_name_len;
    uint64_t flags;
    uint64_t zero;                  // Alignment for compression?
    uint64_t decompressed_size;     // Length of compressed data in decompressed representation.
                                    // There is an uncompressed footer so this is generally smaller
                                    // than size.
} FatTextHeader_t;

#define FATBIN_FLAG_64BIT     0x0000000000000001LL
#define FATBIN_FLAG_DEBUG     0x0000000000000002LL
#define FATBIN_FLAG_LINUX     0x0000000000000010LL
#define FATBIN_FLAG_COMPRESS  0x0000000000002000LL

#define EIATTR_PARAM_CBANK              0xa
#define EIATTR_EXTERNS                  0xf
#define EIATTR_FRAME_SIZE               0x11
#define EIATTR_MIN_STACK_SIZE           0x12
#define EIATTR_KPARAM_INFO              0x17
#define EIATTR_CBANK_PARAM_SIZE         0x19
#define EIATTR_MAX_REG_COUNT            0x1b
#define EIATTR_EXIT_INSTR_OFFSETS       0x1c
#define EIATTR_S2RCTAID_INSTR_OFFSETS   0x1d
#define EIATTR_CRS_STACK_SIZE           0x1e
#define EIATTR_SW1850030_WAR            0x2a
#define EIATTR_REGCOUNT                 0x2f
#define EIATTR_SW2393858_WAR            0x30
#define EIATTR_INDIRECT_BRANCH_TARGETS  0x34
#define EIATTR_CUDA_API_VERSION         0x37

#define EIFMT_NVAL                      0x1
#define EIFMT_HVAL                      0x3
#define EIFMT_SVAL                      0x4

#endif 