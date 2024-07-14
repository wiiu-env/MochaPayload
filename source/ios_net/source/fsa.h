#ifndef FSA_H
#define FSA_H

#include <stdint.h>

#define FSA_MOUNTFLAGS_BINDMOUNT (1 << 0)
#define FSA_MOUNTFLAGS_GLOBAL    (1 << 1)

int FSA_Mount(int fd, char *device_path, char *volume_path, uint32_t flags, char *arg_string, int arg_string_len);
int FSA_Unmount(int fd, char *path, uint32_t flags);

int FSA_OpenDir(int fd, char *path, int *outHandle);
int FSA_CloseDir(int fd, int handle);

#endif
