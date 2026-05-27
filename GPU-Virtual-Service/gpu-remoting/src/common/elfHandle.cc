#include "../../include/hook/elfHandle.h"

static const char* myName = "elfHandle";

int InitElf2(void){
    if (elf_version(EV_CURRENT) == EV_NONE) {
        tool::Logging(LOG_ERROR, myName, "ELF library initialization failed: %s\n", elf_errmsg(-1));
        return -1;
    }
    return 0;
}

static int GetStrByElfFlag(char** str, uint64_t flag)
{
    return asprintf(str, "64Bit: %s, Debug: %s, Linux: %s, Compress %s",
        (flag & FATBIN_FLAG_64BIT) ? "yes" : "no",
        (flag & FATBIN_FLAG_DEBUG) ? "yes" : "no",
        (flag & FATBIN_FLAG_LINUX) ? "yes" : "no",
        (flag & FATBIN_FLAG_COMPRESS) ? "yes" : "no");
}

static void PrintFatTextHeader(FatTextHeader_t *th)
{
    char* flagstr = NULL;
    GetStrByElfFlag(&flagstr, th->flags);

    tool::Logging(LOG_REGS, myName, "text_header: fatbin_kind: %#x, header_size %#x, size %#zx, compressed_size %#x,\
 minor %#x, major %#x, arch %d, decompressed_size %#zx\n\tflags: %s\n",
        th->kind,
        th->header_size,
        th->size,
        th->compressed_size,
        th->minor,
        th->major,
        th->arch,
        th->decompressed_size,
        flagstr);
    tool::Logging(LOG_REGS, myName, "\tunknown fields: unknown1: %#x, unknown2: %#x, zeros: %#zx\n",
        th->unknown1,
        th->unknown2,
        th->zero);

    free(flagstr);
}

/** Check the header of a fatbin
 * Performs some integrity checks and returns the elf header
 * @param fatbin_data Pointer to the fatbin data
 * @param fatbin_size Size of the fatbin data
 * @param decompressed_size Pointer to a variable that will be set to the size of the decompressed data
 * @param compressed_data Pointer to a variable that will be set to point to the compressed data
*/
static int GetFatElfHeader(const uint8_t* fatbin_data, size_t fatbin_size, FatElfHeader_t **elf_header)
{
    FatElfHeader_t *eh = NULL;

    if (fatbin_data == NULL || elf_header == NULL) {
        tool::Logging(LOG_ERROR, myName, "fatbin_data is NULL\n");
        return 1;
    }

    // if (fatbin_size < sizeof(struct fat_elf_header)) {
    //     tool::Logging(LOG_ERROR, myName, "fatbin_size is too small");
    //     return 1;
    // }

    eh = (FatElfHeader_t*) fatbin_data;
    if (eh->magic != FATBIN_TEXT_MAGIC) {
        tool::Logging(LOG_ERROR, myName, "Invalid magic  number: expected %#x but got %#x\n", FATBIN_TEXT_MAGIC, eh->magic);
        return 1;
    }

    if (eh->version != 1 || eh->header_size != sizeof(FatElfHeader_t)) {
        tool::Logging(LOG_ERROR, myName, "fatbin text version is wrong or header size is inconsistent.\
            This is a sanity check to avoid reading a new fatbinary format\n");
        return 1;
    }
    
    *elf_header = eh;
    return 0;
}

/** Check the text header of a fatbin
 * Performs some integrity checks and returns the text header
 * @param fatbin_data Pointer to the fatbin data
 * @param fatbin_size Size of the fatbin data
 * @param decompressed_size Pointer to a variable that will be set to the size of the decompressed data
 * @param compressed_data Pointer to a variable that will be set to point to the compressed data
*/
static int GetFatTextHeader(const uint8_t* fatbin_data, size_t fatbin_size, FatTextHeader_t **text_header)
{
    FatTextHeader_t *th = NULL;

    if (fatbin_data == NULL || text_header == NULL) {
        tool::Logging(LOG_ERROR, myName, "fatbin_data is NULL\n");
        return 1;
    }

    // if (fatbin_size < sizeof(struct fat_text_header)) {
    //     tool::Logging(LOG_ERROR, myName, "fatbin_size is too small");
    //     return 1;
    // }

    th = (FatTextHeader_t*)fatbin_data;

    if(th->obj_name_offset != 0) {
        if (((char*)th)[th->obj_name_offset + th->obj_name_len] != '\0') {
            tool::Logging(LOG_REGS, myName, "Fatbin object name is not null terminated\n");
        } else {
            char *obj_name = (char*)th + th->obj_name_offset;
            tool::Logging(LOG_REGS, myName, "Fatbin object name: %s (len:%#x)\n", obj_name, th->obj_name_len);
        }
    }

    *text_header = th;
    return 0;
}

