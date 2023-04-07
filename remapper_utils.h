#ifndef REMAPPER_UTILS_H_
#define REMAPPER_UTILS_H_

#include <stdint.h>

/*  i know there are structures
    but i'm not sure if it's portable in cross compilation
    and that kind of thing */

#if INTPTR_MAX == INT32_MAX
typedef uint32_t elf_pos_t;
typedef uint32_t elf_size_t;

/* ELF header offsets */
# define ELF_HDR_TBL_OFFSET 0x20
# define ELF_SCT_ENTRY_SIZE 0x2E
# define ELF_SCT_CNT_OFFSET 0x30
# define ELF_NAM_SCT_OFFSET 0x32

/* section header offsets */
# define SCTHDR_VIRT_ADDR   0x0C
# define SCTHDR_IMG_OFFSET  0x10
# define SCTHDR_SCT_LENGTH  0x14

# define MPROTECT_ASM(page, pagesize, rights)                           \
    {int __dummy;                                                       \
    __asm__ volatile                                                    \
    (                                                                   \
        "int $0x80"                                                     \
        :   "=a" (__dummy)                                              \
        :   "0"(SYS_mprotect), "b"(page),                               \
            "c"(pagesize), "d"(rights)                                  \
        : "memory"                                                      \
    ); }


# define MUNMAP_ASM(page, pagesize)                                     \
    {int __dummy;                                                       \
    __asm__ volatile                                                    \
    (                                                                   \
        "int $0x80"                                                     \
        : "=a" (__dummy)                                                \
        : "0"(SYS_munmap), "b"(page), "c"(pagesize)                     \
        : "memory"                                                      \
    ); }

# define MMAP_ASM(page, pagesize, rights, flags, fd, offset)                \
    {                                                                       \
    void *__dummy;                                                          \
    size_t __offset = offset;\
    __asm__ volatile                                                        \
    (                                                                       \
        "int $0x80"                                                         \
        : "=a" (__dummy)                                                    \
        :   "a"(SYS_mmap), "b"(page), "c"(pagesize),                        \
            "d"(rights), "S"(flags), "D"(fd)                                \
        : "memory"                                                          \
    );                                                                      \
    }
#elif INTPTR_MAX == INT64_MAX
typedef uint64_t elf_pos_t;
typedef uint64_t elf_size_t;

/* ELF header offsets */
# define ELF_HDR_TBL_OFFSET 0x28
# define ELF_SCT_ENTRY_SIZE 0x3A
# define ELF_SCT_CNT_OFFSET 0x3C
# define ELF_NAM_SCT_OFFSET 0x3E

/* section header offsets */
# define SCTHDR_VIRT_ADDR   0x10
# define SCTHDR_IMG_OFFSET  0x18
# define SCTHDR_SCT_LENGTH  0x20


# define MPROTECT_ASM(page, pagesize, rights)                           \
    {int __dummy;                                                       \
    __asm__ volatile                                                    \
    (                                                                   \
        "syscall"                                                       \
        :   "=a" (__dummy)                                              \
        :   "0"(SYS_mprotect), "D"(page),                               \
            "S"(pagesize), "d"(rights)                                  \
        : "rcx", "r11", "memory"                                        \
    ); }

# define MUNMAP_ASM(page, pagesize)                                     \
    {int __dummy;                                                       \
    __asm__ volatile                                                    \
    (                                                                   \
        "syscall"                                                       \
        : "=a" (__dummy)                                                \
        : "0"(SYS_munmap), "D"(page), "S"(pagesize)                     \
        : "rcx", "r11", "memory"                                        \
    ); }

# define MMAP_ASM(page, pagesize, rights, flags, fd, offset)                \
    {                                                                       \
    void *__dummy;                                                          \
    register long r10 __asm__("r10") = flags;                               \
    register long r8 __asm__("r8") = fd;                                    \
    register long r9 __asm__("r9") = offset;                                \
    __asm__ volatile                                                        \
    (                                                                       \
        "syscall"                                                           \
        : "=a" (__dummy)                                                    \
        :   "0"(SYS_mmap), "D"(page), "S"(pagesize),                        \
            "d"(rights), "r"(r10), "r"(r8), "r"(r9)                         \
        : "rcx", "r11", "memory"                                            \
    );                                                                      \
    }

#else
# error "Environment not 32 or 64-bit."
#endif


#endif /* REMAPPER_UTILS_H_ */