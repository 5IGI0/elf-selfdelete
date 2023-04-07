#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>

#include <unistd.h>
#include <fcntl.h>

#include <sys/sendfile.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>

static char *get_memfd_env(void) {
    static char a[127] = "";

    if (a[0] == 0) {
        sprintf(a, "%d_memfd", getpid());
    }

    return a;
}

/**
 * this function add the memfd's number to the environment variables
 * to be able to shred it after
 */
static char * const *add_memfd(char * const envp[], int fd) {
    char  **ret;
    static char envar_tmp[127];
    size_t      envlen = 0;

    while (envp[envlen]) envlen++;

    ret = malloc(sizeof(char *)*(envlen+2));
    if (ret == NULL) return NULL;
    sprintf(envar_tmp, "%d_memfd=%d", getpid(), fd);

    for (size_t i = 0; i < envlen; i++) {
        ret[i] = (char *)envp[i];
    }
    ret[envlen] = envar_tmp;
    ret[envlen+1] = NULL;
    return (char * const*)ret;
}

/**
 * this functions try to open the binary's file in read-write mode
 * if there is an error, it will open in read-only mode, copy all the content
 * into a memfd, and restart on it (so the binary's file is no longer in use)
 */
int utils_get_rdwr_fd(const char *path, char * const argv[], char * const envp[]) {
    int fd      = -1;
    int memfd   = -1;
    struct stat stat_f;

    fd = open(path, O_RDWR);

    if (fd >= 0 || getenv(get_memfd_env()))
        return fd;

    fd = open(path, O_RDONLY | O_CLOEXEC);
    if (fd < 0)
        return -__LINE__;

    memfd = memfd_create("", O_RDWR);
    if (memfd < 0)
        return close(fd), -__LINE__;

    if (fstat(fd, &stat_f) < 0)
        return close(fd), close(memfd), -__LINE__;

    ssize_t fsnd_ret = sendfile(memfd, fd, NULL, stat_f.st_size);
    if (stat_f.st_size != fsnd_ret) {
        /* sendfile(2):
            Note that a successful call to sendfile() may write fewer bytes than requested;
            the caller should be prepared to retry the call if there were unsent bytes.
           me(7):
            i'm lazy so i won't. but if you want to make a PR, don't hesitate
        */
        return close(fd), close(memfd), -__LINE__;
    }

    char * const*new_env = add_memfd(envp, memfd);
    if (new_env == NULL)
        return close(fd), close(memfd), -__LINE__;
    
    fexecve(memfd, argv, new_env);

    return -__LINE__;
}