/** Decompresses a fatbin file
 * @param input Pointer compressed input data
 * @param input_size Size of compressed data
 * @param output preallocated memory where decompressed output should be stored
 * @param output_size size of output buffer. Should be equal to the size of the decompressed data
 */
static size_t DecompressFatbin(const uint8_t* input, size_t input_size, uint8_t* output, size_t output_size)
{
    size_t ipos = 0, opos = 0;  
    uint64_t next_nclen;  // length of next non-compressed segment
    uint64_t next_clen;   // length of next compressed segment
    uint64_t back_offset; // negative offset where redudant data is located, relative to current opos

    while (ipos < input_size) {
        next_nclen = (input[ipos] & 0xf0) >> 4;
        next_clen = 4 + (input[ipos] & 0xf);
        if (next_nclen == 0xf) {
            do {
                next_nclen += input[++ipos];
            } while (input[ipos] == 0xff);
        }
        
        if (memcpy(output + opos, input + (++ipos), next_nclen) == NULL) {
            tool::Logging(LOG_ERROR, myName, "copying data\n");
            return 0;
        }
#ifdef FATBIN_DECOMPRESS_DEBUG
        printf("%#04zx nocompress (len:%#x):\n", opos, next_nclen);
        tool::HexDump(output + opos, next_nclen);
#endif
        ipos += next_nclen;
        opos += next_nclen;
        if (ipos >= input_size || opos >= output_size) {
            break;
        }
        back_offset = input[ipos] + (input[ipos + 1] << 8);       
        ipos += 2;
        if (next_clen == 0xf+4) {
            do {
                next_clen += input[ipos++];
            } while (input[ipos - 1] == 0xff);
        }
#ifdef FATBIN_DECOMPRESS_DEBUG
        printf("%#04zx compress (decompressed len: %#x, back_offset %#x):\n", opos, next_clen, back_offset);
#endif
        if (next_clen <= back_offset) {
            if (memcpy(output + opos, output + opos - back_offset, next_clen) == NULL) {
                tool::Logging(LOG_ERROR, myName, "Error copying data\n");
                return 0;
            }
        } else {
            if (memcpy(output + opos, output + opos - back_offset, back_offset) == NULL) {
                tool::Logging(LOG_ERROR, myName, "Error copying data\n");
                return 0;
            }
            for (size_t i = back_offset; i < next_clen; i++) {
                output[opos + i] = output[opos + i - back_offset];
            }
        }
#ifdef FATBIN_DECOMPRESS_DEBUG
        tool::HexDump(output + opos, next_clen);
#endif
        opos += next_clen;
    }
    tool::Logging(LOG_REGS, myName, "ipos: %#zx, opos: %#zx, ilen: %#zx, olen: %#zx\n", ipos, opos, input_size, output_size);
    return opos;
}


