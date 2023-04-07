/**
 * @file remapper.h
 * @author 5IGI0
 * @brief [really] loads the binary into memory, and then shreds it
 * @version 0.1
 * @date 2023-02-02
 *
 * @copyright Copyright (c) 2023
 *
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include <unistd.h>

#include <sys/syscall.h>
#include <sys/mman.h>

#ifndef REMAP_SECTION_NAME
#define REMAP_SECTION_NAME ".offset_detect"
#endif

#define MAPPINGS_SIZE 100
#define PAGE_LIST_SIZE 4000
#ifndef REMAP_NEW_CONTENT
static char new_ELF_content[] = "may god be with you <3\n";
#else
static char new_ELF_content[] = REMAP_NEW_CONTENT;
#endif

#include "remapper_utils.h"

#define TRUE_OR_RET(cond)     \
    {                         \
        if ((cond) == 0)      \
        {                     \
            return -__LINE__; \
        }                     \
    }

#define MAPPER_CODE_SIZE ((size_t)(((void *)payload_end_ptr) - ((void *)remapper_main)))
#define ROUND_VAR(a, b) ((a / b) * b)

typedef struct {
    void *mem_addr;
    size_t len;
} mapping_t;

static void remapper_main(void **pages, size_t pagesize, void *tmpbuff);
static void payload_end_ptr(void);
static int get_mapping(FILE *fp, mapping_t *mappings);

static void close_memfd(void) {
    char a[127] = "";
    char *tmp;
    sprintf(a, "%d_memfd", getpid());
    tmp = getenv(a);

    if (tmp != NULL) {
        int fd = atoi(tmp);
        ftruncate(fd, 0);                                    
        write(fd, new_ELF_content, sizeof(new_ELF_content));
        close(fd);
    }
}

static void add_page(void *page, void **pages) {
    for (size_t i = 0; i < PAGE_LIST_SIZE; i++) {
        if (pages[i] == page)
            return;
        
        if (pages[i] == NULL) {
            pages[i] = page;
            return;
        }
    }
}

int remap_binary(int fd) {
    mapping_t mappings[MAPPINGS_SIZE];
    size_t pagesize = getpagesize();
    FILE *fp = fdopen(fd, "r"); /* to use libc's optimizations */
    void *pages[PAGE_LIST_SIZE];
    char *tmpbuff;
    char buff[4];

    memset(mappings, 0, sizeof(mappings));
    memset(pages, 0, sizeof(pages));

    TRUE_OR_RET(fseek(fp, 0, SEEK_SET) == 0)
    TRUE_OR_RET(fread(buff, 1, 4, fp) == 4)
    TRUE_OR_RET(memcmp((char[]){'\x7F', 'E', 'L', 'F'}, buff, 4) == 0)

    // read ELF file to extract mappings
    TRUE_OR_RET(get_mapping(fp, mappings) == 0)

    void *mapped = mmap(NULL, MAPPER_CODE_SIZE, PROT_EXEC | PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    TRUE_OR_RET(mapped != (void *)-1)
    
    // change permission on the upper and lower pages
    void *rounded_page = ((uintptr_t)remapper_main) & (pagesize-1);
    mprotect(rounded_page, pagesize<<1, PROT_READ | PROT_WRITE | PROT_EXEC);

    // copy remapper's code to the new mapped memory
    memcpy(mapped, (void *)remapper_main, MAPPER_CODE_SIZE);

    // compute every pages to be remapped (unlinked to the original file)
    for (size_t i = 0; i < MAPPINGS_SIZE; i++) {
        if (mappings[i].mem_addr == NULL)
            continue;

        for (
            void *start_page = (void *)ROUND_VAR((intptr_t)mappings[i].mem_addr, pagesize);
            start_page < (mappings[i].mem_addr + mappings[i].len); start_page += pagesize) {
            add_page(start_page, pages);
        }
    }

#if REMAPPER_DEBUG_LOG
    puts("pages:");
    for (size_t i = 0; pages[i]; i++)
        printf("%zu: %p\n", i, pages[i]);
#endif

    tmpbuff = malloc(pagesize);
    TRUE_OR_RET(tmpbuff);

    void (*remapper_main_ptr)(void **, size_t, void *) = mapped;
    remapper_main_ptr(pages, pagesize, tmpbuff);

    free(tmpbuff);
    munmap(mapped, MAPPER_CODE_SIZE);
    
    close_memfd();
    
    ftruncate(fd, 0);
    write(fd, new_ELF_content, sizeof(new_ELF_content));
    close(fd);

    return 0;
}

