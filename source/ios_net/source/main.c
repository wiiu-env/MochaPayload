#include "fsa.h"
#include <stdio.h>

uint32_t DLP_FSAInit_patch(void *u1) {
    int (*const real_DLP_FSAInit_patch)(void *) = (void *) 0x0123c0a1c;
    int handle                                  = real_DLP_FSAInit_patch(u1);
    if (handle != -1) {
        if (FSA_Mount(handle, "/dev/sdcard01", "/vol/dlp__sd", 0, 0, 0) == 0) {
            printf("Mocha (DLP): Mounted sd card for handle (%08X).\n", handle);
        } else {
            printf("Mocha (DLP): Failed to mount sd card for handle (%08X).\n", handle);
        }
    } else {
        printf("Mocha (DLP): Getting FSAClient failed. Could not mount sd card.\n");
    }
    return handle;
}

uint32_t DLP_FSADeinit_patch(int fsaHandle) {
    uint32_t (*const real_NET_DLP_deinit_patch)(int) = (void *) 0x123c0948;
    if (FSA_Unmount(fsaHandle, "/vol/dlp__sd", 0) == 0) {
        printf("Mocha (DLP): Unmounted sd card for handle (%08X).\n", fsaHandle);
    } else {
        printf("Mocha (DLP): Failed to unmount sd card for handle (%08X).\n", fsaHandle);
    }
    return real_NET_DLP_deinit_patch(fsaHandle);
}


uint32_t DLP_FSA_OpenFile(int fd, char *path, char *mode, uint32_t *outhandle) {
    int (*const real_DLP_FSA_OpenFile)(int, char *, char *, uint32_t *) = (void *) 0x123c9480;
    int result                                                          = real_DLP_FSA_OpenFile(fd, path, mode, outhandle);
    printf("DLP_FSA_OpenFile(%08X %s %s %p) returned %d \n", fd, path, mode, outhandle, result);
    return result;
}

uint32_t DLP_GetChildTitleId(uint32_t *titleId, uint32_t uniqueId, int fsaHandle, const char *path, uint8_t childIndex) {
    int (*const real_DLP_GetChildTitleId)(uint32_t * titleId, uint32_t uniqueId, int fsaHandle, const char *, uint8_t childIndex) = (void *) 0x1239bd38;
    int result                                                                                                                    = real_DLP_GetChildTitleId(titleId, uniqueId, fsaHandle, path, childIndex);
    printf("DLP_GetChildTitleId(%08X%08X unique %08X handle %08X path \"%s\" childindex %02X) returned %d \n", titleId[0], titleId[1], uniqueId, fsaHandle, path, childIndex, result);
    return result;
}