static ssize_t DecompressSingleSection(const uint8_t *input, uint8_t **output, size_t *output_size,
                                         FatElfHeader_t *eh, FatTextHeader_t *th)
{
    size_t padding;
    size_t input_read = 0;
    size_t output_written = 0;
    size_t decompress_ret = 0;
    const uint8_t zeroes[8] = {0};

    if (input == NULL || output == NULL || eh == NULL || th == NULL) {
        tool::Logging(LOG_ERROR, myName, "invalid parameters\n");
        return 1;
    }

    // add max padding of 7 bytes
    if ((*output = (uint8_t*)malloc(th->decompressed_size + 7)) == NULL) {
        tool::Logging(LOG_ERROR, myName, "Error allocating memory of size %#zx for output buffer: %s\n", 
                th->decompressed_size, strerror(errno));
        goto error;
    }
    PrintFatTextHeader(th);

    if ((decompress_ret = DecompressFatbin(input, th->compressed_size, *output, th->decompressed_size)) != th->decompressed_size) {
        tool::Logging(LOG_ERROR, myName, "Decompression failed: decompressed size is %#zx, but header says %#zx\n", 
                decompress_ret, th->decompressed_size);
        tool::Logging(LOG_ERROR, myName, "input pos: %#zx, output pos: %#zx\n", input - (uint8_t*)eh, *output);
        tool::HexDump(input, 0x160);
        if (decompress_ret >= 0x60)
            tool::HexDump((*output) + decompress_ret - 0x60, 0x60);
        goto error;
    }
    input_read += th->compressed_size;
    output_written += th->decompressed_size;

    padding = ((8 - (size_t)(input + input_read)) % 8);
    if (memcmp(input + input_read, zeroes, padding) != 0) {
        tool::Logging(LOG_ERROR, myName, "expected %#zx zero bytes, got:\n", padding);
        tool::HexDump(input + input_read, 0x60);
        goto error;
    }
    input_read += padding;

    padding = ((8 - (size_t)th->decompressed_size) % 8);
    // Because we always allocated enough memory for one more elf_header and this is smaller than
    // the maximal padding of 7, we do not have to reallocate here.
    memset(*output, 0, padding);
    output_written += padding;

    *output_size = output_written;
    return input_read;
 error:
    free(*output);
    *output = NULL;
    return -1;
}

int GetFatbinInfo(FatHeader_t *fatbin, std::vector<KernelInfo_t*> *kernel_list, uint8_t** fatbin_mem, size_t* fatbin_size) {
    FatElfHeader_t* eh;
    FatTextHeader_t* th;
    const uint8_t *input_pos = NULL;
    const uint8_t *fatbin_data = NULL;
    uint8_t *text_data = NULL;
    size_t text_data_size = 0;
    size_t fatbin_total_size = 0;
    int ret = -1;
    if (fatbin == NULL || fatbin_mem == NULL || fatbin_size == NULL) {
        tool::Logging(LOG_ERROR, myName, "at least one parameter is NULL\n");
        goto error;
    }
    fatbin_data = input_pos = (const uint8_t*)fatbin->text;
    if (fatbin->magic != FATBIN_STRUCT_MAGIC) {
        tool::Logging(LOG_ERROR, myName, "fatbin struct magic number is wrong. Got %llx, expected %llx.\n", fatbin->magic, FATBIN_STRUCT_MAGIC);
        goto error;
    }
    tool::Logging(LOG_REGS, myName, "Fatbin: magic: %x, version: %x, text: %lx, data: %lx, ptr: %lx, ptr2: %lx, zero: %lx\n",
           fatbin->magic, fatbin->version, fatbin->text, fatbin->data, fatbin->unknown, fatbin->text2, fatbin->zero);

    if (GetFatElfHeader((uint8_t*)fatbin->text, sizeof(FatElfHeader_t), &eh) != 0) {
        tool::Logging(LOG_ERROR, myName, "Something went wrong while checking the elf header.\n");
        goto error;
    }
    // tool::Logging(LOG_REGS, myName, "elf header: magic: %#x, version: %#x, header_size: %#x, size: %#zx",
    //        eh->magic, eh->version, eh->header_size, eh->size); 

    input_pos += eh->header_size;
    fatbin_total_size = eh->header_size + eh->size;
    do {
        if (GetFatTextHeader(input_pos, *fatbin_size - (input_pos - fatbin_data) - eh->header_size, &th) != 0) {
            tool::Logging(LOG_ERROR, myName, "Something went wrong while checking the text header.\n");
            goto error;
        }
        //print_header(th);
        input_pos += th->header_size;
        if (th->kind != 2) { // section does not cotain device code (but e.g. PTX)
            input_pos += th->size;
            continue;
        }
        if (th->flags & FATBIN_FLAG_DEBUG) {
            tool::Logging(LOG_REGS, myName, "fatbin contains debug information.\n");
        }

        if (th->flags & FATBIN_FLAG_COMPRESS) {
            ssize_t input_read;

            tool::Logging(LOG_REGS, myName, "fatbin contains compressed device code. Decompressing...\n");
            if ((input_read = DecompressSingleSection(input_pos, &text_data, &text_data_size, eh, th)) < 0) {
                tool::Logging(LOG_ERROR, myName, "Something went wrong while decompressing text section.\n");
                goto error;
            }
            input_pos += input_read;
            //hexdump(text_data, text_data_size);
        } else {
            text_data = (uint8_t*)input_pos;
            text_data_size = th->size;
            input_pos += th->size;
        }
        // print_header(th);
        if (GetParameterInfo(kernel_list, text_data , text_data_size) != 0) {
            tool::Logging(LOG_ERROR, myName, "error getting parameter info\n");
            goto error;
        }
        if (th->flags & FATBIN_FLAG_COMPRESS) {
            free(text_data);
        }
    } while (input_pos < (uint8_t*)eh + eh->header_size + eh->size);

    // if (get_elf_header((uint8_t*)fatbin->text2, sizeof(struct fat_elf_header), &eh) != 0) {
    //     tool::Logging(LOG_ERROR, myName, "Something went wrong while checking the header.");
    //     goto error;
    // }
    // fatbin_total_size += eh->header_size + eh->size;

    *fatbin_mem = (uint8_t*)fatbin->text;
    *fatbin_size = fatbin_total_size;
    ret = 0;
 error:
    return ret;
}

