#include <stdio.h>
#include <err.h>

#include <unistd.h>

#include <fcntl.h>

#include "remap_utils.h"
#include "remapper.h"

int main(int argc, char * const argv[], char * const envp[]) {
    // some trick to open the binary's file in read-write mode
    int fd = utils_get_rdwr_fd(argv[0], argv, envp);
    unlink(argv[0]);

    (void)argc;

    if (fd < 0)
        err(1, "open");
    
    // remaping + shred the original binary
    remap_binary(fd);
    
    printf("look at my binary and `/proc/%d/exe`.\n", getpid());
    pause();
    return 0;
}