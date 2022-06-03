#include "ipc_types.h"
#include <stdint.h>
#include <assert.h>

typedef struct __attribute__((packed)) {
    uint32_t initialized;
    uint64_t titleId;
    uint32_t processId;
    uint32_t groupId;
    uint32_t unk0;
    uint64_t capabilityMask;
    uint8_t unk1[0x4518];
    char unk2[0x280];
    char unk3[0x280];
    void* mutex;
} FSAProcessData;
static_assert(sizeof(FSAProcessData) == 0x4A3C, "FSAProcessData: wrong size");

typedef struct __attribute__((packed)) {
    uint32_t opened;
    FSAProcessData* processData;
    char unk0[0x10];
    char unk1[0x90];
    uint32_t unk2;
    char work_dir[0x280];
    uint32_t unk3;
} FSAClientHandle;
static_assert(sizeof(FSAClientHandle) == 0x330, "FSAClientHandle: wrong size");

FSAClientHandle *patchedClientHandles[0x64];

int FSA_ioctl0x28_hook(FSAClientHandle *handle) {
    int found = 0;
    for (int i = 0; i < 0x64; i++) {
        if (patchedClientHandles[i] == handle) {
            return 0;
        }
        if (patchedClientHandles[i] == 0) {
            patchedClientHandles[i] = handle;
            // printf("Will patch %p in the future. Slot %d\n", handle, i);
            found = 1;
            break;
        }
    }
    if (!found) {
        // printf("Failed to find free slot for %p\n", handle);
    }
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
    for (int i = 0; i < 64; i++) {
        if (patchedClientHandles[i] == clientHandle) {
            clientHandle->processData->capabilityMask = 0xffffffffffffffffL;
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
    for (int i = 0; i < 64; i++) {
        if (patchedClientHandles[i] == clientHandle) {
            // printf("IOCTL: Force mask to 0xFFFFFFFFFFFFFFFF for client %08X\n", (uint32_t) clientHandle);
            clientHandle->processData->capabilityMask = 0xffffffffffffffffL;
            toBeRestored                       = 1;
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
    for (int i = 0; i < 64; i++) {
        if (patchedClientHandles[i] == clientHandle) {
            // printf("Close: %p will be closed, reset slot %d\n", clientHandle, i);
            patchedClientHandles[i] = 0;
            break;
        }
    }
    return real_FSA_IOS_Close(fd, request);
}