static int GetSectionByName(Elf *elf, const char *name, Elf_Scn **section)
{
    Elf_Scn *scn = NULL;
    GElf_Shdr shdr;
    char *section_name = NULL;
    size_t str_section_index;

    if (elf == NULL || name == NULL || section == NULL) {
        tool::Logging(LOG_ERROR, myName, "invalid argument\n");
        return -1;
    }

    if (elf_getshdrstrndx(elf, &str_section_index) != 0) {
        tool::Logging(LOG_ERROR, myName, "elf_getshstrndx failed\n");
        return -1;
    }

    while ((scn = elf_nextscn(elf, scn)) != NULL) {
        if (gelf_getshdr(scn, &shdr) != &shdr) {
            tool::Logging(LOG_ERROR, myName, "gelf_getshdr failed\n");
            return -1;
        }
        if ((section_name = elf_strptr(elf, str_section_index, shdr.sh_name)) == NULL) {
            tool::Logging(LOG_ERROR, myName, "elf_strptr failed\n");
            return -1;
        }
        if (strcmp(section_name, name) == 0) {
            *section = scn;
            return 0;
        }
    }
    return -1;
}

static char* GetKernelSectionFromKernelName(const char *kernel_name)
{
    char *section_name = NULL;
    if (kernel_name == NULL) {
        tool::Logging(LOG_ERROR, myName, "invalid argument\n");
        return NULL;
    }

    if (kernel_name[0] == '$') {
        const char *p;
        if ((p = strchr(kernel_name+1, '$')) == NULL) {
            tool::Logging(LOG_ERROR, myName, "invalid kernel name\n");
            return NULL;
        }
        int len = (p - kernel_name) - 1;
        if (asprintf(&section_name, ".nv.info.%.*s", len, kernel_name+1) == -1) {
            tool::Logging(LOG_ERROR, myName, "asprintf failed\n");
            return NULL;
        }
    } else {
        if (asprintf(&section_name, ".nv.info.%s", kernel_name) == -1) {
            tool::Logging(LOG_ERROR, myName, "asprintf failed\n");
            return NULL;
        }
    }
    return section_name;
}

