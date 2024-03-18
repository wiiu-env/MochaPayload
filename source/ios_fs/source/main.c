#include "fsa.h"
#include "ipc_types.h"
#include <stdint.h>
#include <stdio.h>

#define PATCHED_CLIENT_HANDLES_MAX_COUNT 0x40

// Disable raw access to every device except ODD and USB
#define DISABLED_CAPABILITIES                                                                                                       \
    (FSA_CAPABILITY_SLCCMPT_RAW_OPEN | FSA_CAPABILITY_SLC_RAW_OPEN | FSA_CAPABILITY_MLC_RAW_OPEN | FSA_CAPABILITY_SDCARD_RAW_OPEN | \
     FSA_CAPABILITY_HFIO_RAW_OPEN | FSA_CAPABILITY_RAMDISK_RAW_OPEN | FSA_CAPABILITY_OTHER_RAW_OPEN)

FSAClientHandle *patchedClientHandles[PATCHED_CLIENT_HANDLES_MAX_COUNT];

int (*const IOS_ResourceReply)(void *, int32_t) = (void *) 0x107f6b4c;

int FSA_ioctl0x28_hook(FSAClientHandle *handle, void *request) {
    int res = -5;
    for (int i = 0; i < PATCHED_CLIENT_HANDLES_MAX_COUNT; i++) {
        if (patchedClientHandles[i] == handle) {
            res = 0;
            break;
        }
        if (patchedClientHandles[i] == 0) {
            patchedClientHandles[i] = handle;
            res                     = 0;
            break;
        }
    }

    IOS_ResourceReply(request, res);
    return 0;
}

typedef struct __attribute__((packed)) {
    ipcmessage ipcmessage;
} ResourceRequest;

int (*const real_FSA_IOCTLV)(ResourceRequest *, uint32_t, uint32_t) = (void *) 0x10703164;
int (*const get_handle_from_val)(uint32_t)                          = (void *) 0x107046d4;
int FSA_IOCTLV_HOOK(ResourceRequest *param_1, uint32_t u2, uint32_t u3) {
    FSAClientHandle *clientHandle = (FSAClientHandle *) get_handle_from_val(param_1->ipcmessage.fd);
    uint64_t oldValue             = clientHandle->processData->capabilityMask;
    int toBeRestored              = 0;
    for (int i = 0; i < PATCHED_CLIENT_HANDLES_MAX_COUNT; i++) {
        if (patchedClientHandles[i] == clientHandle) {
            clientHandle->processData->capabilityMask = 0xffffffffffffffffL & ~DISABLED_CAPABILITIES;
            // printf("IOCTL: Force mask to 0xFFFFFFFFFFFFFFFF for client %08X\n", (uint32_t) clientHandle);
            toBeRestored = 1;
            break;
        }
    }
    int res = real_FSA_IOCTLV(param_1, u2, u3);

    if (toBeRestored) {
        // printf("IOCTL: Restore mask for client %08X\n", (uint32_t) clientHandle);
        clientHandle->processData->capabilityMask = oldValue;
    }

    return res;
}

int (*const real_FSA_IOCTL)(ResourceRequest *, uint32_t, uint32_t, uint32_t) = (void *) 0x107010a8;

int FSA_IOCTL_HOOK(ResourceRequest *request, uint32_t u2, uint32_t u3, uint32_t u4) {
    FSAClientHandle *clientHandle = (FSAClientHandle *) get_handle_from_val(request->ipcmessage.fd);
    uint64_t oldValue             = clientHandle->processData->capabilityMask;
    int toBeRestored              = 0;
    for (int i = 0; i < PATCHED_CLIENT_HANDLES_MAX_COUNT; i++) {
        if (patchedClientHandles[i] == clientHandle) {
            // printf("IOCTL: Force mask to 0xFFFFFFFFFFFFFFFF for client %08X\n", (uint32_t) clientHandle);
            clientHandle->processData->capabilityMask = 0xffffffffffffffffL & ~DISABLED_CAPABILITIES;
            toBeRestored                              = 1;
            break;
        }
    }
    int res = real_FSA_IOCTL(request, u2, u3, u4);

    if (toBeRestored) {
        // printf("IOCTL: Restore mask for client %08X\n", (uint32_t) clientHandle);
        clientHandle->processData->capabilityMask = oldValue;
    }

    return res;
}

int (*const real_FSA_IOS_Close)(uint32_t fd, ResourceRequest *request) = (void *) 0x10704864;
int FSA_IOS_Close_Hook(uint32_t fd, ResourceRequest *request) {
    FSAClientHandle *clientHandle = (FSAClientHandle *) get_handle_from_val(fd);
    for (int i = 0; i < PATCHED_CLIENT_HANDLES_MAX_COUNT; i++) {
        if (patchedClientHandles[i] == clientHandle) {
            // printf("Close: %p will be closed, reset slot %d\n", clientHandle, i);
            patchedClientHandles[i] = 0;
            break;
        }
    }
    return real_FSA_IOS_Close(fd, request);
}