static int copy_section(size_t section_pos, FILE *fp, char **dest)
{
    elf_pos_t section_name_pos;
    elf_size_t section_name_size;

    TRUE_OR_RET(fseek(fp, section_pos + SCTHDR_IMG_OFFSET, SEEK_SET) == 0);
    TRUE_OR_RET(fread(&section_name_pos, 1, sizeof(section_name_pos), fp) == sizeof(section_name_pos));
    TRUE_OR_RET(fseek(fp, section_pos + SCTHDR_SCT_LENGTH, SEEK_SET) == 0);
    TRUE_OR_RET(fread(&section_name_size, 1, sizeof(section_name_size), fp) == sizeof(section_name_size));

    TRUE_OR_RET(fseek(fp, section_name_pos, SEEK_SET) == 0);
    TRUE_OR_RET(*dest = malloc(section_name_size));
    TRUE_OR_RET(fread(*dest, 1, section_name_size, fp) == section_name_size);

    return 0;
}

int __attribute__((section(REMAP_SECTION_NAME))) detect_offset_var;
static int get_mapping(FILE *fp, mapping_t *mappings)
{
    char buff[0x40];
    elf_pos_t section_table;
    uint16_t section_count;
    uint16_t section_hdr_size;
    char *section_names;
    uintptr_t map_offset = 0;

    TRUE_OR_RET(fseek(fp, 0, SEEK_SET) == 0)
    TRUE_OR_RET(fread(buff, 1, 0x40, fp) == 0x40)

    section_table = *(elf_pos_t *)(&buff[ELF_HDR_TBL_OFFSET]);
    section_hdr_size = *(uint16_t *)(&buff[ELF_SCT_ENTRY_SIZE]);
    section_count = *(uint16_t *)(&buff[ELF_SCT_CNT_OFFSET]);

    TRUE_OR_RET(copy_section(
                    section_table + ((*(uint16_t *)(&buff[ELF_NAM_SCT_OFFSET])) * section_hdr_size),
                    fp, &section_names) == 0);

    for (size_t i = 0; i < section_count; i++)
    {
        TRUE_OR_RET(fseek(fp, section_table, SEEK_SET) == 0)
        section_table += section_hdr_size;

        TRUE_OR_RET(fread(buff, 1, 0x40, fp) >= section_hdr_size)

        mappings[i].mem_addr = *(void **)&buff[SCTHDR_VIRT_ADDR];
        mappings[i].len = *(elf_size_t *)&buff[SCTHDR_SCT_LENGTH];
        if (strcmp(section_names + (*(uint16_t *)buff), REMAP_SECTION_NAME) == 0)
            map_offset = ((void *)&detect_offset_var) - mappings[i].mem_addr;
    }

    for (size_t i = 0; i < section_count; i++)
    {
        if (mappings[i].mem_addr)
            mappings[i].mem_addr += map_offset;
    }

#if REMAPPER_DEBUG_LOG
    puts("sections:");
    for (size_t i = 0; i < section_count; i++)
    {
        printf("%zu: %p -> %zu\n", i, mappings[i].mem_addr, mappings[i].len);
    }
    fflush(stdout);
#endif

    free(section_names);

    return 0;
}

static inline __attribute__((always_inline)) void fn_memcpy(uint8_t *dst, uint8_t *src, size_t len)
{
    for (size_t i = 0; i < len; i++)
    {
        dst[i] = src[i];
    }
}

static void remapper_main(void **pages, size_t pagesize, void *tmpbuff)
{

    for (size_t i = 0; pages[i]; i++)
    {
        MPROTECT_ASM(pages[i], pagesize, PROT_READ | PROT_WRITE | PROT_EXEC);
        fn_memcpy(tmpbuff, pages[i], pagesize);
        MUNMAP_ASM(pages[i], pagesize);
        MMAP_ASM(pages[i], pagesize, PROT_EXEC | PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        fn_memcpy(pages[i], tmpbuff, pagesize);
    }
    return;
}

static void payload_end_ptr(void){};