static int GetParaForKernel(Elf *elf, KernelInfo_t *kernel, void* memory, size_t memsize)
{
    struct __attribute__((__packed__)) nv_info_kernel_entry {
        uint8_t format;
        uint8_t attribute;
        uint16_t values_size;
        uint32_t values;
    };
    struct __attribute__((__packed__)) nv_info_kparam_info {
        uint32_t index;
        uint16_t ordinal;
        uint16_t offset;
        uint16_t unknown : 12;
        uint8_t  cbank : 6;
        uint16_t size : 14;
        // missing are "space" (possible padding info?), and "Pointee's logAlignment"
        // these were always 0 in the kernels I tested
    };
    int ret = -1;
    char *section_name = NULL;
    Elf_Scn *section = NULL;
    Elf_Data *data = NULL;
    size_t secpos=0;
    int i=0;

    if (kernel == NULL || kernel->name == NULL || memory == NULL) {
        tool::Logging(LOG_ERROR, myName, "at least one parameter is NULL\n");
        goto cleanup;
    }
    kernel->paramNum = 0;
    kernel->paramSize = 0;
    kernel->paramOffsets = NULL;
    kernel->paramSizes = NULL;

    if ((section_name = GetKernelSectionFromKernelName(kernel->name)) == NULL) {
        tool::Logging(LOG_ERROR, myName, "GetKernelSectionFromKernelName failed\n");
        goto cleanup;
    }

    if (GetSectionByName(elf, section_name, &section) != 0) {
        tool::Logging(LOG_ERROR, myName, "section %s not found\n", section_name);
        goto cleanup;
    }

    if ((data = elf_getdata(section, NULL)) == NULL) {
        tool::Logging(LOG_ERROR, myName, "error getting section data\n");
        goto cleanup;
    }

    while (secpos < data->d_size) {
        struct nv_info_kernel_entry *entry = (struct nv_info_kernel_entry*)((uint8_t*)data->d_buf+secpos);
        // printf("entry %d: format: %#x, attr: %#x, ", i++, entry->format, entry->attribute);
        if (entry->format == EIFMT_SVAL && entry->attribute == EIATTR_KPARAM_INFO) {
            if (entry->values_size != 0xc) {
                tool::Logging(LOG_ERROR, myName, "EIATTR_KPARAM_INFO values size has not the expected value of 0xc\n");
                goto cleanup;
            }
            struct nv_info_kparam_info *kparam = (struct nv_info_kparam_info*)&entry->values;
            // printf("kparam: index: %#x, ordinal: %#x, offset: %#x, unknown: %#0x, cbank: %#0x, size: %#0x\n",
            //     kparam->index, kparam->ordinal, kparam->offset, kparam->unknown, kparam->cbank, kparam->size);
            tool::Logging(LOG_REGS, myName, "param %d: offset: %#x, size: %#x\n", kparam->ordinal, kparam->offset, kparam->size);
            if (kparam->ordinal >= kernel->paramNum) {
                kernel->paramOffsets = (uint16_t*)realloc(kernel->paramOffsets,
                                              (kparam->ordinal+1)*sizeof(uint16_t));
                kernel->paramSizes = (uint16_t*)realloc(kernel->paramSizes,
                                            (kparam->ordinal+1)*sizeof(uint16_t));
                kernel->paramNum = kparam->ordinal+1;
            }
            kernel->paramOffsets[kparam->ordinal] = kparam->offset;
            kernel->paramSizes[kparam->ordinal] = kparam->size;
            secpos += sizeof(struct nv_info_kernel_entry) + entry->values_size-4;
        } else if (entry->format == EIFMT_HVAL && entry->attribute == EIATTR_CBANK_PARAM_SIZE) {
            kernel->paramSize = entry->values_size;
            tool::Logging(LOG_REGS, myName, "cbank_param_size: %#0x\n", entry->values_size);
            secpos += sizeof(struct nv_info_kernel_entry)-4;
        } else if (entry->format == EIFMT_HVAL) {
            // printf("hval: %#x(%d)\n", entry->values_size, entry->values_size);
            secpos += sizeof(struct nv_info_kernel_entry)-4;
        } else if (entry->format == EIFMT_SVAL) {
            // printf("sval_size: %#x ", entry->values_size);
            // for (int j=0; j*sizeof(uint32_t) < entry->values_size; j++) {
            //     printf("val%d: %#x(%d) ", j, (&entry->values)[j], (&entry->values)[j]);
            // }
            // printf("\n");
            secpos += sizeof(struct nv_info_kernel_entry) + entry->values_size-4;
        } else if (entry->format == EIFMT_NVAL) {
            // printf("nval\n");
            secpos += sizeof(struct nv_info_kernel_entry)-4;
        } else {
            tool::Logging(LOG_REGS, myName, "unknown format: %#x\n", entry->format);
            secpos += sizeof(struct nv_info_kernel_entry)-4;
        }
    }
    // printf("remaining: %d\n", data->d_size % sizeof(struct nv_info_kernel_entry));
    ret = 0;
 cleanup:
    free(section_name);
    return ret;
}

static int GetSymbolTable(Elf *elf, Elf_Data **symbol_table_data, size_t *symbol_table_size, GElf_Shdr *symbol_table_shdr)
{
    GElf_Shdr shdr;
    Elf_Scn *section = NULL;

    if (elf == NULL || symbol_table_data == NULL || symbol_table_size == NULL) {
        tool::Logging(LOG_ERROR, myName, "invalid argument\n");
        return -1;
    }

    if (GetSectionByName(elf, ".symtab", &section) != 0) {
        tool::Logging(LOG_ERROR, myName, "could not find .symtab section\n");
        return -1;
    }

    if (gelf_getshdr(section, &shdr) == NULL) {
        tool::Logging(LOG_ERROR, myName, "gelf_getshdr failed\n");
        return -1;
    }

    if (symbol_table_shdr != NULL) {
        *symbol_table_shdr = shdr;
    }

    if(shdr.sh_type != SHT_SYMTAB) {
        tool::Logging(LOG_ERROR, myName, "not a symbol table: %d\n", shdr.sh_type);
        return -1;
    }

    if ((*symbol_table_data = elf_getdata(section, NULL)) == NULL) {
        tool::Logging(LOG_ERROR, myName, "elf_getdata failed\n");
        return -1;
    }

    *symbol_table_size = shdr.sh_size / shdr.sh_entsize;

    return 0;
}

static int CheckElf(Elf *elf)
{
    Elf_Kind ek;
    GElf_Ehdr ehdr;

    int elfclass;
    char *id;
    size_t program_header_num;
    size_t sections_num;
    size_t section_str_num;
    int ret = -1;

    if ((ek = elf_kind(elf)) != ELF_K_ELF) {
        tool::Logging(LOG_ERROR, myName, "elf_kind is not ELF_K_ELF, but %d\n", ek);
        goto cleanup;
    }

    if (gelf_getehdr(elf, &ehdr) == NULL) {
        tool::Logging(LOG_ERROR, myName, "gelf_getehdr failed\n");
        goto cleanup;
    }

    if ((elfclass = gelf_getclass(elf)) == ELFCLASSNONE) {
        tool::Logging(LOG_ERROR, myName, "gelf_getclass failed\n");
        goto cleanup;
    }

    if ((id = elf_getident(elf, NULL)) == NULL) {
        tool::Logging(LOG_ERROR, myName, "elf_getident failed\n");
        goto cleanup;
    }

    tool::Logging(LOG_REGS, myName, "elfclass: %d-bit; elf ident[0..%d]: %7s\n",
        (elfclass == ELFCLASS32) ? 32 : 64,
        EI_ABIVERSION, id);

    if (elf_getshdrnum(elf, &sections_num) != 0) {
        tool::Logging(LOG_ERROR, myName, "elf_getphdrnum failed\n");
        goto cleanup;
    }

    if (elf_getphdrnum(elf, &program_header_num) != 0) {
        tool::Logging(LOG_ERROR, myName, "elf_getshdrnum failed\n");
        goto cleanup;
    }

    if (elf_getshdrstrndx(elf, &section_str_num) != 0) {
        tool::Logging(LOG_ERROR, myName, "elf_getshstrndx Wfailed\n");
        goto cleanup;
    }

    tool::Logging(LOG_REGS, myName, "elf contains %d sections, %d program_headers, string table section: %d\n",
        sections_num, program_header_num, section_str_num);

    ret = 0;
cleanup:
    return ret;
}

int GetParameterInfo(std::vector<KernelInfo_t*> *kernel_list, void* memory, size_t memsize){
    struct __attribute__((__packed__)) nv_info_entry{
        uint8_t format;
        uint8_t attribute;
        uint16_t values_size;
        uint32_t kernel_id;
        uint32_t value;
    };

    Elf *elf = NULL;
    Elf_Scn *section = NULL;
    Elf_Data *data = NULL, *symbol_table_data = NULL;
    GElf_Shdr symtab_shdr;
    size_t symnum;
    int i = 0;
    GElf_Sym sym;

    int ret = -1;
    KernelInfo_t *ki = NULL;
    const char *kernel_str;

    if (memory == NULL || memsize == 0) {
        tool::Logging(LOG_ERROR, myName, "memory was NULL or memsize was 0\n");
        return -1;
    }

// #define ELF_DUMP_TO_FILE 1

// #ifdef ELF_DUMP_TO_FILE
    // FILE* fd2 = fopen("flexgv-elf-dump", "wb");
    // fwrite(memory, memsize, 1, fd2);
    // fclose(fd2);
// #endif

    if ((elf = elf_memory((char*)memory, memsize)) == NULL) {
        tool::Logging(LOG_ERROR, myName, "elf_memory failed\n");
        goto cleanup;
    }

    if (CheckElf(elf) != 0) {
        tool::Logging(LOG_ERROR, myName, "check_elf failed\n");
        goto cleanup;
    }

    if (GetSymbolTable(elf, &symbol_table_data, &symnum, &symtab_shdr) != 0) {
        tool::Logging(LOG_ERROR, myName, "could not get symbol table\n");
        goto cleanup;
    }

    if (GetSectionByName(elf, ".nv.info", &section) != 0) {
        tool::Logging(LOG_REGS, myName, "could not find .nv.info section. This means this binary does not contain any kernels.\n");
        ret = 0;    // This is not an error.
        goto cleanup;
    }

    if ((data = elf_getdata(section, NULL)) == NULL) {
        tool::Logging(LOG_ERROR, myName, "elf_getdata failed\n");
        goto cleanup;
    }

    for (size_t secpos=0; secpos < data->d_size; secpos += sizeof(struct nv_info_entry)) {
        struct nv_info_entry *entry = (struct nv_info_entry *)((uint8_t*)data->d_buf+secpos);
        // tool::Logging(LOG_REGS, myName, "%d: format: %#x, attr: %#x, values_size: %#x kernel: %#x, sval: %#x(%d)", 
        // i++, entry->format, entry->attribute, entry->values_size, entry->kernel_id, 
        // entry->value, entry->value);

        if (entry->values_size != 8) {
            tool::Logging(LOG_ERROR, myName, "unexpected values_size: %#x\n", entry->values_size);
            continue;
        }

        if (entry->attribute != EIATTR_FRAME_SIZE) {
            continue;
        }

        if (entry->kernel_id >= symnum) {
            tool::Logging(LOG_ERROR, myName, "kernel_id out of bounds: %#x\n", entry->kernel_id);
            continue;
        }

        if (gelf_getsym(symbol_table_data, entry->kernel_id, &sym) == NULL) {
            tool::Logging(LOG_ERROR, myName, "gelf_getsym failed for entry %d\n", entry->kernel_id);
            continue;
        }

        if ((kernel_str = elf_strptr(elf, symtab_shdr.sh_link, sym.st_name) ) == NULL) {
            tool::Logging(LOG_ERROR, myName, "strptr failed for entry %d\n", entry->kernel_id);
            continue;
        }

        /* When using (some?) intrinsics, nvcc adds symbols for them in the .nv.info table.
        * They are prefixed with $__internal_7_$ and are not kernels. We skip them he
        */
        const char *intrinsics_prefix = "$__internal_";
        if (strncmp(kernel_str, intrinsics_prefix, strlen(intrinsics_prefix)) == 0) {
            continue;
        }

        if (GetKernelInfoByKernelName(kernel_list, kernel_str) != NULL) {
            continue;
        }

        tool::Logging(LOG_REGS, myName, "found new kernel: %s (symbol table id: %#x)\n", kernel_str, entry->kernel_id);

        ki = (KernelInfo_t*)malloc(sizeof(KernelInfo_t));
        kernel_list->push_back(ki);        

        size_t buflen = strlen(kernel_str)+1;
        if ((ki->name = (char*)malloc(buflen)) == NULL) {
            tool::Logging(LOG_ERROR, myName, "malloc failed\n");
            goto cleanup;
        }
        if (strncpy(ki->name, kernel_str, buflen) != ki->name) {
            tool::Logging(LOG_ERROR, myName, "strncpy failed\n");
            goto cleanup;
        }

        if (GetParaForKernel(elf, ki, memory, memsize) != 0) {
            tool::Logging(LOG_ERROR, myName, "GetParaForKernel failed for kernel %s\n", kernel_str);
            goto cleanup;
        }
    }

    ret = 0;
 cleanup:
    if (elf != NULL) {
        elf_end(elf);
    }
    return ret;
}

KernelInfo_t* GetKernelInfoByKernelName(std::vector<KernelInfo_t*> *kernel_list, const char* kernelName) {
    if (kernel_list == NULL) {
        tool::Logging(LOG_ERROR, myName, "kernelMap is NULL\n");
        return NULL;        
    }
    if (kernel_list->empty()) {
        return NULL;
    }
    for (auto ki = kernel_list->begin(); ki != kernel_list->end(); ki++) {
        if (strcmp((*ki)->name, kernelName) == 0) {
            return *ki;
        }
    }
    return NULL;
}